/*

 Copyright (c) 2013, SMB Phone Inc.
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 The views and conclusions contained in the software and documentation are those
 of the authors and should not be interpreted as representing official policies,
 either expressed or implied, of the FreeBSD Project.

 */

#include <hookflash/stack/internal/stack_Account.h>
#include <hookflash/stack/internal/stack_AccountPeerLocation.h>
#include <hookflash/stack/internal/stack_AccountFinder.h>
#include <hookflash/stack/internal/stack_MessageMonitor.h>
#include <hookflash/stack/internal/stack_Location.h>
#include <hookflash/stack/internal/stack_Helper.h>
#include <hookflash/stack/internal/stack_Peer.h>
#include <hookflash/stack/internal/stack_Stack.h>
#include <hookflash/stack/IPeerFiles.h>
#include <hookflash/stack/IPeerFilePublic.h>
#include <hookflash/stack/message/peer-to-peer/PeerIdentifyRequest.h>
#include <hookflash/stack/message/peer-to-peer/PeerKeepAliveRequest.h>
#include <hookflash/stack/message/peer-finder/PeerLocationFindRequest.h>
#include <hookflash/stack/message/peer-finder/PeerLocationFindResult.h>
#include <hookflash/stack/message/peer-finder/PeerLocationFindReply.h>
#include <hookflash/stack/message/IMessageHelper.h>

#include <zsLib/XML.h>
#include <zsLib/Log.h>
#include <zsLib/helpers.h>

#ifndef _WIN32
#include <unistd.h>
#endif //_WIN32

#define HOOKFLASH_STACK_PEER_TO_PEER_RUDP_CONNECTION_INFO "text/x-openpeer-json-plain"
#define HOOKFLASH_STACK_PEER_KEEP_ALIVE_REQUEST_TIMEOUT_IN_SECONDS (2*60)
#define HOOKFLASH_STACK_CONNECTION_MANAGER_PEER_IDENTIFY_EXPIRES_IN_SECONDS (2*60)

#define HOOKFLASH_STACK_ACCOUNT_PEER_LOCATION_SEND_ICE_KEEP_ALIVE_INDICATIONS_IN_SECONDS (15)
#define HOOKFLASH_STACK_ACCOUNT_PEER_LOCATION_EXPECT_SESSION_DATA_IN_SECONDS (50)

#define HOOKFLASH_STACK_ACCOUNT_PEER_LOCATION_BACKGROUNDING_TIMEOUT_IN_SECONDS (HOOKFLASH_STACK_ACCOUNT_PEER_LOCATION_EXPECT_SESSION_DATA_IN_SECONDS + 40)

namespace hookflash { namespace stack { ZS_DECLARE_SUBSYSTEM(hookflash_stack) } }


namespace hookflash
{
  namespace stack
  {
    namespace internal
    {
      using zsLib::Stringize;

