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
#include <openpeer/stack/internal/stack_ServiceNamespaceGrantSession.h>

#include <openpeer/stack/message/identity-lockbox/LockboxAccessResult.h>
#include <openpeer/stack/message/identity-lockbox/LockboxNamespaceGrantChallengeValidateResult.h>
#include <openpeer/stack/message/identity-lockbox/LockboxIdentitiesUpdateResult.h>
#include <openpeer/stack/message/identity-lockbox/LockboxContentGetResult.h>
#include <openpeer/stack/message/identity-lockbox/LockboxContentSetResult.h>
#include <openpeer/stack/message/peer/PeerServicesGetResult.h>

#include <openpeer/stack/IBootstrappedNetwork.h>
#include <openpeer/stack/IMessageMonitor.h>
#include <openpeer/stack/IMessageSource.h>
#include <openpeer/stack/IServiceLockbox.h>
#include <openpeer/stack/IServiceSalt.h>

#include <openpeer/services/IWakeDelegate.h>

#include <zsLib/MessageQueueAssociator.h>

#include <list>

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      using message::identity_lockbox::LockboxAccessResult;
      using message::identity_lockbox::LockboxAccessResultPtr;
      using message::identity_lockbox::LockboxNamespaceGrantChallengeValidateResult;
      using message::identity_lockbox::LockboxNamespaceGrantChallengeValidateResultPtr;
      using message::identity_lockbox::LockboxIdentitiesUpdateResult;
      using message::identity_lockbox::LockboxIdentitiesUpdateResultPtr;
      using message::identity_lockbox::LockboxContentGetResult;
      using message::identity_lockbox::LockboxContentGetResultPtr;
      using message::identity_lockbox::LockboxContentSetResult;
      using message::identity_lockbox::LockboxContentSetResultPtr;
      using message::peer::PeerServicesGetResult;
      using message::peer::PeerServicesGetResultPtr;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServiceLockboxSessionForAccount
      #pragma mark

      interaction IServiceLockboxSessionForAccount
      {
        IServiceLockboxSessionForAccount &forAccount() {return *this;}
        const IServiceLockboxSessionForAccount &forAccount() const {return *this;}

        virtual IServiceLockboxSession::SessionStates getState(
                                                               WORD *lastErrorCode = NULL,
                                                               String *lastErrorReason = NULL
                                                               ) const = 0;

        virtual IPeerFilesPtr getPeerFiles() const = 0;

        virtual void attach(AccountPtr account) = 0;

        virtual Service::MethodPtr findServiceMethod(
                                                     const char *serviceType,
                                                     const char *method
                                                     ) const = 0;

        virtual BootstrappedNetworkPtr getBootstrappedNetwork() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServiceLockboxSessionForServiceIdentity
      #pragma mark

      interaction IServiceLockboxSessionForServiceIdentity
      {
        IServiceLockboxSessionForServiceIdentity &forServiceIdentity() {return *this;}
        const IServiceLockboxSessionForServiceIdentity &forServiceIdentity() const {return *this;}

        virtual IServiceLockboxSession::SessionStates getState(
                                                               WORD *lastErrorCode = NULL,
                                                               String *lastErrorReason = NULL
                                                               ) const = 0;

        virtual IPeerFilesPtr getPeerFiles() const = 0;

        virtual LockboxInfo getLockboxInfo() const = 0;
        virtual IdentityInfo getIdentityInfoForIdentity(
                                                        ServiceIdentitySessionPtr session,
                                                        IPeerFilesPtr *outPeerFiles = NULL
                                                        ) const = 0;

        virtual void notifyStateChanged() = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession
      #pragma mark

      class ServiceLockboxSession : public Noop,
                                    public zsLib::MessageQueueAssociator,
                                    public IServiceLockboxSession,
                                    public IMessageSource,
                                    public IServiceLockboxSessionForAccount,
                                    public IServiceLockboxSessionForServiceIdentity,
                                    public IWakeDelegate,
                                    public IBootstrappedNetworkDelegate,
                                    public IServiceSaltFetchSignedSaltQueryDelegate,
                                    public IServiceNamespaceGrantSessionForServicesWaitForWaitDelegate,
                                    public IServiceNamespaceGrantSessionForServicesQueryDelegate,
                                    public IMessageMonitorResultDelegate<LockboxAccessResult>,
                                    public IMessageMonitorResultDelegate<LockboxNamespaceGrantChallengeValidateResult>,
                                    public IMessageMonitorResultDelegate<LockboxIdentitiesUpdateResult>,
                                    public IMessageMonitorResultDelegate<LockboxContentGetResult>,
                                    public IMessageMonitorResultDelegate<LockboxContentSetResult>,
                                    public IMessageMonitorResultDelegate<PeerServicesGetResult>
      {
      public:
        friend interaction IServiceLockboxSessionFactory;
        friend interaction IServiceLockboxSession;

        typedef IServiceLockboxSession::SessionStates SessionStates;

        typedef PUID ServiceIdentitySessionID;
        typedef std::map<ServiceIdentitySessionID, ServiceIdentitySessionPtr> ServiceIdentitySessionMap;

        typedef std::list<DocumentPtr> DocumentList;

        typedef LockboxContentGetResult::NamespaceURLNameValueMap NamespaceURLNameValueMap;

      protected:
        ServiceLockboxSession(
                              IMessageQueuePtr queue,
                              BootstrappedNetworkPtr network,
                              IServiceLockboxSessionDelegatePtr delegate,
                              ServiceNamespaceGrantSessionPtr grantSession
                              );
        
        ServiceLockboxSession(Noop) : Noop(true), MessageQueueAssociator(IMessageQueuePtr()) {};

        void init();

      public:
        ~ServiceLockboxSession();

        static ServiceLockboxSessionPtr convert(IServiceLockboxSessionPtr session);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IServiceLockboxSession
        #pragma mark

        static String toDebugString(IServiceLockboxSessionPtr session, bool includeCommaPrefix = true);

        static ServiceLockboxSessionPtr login(
                                              IServiceLockboxSessionDelegatePtr delegate,
                                              IServiceLockboxPtr serviceLockbox,
                                              IServiceNamespaceGrantSessionPtr grantSession,
                                              IServiceIdentitySessionPtr identitySession,
                                              bool forceNewAccount = false
                                              );

        static ServiceLockboxSessionPtr relogin(
                                                IServiceLockboxSessionDelegatePtr delegate,
                                                IServiceLockboxPtr serviceLockbox,
                                                IServiceNamespaceGrantSessionPtr grantSession,
                                                const char *lockboxAccountID,
                                                const SecureByteBlock &lockboxKey
                                                );

        virtual PUID getID() const {return mID;}

        virtual IServiceLockboxPtr getService() const;

        virtual SessionStates getState(
                                       WORD *lastErrorCode,
                                       String *lastErrorReason
                                       ) const;

        virtual IPeerFilesPtr getPeerFiles() const;

        virtual String getAccountID() const;
        virtual String getDomain() const;
        virtual String getStableID() const;

        virtual SecureByteBlockPtr getLockboxKey() const;
        

        virtual ServiceIdentitySessionListPtr getAssociatedIdentities() const;
        virtual void associateIdentities(
                                         const ServiceIdentitySessionList &identitiesToAssociate,
                                         const ServiceIdentitySessionList &identitiesToRemove
                                         );

        virtual void cancel();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IMessageSource
        #pragma mark

        // (duplicate) virtual PUID getID() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IServiceLockboxSessionForAccount
        #pragma mark

        // (duplicate) virtual SessionStates getState(
        //                                            WORD *lastErrorCode,
        //                                            String *lastErrorReason
        //                                            ) const;

        // (duplicate) virtual IPeerFilesPtr getPeerFiles() const;

        virtual void attach(AccountPtr account);

        virtual Service::MethodPtr findServiceMethod(
                                                     const char *serviceType,
                                                     const char *method
                                                     ) const;

        virtual BootstrappedNetworkPtr getBootstrappedNetwork() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IServiceLockboxSessionForServiceIdentity
        #pragma mark

        // (duplicate) virtual SessionStates getState(
        //                                            WORD *lastErrorCode,
        //                                            String *lastErrorReason
        //                                            ) const;

        // (duplicate) virtual IPeerFilesPtr getPeerFiles() const;

        virtual LockboxInfo getLockboxInfo() const;
        virtual IdentityInfo getIdentityInfoForIdentity(
                                                        ServiceIdentitySessionPtr session,
                                                        IPeerFilesPtr *outPeerFiles = NULL
                                                        ) const;

        virtual void notifyStateChanged();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IWakeDelegate
        #pragma mark

        virtual void onWake();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IBootstrappedNetworkDelegate
        #pragma mark

        virtual void onBootstrappedNetworkPreparationCompleted(IBootstrappedNetworkPtr bootstrappedNetwork);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IServiceSaltFetchSignedSaltQueryDelegate
        #pragma mark

        virtual void onServiceSaltFetchSignedSaltCompleted(IServiceSaltFetchSignedSaltQueryPtr query);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IServiceNamespaceGrantSessionForServicesWaitForWaitDelegate
        #pragma mark

        virtual void onServiceNamespaceGrantSessionForServicesWaitComplete(IServiceNamespaceGrantSessionPtr session);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IServiceNamespaceGrantSessionForServicesQueryDelegate
        #pragma mark

        virtual void onServiceNamespaceGrantSessionForServicesQueryComplete(
                                                                            IServiceNamespaceGrantSessionForServicesQueryPtr query,
                                                                            ElementPtr namespaceGrantChallengeBundleEl
                                                                            );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<LockboxAccessResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        LockboxAccessResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             LockboxAccessResultPtr ignore, // will always be NULL
                                                             message::MessageResultPtr result
                                                             );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<LockboxNamespaceGrantChallengeValidateResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        LockboxNamespaceGrantChallengeValidateResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             LockboxNamespaceGrantChallengeValidateResultPtr ignore, // will always be NULL
                                                             message::MessageResultPtr result
                                                             );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<LockboxIdentitiesUpdateResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        LockboxIdentitiesUpdateResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             LockboxIdentitiesUpdateResultPtr ignore, // will always be NULL
                                                             message::MessageResultPtr result
                                                             );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<LockboxContentGetResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        LockboxContentGetResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             LockboxContentGetResultPtr ignore, // will always be NULL
                                                             message::MessageResultPtr result
                                                             );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<LockboxContentSetResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        LockboxContentSetResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             LockboxContentSetResultPtr ignore, // will always be NULL
                                                             message::MessageResultPtr result
                                                             );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<PeerServicesGetResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        PeerServicesGetResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             PeerServicesGetResultPtr ignore, // will always be NULL
                                                             message::MessageResultPtr result
                                                             );

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => (internal)
        #pragma mark

        RecursiveLock &getLock() const;

        String log(const char *message) const;
        virtual String getDebugValueString(bool includeCommaPrefix = true) const;

        bool isShutdown() const {return SessionState_Shutdown == mCurrentState;}

        void step();
        bool stepBootstrapper();
        bool stepGrantLock();
        bool stepIdentityLogin();
        bool stepLockboxAccess();
        bool stepGrantLockClear();
        bool stepGrantChallenge();
        bool stepContentGet();
        bool stepPreparePeerFiles();
        bool stepServicesGet();
        bool stepLoginIdentityBecomeAssociated();
        bool stepConvertFromServerToRealIdentities();
        bool stepPruneDuplicatePendingIdentities();
        bool stepPruneShutdownIdentities();
        bool stepPendingAssociationAndRemoval();
        bool stepContentUpdate();

        void postStep();

        void clearShutdown(ServiceIdentitySessionMap &identities) const;
        void handleRemoveDisposition(
                                     const IdentityInfo &info,
                                     ServiceIdentitySessionMap &sessions
                                     ) const;

        void setState(SessionStates state);
        void setError(WORD errorCode, const char *reason = NULL);
        void calculateAndNotifyIdentityChanges();
        void sendInnerWindowMessage(MessagePtr message);
        String getContent(
                          const char *namespaceURL,
                          const char *valueName
                          ) const;
        String getRawContent(
                             const char *namespaceURL,
                             const char *valueName
                             ) const;
        void setContent(
                        const char *namespaceURL,
                        const char *valueName,
                        const char *value
                        );
        void clearContent(
                          const char *namespaceURL,
                          const char *valueName
                          );

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => (data)
        #pragma mark

        AutoPUID mID;
        mutable RecursiveLock mLock;
        ServiceLockboxSessionWeakPtr mThisWeak;

        IServiceLockboxSessionDelegatePtr mDelegate;
        AccountWeakPtr mAccount;

        SessionStates mCurrentState;

        AutoWORD mLastError;
        String mLastErrorReason;

        BootstrappedNetworkPtr mBootstrappedNetwork;
        ServiceNamespaceGrantSessionPtr mGrantSession;
        IServiceNamespaceGrantSessionForServicesQueryPtr mGrantQuery;
        IServiceNamespaceGrantSessionForServicesWaitPtr mGrantWait;

        IMessageMonitorPtr mLockboxAccessMonitor;
        IMessageMonitorPtr mLockboxNamespaceGrantChallengeValidateMonitor;
        IMessageMonitorPtr mLockboxIdentitiesUpdateMonitor;
        IMessageMonitorPtr mLockboxContentGetMonitor;
        IMessageMonitorPtr mLockboxContentSetMonitor;
        IMessageMonitorPtr mPeerServicesGetMonitor;
        
        LockboxInfo mLockboxInfo;

        ServiceIdentitySessionPtr mLoginIdentity;

        IPeerFilesPtr mPeerFiles;

        AutoBool mObtainedLock;

        AutoBool mLoginIdentitySetToBecomeAssociated;

        AutoBool mForceNewAccount;

        IServiceSaltFetchSignedSaltQueryPtr mSaltQuery;
        ServiceTypeMap mServicesByType;

        IdentityInfoList mServerIdentities;

        ServiceIdentitySessionMap mAssociatedIdentities;
        SecureByteBlockPtr mLastNotificationHash;

        ServiceIdentitySessionMap mPendingUpdateIdentities;
        ServiceIdentitySessionMap mPendingRemoveIdentities;

        SecureByteBlockPtr mReloginChangeHash;

        NamespaceURLNameValueMap mContent;

        NamespaceURLNameValueMap mUpdatedContent;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServiceLockboxSessionFactory
      #pragma mark

      interaction IServiceLockboxSessionFactory
      {
        static IServiceLockboxSessionFactory &singleton();

        virtual ServiceLockboxSessionPtr login(
                                               IServiceLockboxSessionDelegatePtr delegate,
                                               IServiceLockboxPtr ServiceLockbox,
                                               IServiceNamespaceGrantSessionPtr grantSession,
                                               IServiceIdentitySessionPtr identitySession,
                                               bool forceNewAccount = false
                                               );
        
        virtual ServiceLockboxSessionPtr relogin(
                                                 IServiceLockboxSessionDelegatePtr delegate,
                                                 IServiceLockboxPtr serviceLockbox,
                                                 IServiceNamespaceGrantSessionPtr grantSession,
                                                 const char *lockboxAccountID,
                                                 const SecureByteBlock &lockboxKey
                                                 );
      };
      
    }
  }
}
