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

#include <hookflash/stack/internal/types.h>
#include <hookflash/stack/IBootstrappedNetwork.h>
#include <hookflash/stack/IMessageMonitor.h>
#include <hookflash/stack/IMessageSource.h>
#include <hookflash/stack/IServiceIdentity.h>
#include <hookflash/stack/message/identity/IdentityAccessLockboxUpdateResult.h>
#include <hookflash/stack/message/identity/IdentityLookupUpdateResult.h>
#include <hookflash/stack/message/identity/IdentitySignResult.h>

#include <zsLib/MessageQueueAssociator.h>

#include <list>

namespace hookflash
{
  namespace stack
  {
    namespace internal
    {
      using message::identity::IdentityAccessLockboxUpdateResult;
      using message::identity::IdentityAccessLockboxUpdateResultPtr;
      using message::identity::IdentityLookupUpdateResult;
      using message::identity::IdentityLookupUpdateResultPtr;
      using message::identity::IdentitySignResult;
      using message::identity::IdentitySignResultPtr;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServiceIdentitySessionForServiceLockbox
      #pragma mark

      interaction IServiceIdentitySessionForServiceLockbox
      {
        IServiceIdentitySessionForServiceLockbox &forPeerContact() {return *this;}
        const IServiceIdentitySessionForServiceLockbox &forPeerContact() const {return *this;}

        static ServiceIdentitySessionPtr reload(
                                                BootstrappedNetworkPtr network,
                                                const char *identityURI
                                                );

        virtual PUID getID() const = 0;

        virtual void associate(ServiceLockboxSessionPtr lockbox) = 0;
        virtual void killAssociation(ServiceLockboxSessionPtr lockbox) = 0;

        virtual void notifyStateChanged() = 0;

        virtual bool isLoginComplete() const = 0;
        virtual bool isShutdown() const = 0;

        virtual IdentityInfo getIdentityInfo() const = 0;
        virtual SecureByteBlockPtr getPrivatePeerFileSecret() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServiceIdentitySessionAsyncDelegate
      #pragma mark

      interaction IServiceIdentitySessionAsyncDelegate
      {
        virtual void onStep() = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession
      #pragma mark

      class ServiceIdentitySession : public Noop,
                                     public zsLib::MessageQueueAssociator,
                                     public IServiceIdentitySession,
                                     public IMessageSource,
                                     public IServiceIdentitySessionForServiceLockbox,
                                     public IServiceIdentitySessionAsyncDelegate,
                                     public IBootstrappedNetworkDelegate,
                                     public IMessageMonitorResultDelegate<IdentityAccessLockboxUpdateResult>,
                                     public IMessageMonitorResultDelegate<IdentityLookupUpdateResult>,
                                     public IMessageMonitorResultDelegate<IdentitySignResult>
      {
      public:
        friend interaction IServiceIdentitySessionFactory;
        friend interaction IServiceIdentitySession;

        typedef std::list<DocumentPtr> DocumentList;

      protected:
        ServiceIdentitySession(
                               IMessageQueuePtr queue,
                               IServiceLockboxSessionPtr existingLockbox,
                               BootstrappedNetworkPtr network,
                               IServiceIdentitySessionDelegatePtr delegate,
                               const char *outerFrameURLUponReload
                               );

        ServiceIdentitySession(Noop) : Noop(true), MessageQueueAssociator(IMessageQueuePtr()) {};

        void init();

      public:
        ~ServiceIdentitySession();

        static ServiceIdentitySessionPtr convert(IServiceIdentitySessionPtr query);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceIdentitySession => IServiceIdentitySession
        #pragma mark

        static String toDebugString(IServiceIdentitySessionPtr session, bool includeCommaPrefix = true);

        static ServiceIdentitySessionPtr loginWithIdentity(
                                                           IServiceLockboxSessionPtr existingLockbox,
                                                           IServiceIdentitySessionDelegatePtr delegate,
                                                           const char *outerFrameURLUponReload,
                                                           const char *identityURI,
                                                           IServiceIdentityPtr provider = IServiceIdentityPtr() // required if identity URI does not have domain
                                                           );

        static ServiceIdentitySessionPtr loginWithIdentityProvider(
                                                                   IServiceLockboxSessionPtr existingLockbox,
                                                                   IServiceIdentitySessionDelegatePtr delegate,
                                                                   const char *outerFrameURLUponReload,
                                                                   IServiceIdentityPtr provider,
                                                                   const char *legacyIdentityBaseURI = NULL
                                                                   );

        static ServiceIdentitySessionPtr loginWithIdentityBundle(
                                                                 IServiceLockboxSessionPtr existingLockbox,
                                                                 IServiceIdentitySessionDelegatePtr delegate,
                                                                 const char *outerFrameURLUponReload,
                                                                 ElementPtr signedIdentityBundleEl
                                                                 );

        virtual PUID getID() const {return mID;}

        virtual IServiceIdentityPtr getService() const;

        virtual SessionStates getState(
                                       WORD *outLastErrorCode,
                                       String *outLastErrorReason
                                       ) const;

        virtual bool isDelegateAttached() const;
        virtual void attachDelegate(
                                    IServiceIdentitySessionDelegatePtr delegate,
                                    const char *outerFrameURLUponReload
                                    );

        virtual String getIdentityURI() const;
        virtual String getIdentityProviderDomain() const;
        virtual ElementPtr getSignedIdentityBundle() const;

        virtual String getInnerBrowserWindowFrameURL() const;

        virtual void notifyBrowserWindowVisible();
        virtual void notifyBrowserWindowClosed();

        virtual DocumentPtr getNextMessageForInnerBrowerWindowFrame();
        virtual void handleMessageFromInnerBrowserWindowFrame(DocumentPtr unparsedMessage);

        virtual void forceLoginToContinue();

        virtual void cancel();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceIdentitySession => IMessageSource
        #pragma mark

        // (duplicate) virtual PUID getID() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceIdentitySession => IServiceIdentitySessionForServiceLockbox
        #pragma mark

        static ServiceIdentitySessionPtr reload(
                                                IServiceLockboxSessionPtr existingLockbox,
                                                BootstrappedNetworkPtr network,
                                                const char *identityURI
                                                );

        // (duplicate) virtual PUID getID() const;

        virtual void associate(ServiceLockboxSessionPtr lockbox);
        virtual void killAssociation(ServiceLockboxSessionPtr lockbox);

        virtual void notifyStateChanged();

        virtual bool isLoginComplete() const;
        virtual bool isShutdown() const;

        virtual IdentityInfo getIdentityInfo() const;
        virtual SecureByteBlockPtr getPrivatePeerFileSecret() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceIdentitySession => IServiceIdentitySessionAsyncDelegate
        #pragma mark

        virtual void onStep();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceIdentitySession => IBootstrappedNetworkDelegate
        #pragma mark

        virtual void onBootstrappedNetworkPreparationCompleted(IBootstrappedNetworkPtr bootstrappedNetwork);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceIdentitySession => IMessageMonitorResultDelegate<IdentityAccessLockboxUpdateResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        IdentityAccessLockboxUpdateResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             IdentityAccessLockboxUpdateResultPtr ignore, // will always be NULL
                                                             message::MessageResultPtr result
                                                             );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceIdentitySession => IMessageMonitorResultDelegate<IdentityLookupUpdateResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        IdentityLookupUpdateResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             IdentityLookupUpdateResultPtr ignore, // will always be NULL
                                                             message::MessageResultPtr result
                                                             );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceIdentitySession => IMessageMonitorResultDelegate<IdentitySignResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        IdentitySignResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             IdentitySignResultPtr ignore, // will always be NULL
                                                             message::MessageResultPtr result
                                                             );

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceIdentitySession => (internal)
        #pragma mark

