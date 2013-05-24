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

#include <hookflash/stack/internal/stack_ServiceLockboxSession.h>
#include <hookflash/stack/internal/stack_ServiceIdentitySession.h>
#include <hookflash/stack/message/identity-lockbox/LockboxAccessRequest.h>
#include <hookflash/stack/message/identity-lockbox/LockboxIdentitiesUpdateRequest.h>
#include <hookflash/stack/message/identity-lockbox/LockboxNamespaceGrantWindowRequest.h>
#include <hookflash/stack/message/identity-lockbox/LockboxNamespaceGrantStartNotify.h>
#include <hookflash/stack/message/identity-lockbox/LockboxNamespaceGrantCompleteNotify.h>
#include <hookflash/stack/message/identity-lockbox/LockboxContentGetRequest.h>
#include <hookflash/stack/message/identity-lockbox/LockboxContentSetRequest.h>

#include <hookflash/stack/internal/stack_BootstrappedNetwork.h>
#include <hookflash/stack/internal/stack_Account.h>
#include <hookflash/stack/internal/stack_Helper.h>
#include <hookflash/stack/IHelper.h>
#include <hookflash/stack/IPeer.h>
#include <hookflash/stack/IPeerFiles.h>
#include <hookflash/stack/IPeerFilePrivate.h>
#include <hookflash/stack/IPeerFilePublic.h>
#include <hookflash/stack/message/IMessageHelper.h>
#include <hookflash/stack/internal/stack_Stack.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>
#include <zsLib/helpers.h>

#include <zsLib/Stringize.h>

#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include <cryptopp/md5.h>

#define HOOKFLASH_STACK_SERVICE_PEER_CONTACT_TIMEOUT_IN_SECONDS (60*2)


namespace hookflash { namespace stack { ZS_DECLARE_SUBSYSTEM(hookflash_stack) } }

namespace hookflash
{
  namespace stack
  {
    namespace internal
    {
      using CryptoPP::Weak::MD5;

      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

      using zsLib::Stringize;

      using message::identity_lockbox::LockboxAccessRequest;
      using message::identity_lockbox::LockboxAccessRequestPtr;
      using message::identity_lockbox::LockboxIdentitiesUpdateRequest;
      using message::identity_lockbox::LockboxIdentitiesUpdateRequestPtr;
      using message::identity_lockbox::LockboxNamespaceGrantWindowRequest;
      using message::identity_lockbox::LockboxNamespaceGrantWindowRequestPtr;
      using message::identity_lockbox::LockboxNamespaceGrantStartNotify;
      using message::identity_lockbox::LockboxNamespaceGrantStartNotifyPtr;
      using message::identity_lockbox::LockboxNamespaceGrantCompleteNotify;
      using message::identity_lockbox::LockboxNamespaceGrantCompleteNotifyPtr;
      using message::identity_lockbox::LockboxContentGetRequest;
      using message::identity_lockbox::LockboxContentGetRequestPtr;
      using message::identity_lockbox::LockboxContentSetRequest;
      using message::identity_lockbox::LockboxContentSetRequestPtr;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession
      #pragma mark

      //-----------------------------------------------------------------------
      ServiceLockboxSession::ServiceLockboxSession(
                                                   IMessageQueuePtr queue,
                                                   BootstrappedNetworkPtr network,
                                                   IServiceLockboxSessionDelegatePtr delegate
                                                   ) :
        zsLib::MessageQueueAssociator(queue),
        mID(zsLib::createPUID()),
        mDelegate(delegate ? IServiceLockboxSessionDelegateProxy::createWeak(IStackForInternal::queueDelegate(), delegate) : IServiceLockboxSessionDelegatePtr()),
        mBootstrappedNetwork(network),
        mCurrentState(SessionState_Pending),
        mLastError(0)
      {
        ZS_LOG_DEBUG(log("created"))
      }