      using message::peer_to_peer::PeerIdentifyRequest;
      using message::peer_to_peer::PeerIdentifyRequestPtr;
      using message::peer_to_peer::PeerKeepAliveRequest;
      using message::peer_to_peer::PeerKeepAliveRequestPtr;
      using message::peer_finder::PeerLocationFindReply;
      using message::peer_finder::PeerLocationFindReplyPtr;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountPeerLocationForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      AccountPeerLocationPtr IAccountPeerLocationForAccount::create(
                                                                    IAccountPeerLocationDelegatePtr delegate,
                                                                    AccountPtr outer,
                                                                    const LocationInfo &locationInfo
                                                                    )
      {
        return IAccountPeerLocationFactory::singleton().create(delegate, outer, locationInfo);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation
      #pragma mark

      //-----------------------------------------------------------------------
      AccountPeerLocation::AccountPeerLocation(
                                               IMessageQueuePtr queue,
                                               IAccountPeerLocationDelegatePtr delegate,
                                               AccountPtr outer,
                                               const LocationInfo &locationInfo
                                               ) :
        MessageQueueAssociator(queue),

        mID(zsLib::createPUID()),
        mDelegate(IAccountPeerLocationDelegateProxy::createWeak(IStackForInternal::queueStack(), delegate)),
        mOuter(outer),
        mCurrentState(IAccount::AccountState_Pending),
        mShouldRefindNow(false),
        mLastActivity(zsLib::now()),
        mLocationInfo(locationInfo),
        mLocation(Location::convert(locationInfo.mLocation)),
        mPeer(mLocation->forAccount().getPeer()),
        mIncoming(false)
      {
        ZS_LOG_BASIC(log("created"))
        ZS_THROW_BAD_STATE_IF(!mPeer)
      }

      //---------------------------------------------------------------------
      void AccountPeerLocation::init()
      {
        ZS_LOG_DEBUG(log("initialized") + getDebugValueString())
      }

      //---------------------------------------------------------------------
      AccountPeerLocation::~AccountPeerLocation()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_BASIC(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      String AccountPeerLocation::toDebugString(AccountPeerLocationPtr peerLocation, bool includeCommaPrefix)
      {
        if (!peerLocation) return includeCommaPrefix ? String(", account peer location=(null)") : String("account peer location=(null)");
        return peerLocation->getDebugValueString(includeCommaPrefix);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => IAccountPeerLocationForAccount
      #pragma mark

      //---------------------------------------------------------------------
      AccountPeerLocationPtr AccountPeerLocation::create(
                                                         IAccountPeerLocationDelegatePtr delegate,
                                                         AccountPtr outer,
                                                         const LocationInfo &locationInfo
                                                         )
      {
        AccountPeerLocationPtr pThis(new AccountPeerLocation(IStackForInternal::queueStack(), delegate, outer, locationInfo));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      LocationPtr AccountPeerLocation::getLocation() const
      {
        AutoRecursiveLock lock(getLock());
        return mLocation;
      }

      //-----------------------------------------------------------------------
      const LocationInfo &AccountPeerLocation::getLocationInfo() const
      {
        AutoRecursiveLock lock(getLock());
        return mLocationInfo;
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::shutdown()
      {
        AutoRecursiveLock lock(getLock());
        cancel();
      }

      //-----------------------------------------------------------------------
      IAccount::AccountStates AccountPeerLocation::getState() const
      {
        AutoRecursiveLock lock(getLock());
        return mCurrentState;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::shouldRefindNow() const
      {
        AutoRecursiveLock lock(getLock());
        bool refind = mShouldRefindNow;
        mShouldRefindNow = false;
        return refind;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::isConnected() const
      {
        AutoRecursiveLock lock(getLock());
        return isReady();
      }

      //-----------------------------------------------------------------------
      Time AccountPeerLocation::getTimeOfLastActivity() const
      {
        AutoRecursiveLock lock(getLock());
        return mLastActivity;
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::connectLocation(
                                                const CandidateList &candidates,
                                                ICEControls control
                                                )
      {
        AutoRecursiveLock lock(getLock());
        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_WARNING(Detail, log("received request to connect but location is already shutting down/shutdown"))
          return;
        }

        IRUDPICESocketPtr socket = getSocket();
        if (!socket) {
          ZS_LOG_WARNING(Detail, log("no ICE socket found for peer location"))
          return;
        }

        ZS_LOG_DETAIL(log("creating session from remote candidates") + ", total candidates=" + Stringize<size_t>(candidates.size()).string())

        mSocketSession = socket->createSessionFromRemoteCandidates(mThisWeak.lock(), candidates, control);

        if (mSocketSession) {
          ZS_LOG_DEBUG(log("setting keep alive properties for socket session"))
          mSocketSession->setKeepAliveProperties(
                                                 Seconds(HOOKFLASH_STACK_ACCOUNT_PEER_LOCATION_SEND_ICE_KEEP_ALIVE_INDICATIONS_IN_SECONDS),
                                                 Seconds(HOOKFLASH_STACK_ACCOUNT_PEER_LOCATION_EXPECT_SESSION_DATA_IN_SECONDS),
                                                 Duration(),
                                                 Seconds(HOOKFLASH_STACK_ACCOUNT_PEER_LOCATION_BACKGROUNDING_TIMEOUT_IN_SECONDS)
                                                 );
        } else {
          ZS_LOG_ERROR(Detail, log("failed to create socket session"))
        }

        (IAccountPeerLocationAsyncDelegateProxy::create(mThisWeak.lock()))->onStep();
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::incomingRespondWhenCandidatesReady(PeerLocationFindRequestPtr request)
      {
        AutoRecursiveLock lock(getLock());
        if (isShutdown()) return;

        mIncoming = true;

        mPendingRequests.push_back(request);
        (IAccountPeerLocationAsyncDelegateProxy::create(mThisWeak.lock()))->onStep();
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::hasReceivedCandidateInformation() const
      {
        AutoRecursiveLock lock(getLock());
        return mSocketSession;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::send(MessagePtr message) const
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!message)

        AutoRecursiveLock lock(getLock());

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("attempted to send a message but the location is shutdown"))
          return false;
        }

        if (!isReady()) {
          PeerIdentifyRequestPtr identifyRequest = PeerIdentifyRequest::convert(message);
          PeerIdentifyResultPtr identifyResult = PeerIdentifyResult::convert(message);
          if ((!identifyRequest) &&
              (!identifyResult)) {
            ZS_LOG_WARNING(Detail, log("attempted to send a message as the location is not ready"))
            return false;
          }
        }

        if (!mMessaging) {
          ZS_LOG_WARNING(Detail, log("requested to send a message but messaging is not ready"))
          return false;
        }

        DocumentPtr document = message->encode();

        boost::shared_array<char> output;
        ULONG length = 0;
        output = document->writeAsJSON(&length);

        ZS_LOG_DETAIL(log("-------------------------------------------------------------------------------------------"))
        ZS_LOG_DETAIL(log("> > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > >"))
        ZS_LOG_DETAIL(log("-------------------------------------------------------------------------------------------"))
        ZS_LOG_DETAIL(log("PEER SEND MESSAGE") + "=" + "\n" + ((CSTR)(output.get())) + "\n")
        ZS_LOG_DETAIL(log("-------------------------------------------------------------------------------------------"))
        ZS_LOG_DETAIL(log("> > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > >"))
        ZS_LOG_DETAIL(log("-------------------------------------------------------------------------------------------"))

        ZS_LOG_DETAIL(log("v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v"))
        ZS_LOG_DETAIL(log("||| MESSAGE INFO |||") + Message::toDebugString(message))
        ZS_LOG_DETAIL(log("^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^"))

        mLastActivity = zsLib::now();
        mMessaging->send((const BYTE *)(output.get()), length);
        return true;
      }

      //-----------------------------------------------------------------------
      IMessageMonitorPtr AccountPeerLocation::sendRequest(
                                                          IMessageMonitorDelegatePtr delegate,
                                                          MessagePtr requestMessage,
                                                          Duration timeout
                                                          ) const
      {
        IMessageMonitorPtr monitor = IMessageMonitor::monitor(delegate, requestMessage, timeout);
        if (!monitor) {
          ZS_LOG_WARNING(Detail, log("failed to create monitor"))
          return IMessageMonitorPtr();
        }

        bool result = send(requestMessage);
        if (!result) {
          // notify that the message requester failed to send the message...
          MessageMonitor::convert(monitor)->forAccountPeerLocation().notifyMessageSendFailed();
          return monitor;
        }

        ZS_LOG_DEBUG(log("request successfully created"))
        return monitor;
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::sendKeepAlive()
      {
        AutoRecursiveLock lock(getLock());

        if (mKeepAliveMonitor) {
          ZS_LOG_WARNING(Detail, log("keep alive requester is already in progress"))
          return;
        }

        AccountPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_ERROR(Debug, log("stack account appears to be gone thus cannot keep alive"))
          return;
        }

        PeerKeepAliveRequestPtr request = PeerKeepAliveRequest::create();
        request->domain(outer->forAccountPeerLocation().getDomain());

        mKeepAliveMonitor = sendRequest(IMessageMonitorResultDelegate<PeerKeepAliveResult>::convert(mThisWeak.lock()), request, Seconds(HOOKFLASH_STACK_PEER_KEEP_ALIVE_REQUEST_TIMEOUT_IN_SECONDS));
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => IAccountPeerLocationAsyncDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void AccountPeerLocation::onStep()
      {
        AutoRecursiveLock lock(getLock());
        ZS_LOG_DEBUG(log("on step"))
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => IRUDPICESocketDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void AccountPeerLocation::onRUDPICESocketStateChanged(
                                                            IRUDPICESocketPtr socket,
                                                            RUDPICESocketStates state
                                                            )
      {
        AutoRecursiveLock lock(getLock());
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => IRUDPICESocketSessionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void AccountPeerLocation::onRUDPICESocketSessionStateChanged(
                                                                   IRUDPICESocketSessionPtr session,
                                                                   RUDPICESocketSessionStates state
                                                                   )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!session)

        AutoRecursiveLock lock(getLock());

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("received socket session state after already shutdown") + ", session ID=" + Stringize<PUID>(session->getID()).string())
          return;
        }

        if (session != mSocketSession) {
          ZS_LOG_WARNING(Detail, log("received socket session state changed from an obsolete session") + ", session ID=" + Stringize<PUID>(session->getID()).string())
          return;
        }

        if ((IRUDPICESocketSession::RUDPICESocketSessionState_ShuttingDown == state) ||
            (IRUDPICESocketSession::RUDPICESocketSessionState_Shutdown == state))
        {
          IRUDPICESocketSession::RUDPICESocketSessionShutdownReasons reason = session->getShutdownReason();
          ZS_LOG_WARNING(Detail, log("notified RUDP ICE socket session is shutdown") + ", reason=" + IRUDPICESocketSession::toString(reason))
          if ((IRUDPICESocketSession::RUDPICESocketSessionShutdownReason_Timeout == reason) ||
              (IRUDPICESocketSession::RUDPICESocketSessionShutdownReason_BackgroundingTimeout)) {
            mShouldRefindNow = true;
          }
          cancel();
          return;
        }

        step();
      }

      //---------------------------------------------------------------------
      void AccountPeerLocation::onRUDPICESocketSessionChannelWaiting(IRUDPICESocketSessionPtr session)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!session)

        ZS_LOG_DEBUG(log("received RUDP channel waiting") + ", sessionID=" + Stringize<PUID>(session->getID()).string())

        AutoRecursiveLock lock(getLock());

        if (isShutdown()) return;
        if (isShuttingDown()) {
          ZS_LOG_WARNING(Debug, log("incoming RUDP session ignored during shutdown process"))
          return; // ignore incoming channels during the shutdown
        }

        if (session != mSocketSession) return;

        if (mMessaging) {
          if (IRUDPMessaging::RUDPMessagingState_Connected != mMessaging->getState())
          {
            ZS_LOG_WARNING(Debug, log("incoming RUDP session replacing existing RUDP session since the outgoing RUDP session was not connected"))
            // out messaging was not connected so use this one instead since it is connected
            IRUDPMessagingPtr messaging = IRUDPMessaging::acceptChannel(IStackForInternal::queueServices(), mSocketSession, mThisWeak.lock());
            if (!messaging) return;

            if (HOOKFLASH_STACK_PEER_TO_PEER_RUDP_CONNECTION_INFO != messaging->getRemoteConnectionInfo()) {
              ZS_LOG_WARNING(Detail, log("received unknown incoming connection type thus shutting down incoming connection") + ", type=" + messaging->getRemoteConnectionInfo())
              messaging->shutdown();
              return;
            }

            if (mIdentifyMonitor) {
              mIdentifyMonitor->cancel();
              mIdentifyMonitor.reset();
            }

            mMessaging->shutdown();
            mMessaging.reset();

            mMessaging = messaging;
            mIncoming = true;

            step();
            return;
          }

          ZS_LOG_WARNING(Detail, log("incoming RUDP session ignored since an outgoing RUDP session is already established"))

          // we already have a connected channel, so dump this one...
          IRUDPMessagingPtr messaging = IRUDPMessaging::acceptChannel(IStackForInternal::queueServices(), mSocketSession, mThisWeak.lock());
          messaging->shutdown();

          ZS_LOG_WARNING(Detail, log("sending keep alive as it is likely the remote party connection request is valid and our current session is stale"))
          sendKeepAlive();
          return;
        }

        ZS_LOG_DEBUG(log("incoming RUDP session being answered"))

        // no messaging present, accept this incoming channel
        mMessaging = IRUDPMessaging::acceptChannel(IStackForInternal::queueServices(), mSocketSession, mThisWeak.lock());
        mIncoming = true;

        if (HOOKFLASH_STACK_PEER_TO_PEER_RUDP_CONNECTION_INFO != mMessaging->getRemoteConnectionInfo()) {
          ZS_LOG_WARNING(Detail, log("received unknown incoming connection type thus shutting down incoming connection") + ", type=" + mMessaging->getRemoteConnectionInfo())

          mMessaging->shutdown();
          mMessaging.reset();
          return;
        }

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => IRUDPMessagingDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void AccountPeerLocation::onRUDPMessagingStateChanged(
                                                            IRUDPMessagingPtr messaging,
                                                            RUDPMessagingStates state
                                                            )
      {
        AutoRecursiveLock lock(getLock());
        if (isShutdown()) return;

        if (messaging != mMessaging) {
          ZS_LOG_WARNING(Detail, log("received messaging state changed from an obsolete RUDP messaging") + ", messaging ID=" + Stringize<PUID>(messaging->getID()).string())
          return;
        }

        if ((IRUDPMessaging::RUDPMessagingState_ShuttingDown == state) ||
            (IRUDPMessaging::RUDPMessagingState_Shutdown == state))
        {
          IRUDPMessaging::RUDPMessagingShutdownReasons reason = messaging->getShutdownReason();
          ZS_LOG_WARNING(Detail, log("notified messaging shutdown") + ", reason=" + IRUDPMessaging::toString(reason))
          if (IRUDPMessaging::RUDPMessagingShutdownReason_Timeout == reason) {
            mShouldRefindNow = true;
          }
          cancel();
          return;
        }

        step();
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::onRUDPMessagingReadReady(IRUDPMessagingPtr session)
      {
        typedef IRUDPMessaging::MessageBuffer MessageBuffer;

        AutoRecursiveLock lock(getLock());

        if (isShutdown()) return;

        if (session != mMessaging) {
          ZS_LOG_WARNING(Debug, log("messaging ready ready arrived for obsolete messaging"))
          return;
        }

        while (true) {
          MessageBuffer buffer = mMessaging->getBufferLargeEnoughForNextMessage();
          if (!buffer) return;

          mMessaging->receive(buffer.get());

          bool hasPeerFile = mPeer->forAccount().getPeerFilePublic();

          ZS_LOG_DETAIL(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_DETAIL(log("< < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < <"))
          ZS_LOG_DETAIL(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_DETAIL(log("PEER RECEIVED MESSAGE=") + "\n" + ((CSTR)(buffer.get())) + "\n")
          ZS_LOG_DETAIL(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_DETAIL(log("< < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < <"))
          ZS_LOG_DETAIL(log("-------------------------------------------------------------------------------------------"))

          mLastActivity = zsLib::now();

          DocumentPtr document = Document::createFromAutoDetect((CSTR)(buffer.get()));
          MessagePtr message = Message::create(document, mLocation);

          if (!message) {
            ZS_LOG_WARNING(Detail, log("failed to create a message object from incoming message"))
            return;
          }

          ZS_LOG_DETAIL(log("v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v"))
          ZS_LOG_DETAIL(log("||| MESSAGE INFO |||") + Message::toDebugString(message))
          ZS_LOG_DETAIL(log("^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^v^"))

          if (IMessageMonitor::handleMessageReceived(message)) {
            ZS_LOG_DEBUG(log("handled message via message handler"))
            return;
          }

          // this is something new/incoming from the remote server...
          AccountPtr outer = mOuter.lock();
          if (!outer) {
            ZS_LOG_WARNING(Detail, log("peer message is ignored since account is now gone"))
            return;
          }

          // can't process requests until the incoming is received, drop the message
          if (((!hasPeerFile) ||
               (mIncoming)) &&
               (Time() == mIdentifyTime)) {

            // scope: handle identify request
            {
              PeerIdentifyRequestPtr request = PeerIdentifyRequest::convert(message);
              if (!request) {
                ZS_LOG_WARNING(Detail, log("unable to forward message to account since message arrived before peer was identified"))
                goto identify_failure;
              }

              ZS_LOG_DEBUG(log("handling peer identify message"))

              IPeerFilesPtr peerFiles = outer->forAccountPeerLocation().getPeerFiles();
              if (!peerFiles) {
                ZS_LOG_ERROR(Detail, log("failed to obtain local peer files"))
                goto identify_failure;
              }

              IPeerFilePublicPtr peerFilePublic = peerFiles->getPeerFilePublic();
              if (!peerFiles) {
                ZS_LOG_ERROR(Detail, log("failed to obtain local public peer file"))
                goto identify_failure;
              }

              if (request->findSecret() != peerFilePublic->getFindSecret()) {
                ZS_LOG_ERROR(Detail, log("find secret did not match") + ", request find secret=" + request->findSecret() + IPeerFilePublic::toDebugString(peerFilePublic))
                goto identify_failure;
              }

              LocationPtr location = Location::convert(mLocationInfo.mLocation);

              if (location->forAccount().getPeerURI() != mPeer->forAccount().getPeerURI()) {
                ZS_LOG_ERROR(Detail, log("peer file does not match expecting peer URI") + ILocation::toDebugString(location) + IPeer::toDebugString(mPeer))
                goto identify_failure;
              }

              mIdentifyTime = zsLib::now();

              PeerIdentifyResultPtr result = PeerIdentifyResult::create(request);

              LocationPtr selfLocation = ILocationForAccount::getForLocal(outer);

              LocationInfoPtr info = selfLocation->forAccount().getLocationInfo();
              info->mCandidates.clear();

              result->locationInfo(*info);
              send(result);

              step();
              return;
            }

          identify_failure:
            {
              MessageResultPtr result = MessageResult::create(message, IHTTP::HTTPStatusCode_Forbidden);
              if (result) {
                send(result);
              }
            }
            return;
          }

          // handle incoming keep alives
          {
            PeerKeepAliveRequestPtr request = PeerKeepAliveRequest::convert(message);
            if (request) {
              ZS_LOG_DEBUG(log("handling incoming peer keep alive request"))

              PeerKeepAliveResultPtr result = PeerKeepAliveResult::create(request);
              send(result);
              return;
            }
          }

          ZS_LOG_DEBUG(log("unknown message, will forward message to account"))

          // send the message to the outer (asynchronously)
          try {
            mDelegate->onAccountPeerLocationMessageIncoming(mThisWeak.lock(), message);
          } catch (IAccountPeerLocationDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate gone"))
          }
        }
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::onRUDPMessagingWriteReady(IRUDPMessagingPtr session)
      {
        ZS_LOG_TRACE(log("RUDP messaging write ready (ignored)"))
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => IMessageMonitorResultDelegate<PeerIdentifyResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::handleMessageMonitorResultReceived(
                                                                   IMessageMonitorPtr monitor,
                                                                   PeerIdentifyResultPtr result
                                                                   )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mIdentifyMonitor) {
          ZS_LOG_WARNING(Detail, log("received a result on an obsolete monitor"))
          return false;
        }

        mIdentifyMonitor->cancel();
        mIdentifyMonitor.reset();

        LocationPtr location = Location::convert(result->locationInfo().mLocation);
        if (!location) {
          ZS_LOG_ERROR(Detail, log("location not properly identified"))
          cancel();
          return true;
        }

        if (location->forAccount().getPeerURI() != mPeer->forAccount().getPeerURI()) {
          ZS_LOG_ERROR(Detail, log("peer URL does not match expecting peer URI") + ILocation::toDebugString(location) + IPeer::toDebugString(mPeer))
          cancel();
          return true;
        }

        mIdentifyTime = zsLib::now();

        (IAccountPeerLocationAsyncDelegateProxy::create(mThisWeak.lock()))->onStep();
        return true;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::handleMessageMonitorErrorResultReceived(
                                                                        IMessageMonitorPtr monitor,
                                                                        PeerIdentifyResultPtr ignore, // will always be NULL
                                                                        MessageResultPtr result
                                                                        )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mIdentifyMonitor) {
          ZS_LOG_WARNING(Detail, log("received a result on an obsolete monitor"))
          return false;
        }

        ZS_LOG_ERROR(Detail, log("identify request received an error"))

        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => IMessageMonitorResultDelegate<PeerKeepAliveResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::handleMessageMonitorResultReceived(
                                                                   IMessageMonitorPtr monitor,
                                                                   PeerKeepAliveResultPtr result
                                                                   )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mKeepAliveMonitor) {
          ZS_LOG_WARNING(Detail, log("received a result on an obsolete monitor"))
          return false;
        }

        mKeepAliveMonitor->cancel();
        mKeepAliveMonitor.reset();
        return true;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::handleMessageMonitorErrorResultReceived(
                                                                        IMessageMonitorPtr monitor,
                                                                        PeerKeepAliveResultPtr ignore, // will always be NULL
                                                                        MessageResultPtr result
                                                                        )
      {
        if (monitor != mKeepAliveMonitor) {
          ZS_LOG_WARNING(Detail, log("received a result on an obsolete monitor"))
          return false;
        }

        ZS_LOG_ERROR(Detail, log("keep alive request received an error"))

        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      RecursiveLock &AccountPeerLocation::getLock() const
      {
        AccountPtr outer = mOuter.lock();
        if (!outer) return mBogusLock;
        return outer->forAccountPeerLocation().getLock();
      }

      //-----------------------------------------------------------------------
      IRUDPICESocketPtr AccountPeerLocation::getSocket() const
      {
        AccountPtr outer = mOuter.lock();
        if (!outer) return IRUDPICESocketPtr();
        return outer->forAccountPeerLocation().getSocket();
      }

      //-----------------------------------------------------------------------
      String AccountPeerLocation::log(const char *message) const
      {
        return String("AccountPeerLocation [") + Stringize<PUID>(mID).string() + "] " + message;
      }

      //-----------------------------------------------------------------------
      String AccountPeerLocation::getDebugValueString(bool includeCommaPrefix) const
      {
        AutoRecursiveLock lock(getLock());
        bool firstTime = !includeCommaPrefix;
        return Helper::getDebugValue("account peer location id", Stringize<typeof(mID)>(mID).string(), firstTime) +
               Helper::getDebugValue("state", IAccount::toString(mCurrentState), firstTime) +
               Helper::getDebugValue("refind", mShouldRefindNow ? String("true") : String(), firstTime) +
               Helper::getDebugValue("last activity", Time() != mLastActivity ? IMessageHelper::timeToString(mLastActivity) : String(), firstTime) +
               Helper::getDebugValue("pending requests", mPendingRequests.size() > 0 ? Stringize<size_t>(mPendingRequests.size()).string() : String(), firstTime) +
               mLocationInfo.getDebugValueString() +
               (mLocation != mLocationInfo.mLocation ? ILocation::toDebugString(mLocation) : String()) +
               IPeer::toDebugString(mPeer) +
               Helper::getDebugValue("rudp ice socket subscription id", mSocketSubscription ? Stringize<typeof(PUID)>(mSocketSubscription->getID()).string() : String(), firstTime) +
               Helper::getDebugValue("rudp ice socket session id", mSocketSession ? Stringize<typeof(PUID)>(mSocketSession->getID()).string() : String(), firstTime) +
               Helper::getDebugValue("rudp messagine id", mMessaging ? Stringize<typeof(PUID)>(mMessaging->getID()).string() : String(), firstTime) +
               Helper::getDebugValue("incoming", mIncoming ? String("true") : String(), firstTime) +
               Helper::getDebugValue("identify time", Time() != mIdentifyTime ? IMessageHelper::timeToString(mIdentifyTime) : String(), firstTime) +
               Helper::getDebugValue("identity monitor", mIdentifyMonitor ? String("true") : String(), firstTime) +
               Helper::getDebugValue("keep alive monitor", mKeepAliveMonitor ? String("true") : String(), firstTime);
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::cancel()
      {
        AutoRecursiveLock lock(getLock());
        ZS_LOG_DEBUG(log("cancel called"))

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("already shutdown"))
          return;
        }

        setState(IAccount::AccountState_ShuttingDown);

        if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeak.lock();

        if (mIdentifyMonitor) {
          mIdentifyMonitor->cancel();
          mIdentifyMonitor.reset();
        }

        if (mKeepAliveMonitor) {
          mKeepAliveMonitor->cancel();
          mKeepAliveMonitor.reset();
        }

        if (mSocketSubscription) {
          mSocketSubscription->cancel();
          mSocketSubscription.reset();
        }

        if (mGracefulShutdownReference) {

          if (mMessaging) {
            ZS_LOG_DEBUG(log("requesting messaging shutdown"))
            mMessaging->shutdown();

            if (IRUDPMessaging::RUDPMessagingState_Shutdown != mMessaging->getState()) {
              ZS_LOG_DEBUG(log("waiting for RUDP messaging to shutdown"))
              return;
            }
          }

          if (mSocketSession) {
            ZS_LOG_DEBUG(log("requesting RUDP socket session shutdown"))
            mSocketSession->shutdown();

            if (IRUDPICESocketSession::RUDPICESocketSessionState_Shutdown != mSocketSession->getState()) {
              ZS_LOG_DEBUG(log("waiting for RUDP ICE socket session to shutdown"))
              return;
            }
          }

        }

        setState(IAccount::AccountState_Shutdown);

        mGracefulShutdownReference.reset();
        mDelegate.reset();

        mPendingRequests.clear();

        if (mMessaging) {
          ZS_LOG_DEBUG(log("hard shutdown of RUDP messaging"))
          mMessaging->shutdown();
          mMessaging.reset();
        }

        if (mSocketSession) {
          ZS_LOG_DEBUG(log("hard shutdown of RUDP ICE socket session"))
          mSocketSession->shutdown();
          mSocketSession.reset();
        }

        ZS_LOG_DEBUG(log("cancel completed"))
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::step()
      {
        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_DEBUG(log("step forwarding to cancel since shutting down/shutdown"))
          cancel();
          return;
        }

        ZS_LOG_DEBUG(log("step") + getDebugValueString())

        AccountPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_ERROR(Debug, log("stack account appears to be gone thus shutting down"))
          cancel();
          return;
        }

        IRUDPICESocketPtr socket = getSocket();
        if (!socket) {
          ZS_LOG_ERROR(Debug, log("attempted to get RUDP ICE socket but ICE socket is gone"))
          cancel();
          return;
        }

        if (!stepSocketSubscription(socket)) return;
        if (!stepPendingRequests()) return;
        if (!stepSocketSession()) return;
        if (!stepIncomingIdentify()) return;
        if (!stepMessaging()) return;
        if (!stepIdentify()) return;

        setState(IAccount::AccountState_Ready);

        ZS_LOG_DEBUG(log("step complete") + getDebugValueString())
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::stepSocketSubscription(IRUDPICESocketPtr socket)
      {
        if (mSocketSubscription) {
          socket->wakeup();

          if (IRUDPICESocket::RUDPICESocketState_Ready != socket->getState()) {
            ZS_LOG_DEBUG(log("waiting for RUDP ICE socket to wake up"))
            return false;
          }

          ZS_LOG_DEBUG(log("RUDP socket is awake"))
          return true;
        }

        ZS_LOG_DEBUG(log("subscribing to the socket state"))
        mSocketSubscription = socket->subscribe(mThisWeak.lock());
        if (!mSocketSubscription) {
          ZS_LOG_ERROR(Detail, log("failed to subscribe to socket"))
          cancel();
          return false;
        }

        // ensure the socket has been woken up during the subscription process
        IAccountPeerLocationAsyncDelegateProxy::create(mThisWeak.lock())->onStep();
        return false;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::stepPendingRequests()
      {
        if (mPendingRequests.size() < 1) {
          ZS_LOG_DEBUG(log("no pending requests"))
          return true;
        }

        AccountPtr outer = mOuter.lock();
        ZS_THROW_BAD_STATE_IF(!outer)

        LocationPtr finderLocation = ILocationForAccount::getForFinder(outer);

        if (!finderLocation->forAccount().isConnected()) {
          ZS_LOG_WARNING(Detail, log("cannot respond to pending find request if the finder is not ready thus shutting down"))
          return false;
        }

        LocationPtr selfLocation = ILocationForAccount::getForLocal(outer);
        LocationInfoPtr selfLocationInfo = selfLocation->forAccount().getLocationInfo();

        while (mPendingRequests.size() > 0) {
          PeerLocationFindRequestPtr pendingRequest = mPendingRequests.front();
          mPendingRequests.pop_front();

          CandidateList remoteCandidates;
          remoteCandidates = pendingRequest->locationInfo().mCandidates;

          // local candidates are now preparerd, the request can be answered
          PeerLocationFindReplyPtr reply = PeerLocationFindReply::create(pendingRequest);

          reply->locationInfo(*selfLocationInfo);
          reply->peerFiles(outer->forAccountPeerLocation().getPeerFiles());

          if (mPendingRequests.size() < 1) {
            // this is the final location, use this location as the connection point as it likely to be the latest of all the requests (let's hope)
            connectLocation(remoteCandidates, IICESocket::ICEControl_Controlled);
          }

          finderLocation->forAccount().sendMessage(reply);
        }

        ZS_LOG_DEBUG(log("handled pending requests"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::stepSocketSession()
      {
        if (!mSocketSession) {
          ZS_LOG_DEBUG(log("waiting for a RUDP ICE socket session connection"))
          return false;
        }

        if (IRUDPICESocketSession::RUDPICESocketSessionState_Ready != mSocketSession->getState()) {
          ZS_LOG_DEBUG(log("waiting for RUDP ICE socket session to complete"))
          return false;
        }

        ZS_LOG_DEBUG(log("socket session is ready"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::stepIncomingIdentify()
      {
        if (!mIncoming) {
          ZS_LOG_DEBUG(log("session is outgoing"))
          return true;
        }

        if ((Time() != mIdentifyTime) &&
            (mPeer->forAccount().getPeerFilePublic())) {
          ZS_LOG_DEBUG(log("already received identify request"))
          return true;
        }

        ZS_LOG_DEBUG(log("waiting for an incoming connection and identification"))
        return false;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::stepMessaging()
      {
        if (mMessaging) {
          if (IRUDPMessaging::RUDPMessagingState_Connected != mMessaging->getState()) {
            ZS_LOG_DEBUG(log("waiting for the RUDP messaging to connect"))
            return false;
          }
          ZS_LOG_DEBUG(log("messaging is ready"))
          return true;
        }

        if (mIncoming) {
          ZS_LOG_DEBUG(log("no need to create messaging since incoming"))
          return true;
        }

        ZS_LOG_DEBUG(log("requesting messaging channel open"))
        mMessaging = IRUDPMessaging::openChannel(IStackForInternal::queueServices(), mSocketSession, mThisWeak.lock(), HOOKFLASH_STACK_PEER_TO_PEER_RUDP_CONNECTION_INFO);

        if (!mMessaging) {
          ZS_LOG_DEBUG(log("unable to open a messaging channel to remote peer thus shutting down"))
          cancel();
          return false;
        }

        ZS_LOG_DEBUG(log("waiting for messaging to complete"))
        return false;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::stepIdentify()
      {
        if (mIncoming) {
          ZS_LOG_DEBUG(log("no need to identify"))
          return true;
        }

        if (mIdentifyMonitor) {
          ZS_LOG_DEBUG(log("waiting for identify to complete"))
          return false;
        }

        if (Time() != mIdentifyTime) {
          ZS_LOG_DEBUG(log("identify already complete"))
          return true;
        }

        AccountPtr outer = mOuter.lock();

        ZS_LOG_DETAIL(log("Peer Location connected, sending identity..."))

        // we have connected, perform the identity request since we made the connection outgoing...
        PeerIdentifyRequestPtr request = PeerIdentifyRequest::create();
        request->domain(outer->forAccountPeerLocation().getDomain());

        IPeerFilePublicPtr remotePeerFilePublic = mPeer->forAccount().getPeerFilePublic();
        if (remotePeerFilePublic) {
          request->findSecret(remotePeerFilePublic->getFindSecret());
        }

        LocationPtr selfLocation = ILocationForAccount::getForLocal(outer);
        LocationInfoPtr selfLocationInfo = selfLocation->forAccount().getLocationInfo();
        selfLocationInfo->mCandidates.clear();

        request->location(*selfLocationInfo);
        request->peerFiles(outer->forAccountPeerLocation().getPeerFiles());

        mIdentifyMonitor = sendRequest(IMessageMonitorResultDelegate<PeerIdentifyResult>::convert(mThisWeak.lock()), request, Seconds(HOOKFLASH_STACK_CONNECTION_MANAGER_PEER_IDENTIFY_EXPIRES_IN_SECONDS));
        return false;
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::setState(IAccount::AccountStates state)
      {
        if (state == mCurrentState) return;

        ZS_LOG_BASIC(log("state changed") + ", old state=" + IAccount::toString(mCurrentState) + ", new state=" + IAccount::toString(state) + getDebugValueString())

        mCurrentState = state;

        if (!mDelegate) return;

        AccountPeerLocationPtr pThis = mThisWeak.lock();

        if (pThis) {
          try {
            mDelegate->onAccountPeerLocationStateChanged(mThisWeak.lock(), state);
          } catch (IAccountPeerLocationDelegateProxy::Exceptions::DelegateGone &) {
          }
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
    }
  }
}