        RecursiveLock &getLock() const;

        String log(const char *message) const;

        virtual String getDebugValueString(bool includeCommaPrefix = true) const;

        void step();
        bool stepLoginStart();
        bool stepBrowserWinodw();
        bool stepLoginComplete();
        bool stepAssociate();
        bool stepSign();

        void setState(SessionStates state);
        void setError(WORD errorCode, const char *reason = NULL);
        void sendInnerWindowMessage(MessagePtr message);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceIdentitySession => (data)
        #pragma mark

        PUID mID;
        mutable RecursiveLock mLock;
        ServiceIdentitySessionWeakPtr mThisWeak;

        IServiceIdentitySessionDelegatePtr mDelegate;
        ServiceLockboxSessionWeakPtr mAssociatedLockbox;
//        bool mKillAssociation;

        BootstrappedNetworkPtr mBootstrappedNetwork;

//        IMessageMonitorPtr mLoginStartMonitor;
//        IMessageMonitorPtr mLoginCompleteMonitor;
//        IMessageMonitorPtr mAssociateMonitor;
//        IMessageMonitorPtr mSignMonitor;
//
        SessionStates mCurrentState;
        SessionStates mLastReportedState;

        WORD mLastError;
        String mLastErrorReason;
//
//        bool mLoadedLoginURL;
//        bool mBrowserWindowVisible;
//        bool mBrowserRedirectionComplete;
//
//        String mClientLoginSecret;
//
//        String mClientToken;
//        String mServerToken;
//
//        bool mNeedsBrowserWindowVisible;
//        bool mNotificationSentToInnerBrowserWindow;
//
//        String mRedirectAfterLoginCompleteURL;
//        String mIdentityLoginURL;
//        String mIdentityLoginCompleteURL;
//        Time mIdentityLoginExpires;
//
//        SecureByteBlockPtr mPrivatePeerFileSecret;
//        IdentityInfo mIdentityInfo;
//
//        ElementPtr mSignedIdentityBundleEl;
//        bool mSignedIdentityBundleValidityChecked;
//        bool mDownloadedSignedIdentityBundle;
//
//        DocumentList mPendingMessagesToDeliver;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServiceIdentitySessionFactory
      #pragma mark