      //-----------------------------------------------------------------------
      ServiceLockboxSession::~ServiceLockboxSession()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::init()
      {
        IBootstrappedNetworkForServices::prepare(mBootstrappedNetwork->forServices().getDomain(), mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      ServiceLockboxSessionPtr ServiceLockboxSession::convert(IServiceLockboxSessionPtr query)
      {
        return boost::dynamic_pointer_cast<ServiceLockboxSession>(query);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IServiceLockboxSession
      #pragma mark

      //-----------------------------------------------------------------------
      String ServiceLockboxSession::toDebugString(IServiceLockboxSessionPtr session, bool includeCommaPrefix)
      {
        if (!session) return includeCommaPrefix ? String(", peer contact session=(null)") : String("peer contact session=");
        return ServiceLockboxSession::convert(session)->getDebugValueString(includeCommaPrefix);
      }

      //-----------------------------------------------------------------------
      ServiceLockboxSessionPtr ServiceLockboxSession::login(
                                                            IServiceLockboxSessionDelegatePtr delegate,
                                                            IServiceLockboxPtr serviceLockbox,
                                                            IServiceIdentitySessionPtr identitySession
                                                            )
      {
        ServiceLockboxSessionPtr pThis(new ServiceLockboxSession(IStackForInternal::queueStack(), BootstrappedNetwork::convert(serviceLockbox), delegate));
        pThis->mThisWeak = pThis;
        pThis->mLoginIdentity = ServiceIdentitySession::convert(identitySession);
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      ServiceLockboxSessionPtr ServiceLockboxSession::relogin(
                                                              IServiceLockboxSessionDelegatePtr delegate,
                                                              IServiceLockboxPtr serviceLockbox,
                                                              const char *lockboxAccountID,
                                                              const char *identityHalfLockboxKey,
                                                              const char *lockboxHalfLockboxKey
                                                              )
      {
        ZS_THROW_BAD_STATE_IF(!delegate)
        ZS_THROW_BAD_STATE_IF(!lockboxAccountID)
        ZS_THROW_BAD_STATE_IF(!identityHalfLockboxKey)
        ZS_THROW_BAD_STATE_IF(!lockboxHalfLockboxKey)

        ServiceLockboxSessionPtr pThis(new ServiceLockboxSession(IStackForInternal::queueStack(), BootstrappedNetwork::convert(serviceLockbox), delegate));
        pThis->mThisWeak = pThis;
        pThis->mLockboxInfo.mAccountID = lockboxAccountID;
        pThis->mLockboxInfo.mKeyIdentityHalf = IHelper::convertToBuffer(identityHalfLockboxKey);
        pThis->mLockboxInfo.mKeyLockboxHalf = IHelper::convertToBuffer(lockboxHalfLockboxKey);
        pThis->init();
        return pThis;
        return ServiceLockboxSessionPtr();
      }

      //-----------------------------------------------------------------------
      IServiceLockboxPtr ServiceLockboxSession::getService() const
      {
        AutoRecursiveLock lock(getLock());
        return mBootstrappedNetwork;
      }

#if 0

      //-----------------------------------------------------------------------
      IServiceLockboxSession::SessionStates ServiceLockboxSession::getState(
                                                                            WORD *outLastErrorCode,
                                                                            String *outLastErrorReason
                                                                            ) const
      {
        AutoRecursiveLock lock(getLock());
        if (outLastErrorCode) *outLastErrorCode = mLastError;
        if (outLastErrorReason) *outLastErrorReason = mLastErrorReason;
        return mCurrentState;
      }

      //-----------------------------------------------------------------------
      IPeerFilesPtr ServiceLockboxSession::getPeerFiles() const
      {
        AutoRecursiveLock lock(getLock());
        return mPeerFiles;
      }

      //-----------------------------------------------------------------------
      String ServiceLockboxSession::getLockboxAccountID() const
      {
        AutoRecursiveLock lock(getLock());
        return mLockboxInfo.mAccountID;
      }

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::getLockboxKey(
                                                SecureByteBlockPtr &outIdentityHalf,
                                                SecureByteBlockPtr &outLockboxHalf
                                                )
      {
        AutoRecursiveLock lock(getLock());

        outIdentityHalf = stack::IHelper::convertToBuffer(mLockboxInfo.mKeyIdentityHalf);
        outLockboxHalf = stack::IHelper::convertToBuffer(mLockboxInfo.mKeyLockboxHalf);
      }

      //-----------------------------------------------------------------------
      ServiceIdentitySessionListPtr ServiceLockboxSession::getAssociatedIdentities() const
      {
        AutoRecursiveLock lock(getLock());
        ServiceIdentitySessionListPtr result(new ServiceIdentitySessionList);
        for (ServiceIdentitySessionMap::const_iterator iter = mAssociatedIdentities.begin(); iter != mAssociatedIdentities.end(); ++iter)
        {
          result->push_back((*iter).second);
        }
        return result;
      }

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::associateIdentities(
                                                      const ServiceIdentitySessionList &identitiesToAssociate,
                                                      const ServiceIdentitySessionList &identitiesToRemove
                                                      )
      {
        AutoRecursiveLock lock(getLock());

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("unable to associate identity as already shutdown") + getDebugValueString())
          return;
        }

        for (ServiceIdentitySessionList::const_iterator iter = identitiesToAssociate.begin(); iter != identitiesToAssociate.end(); ++iter)
        {
          ServiceIdentitySessionPtr session = ServiceIdentitySession::convert(*iter);
          session->forPeerContact().associate(mThisWeak.lock());
          mPendingUpdateIdentities[session->forPeerContact().getID()] = session;
        }
        for (ServiceIdentitySessionList::const_iterator iter = identitiesToRemove.begin(); iter != identitiesToRemove.end(); ++iter)
        {
          ServiceIdentitySessionPtr session = ServiceIdentitySession::convert(*iter);
          mPendingRemoveIdentities[session->forPeerContact().getID()] = session;
        }
        // handle the association now (but do it asynchronously)
        IServiceLockboxSessionAsyncProxy::create(mThisWeak.lock())->onStep();
      }

      //-----------------------------------------------------------------------
      String ServiceLockboxSession::getInnerBrowserWindowFrameURL() const
      {
      }
      
      //-----------------------------------------------------------------------
      void ServiceLockboxSession::notifyBrowserWindowVisible()
      {
      }

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::notifyBrowserWindowClosed()
      {
      }
      
      //-----------------------------------------------------------------------
      DocumentPtr ServiceLockboxSession::getNextMessageForInnerBrowerWindowFrame()
      {
      }

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::handleMessageFromInnerBrowserWindowFrame(DocumentPtr unparsedMessage)
      {
      }

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::cancel()
      {
        AutoRecursiveLock lock(getLock());

        if (isShutdown()) {
          ZS_LOG_DEBUG(log("already shutdown"))
          return;
        }

        if (mLoginMonitor) {
          mLoginMonitor->cancel();
          mLoginMonitor.reset();
        }
        if (mPeerFilesGetMonitor) {
          mPeerFilesGetMonitor->cancel();
          mPeerFilesGetMonitor.reset();
        }
        if (mPeerFilesSetMonitor) {
          mPeerFilesSetMonitor->cancel();
          mPeerFilesSetMonitor.reset();
        }
        if (mServicesMonitor) {
          mServicesMonitor->cancel();
          mServicesMonitor.reset();
        }
        if (mAssociateMonitor) {
          mAssociateMonitor->cancel();
          mAssociateMonitor.reset();
        }

        if (mSaltQuery) {
          mSaltQuery->cancel();
          mSaltQuery.reset();
        }

        setState(SessionState_Shutdown);

        mAccount.reset();

        if (mLoginIdentity) {
          mLoginIdentity->forPeerContact().notifyStateChanged();
          mLoginIdentity.reset();
        }

        for (ServiceIdentitySessionMap::iterator iter = mAssociatedIdentities.begin(); iter != mAssociatedIdentities.end(); ++iter)
        {
          ServiceIdentitySessionPtr session = (*iter).second;
          session->forPeerContact().notifyStateChanged();
        }

        for (ServiceIdentitySessionMap::iterator iter = mPendingUpdateIdentities.begin(); iter != mPendingUpdateIdentities.end(); ++iter)
        {
          ServiceIdentitySessionPtr session = (*iter).second;
          session->forPeerContact().notifyStateChanged();
        }

        for (ServiceIdentitySessionMap::iterator iter = mPendingUpdateIdentities.begin(); iter != mPendingUpdateIdentities.end(); ++iter)
        {
          ServiceIdentitySessionPtr session = (*iter).second;
          session->forPeerContact().notifyStateChanged();
        }

        mAssociatedIdentities.clear();
        mPendingUpdateIdentities.clear();
        mPendingRemoveIdentities.clear();

        mDelegate.reset();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IMessageSource
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IServiceLockboxSessionForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::attach(AccountPtr account)
      {
        AutoRecursiveLock lock(getLock());
        mAccount = account;
      }

      //-----------------------------------------------------------------------
      Service::MethodPtr ServiceLockboxSession::findServiceMethod(
                                                                  const char *serviceType,
                                                                  const char *method
                                                                  ) const
      {
        if (NULL == serviceType) return Service::MethodPtr();
        if (NULL == method) return Service::MethodPtr();

        ServiceTypeMap::const_iterator found = mServicesByType.find(serviceType);
        if (found == mServicesByType.end()) return Service::MethodPtr();

        const Service *service = &(*found).second;

        Service::MethodMap::const_iterator foundMethod = service->mMethods.find(method);
        if (foundMethod == service->mMethods.end()) return Service::MethodPtr();

        Service::MethodPtr result(new Service::Method);
        (*result) = ((*foundMethod).second);

        return result;
      }

      //-----------------------------------------------------------------------
      BootstrappedNetworkPtr ServiceLockboxSession::getBootstrappedNetwork() const
      {
        AutoRecursiveLock lock(getLock());
        return mBootstrappedNetwork;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IServiceLockboxSessionForServiceIdentity
      #pragma mark

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::notifyStateChanged()
      {
        AutoRecursiveLock lock(getLock());
        ZS_LOG_DEBUG(log("notify state changed"))
        IServiceLockboxSessionAsyncProxy::create(mThisWeak.lock())->onStep();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IServiceLockboxSessionAsync
      #pragma mark

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::onStep()
      {
        AutoRecursiveLock lock(getLock());
        ZS_LOG_DEBUG(log("on step"))
        IServiceLockboxSessionAsyncProxy::create(mThisWeak.lock())->onStep();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IBootstrappedNetworkDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::onBootstrappedNetworkPreparationCompleted(IBootstrappedNetworkPtr bootstrappedNetwork)
      {
        ZS_LOG_DEBUG(log("bootstrapper reported complete"))

        AutoRecursiveLock lock(getLock());
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IServiceSaltFetchSignedSaltQueryDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::onServiceSaltFetchSignedSaltCompleted(IServiceSaltFetchSignedSaltQueryPtr query)
      {
        ZS_LOG_DEBUG(log("salt service reported complete"))

        AutoRecursiveLock lock(getLock());
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<IdentityLoginStartResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorResultReceived(
                                                                         IMessageMonitorPtr monitor,
                                                                         PeerContactLoginResultPtr result
                                                                         )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mLoginMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }

        mLoginMonitor->cancel();
        mLoginMonitor.reset();

        mContactUserID = result->contactUserID();
        mContactAccessToken = result->contactAccessToken();
        mContactAccessSecret = result->contactAccessSecret();
        mContactAccessExpires = result->contactAccessExpires();

        mRegeneratePeerFiles = result->peerFilesRegenerate();

        if ((mContactUserID.isEmpty()) ||
            (mContactAccessSecret.isEmpty()) ||
            (mContactAccessSecret.isEmpty())) {
          ZS_LOG_ERROR(Detail, log("login result missing information") + getDebugValueString())
          setError(IHTTP::HTTPStatusCode_PreconditionFailed, "Login result from server missing critical data");
          cancel();
          return true;
        }

        step();

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              PeerContactLoginResultPtr ignore, // will always be NULL
                                                                              message::MessageResultPtr result
                                                                              )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mLoginMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }

        setError(result->errorCode(), result->errorReason());

        ZS_LOG_WARNING(Detail, log("peer contact login failed"))

        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<PrivatePeerFileGetResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorResultReceived(
                                                                         IMessageMonitorPtr monitor,
                                                                         PrivatePeerFileGetResultPtr result
                                                                         )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mPeerFilesGetMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }

        mPeerFilesGetMonitor->cancel();
        mPeerFilesGetMonitor.reset();

        ElementPtr privatePeer = result->privatePeer();
        if (!privatePeer) {
          ZS_LOG_ERROR(Detail, log("server failed to return a private peer file when fetched"))
          setError(IHTTP::HTTPStatusCode_PreconditionFailed, "Server failed to return private peer file");
          cancel();
          return false;
        }

        SecureByteBlockPtr privatePeerSecret;

        if (mLoginIdentity) {
          privatePeerSecret = mLoginIdentity->forPeerContact().getPrivatePeerFileSecret();
          if (privatePeerSecret) {
            mPeerFiles = IPeerFiles::loadFromElement((const char *)((const BYTE *)(*privatePeerSecret)), privatePeer);
          }
        }

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              PrivatePeerFileGetResultPtr ignore, // will always be NULL
                                                                              message::MessageResultPtr result
                                                                              )
      {
        AutoRecursiveLock lock(getLock());

        if (monitor != mPeerFilesGetMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }

        mPeerFilesGetMonitor->cancel();
        mPeerFilesGetMonitor.reset();

        ZS_LOG_WARNING(Detail, log("peer files get failed thus will regenerate new peer files") + Message::toDebugString(result))

        mRegeneratePeerFiles = true;

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<PrivatePeerFileSetResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorResultReceived(
                                                                         IMessageMonitorPtr monitor,
                                                                         PrivatePeerFileSetResultPtr result
                                                                         )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mPeerFilesSetMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }
        mPeerFilesSetMonitor->cancel();
        mPeerFilesSetMonitor.reset();
        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              PrivatePeerFileSetResultPtr ignore, // will always be NULL
                                                                              message::MessageResultPtr result
                                                                              )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mPeerFilesSetMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }

        setError(result->errorCode(), result->errorReason());

        ZS_LOG_WARNING(Detail, log("private peer file set failed"))

        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<PeerContactServicesGetResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorResultReceived(
                                                                         IMessageMonitorPtr monitor,
                                                                         PeerContactServicesGetResultPtr result
                                                                         )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mServicesMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }

