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

#pragma once

#include <openpeer/stack/internal/types.h>
#include <openpeer/stack/IAccount.h>
#include <openpeer/stack/IPeer.h>
#include <openpeer/stack/IPeerSubscription.h>
#include <openpeer/stack/internal/stack_AccountFinder.h>
#include <openpeer/stack/internal/stack_AccountPeerLocation.h>
#include <openpeer/stack/internal/stack_ServiceLockboxSession.h>
#include <openpeer/stack/internal/stack_IFinderRelayChannel.h>

#include <openpeer/stack/IMessageMonitor.h>

#include <openpeer/services/IRUDPICESocket.h>
#include <openpeer/services/ITransportStream.h>

#include <zsLib/MessageQueueAssociator.h>

#include <zsLib/Timer.h>

#include <map>

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForAccountFinder
      #pragma mark

      interaction IAccountForAccountFinder
      {
        IAccountForAccountFinder &forAccountFinder() {return *this;}
        const IAccountForAccountFinder &forAccountFinder() const {return *this;}

        virtual RecursiveLock &getLock() const = 0;

        virtual String getDomain() const = 0;

        virtual IRUDPICESocketPtr getSocket() const = 0;

        virtual IPeerFilesPtr getPeerFiles() const = 0;

        virtual bool extractNextFinder(
                                       Finder &outFinder,
                                       IPAddress &outFinderIP
                                       ) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForFinderRelayChannel
      #pragma mark

      interaction IAccountForFinderRelayChannel
      {
        IAccountForFinderRelayChannel &forFinderRelay() {return *this;}
        const IAccountForFinderRelayChannel &forFinderRelay() const {return *this;}

        virtual IPeerFilesPtr getPeerFiles() const = 0;
      };
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForAccountPeerLocation
      #pragma mark

      interaction IAccountForAccountPeerLocation
      {
        IAccountForAccountPeerLocation &forAccountPeerLocation() {return *this;}
        const IAccountForAccountPeerLocation &forAccountPeerLocation() const {return *this;}

        virtual RecursiveLock &getLock() const = 0;

        virtual String getDomain() const = 0;

        virtual IRUDPICESocketPtr getSocket() const = 0;

        virtual IPeerFilesPtr getPeerFiles() const = 0;

        virtual bool isFinderReady() const = 0;

        virtual String getLocalContextID(const String &peerURI) const = 0;
        virtual String getLocalPassword(const String &peerURI) const = 0;

        virtual bool sendViaRelay(
                                  const String &peerURI,
                                  AccountPeerLocationPtr peerLocation,
                                  const BYTE *buffer,
                                  ULONG bufferSizeInBytes
                                  ) const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForLocation
      #pragma mark

      interaction IAccountForLocation
      {
        IAccountForLocation &forLocation() {return *this;}
        const IAccountForLocation &forLocation() const {return *this;}

        virtual LocationPtr findExistingOrUse(LocationPtr location) = 0;
        virtual LocationPtr getLocationForLocal() const = 0;
        virtual LocationPtr getLocationForFinder() const = 0;
        virtual void notifyDestroyed(Location &location) = 0;

        virtual const String &getLocationID() const = 0;
        virtual PeerPtr getPeerForLocal() const = 0;

        virtual LocationInfoPtr getLocationInfo(LocationPtr location) const = 0;

        virtual ILocation::LocationConnectionStates getConnectionState(LocationPtr location) const = 0;

        virtual bool send(
                          LocationPtr location,
                          MessagePtr message
                          ) const = 0;

        virtual void hintNowAvailable(LocationPtr location) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForMessageIncoming
      #pragma mark

      interaction IAccountForMessageIncoming
      {
        IAccountForMessageIncoming &forMessageIncoming() {return *this;}
        const IAccountForMessageIncoming &forMessageIncoming() const {return *this;}

        virtual bool send(
                          LocationPtr location,
                          MessagePtr response
                          ) const = 0;
        virtual void notifyMessageIncomingResponseNotSent(MessageIncoming &messageIncoming) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForMessages
      #pragma mark

      interaction IAccountForMessages
      {
        IAccountForMessages &forMessages() {return *this;}
        const IAccountForMessages &forMessages() const {return *this;}

        virtual IPeerFilesPtr getPeerFiles() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForPeer
      #pragma mark

      interaction IAccountForPeer
      {
        IAccountForPeer &forPeer() {return *this;}
        const IAccountForPeer &forPeer() const {return *this;}

        virtual PeerPtr findExistingOrUse(PeerPtr peer) = 0;
        virtual void notifyDestroyed(Peer &peer) = 0;

        virtual RecursiveLock &getLock() const = 0;

        virtual IPeer::PeerFindStates getPeerState(const String &peerURI) const = 0;
        virtual LocationListPtr getPeerLocations(
                                                 const String &peerURI,
                                                 bool includeOnlyConnectedLocations
                                                 ) const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForPeerSubscription
      #pragma mark

      interaction IAccountForPeerSubscription
      {
        IAccountForPeerSubscription &forPeerSubscription() {return *this;}
        const IAccountForPeerSubscription &forPeerSubscription() const {return *this;}

        virtual void subscribe(PeerSubscriptionPtr subscription) = 0;
        virtual void notifyDestroyed(PeerSubscription &subscription) = 0;

        virtual RecursiveLock &getLock() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForPublicationRepository
      #pragma mark

      interaction IAccountForPublicationRepository
      {
        IAccountForPublicationRepository &forRepo() {return *this;}
        const IAccountForPublicationRepository &forRepo() const {return *this;}

        virtual PublicationRepositoryPtr getRepository() const = 0;

        virtual String getDomain() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForServiceLockboxSession
      #pragma mark

      interaction IAccountForServiceLockboxSession
      {
        IAccountForServiceLockboxSession &forServiceLockboxSession() {return *this;}
        const IAccountForServiceLockboxSession &forServiceLockboxSession() const {return *this;}

        virtual void notifyServiceLockboxSessionStateChanged() = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account
      #pragma mark

      class Account : public Noop,
                      public MessageQueueAssociator,
                      public IAccount,
                      public IAccountForAccountFinder,
                      public IAccountForFinderRelayChannel,
                      public IAccountForAccountPeerLocation,
                      public IAccountForLocation,
                      public IAccountForMessageIncoming,
                      public IAccountForMessages,
                      public IAccountForPeer,
                      public IAccountForPeerSubscription,
                      public IAccountForPublicationRepository,
                      public IAccountForServiceLockboxSession,
                      public IWakeDelegate,
                      public IAccountFinderDelegate,
                      public IAccountPeerLocationDelegate,
                      public IDNSDelegate,
                      public IRUDPICESocketDelegate,
                      public IMessageMonitorDelegate,
                      public IFinderRelayChannelDelegate,
                      public ITransportStreamWriterDelegate,
                      public ITransportStreamReaderDelegate,
                      public ITimerDelegate
      {
      public:
        friend interaction IAccountFactory;
        friend interaction IAccount;

        typedef IAccount::AccountStates AccountStates;

        typedef ULONG ChannelNumber;

        struct RelayInfo;
        friend struct RelayInfo;
        typedef boost::shared_ptr<RelayInfo> RelayInfoPtr;
        typedef boost::weak_ptr<RelayInfo> RelayInfoWeakPtr;

        struct PeerInfo;
        friend struct PeerInfo;
        typedef boost::shared_ptr<PeerInfo> PeerInfoPtr;
        typedef boost::weak_ptr<PeerInfo> PeerInfoWeakPtr;

        typedef String PeerURI;
        typedef String LocationID;
        typedef PUID PeerSubscriptionID;
        typedef std::pair<PeerURI, LocationID> PeerLocationIDPair;

        typedef std::map<ChannelNumber, RelayInfoPtr> RelayInfoMap;

        typedef std::map<PeerURI, PeerWeakPtr> PeerMap;
        typedef std::map<PeerURI, PeerInfoPtr> PeerInfoMap;

        typedef std::map<PeerSubscriptionID, PeerSubscriptionWeakPtr> PeerSubscriptionMap;

        typedef std::map<PeerLocationIDPair, LocationWeakPtr> LocationMap;

      protected:
        Account(
                IMessageQueuePtr queue,
                IAccountDelegatePtr delegate,
                ServiceLockboxSessionPtr peerContactSession
                );
        
        Account(Noop) : Noop(true), MessageQueueAssociator(IMessageQueuePtr()) {};

        void init();

      public:
        ~Account();

        static AccountPtr convert(IAccountPtr account);// {return AccountPtr();}

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccount
        #pragma mark

        static String toDebugString(IAccountPtr account, bool includeCommaPrefix = true);

        static AccountPtr create(
                                 IAccountDelegatePtr delegate,
                                 IServiceLockboxSessionPtr peerContactSession
                                 );

        virtual PUID getID() const {return mID;}

        virtual AccountStates getState(
                                       WORD *outLastErrorCode = NULL,
                                       String *outLastErrorReason = NULL
                                       ) const;

        virtual IServiceLockboxSessionPtr getLockboxSession() const;

        virtual void getNATServers(
                                   String &outTURNServer,
                                   String &outTURNUsername,
                                   String &outTURNPassword,
                                   String &outSTUNServer
                                   ) const;

        virtual void shutdown();


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForAccountFinder
        #pragma mark

        // (duplicate) virtual RecursiveLock &getLock() const;

        virtual String getDomain() const;

        virtual IRUDPICESocketPtr getSocket() const;

        // (duplicate) virtual IPeerFilesPtr getPeerFiles() const;

        virtual bool extractNextFinder(
                                       Finder &outFinder,
                                       IPAddress &outFinderIP
                                       );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForFinderRelayChannel
        #pragma mark

        // (duplicate) virtual IPeerFilesPtr getPeerFiles() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForAccountPeerLocation
        #pragma mark

        // (duplicate) virtual RecursiveLock &getLock() const;

        // (duplicate) virtual String getDomain() const;

        // (duplicate) virtual IRUDPICESocketPtr getSocket() const;

        // (duplicate) virtual IPeerFilesPtr getPeerFiles() const;

        virtual bool isFinderReady() const;

        virtual String getLocalContextID(const String &peerURI) const;
        virtual String getLocalPassword(const String &peerURI) const;

        virtual bool sendViaRelay(
                                  const String &peerURI,
                                  AccountPeerLocationPtr peerLocation,
                                  const BYTE *buffer,
                                  ULONG bufferSizeInBytes
                                  ) const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForLocation
        #pragma mark

        virtual LocationPtr findExistingOrUse(LocationPtr location);
        virtual LocationPtr getLocationForLocal() const;
        virtual LocationPtr getLocationForFinder() const;
        virtual void notifyDestroyed(Location &location);

        virtual const String &getLocationID() const;
        virtual PeerPtr getPeerForLocal() const;

        virtual LocationInfoPtr getLocationInfo(LocationPtr location) const;

        virtual ILocation::LocationConnectionStates getConnectionState(LocationPtr location) const;

        virtual bool send(
                          LocationPtr location,
                          MessagePtr message
                          ) const;

        virtual void hintNowAvailable(LocationPtr location);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForMessageIncoming
        #pragma mark

        // (duplicate) virtual bool send(
        //                              LocationPtr location,
        //                              MessagePtr response
        //                              ) const;
        virtual void notifyMessageIncomingResponseNotSent(MessageIncoming &messageIncoming);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForMessages
        #pragma mark

        virtual IPeerFilesPtr getPeerFiles() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForPeer
        #pragma mark

        virtual PeerPtr findExistingOrUse(PeerPtr peer);
        virtual void notifyDestroyed(Peer &peer);

        virtual RecursiveLock &getLock() const;

        virtual IPeer::PeerFindStates getPeerState(const String &peerURI) const;
        virtual LocationListPtr getPeerLocations(
                                                 const String &peerURI,
                                                 bool includeOnlyConnectedLocations
                                                 ) const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForPeerSubscription
        #pragma mark

        virtual void subscribe(PeerSubscriptionPtr subscription);
        virtual void notifyDestroyed(PeerSubscription &subscription);

        // (duplicate) virtual RecursiveLock &getLock() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForPublicationRepository
        #pragma mark

        virtual PublicationRepositoryPtr getRepository() const;

        // (duplicate) virtual String getDomain() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForServiceLockboxSession
        #pragma mark

        virtual void notifyServiceLockboxSessionStateChanged();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountFinderDelegate
        #pragma mark

        virtual void onAccountFinderStateChanged(
                                                 AccountFinderPtr finder,
                                                 AccountStates state
                                                 );

        virtual void onAccountFinderMessageIncoming(
                                                    AccountFinderPtr peerLocation,
                                                    MessagePtr message
                                                    );

        virtual void onAccountFinderIncomingRelayChannel(
                                                         AccountFinderPtr finder,
                                                         IFinderRelayChannelPtr relayChannel,
                                                         ITransportStreamPtr receiveStream,
                                                         ITransportStreamPtr sendStream,
                                                         ChannelNumber channelNumber
                                                         );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountPeerLocationDelegate
        #pragma mark

        virtual void onAccountPeerLocationStateChanged(
                                                       AccountPeerLocationPtr peerLocation,
                                                       AccountStates state
                                                       );

        virtual void onAccountPeerLocationMessageIncoming(
                                                          AccountPeerLocationPtr peerLocation,
                                                          MessagePtr message
                                                          );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IDNSDelegate
        #pragma mark

        virtual void onLookupCompleted(IDNSQueryPtr query);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IRUDPICESocketDelegate
        #pragma mark

        virtual void onRUDPICESocketStateChanged(
                                                 IRUDPICESocketPtr socket,
                                                 RUDPICESocketStates state
                                                 );

        virtual void onRUDPICESocketCandidatesChanged(IRUDPICESocketPtr socket);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IMessageMonitorDelegate
        #pragma mark

        virtual bool handleMessageMonitorMessageReceived(
                                                         IMessageMonitorPtr requester,
                                                         MessagePtr message
                                                         );

        virtual void onMessageMonitorTimedOut(IMessageMonitorPtr requester);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IWakeDelegate
        #pragma mark

        virtual void onWake();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IFinderRelayChannelDelegate
        #pragma mark

        virtual void onFinderRelayChannelStateChanged(
                                                      IFinderRelayChannelPtr channel,
                                                      IFinderRelayChannel::SessionStates state
                                                      );

        virtual void onFinderRelayChannelNeedsContext(IFinderRelayChannelPtr channel);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => ITransportStreamWriterDelegate
        #pragma mark

        virtual void onTransportStreamWriterReady(ITransportStreamWriterPtr writer);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => ITransportStreamReaderDelegate
        #pragma mark

        virtual void onTransportStreamReaderReady(ITransportStreamReaderPtr reader);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => ITimerDelegate
        #pragma mark

        virtual void onTimer(TimerPtr timer);


      private:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => (internal)
        #pragma mark

        bool isPending() const      {return AccountState_Pending == mCurrentState;}
        bool isReady() const        {return AccountState_Ready == mCurrentState;}
        bool isShuttingDown() const {return AccountState_ShuttingDown ==  mCurrentState;}
        bool isShutdown() const     {return AccountState_Shutdown ==  mCurrentState;}

        String log(const char *message) const;

        virtual String getDebugValueString(bool includeCommaPrefix = true) const;

        void cancel();
        void step();
        bool stepTimer();
        bool stepRepository();
        bool stepLockboxSession();
        bool stepLocations();
        bool stepSocket();
        bool stepFinder();
        bool stepPeers();

        void setState(AccountStates accountState);
        void setError(WORD errorCode, const char *reason = NULL);

        virtual CandidatePtr getRelayCandidate(const String &peerURI) const;

        void setFindState(
                          PeerInfo &peerInfo,
                          IPeer::PeerFindStates state
                          );

        bool shouldFind(
                        const String &peerURI,
                        const PeerInfoPtr &peerInfo
                        ) const;

        bool shouldShutdownInactiveLocations(
                                             const String &contactID,
                                             const PeerInfoPtr &peer
                                             ) const;

        void shutdownPeerLocationsNotNeeded(
                                            const String &peerURI,
                                            PeerInfoPtr &peerInfo
                                            );

        void sendPeerKeepAlives(
                                const String &peerURI,
                                PeerInfoPtr &peerInfo
                                );
        void performPeerFind(
                             const String &peerURI,
                             PeerInfoPtr &peerInfo
                             );

        void handleFindRequestComplete(IMessageMonitorPtr requester);

        void handleFinderRelatedFailure();

        void notifySubscriptions(
                                 LocationPtr location,
                                 ILocation::LocationConnectionStates state
                                 );

        void notifySubscriptions(
                                 PeerPtr peer,
                                 IPeer::PeerFindStates state
                                 );

        void notifySubscriptions(MessageIncomingPtr messageIncoming);

      public:

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account::RelayInfo
        #pragma mark

        struct RelayInfo
        {
          AutoPUID mID;
          ChannelNumber mChannel;
          String mLocalContext;
          String mRemoteContext;

          IFinderRelayChannelPtr mRelayChannel;
          ITransportStreamReaderPtr mReceiveStream;
          ITransportStreamWriterPtr mSendStream;

          IFinderRelayChannelSubscriptionPtr mRelayChannelSubscription;
          ITransportStreamReaderSubscriptionPtr mReceiveStreamSubscription;
          ITransportStreamWriterSubscriptionPtr mSendStreamSubscription;

          AccountPeerLocationPtr mAccountPeerLocation;

          static RelayInfoPtr create();

          RelayInfo() : mChannel(0) {}
          ~RelayInfo();

          String getDebugValueString(bool includeCommaPrefix = true) const;
          void cancel();
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account::PeerInfo
        #pragma mark

        struct PeerInfo
        {
          typedef std::map<LocationID, AccountPeerLocationPtr> PeerLocationMap;     // every location needs a session
          typedef std::map<LocationID, LocationID> FindingBecauseOfLocationIDMap;   // using this to track the reason why the find needs to be initated or reinitated

          static String toDebugString(PeerInfoPtr peerInfo, bool includeCommaPrefix = true);

          static PeerInfoPtr create();
          void findTimeReset();
          void findTimeScheduleNext();
          String getDebugValueString(bool includeCommaPrefix = true) const;

          AutoPUID mID;
          bool mFindAtNextPossibleMoment;

          PeerPtr mPeer;
          PeerLocationMap mLocations;                                 // list of connecting/connected peer locations

          IMessageMonitorPtr mPeerFindMonitor;                        // the request monitor when a search is being conducted
          FindingBecauseOfLocationIDMap mPeerFindBecauseOfLocations;  // peer find is being done because of locations that are known but not yet discovered

          FindingBecauseOfLocationIDMap mPeerFindNeedsRedoingBecauseOfLocations;  // peer find needs to be redone as soon as complete because of locations that are known but not yet discovered

          IPeer::PeerFindStates mCurrentFindState;
          ULONG mTotalSubscribers;                                    // total number of external subscribers to this peer

          // If a peer location was NOT found, we need to keep trying the search periodically but with exponential back off.
          // These variables keep track of that backoff. We don't need to do any finds once connecting/connected to a single location
          // because the peer location will notify us of other peer locations for the existing peer.
          // NOTE: Presence can also give us a hint to when we should redo the search.
          Time mNextScheduledFind;                                 // if peer was not found, schedule finds to try again
          Duration mLastScheduleFindDuration;                      // how long was the duration between finds (used because it will double each time a search is completed)

          bool mPreventCrazyRefindNextTime;

          RelayInfoMap mRelayInfos;                                 // all the pending relays sessions
        };

      protected:
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => (data)
        #pragma mark

        AutoPUID mID;
        mutable RecursiveLock mLock;
        AccountWeakPtr mThisWeak;
        AccountPtr mGracefulShutdownReference;

        AccountStates mCurrentState;
        WORD mLastError;
        String mLastErrorReason;

        IAccountDelegatePtr mDelegate;

        TimerPtr mTimer;
        Time mLastTimerFired;
        Time mBlockLocationShutdownsUntil;

        ServiceLockboxSessionPtr mLockboxSession;
        Service::MethodPtr mTURN;
        Service::MethodPtr mSTUN;

        IRUDPICESocketPtr mSocket;

        String mMasterPeerSecret;

        String mLocationID;
        PeerPtr mSelfPeer;
        LocationPtr mSelfLocation;
        LocationPtr mFinderLocation;

        PublicationRepositoryPtr mRepository;

        AccountFinderPtr mFinder;

        Time mFinderRetryAfter;
        Duration mLastRetryFinderAfterDuration;

        FinderList mAvailableFinders;
        IDNS::SRVResultPtr mAvailableFinderSRVResult;
        IMessageMonitorPtr mFindersGetMonitor;
        IDNSQueryPtr mFinderDNSLookup;

        PeerInfoMap mPeerInfos;

        PeerSubscriptionMap mPeerSubscriptions;

        PeerMap mPeers;
        LocationMap mLocations;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountFactory
      #pragma mark

      interaction IAccountFactory
      {
        static IAccountFactory &singleton();

        virtual AccountPtr create(
                                  IAccountDelegatePtr delegate,
                                  IServiceLockboxSessionPtr peerContactSession
                                  );
      };

    }
  }
}