      interaction IServiceIdentitySessionFactory
      {
        static IServiceIdentitySessionFactory &singleton();

        virtual ServiceIdentitySessionPtr loginWithIdentity(
                                                            IServiceLockboxSessionPtr existingLockbox,
                                                            IServiceIdentitySessionDelegatePtr delegate,
                                                            const char *outerFrameURLUponReload,
                                                            const char *identityURI,
                                                            IServiceIdentityPtr provider = IServiceIdentityPtr() // required if identity URI does not have domain
                                                            );

        virtual ServiceIdentitySessionPtr loginWithIdentityProvider(
                                                                    IServiceLockboxSessionPtr existingLockbox,
                                                                    IServiceIdentitySessionDelegatePtr delegate,
                                                                    const char *outerFrameURLUponReload,
                                                                    IServiceIdentityPtr provider,
                                                                    const char *legacyIdentityBaseURI = NULL
                                                                    );

        virtual ServiceIdentitySessionPtr loginWithIdentityBundle(
                                                                  IServiceLockboxSessionPtr existingLockbox,
                                                                  IServiceIdentitySessionDelegatePtr delegate,
                                                                  const char *outerFrameURLUponReload,
                                                                  ElementPtr signedIdentityBundle
                                                                  );

        static ServiceIdentitySessionPtr reload(
                                                IServiceLockboxSessionPtr existingLockbox,
                                                BootstrappedNetworkPtr network,
                                                const char *identityURI
                                                );
      };
    }
  }
}

ZS_DECLARE_PROXY_BEGIN(hookflash::stack::internal::IServiceIdentitySessionAsyncDelegate)
ZS_DECLARE_PROXY_METHOD_0(onStep)
ZS_DECLARE_PROXY_END()