        mServicesMonitor->cancel();
        mServicesMonitor.reset();
        
        mServicesByType = result->servicesByType();
        if (mServicesByType.size() < 1) {
          // make sure to add at least one bogus service so we know this request completed
          Service service;
          service.mID = "bogus";
          service.mType = "bogus";

          mServicesByType[service.mType] = service;
        }

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              PeerContactServicesGetResultPtr ignore, // will always be NULL
                                                                              message::MessageResultPtr result
                                                                              )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mServicesMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }

        setError(result->errorCode(), result->errorReason());

        ZS_LOG_WARNING(Detail, log("peer contact services get failed"))

        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<PeerContactIdentityAssociateResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorResultReceived(
                                                                         IMessageMonitorPtr monitor,
                                                                         PeerContactIdentityAssociateResultPtr result
                                                                         )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mAssociateMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }

        mAssociateMonitor->cancel();
        mAssociateMonitor.reset();

        MessagePtr originalMessage = monitor->getMonitoredMessage();
        PeerContactIdentityAssociateRequestPtr originalRequest = PeerContactIdentityAssociateRequest::convert(originalMessage);
        ZS_THROW_BAD_STATE_IF(!originalRequest)

        IdentityInfoList originalIdentityInfoList = originalRequest->identities();
        const IdentityInfoList resultIdentityInfoList = result->identities();

        for (IdentityInfoList::const_iterator outerIter = resultIdentityInfoList.begin(); outerIter != resultIdentityInfoList.end(); ++outerIter)
        {
          const IdentityInfo &outerInfo = (*outerIter);

          for (IdentityInfoList::iterator iter = originalIdentityInfoList.begin(); iter != originalIdentityInfoList.end();)
          {
            IdentityInfoList::iterator current = iter;
            ++iter;

            IdentityInfo &info = (*current);

            if (IdentityInfo::Disposition_Remove != info.mDisposition) continue;
            if (outerInfo.mHash != IHelper::convertToHex(*IHelper::hash(info.mURI))) continue;
            if (outerInfo.mProvider != info.mProvider) continue;

            ZS_LOG_WARNING(Detail, log("asked to remove identity association but server failed to remove (thus must try again)"))
            originalIdentityInfoList.erase(current);
          }
        }

        for (IdentityInfoList::const_iterator iter = originalIdentityInfoList.begin(); iter != originalIdentityInfoList.end(); ++iter)
        {
          const IdentityInfo &info = (*iter);
          switch (info.mDisposition) {
            case IdentityInfo::Disposition_NA:      break;
            case IdentityInfo::Disposition_Update:
            {
              bool found = false;
              // if this was on the pending list the move it to the associated list
              for (ServiceIdentitySessionMap::iterator iter = mPendingUpdateIdentities.begin(); iter != mPendingUpdateIdentities.end();)
              {
                ServiceIdentitySessionMap::iterator current = iter;
                ++iter;

                ServiceIdentitySessionPtr session = ((*current).second);
                IdentityInfo sessionInfo = session->forPeerContact().getIdentityInfo();

                if (IHelper::convertToHex(*IHelper::hash(sessionInfo.mURI)) != info.mHash) continue;
                if (sessionInfo.mProvider != info.mProvider) continue;

                session->forPeerContact().notifyStateChanged();

                mAssociatedIdentities[session->forPeerContact().getID()] = session;
                mPendingUpdateIdentities.erase(current);
                found = true;
                break;
              }

              if (found) break;

              for (ServiceIdentitySessionMap::iterator iter = mAssociatedIdentities.begin(); iter != mAssociatedIdentities.end();)
              {
                ServiceIdentitySessionMap::iterator current = iter;
                ++iter;

                ServiceIdentitySessionPtr session = ((*current).second);
                IdentityInfo sessionInfo = session->forPeerContact().getIdentityInfo();

                if (IHelper::convertToHex(*IHelper::hash(sessionInfo.mURI)) != info.mHash) continue;
                if (sessionInfo.mProvider != info.mProvider) continue;

                session->forPeerContact().notifyStateChanged();

                found = true;
                break;
              }

              if (found) {
                ZS_LOG_DEBUG(log("already know about this identity thus no reason to create a new identity"))
                break;
              }

              ZS_LOG_DEBUG(log("thus identity is not known, attempt to load it"))

              IPeerFilePrivatePtr peerFilePrivate;
              if (mPeerFiles ) {
                peerFilePrivate = mPeerFiles->getPeerFilePrivate();
              }

              ZS_LOG_DEBUG(log("need to create identity session") + info.getDebugValueString())

              if ((info.mHash.hasData()) &&
                  (info.mProvider.hasData()) &&
                  (info.mSecretSalt.hasData()) &&
                  (info.mReloginAccessKeyEncrypted.hasData()) &&
                  (peerFilePrivate)) {

                BootstrappedNetworkPtr network = IBootstrappedNetworkForServices::prepare(info.mProvider);

                SecureByteBlockPtr password = peerFilePrivate->getPassword();

                SecureByteBlockPtr key = IHelper::hmac(*IHelper::hmacKey((const char *)((const BYTE *)(*password))), "relogin:" + info.mHash);
                SecureByteBlockPtr iv = IHelper::hash(info.mSecretSalt + ":" + IHelper::convertToBase64(*peerFilePrivate->getSalt()));

                // key = hmac(<private-peer-file-secret>, "relogin:" + hash(<identity>))
                // iv=hash(base64(<identity-secret-salt>) + ":" + base64(<private-peer-file-salt>))

                String reloginAccessKey = IHelper::convertToString(*IHelper::decrypt(*key, *iv, *IHelper::convertFromBase64(info.mReloginAccessKeyEncrypted)));

                ZS_LOG_DEBUG(log("creating new identity session") + ", relogin access key=" + reloginAccessKey + info.getDebugValueString())

                ServiceIdentitySessionPtr session = IServiceIdentitySessionForServiceLockbox::relogin(network, reloginAccessKey);
                mAssociatedIdentities[session->forPeerContact().getID()] = session;
                session->forPeerContact().associate(mThisWeak.lock());
              } else {
                ZS_LOG_WARNING(Detail, log("could not create session as data is missing"))
              }

              // IdentityInfo members expecting to be returned:
              //
              // mURI (if known)
              // mHash
              // mProvider
              // mReloginAccessKeyEncrypted

              break;
            }
            case IdentityInfo::Disposition_Remove:
            {
              // this identity was requested to be removed
              handleRemoveDisposition(info, mAssociatedIdentities);
              handleRemoveDisposition(info, mPendingUpdateIdentities);
              handleRemoveDisposition(info, mPendingRemoveIdentities);
              break;
            }
          }
        }

        step();

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              PeerContactIdentityAssociateResultPtr ignore, // will always be NULL
                                                                              message::MessageResultPtr result
                                                                              )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mAssociateMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }

        setError(result->errorCode(), result->errorReason());

        ZS_LOG_WARNING(Detail, log("associate failed"))

        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      RecursiveLock &ServiceLockboxSession::getLock() const
      {
        return mLock;
      }

      //-----------------------------------------------------------------------
      String ServiceLockboxSession::log(const char *message) const
      {
        return String("ServiceLockboxSession [") + Stringize<PUID>(mID).string() + "] " + message;
      }

      //-----------------------------------------------------------------------
      String ServiceLockboxSession::getDebugValueString(bool includeCommaPrefix) const
      {
        AutoRecursiveLock lock(getLock());
        bool firstTime = !includeCommaPrefix;
        return Helper::getDebugValue("peer contact session id", Stringize<typeof(mID)>(mID).string(), firstTime) +
               IBootstrappedNetwork::toDebugString(mBootstrappedNetwork) +
               Helper::getDebugValue("state", toString(mCurrentState), firstTime) +
               Helper::getDebugValue("error code", 0 != mLastError ? Stringize<typeof(mLastError)>(mLastError).string() : String(), firstTime) +
               Helper::getDebugValue("error reason", mLastErrorReason, firstTime) +
               IPeerFiles::toDebugString(mPeerFiles) +
               Helper::getDebugValue("idenitty", mLoginIdentity ? String("true") : String(), firstTime) +
               Helper::getDebugValue("contact user ID", mContactUserID, firstTime) +
               Helper::getDebugValue("contact access token", mContactAccessToken, firstTime) +
               Helper::getDebugValue("contact access secret", mContactAccessSecret, firstTime) +
               Helper::getDebugValue("contact access expires", Time() != mContactAccessExpires ? IMessageHelper::timeToString(mContactAccessExpires) : String(), firstTime) +
               Helper::getDebugValue("regenerate", mRegeneratePeerFiles ? String("true") : String(), firstTime) +
               Helper::getDebugValue("services by type", mServicesByType.size() > 0 ? Stringize<size_t>(mServicesByType.size()).string() : String(), firstTime) +
               Helper::getDebugValue("associated identities", mAssociatedIdentities.size() > 0 ? Stringize<size_t>(mAssociatedIdentities.size()).string() : String(), firstTime) +
               Helper::getDebugValue("last notification hash", mLastNotificationHash ? IHelper::convertToHex(*mLastNotificationHash) : String(), firstTime) +
               Helper::getDebugValue("pending updated identities", mPendingUpdateIdentities.size() > 0 ? Stringize<size_t>(mPendingUpdateIdentities.size()).string() : String(), firstTime) +
               Helper::getDebugValue("pending remove identities", mPendingRemoveIdentities.size() > 0 ? Stringize<size_t>(mPendingRemoveIdentities.size()).string() : String(), firstTime);
      }

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::step()
      {
        if (isShutdown()) {
          ZS_LOG_DEBUG(log("step - already shutdown"))
          cancel();
          return;
        }

        if (!stepLogin()) goto post_step;
        if (!stepPeerFiles()) goto post_step;
        if (!stepServices()) goto post_step;
        if (!stepAssociate()) goto post_step;

        setState(SessionState_Ready);

      post_step:
        postStep();
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::stepLogin()
      {
        if (mLoginMonitor) {
          ZS_LOG_DEBUG(log("waiting for login monitor"))
          return false;
        }

        if (mContactAccessToken.hasData()) {
          ZS_LOG_DEBUG(log("login step is done"))
          return true;
        }

        if (!mBootstrappedNetwork->forServices().isPreparationComplete()) {
          ZS_LOG_DEBUG(log("waiting for bootstrapper to complete"))
          return false;;
        }

        WORD errorCode = 0;
        String reason;

        if (!mBootstrappedNetwork->forServices().wasSuccessful(&errorCode, &reason)) {
          ZS_LOG_WARNING(Detail, log("login failed because of bootstrapper failure"))
          setError(errorCode, reason);
          cancel();
          return false;
        }

        IdentityInfo identityInfo;

        if (!mPeerFiles) {
          // not attempting to login via peer files, need to make sure identity is already logged in...
          ZS_THROW_BAD_STATE_IF(!mLoginIdentity)

          if (!mLoginIdentity->forPeerContact().isLoginComplete()) {
            ZS_LOG_DEBUG(log("waiting for identity login to complete"))
            return false;
          }

          identityInfo = mLoginIdentity->forPeerContact().getIdentityInfo();

          if (mLoginIdentity->forPeerContact().isShutdown()) {
            ZS_LOG_WARNING(Detail, log("identity being used to login is shutdown"))
            setError(IHTTP::HTTPStatusCode_ClientClosedRequest, "Identity used for login already shutdown");
            return false;
          }

          mLoginIdentity->forPeerContact().associate(mThisWeak.lock());
        }

        PeerContactLoginRequestPtr request = PeerContactLoginRequest::create();
        request->domain(mBootstrappedNetwork->forServices().getDomain());

        request->identityInfo(identityInfo);
        request->peerFiles(mPeerFiles);

        mLoginMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<PeerContactLoginResult>::convert(mThisWeak.lock()), request, Seconds(HOOKFLASH_STACK_SERVICE_PEER_CONTACT_TIMEOUT_IN_SECONDS));
        mBootstrappedNetwork->forServices().sendServiceMessage("peer-contact", "peer-contact-login", request);

        ZS_LOG_DEBUG(log("sending login request"))
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::stepPeerFiles()
      {
        if ((mPeerFilesGetMonitor) ||
            (mPeerFilesSetMonitor)) {
          ZS_LOG_DEBUG(log("waiting for peer files request to complete"))
          return false;
        }

        if (!mRegeneratePeerFiles) {
          // check to see if peer files are already associated to identity
          if (mLoginIdentity) {
            IdentityInfo identityInfo = mLoginIdentity->forPeerContact().getIdentityInfo();
            SecureByteBlockPtr privatePeerSecret = mLoginIdentity->forPeerContact().getPrivatePeerFileSecret();

            if ((identityInfo.mContact.hasData()) &&
                (privatePeerSecret)) {
              // should be able to fetch private peer since we know the secret...
              PrivatePeerFileGetRequestPtr request = PrivatePeerFileGetRequest::create();
              request->domain(mBootstrappedNetwork->forServices().getDomain());

              request->contactAccessToken(mContactAccessToken);
              request->contactAccessSecret(mContactAccessSecret);

              String proof = IHelper::convertToHex(*IHelper::hmac(*IHelper::hmacKey((const char *)((const BYTE *)(*privatePeerSecret))), "proof:" + identityInfo.mContact));

              request->privatePeerFileSecretProof(proof);

              mPeerFilesGetMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<PrivatePeerFileGetResult>::convert(mThisWeak.lock()), request, Seconds(HOOKFLASH_STACK_SERVICE_PEER_CONTACT_TIMEOUT_IN_SECONDS));
              mBootstrappedNetwork->forServices().sendServiceMessage("peer-contact", "private-peer-file-get", request);

              ZS_LOG_DEBUG(log("sending private peer file get request"))
              return false;
            }

            ZS_LOG_DEBUG(log("cannot download peer information as peer contact information is not associated to account"))
            //mRegeneratePeerFiles = true;
          }
        }

        if (!mPeerFiles) {
          mRegeneratePeerFiles = true;
        }

        if (!mRegeneratePeerFiles) {
          ZS_LOG_DEBUG(log("step peer files is done"))
          return true;
        }

        if (!mSaltQuery) {
          mSaltQuery = IServiceSaltFetchSignedSaltQuery::fetchSignedSalt(mThisWeak.lock(), IServiceSalt::createServiceSaltFrom(mBootstrappedNetwork), 1);
        }

        if (!mSaltQuery->isComplete()) {
          ZS_LOG_DEBUG(log("waiting for salt service query to complete"))
          return false;
        }

        WORD errorCode = 0;
        String errorReason;

        if (!mSaltQuery->wasSuccessful(&errorCode, &errorReason)) {
          ZS_LOG_WARNING(Detail, log("salt service reported an error"))
          setError(errorCode, errorReason);
          cancel();
          return false;
        }

        ElementPtr signedSaltBundle = mSaltQuery->getNextSignedSalt();
        if (!signedSaltBundle) {
          ZS_LOG_ERROR(Detail, log("salt service failed to return salt"))
          setError(IHTTP::HTTPStatusCode_NoContent, "Salt service failed to return signed salt");
          cancel();
          return false;
        }

        mPeerFiles = IPeerFiles::generate(IHelper::randomString(64), signedSaltBundle);
        if (!mPeerFiles) {
          ZS_LOG_ERROR(Detail, log("failed to generate peer files"))
          setError(IHTTP::HTTPStatusCode_InternalServerError, "Failed to generate peer files");
          cancel();
          return false;
        }

        mSaltQuery->cancel();
        mSaltQuery.reset();

        mRegeneratePeerFiles = false;

        PrivatePeerFileSetRequestPtr request = PrivatePeerFileSetRequest::create();
        request->domain(mBootstrappedNetwork->forServices().getDomain());

        request->contactAccessToken(mContactAccessToken);
        request->contactAccessSecret(mContactAccessSecret);
        request->peerFiles(mPeerFiles);

        mPeerFilesSetMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<PrivatePeerFileSetResult>::convert(mThisWeak.lock()), request, Seconds(HOOKFLASH_STACK_SERVICE_PEER_CONTACT_TIMEOUT_IN_SECONDS));
        mBootstrappedNetwork->forServices().sendServiceMessage("peer-contact", "private-peer-file-set", request);

        ZS_LOG_DEBUG(log("sending private peer file set request"))
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::stepServices()
      {
        if (mServicesMonitor) {
          ZS_LOG_DEBUG(log("waiting for services monitor to complete"))
          return false;
        }
        if (mServicesByType.size() > 0) {
          ZS_LOG_DEBUG(log("services step has completed"))
          return true;
        }

        PeerContactServicesGetRequestPtr request = PeerContactServicesGetRequest::create();
        request->domain(mBootstrappedNetwork->forServices().getDomain());

        request->contactAccessToken(mContactAccessToken);
        request->contactAccessSecret(mContactAccessSecret);

        mServicesMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<PeerContactServicesGetResult>::convert(mThisWeak.lock()), request, Seconds(HOOKFLASH_STACK_SERVICE_PEER_CONTACT_TIMEOUT_IN_SECONDS));
        mBootstrappedNetwork->forServices().sendServiceMessage("peer-contact", "peer-contact-services-get", request);

        ZS_LOG_DEBUG(log("sending private peer file set request"))
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::stepAssociate()
      {
        if (mAssociateMonitor) {
          ZS_LOG_DEBUG(log("waiting for associate monitor to complete"))
          return false;
        }

        if (mLoginIdentity) {
          mAssociatedIdentities[mLoginIdentity->forPeerContact().getID()] = mLoginIdentity;

          mLoginIdentity.reset();

          PeerContactIdentityAssociateRequestPtr request = PeerContactIdentityAssociateRequest::create();
          request->domain(mBootstrappedNetwork->forServices().getDomain());

          request->contactAccessToken(mContactAccessToken);
          request->contactAccessSecret(mContactAccessSecret);
          request->peerFiles(mPeerFiles);

          mAssociateMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<PeerContactIdentityAssociateResult>::convert(mThisWeak.lock()), request, Seconds(HOOKFLASH_STACK_SERVICE_PEER_CONTACT_TIMEOUT_IN_SECONDS));
          mBootstrappedNetwork->forServices().sendServiceMessage("peer-contact", "peer-contact-identity-associate", request);
          ZS_LOG_DEBUG(log("sending original associate request to fetch current associations"))
          return false;
        }

        clearShutdown(mAssociatedIdentities);
        clearShutdown(mPendingUpdateIdentities);
        clearShutdown(mPendingRemoveIdentities);

        if ((mPendingUpdateIdentities.size() < 1) &&
            (mPendingRemoveIdentities.size() < 1)) {
          ZS_LOG_DEBUG(log("no pending identities thus associate is done"))
          return true;
        }

        IdentityInfoList identities;

        for (ServiceIdentitySessionMap::iterator iter = mPendingUpdateIdentities.begin(); iter != mPendingUpdateIdentities.end(); ++iter)
        {
          ServiceIdentitySessionPtr session = (*iter).second;

          if (!session->forPeerContact().isLoginComplete()) {
            ZS_LOG_DEBUG(log("session not ready for association"))
            continue;
          }

          IdentityInfo info = session->forPeerContact().getIdentityInfo();
          if ((info.mURI.isEmpty()) ||
              (info.mProvider.isEmpty()) ||
              (info.mSecretSalt.isEmpty())) {
            ZS_LOG_WARNING(Detail, log("session should be ready for association but it's not"))
            continue;
          }

          info.mDisposition = IdentityInfo::Disposition_Update;
          identities.push_back(info);
        }

        for (ServiceIdentitySessionMap::iterator iter = mPendingRemoveIdentities.begin(); iter != mPendingRemoveIdentities.end(); ++iter)
        {
          ServiceIdentitySessionPtr session = (*iter).second;

          IdentityInfo info = session->forPeerContact().getIdentityInfo();
          if ((info.mURI.isEmpty()) ||
              (info.mProvider.isEmpty())) {
            ZS_LOG_DEBUG(log("session is not ready to be removed"))
            continue;
          }

          info.mDisposition = IdentityInfo::Disposition_Update;
          identities.push_back(info);
        }

        if (identities.size() < 1) {
          ZS_LOG_DEBUG(log("no identities ready for association"))
          return false;
        }

        PeerContactIdentityAssociateRequestPtr request = PeerContactIdentityAssociateRequest::create();
        request->domain(mBootstrappedNetwork->forServices().getDomain());

        request->contactAccessToken(mContactAccessToken);
        request->contactAccessSecret(mContactAccessSecret);

        mAssociateMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<PeerContactIdentityAssociateResult>::convert(mThisWeak.lock()), request, Seconds(HOOKFLASH_STACK_SERVICE_PEER_CONTACT_TIMEOUT_IN_SECONDS));
        mBootstrappedNetwork->forServices().sendServiceMessage("peer-contact", "peer-contact-associate", request);

        ZS_LOG_DEBUG(log("sending peer associate request"))
        return false;
      }

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::postStep()
      {
        MD5 hasher;
        SecureByteBlockPtr output(new SecureByteBlock(hasher.DigestSize()));

        for (ServiceIdentitySessionMap::iterator iter = mAssociatedIdentities.begin(); iter != mAssociatedIdentities.end(); ++iter)
        {
          PUID id = (*iter).first;
          hasher.Update((const BYTE *)(&id), sizeof(id));
        }
        hasher.Final(*output);

        if (mLastNotificationHash) {
          if (0 == IHelper::compare(*output, *mLastNotificationHash)) {
            // no change
            return;
          }
        }

        mLastNotificationHash = output;

        if (mDelegate) {
          try {
            mDelegate->onServiceLockboxSessionAssociatedIdentitiesChanged(mThisWeak.lock());
          } catch(IServiceLockboxSessionDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate gone"))
          }
        }
      }

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::clearShutdown(ServiceIdentitySessionMap &identities) const
      {
        // clear out any identities that are shutdown...
        for (ServiceIdentitySessionMap::iterator iter = identities.begin(); iter != identities.end(); )
        {
          ServiceIdentitySessionMap::iterator current = iter;
          ++iter;

          ServiceIdentitySessionPtr session = (*current).second;
          if (!session->forPeerContact().isShutdown()) continue;

          identities.erase(current);
        }
      }

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::handleRemoveDisposition(
                                                          const IdentityInfo &info,
                                                          ServiceIdentitySessionMap &sessions
                                                          ) const
      {
        for (ServiceIdentitySessionMap::iterator iter = sessions.begin(); iter != sessions.end();)
        {
          ServiceIdentitySessionMap::iterator current = iter;
          ++iter;

          ServiceIdentitySessionPtr session = ((*current).second);
          IdentityInfo sessionInfo = session->forPeerContact().getIdentityInfo();

          if (sessionInfo.mURI != info.mURI) continue;
          if (sessionInfo.mProvider != info.mProvider) continue;

          session->forPeerContact().killAssociation(mThisWeak.lock());

          sessions.erase(current);
        }
      }

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::setState(SessionStates state)
      {
        if (state == mCurrentState) return;

        ZS_LOG_DEBUG(log("state changed") + ", state=" + toString(state) + ", old state=" + toString(mCurrentState) + getDebugValueString())
        mCurrentState = state;

        if (mLoginIdentity) {
          mLoginIdentity->forPeerContact().notifyStateChanged();
        }

        AccountPtr account = mAccount.lock();
        if (account) {
          account->forServiceLockboxSession().notifyServiceLockboxSessionStateChanged();
        }

        ServiceLockboxSessionPtr pThis = mThisWeak.lock();
        if ((pThis) &&
            (mDelegate)) {
          try {
            ZS_LOG_DEBUG(log("attempting to report state to delegate") + getDebugValueString())
            mDelegate->onServiceLockboxSessionStateChanged(pThis, mCurrentState);
          } catch (IServiceLockboxSessionDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate gone"))
          }
        }
      }

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::setError(WORD errorCode, const char *inReason)
      {
        String reason(inReason ? String(inReason) : String());

        if (reason.isEmpty()) {
          reason = IHTTP::toString(IHTTP::toStatusCode(errorCode));
        }
        if (0 != mLastError) {
          ZS_LOG_WARNING(Detail, log("erorr already set thus ignoring new error") + ", new error=" + Stringize<typeof(errorCode)>(errorCode).string() + ", new reason=" + reason + getDebugValueString())
          return;
        }

        mLastError = errorCode;
        mLastErrorReason = reason;
        ZS_LOG_ERROR(Detail, log("error set") + getDebugValueString())
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
#endif //0
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServiceLockboxSession
    #pragma mark


    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServiceLockboxSession
    #pragma mark

    //-------------------------------------------------------------------------
    const char *IServiceLockboxSession::toString(SessionStates state)
    {
      switch (state)
      {
        case SessionState_Pending:                                return "Pending";
        case SessionState_WaitingForBrowserWindowToBeLoaded:      return "Waiting for Browser Window to be Loaded";
        case SessionState_WaitingForBrowserWindowToBeMadeVisible: return "Waiting for Browser Window to be Made Visible";
        case SessionState_WaitingForBrowserWindowToClose:         return "Waiting for Browser Window to Close";
        case SessionState_Ready:                                  return "Ready";
        case SessionState_Shutdown:                               return "Shutdown";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    String IServiceLockboxSession::toDebugString(IServiceLockboxSessionPtr session, bool includeCommaPrefix)
    {
      return internal::ServiceLockboxSession::toDebugString(session, includeCommaPrefix);
    }

    //-------------------------------------------------------------------------
    IServiceLockboxSessionPtr IServiceLockboxSession::login(
                                                                    IServiceLockboxSessionDelegatePtr delegate,
                                                                    IServiceLockboxPtr ServiceLockbox,
                                                                    IServiceIdentitySessionPtr identitySession
                                                                    )
    {
      return internal::IServiceLockboxSessionFactory::singleton().login(delegate, ServiceLockbox, identitySession);
    }

    //-------------------------------------------------------------------------
    IServiceLockboxSessionPtr IServiceLockboxSession::relogin(
                                                              IServiceLockboxSessionDelegatePtr delegate,
                                                              IServiceLockboxPtr serviceLockbox,
                                                              const char *lockboxAccountID,
                                                              const char *identityHalfLockboxKey,
                                                              const char *lockboxHalfLockboxKey
                                                              )
    {
      return internal::IServiceLockboxSessionFactory::singleton().relogin(delegate, serviceLockbox, lockboxAccountID, identityHalfLockboxKey, lockboxHalfLockboxKey);
    }

  }
}
