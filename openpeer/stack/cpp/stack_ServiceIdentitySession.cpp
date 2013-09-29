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

#include <openpeer/stack/internal/stack_ServiceIdentitySession.h>
#include <openpeer/stack/internal/stack_ServiceLockboxSession.h>
#include <openpeer/stack/message/identity/IdentityAccessWindowRequest.h>
#include <openpeer/stack/message/identity/IdentityAccessWindowResult.h>
#include <openpeer/stack/message/identity/IdentityAccessStartNotify.h>
#include <openpeer/stack/message/identity/IdentityAccessCompleteNotify.h>
#include <openpeer/stack/message/identity/IdentityAccessLockboxUpdateRequest.h>
#include <openpeer/stack/message/identity/IdentityLookupUpdateRequest.h>
#include <openpeer/stack/message/identity/IdentityAccessRolodexCredentialsGetRequest.h>
#include <openpeer/stack/message/identity-lookup/IdentityLookupRequest.h>
#include <openpeer/stack/message/rolodex/RolodexAccessRequest.h>
#include <openpeer/stack/message/rolodex/RolodexNamespaceGrantChallengeValidateRequest.h>
#include <openpeer/stack/message/rolodex/RolodexContactsGetRequest.h>
#include <openpeer/stack/internal/stack_BootstrappedNetwork.h>
#include <openpeer/stack/internal/stack_Helper.h>
#include <openpeer/stack/IHelper.h>
#include <openpeer/stack/IPeer.h>
#include <openpeer/stack/IPeerFiles.h>
#include <openpeer/stack/IPeerFilePrivate.h>
#include <openpeer/stack/IPeerFilePublic.h>
#include <openpeer/stack/message/IMessageHelper.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>
#include <zsLib/helpers.h>

#include <zsLib/Stringize.h>

#include <zsLib/RegEx.h>

#define OPENPEER_STACK_SERVICE_IDENTITY_TIMEOUT_IN_SECONDS (60*2)
#define OPENPEER_STACK_SERVICE_IDENTITY_MAX_PERCENTAGE_TIME_REMAINING_BEFORE_RESIGN_IDENTITY_REQUIRED (20) // at 20% of the remaining on the certificate before expiry, resign

#define OPENPEER_STACK_SERVICE_IDENTITY_SIGN_CREATE_SHOULD_NOT_BE_BEFORE_NOW_IN_HOURS (72)
#define OPENPEER_STACK_SERVICE_IDENTITY_MAX_CONSUMED_TIME_PERCENTAGE_BEFORE_IDENTITY_PROOF_REFRESH (80)

#define OPENPEER_STACK_SERVICE_IDENTITY_ROLODEX_CONTACTS_NAMESPACE "https://meta.openpeer.org/permission/rolodex-contacts"

#define OPENPEER_STACK_SERVICE_IDENTITY_ROLODEX_DOWNLOAD_FROZEN_VALUE "FREEZE-"
#define OPENPEER_STACK_SERVICE_IDENTITY_ROLODEX_ERROR_RETRY_TIME_IN_SECONDS ((60)*2)
#define OPENPEER_STACK_SERVICE_IDENTITY_MAX_ROLODEX_ERROR_RETRY_TIME_IN_SECONDS (((60)*60) * 24)

#define OPENPEER_STACK_SERVICE_IDENTITY_ROLODEX_NOT_SUPPORTED_FOR_IDENTITY (404)

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    using zsLib::string;
    using zsLib::Hours;
    using stack::message::IMessageHelper;

    namespace internal
    {
      using services::IHelper;

      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

      using message::identity::IdentityAccessWindowRequest;
      using message::identity::IdentityAccessWindowRequestPtr;
      using message::identity::IdentityAccessWindowResult;
      using message::identity::IdentityAccessWindowResultPtr;
      using message::identity::IdentityAccessStartNotify;
      using message::identity::IdentityAccessStartNotifyPtr;
      using message::identity::IdentityAccessCompleteNotify;
      using message::identity::IdentityAccessCompleteNotifyPtr;
      using message::identity::IdentityAccessLockboxUpdateRequest;
      using message::identity::IdentityAccessLockboxUpdateRequestPtr;
      using message::identity::IdentityLookupUpdateRequest;
      using message::identity::IdentityLookupUpdateRequestPtr;
      using message::identity::IdentityAccessRolodexCredentialsGetRequest;
      using message::identity::IdentityAccessRolodexCredentialsGetRequestPtr;
      using message::identity_lookup::IdentityLookupRequest;
      using message::identity_lookup::IdentityLookupRequestPtr;
      using message::rolodex::RolodexAccessRequest;
      using message::rolodex::RolodexAccessRequestPtr;
      using message::rolodex::RolodexNamespaceGrantChallengeValidateRequest;
      using message::rolodex::RolodexNamespaceGrantChallengeValidateRequestPtr;
      using message::rolodex::RolodexContactsGetRequest;
      using message::rolodex::RolodexContactsGetRequestPtr;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      static void getNamespaces(NamespaceInfoMap &outNamespaces)
      {
        static const char *gPermissions[] = {
          OPENPEER_STACK_SERVICE_IDENTITY_ROLODEX_CONTACTS_NAMESPACE,
          NULL
        };

        for (int index = 0; NULL != gPermissions[index]; ++index)
        {
          NamespaceInfo info;
          info.mURL = gPermissions[index];
          outNamespaces[info.mURL] = info;
        }
      }
      
      //-----------------------------------------------------------------------
      static char getSafeSplitChar(const String &identifier)
      {
        const char *testChars = ",; :./\\*#!$%&@?~+=-_|^<>[]{}()";

        while (*testChars) {
          if (String::npos == identifier.find(*testChars)) {
            return *testChars;
          }

          ++testChars;
        }

        return 0;
      }
      
      //-----------------------------------------------------------------------
      static bool isSame(
                         IPeerFilePublicPtr peerFilePublic1,
                         IPeerFilePublicPtr peerFilePublic2
                         )
      {
        if (!peerFilePublic1) return false;
        if (!peerFilePublic2) return false;

        return peerFilePublic1->getPeerURI() == peerFilePublic2->getPeerURI();
      }

      //-------------------------------------------------------------------------
      static bool extractAndVerifyProof(
                                        ElementPtr identityProofBundleEl,
                                        IPeerFilePublicPtr peerFilePublic,
                                        String *outPeerURI,
                                        String *outIdentityURI,
                                        String *outStableID,
                                        Time *outCreated,
                                        Time *outExpires
                                        )
      {
        if (outPeerURI) {
          *outPeerURI = String();
        }
        if (outIdentityURI) {
          *outIdentityURI = String();
        }
        if (outStableID) {
          *outStableID = String();
        }
        if (outCreated) {
          *outCreated = Time();
        }
        if (outExpires) {
          *outExpires = Time();
        }

        ZS_THROW_INVALID_ARGUMENT_IF(!identityProofBundleEl)

        try {
          ElementPtr identityProofEl = identityProofBundleEl->findFirstChildElementChecked("identityProof");
          ElementPtr contactProofBundleEl = identityProofEl->findFirstChildElementChecked("contactProofBundle");
          ElementPtr contactProofEl = contactProofBundleEl->findFirstChildElementChecked("contactProof");

          ElementPtr stableIDEl = contactProofEl->findFirstChildElement("stableID");      // optional

          ElementPtr contactEl = contactProofEl->findFirstChildElementChecked("contact");
          ElementPtr uriEl = contactProofEl->findFirstChildElementChecked("uri");
          ElementPtr createdEl = contactProofEl->findFirstChildElementChecked("created");
          ElementPtr expiresEl = contactProofEl->findFirstChildElementChecked("expires");

          Time created = IHelper::stringToTime(IMessageHelper::getElementTextAndDecode(createdEl));
          Time expires = IHelper::stringToTime(IMessageHelper::getElementTextAndDecode(expiresEl));

          if ((outStableID) &&
              (stableIDEl)) {
            *outStableID = IMessageHelper::getElementTextAndDecode(stableIDEl);
          }
          if (outCreated) {
            *outCreated = created;
          }
          if (outExpires) {
            *outExpires = expires;
          }

          if (outIdentityURI) {
            *outIdentityURI = IMessageHelper::getElementTextAndDecode(uriEl);
          }

          String peerURI = IMessageHelper::getElementTextAndDecode(contactEl);

          if (outPeerURI) {
            *outPeerURI = peerURI;
          }

          if (peerFilePublic) {
            if (peerURI != peerFilePublic->getPeerURI()) {
              ZS_LOG_WARNING(Detail, String("IServiceIdentity [] peer URI check failed") + ", bundle URI=" + peerURI + ", peer file URI=" + peerFilePublic->getPeerURI())
              return false;
            }
            if (peerFilePublic->verifySignature(contactProofEl)) {
              ZS_LOG_WARNING(Detail, String("IServiceIdentity [] signature validation failed") + ", peer URI=" + peerURI)
              return false;
            }
          }

          Time tick = zsLib::now();
          if (created < tick + Hours(OPENPEER_STACK_SERVICE_IDENTITY_SIGN_CREATE_SHOULD_NOT_BE_BEFORE_NOW_IN_HOURS)) {
            ZS_LOG_WARNING(Detail, String("IServiceIdentity [] creation date is invalid") + ", created=" + IHelper::timeToString(created) + ", now=" + IHelper::timeToString(tick))
            return false;
          }

          if (tick > expires) {
            ZS_LOG_WARNING(Detail, String("IServiceIdentity [] signature expired") + ", expires=" + IHelper::timeToString(expires) + ", now=" + IHelper::timeToString(tick))
            return false;
          }
          
        } catch (zsLib::XML::Exceptions::CheckFailed &) {
          ZS_LOG_WARNING(Detail, "IServiceIdentity [] check failure")
          return false;
        }
        
        return true;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServiceIdentitySessionForServiceLockbox
      #pragma mark

      //-----------------------------------------------------------------------
      ServiceIdentitySessionPtr IServiceIdentitySessionForServiceLockbox::reload(
                                                                                 BootstrappedNetworkPtr provider,
                                                                                 IServiceNamespaceGrantSessionPtr grantSession,
                                                                                 IServiceLockboxSessionPtr existingLockbox,
                                                                                 const char *identityURI,
                                                                                 const char *reloginKey
                                                                                 )
      {
        return IServiceIdentitySessionFactory::singleton().reload(provider, grantSession, existingLockbox, identityURI, reloginKey);
        return ServiceIdentitySessionPtr();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession
      #pragma mark

      //-----------------------------------------------------------------------
      ServiceIdentitySession::ServiceIdentitySession(
                                                     IMessageQueuePtr queue,
                                                     IServiceIdentitySessionDelegatePtr delegate,
                                                     BootstrappedNetworkPtr providerNetwork,
                                                     BootstrappedNetworkPtr identityNetwork,
                                                     ServiceNamespaceGrantSessionPtr grantSession,
                                                     ServiceLockboxSessionPtr existingLockbox,
                                                     const char *outerFrameURLUponReload
                                                     ) :
        zsLib::MessageQueueAssociator(queue),
        mDelegate(delegate ? IServiceIdentitySessionDelegateProxy::createWeak(IStackForInternal::queueDelegate(), delegate) : IServiceIdentitySessionDelegatePtr()),
        mAssociatedLockbox(existingLockbox),
        mProviderBootstrappedNetwork(providerNetwork),
        mIdentityBootstrappedNetwork(identityNetwork),
        mGrantSession(grantSession),
        mCurrentState(SessionState_Pending),
        mLastReportedState(SessionState_Pending),
        mOuterFrameURLUponReload(outerFrameURLUponReload),
        mFrozenVersion(OPENPEER_STACK_SERVICE_IDENTITY_ROLODEX_DOWNLOAD_FROZEN_VALUE + IHelper::randomString(32)),
        mNextRetryAfterFailureTime(Seconds(OPENPEER_STACK_SERVICE_IDENTITY_ROLODEX_ERROR_RETRY_TIME_IN_SECONDS))
      {
        ZS_LOG_DEBUG(log("created"))
        mRolodexInfo.mVersion = mFrozenVersion;
      }

      //-----------------------------------------------------------------------
      ServiceIdentitySession::~ServiceIdentitySession()
      {
        if(isNoop()) return;

        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::init()
      {
        if (mIdentityBootstrappedNetwork) {
          IBootstrappedNetworkForServices::prepare(mIdentityBootstrappedNetwork->forServices().getDomain(), mThisWeak.lock());
        }
        if (mProviderBootstrappedNetwork) {
          IBootstrappedNetworkForServices::prepare(mProviderBootstrappedNetwork->forServices().getDomain(), mThisWeak.lock());
        }

        // one or the other must be valid or a login is not possible
        ZS_THROW_BAD_STATE_IF((!mIdentityBootstrappedNetwork) && (!mProviderBootstrappedNetwork))
      }

      //-----------------------------------------------------------------------
      ServiceIdentitySessionPtr ServiceIdentitySession::convert(IServiceIdentitySessionPtr query)
      {
        return boost::dynamic_pointer_cast<ServiceIdentitySession>(query);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IServiceIdentitySession
      #pragma mark

      //-----------------------------------------------------------------------
      String ServiceIdentitySession::toDebugString(IServiceIdentitySessionPtr session, bool includeCommaPrefix)
      {
        if (!session) return includeCommaPrefix ? String(", identity session=(null)") : String("identity session=(null)");
        return ServiceIdentitySession::convert(session)->getDebugValueString(includeCommaPrefix);
      }

      //-----------------------------------------------------------------------
      ServiceIdentitySessionPtr ServiceIdentitySession::loginWithIdentity(
                                                                          IServiceIdentitySessionDelegatePtr delegate,
                                                                          IServiceIdentityPtr provider,
                                                                          IServiceNamespaceGrantSessionPtr grantSession,
                                                                          IServiceLockboxSessionPtr existingLockbox,
                                                                          const char *outerFrameURLUponReload,
                                                                          const char *identityURI_or_identityBaseURI
                                                                          )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ZS_THROW_INVALID_ARGUMENT_IF(!grantSession)
        ZS_THROW_INVALID_ARGUMENT_IF(!outerFrameURLUponReload)

        if (identityURI_or_identityBaseURI) {
          if (!provider) {
            if (IServiceIdentity::isLegacy(identityURI_or_identityBaseURI)) {
              ZS_THROW_INVALID_ARGUMENT_IF(!provider) // provider can normally be derived from the identity but only if the identity contains a provider
            }
          }

          if ((!IServiceIdentity::isValid(identityURI_or_identityBaseURI)) &&
              (!IServiceIdentity::isValidBase(identityURI_or_identityBaseURI))) {
            ZS_LOG_ERROR(Detail, String("identity URI specified is not valid, uri=") + identityURI_or_identityBaseURI)
            return ServiceIdentitySessionPtr();
          }
        } else {
          ZS_THROW_INVALID_ARGUMENT_IF(!provider) // provider can normally be derived from the identity but only if the identity contains a provider
        }

        BootstrappedNetworkPtr identityNetwork;
        BootstrappedNetworkPtr providerNetwork = BootstrappedNetwork::convert(provider);

        if (identityURI_or_identityBaseURI) {
          if (IServiceIdentity::isValid(identityURI_or_identityBaseURI)) {
            if (!IServiceIdentity::isLegacy(identityURI_or_identityBaseURI)) {
              String domain;
              String identifier;
              IServiceIdentity::splitURI(identityURI_or_identityBaseURI, domain, identifier);
              identityNetwork = IBootstrappedNetworkForServices::prepare(domain);
            }
          }
        }

        ServiceIdentitySessionPtr pThis(new ServiceIdentitySession(IStackForInternal::queueStack(), delegate, providerNetwork, identityNetwork, ServiceNamespaceGrantSession::convert(grantSession), ServiceLockboxSession::convert(existingLockbox), outerFrameURLUponReload));
        pThis->mThisWeak = pThis;
        if (identityURI_or_identityBaseURI) {
          if (IServiceIdentity::isValidBase(identityURI_or_identityBaseURI)) {
            pThis->mIdentityInfo.mBase = identityURI_or_identityBaseURI;
          } else {
            pThis->mIdentityInfo.mURI = identityURI_or_identityBaseURI;
          }
        }
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      ServiceIdentitySessionPtr ServiceIdentitySession::loginWithIdentityPreauthorized(
                                                                                       IServiceIdentitySessionDelegatePtr delegate,
                                                                                       IServiceIdentityPtr provider,
                                                                                       IServiceNamespaceGrantSessionPtr grantSession,
                                                                                       IServiceLockboxSessionPtr existingLockbox,
                                                                                       const char *identityURI,
                                                                                       const char *identityAccessToken,
                                                                                       const char *identityAccessSecret,
                                                                                       Time identityAccessSecretExpires
                                                                                       )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ZS_THROW_INVALID_ARGUMENT_IF(!grantSession)
        ZS_THROW_INVALID_ARGUMENT_IF(!existingLockbox)
        ZS_THROW_INVALID_ARGUMENT_IF(!identityURI)
        ZS_THROW_INVALID_ARGUMENT_IF(!identityAccessToken)
        ZS_THROW_INVALID_ARGUMENT_IF(!identityAccessSecret)

        if (!provider) {
          if (IServiceIdentity::isLegacy(identityURI)) {
            ZS_THROW_INVALID_ARGUMENT_IF(!provider) // provider can normally be derived from the identity but only if the identity contains a provider
          }
        }

        if (!IServiceIdentity::isValid(identityURI)) {
          ZS_LOG_ERROR(Detail, String("identity URI specified is not valid, uri=") + identityURI)
          return ServiceIdentitySessionPtr();
        }

        BootstrappedNetworkPtr identityNetwork;
        BootstrappedNetworkPtr providerNetwork = BootstrappedNetwork::convert(provider);

        if (IServiceIdentity::isValid(identityURI)) {
          if (!IServiceIdentity::isLegacy(identityURI)) {
            String domain;
            String identifier;
            IServiceIdentity::splitURI(identityURI, domain, identifier);
            identityNetwork = IBootstrappedNetworkForServices::prepare(domain);
          }
        }

        ServiceIdentitySessionPtr pThis(new ServiceIdentitySession(IStackForInternal::queueStack(), delegate, providerNetwork, identityNetwork, ServiceNamespaceGrantSession::convert(grantSession), ServiceLockboxSession::convert(existingLockbox), NULL));
        pThis->mThisWeak = pThis;
        pThis->mIdentityInfo.mURI = identityURI;
        pThis->mIdentityInfo.mAccessToken = String(identityAccessToken);
        pThis->mIdentityInfo.mAccessSecret = String(identityAccessSecret);
        pThis->mIdentityInfo.mAccessSecretExpires = identityAccessSecretExpires;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      IServiceIdentityPtr ServiceIdentitySession::getService() const
      {
        AutoRecursiveLock lock(getLock());
        if (mIdentityBootstrappedNetwork) {
          return mIdentityBootstrappedNetwork;
        }
        return mProviderBootstrappedNetwork;
      }

      //-----------------------------------------------------------------------
      IServiceIdentitySession::SessionStates ServiceIdentitySession::getState(
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
      bool ServiceIdentitySession::isDelegateAttached() const
      {
        AutoRecursiveLock lock(getLock());
        return ((bool)mDelegate);
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::attachDelegate(
                                                  IServiceIdentitySessionDelegatePtr delegate,
                                                  const char *outerFrameURLUponReload
                                                  )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!outerFrameURLUponReload)
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)

        ZS_LOG_DEBUG(log("attach delegate called") + ", frame URL=" + outerFrameURLUponReload)

        AutoRecursiveLock lock(getLock());

        mDelegate = IServiceIdentitySessionDelegateProxy::createWeak(IStackForInternal::queueDelegate(), delegate);
        mOuterFrameURLUponReload = (outerFrameURLUponReload ? String(outerFrameURLUponReload) : String());

        try {
          if (mCurrentState != mLastReportedState) {
            mDelegate->onServiceIdentitySessionStateChanged(mThisWeak.lock(), mCurrentState);
            mLastReportedState = mCurrentState;
          }
          if (mPendingMessagesToDeliver.size() > 0) {
            mDelegate->onServiceIdentitySessionPendingMessageForInnerBrowserWindowFrame(mThisWeak.lock());
          }
        } catch(IServiceIdentitySessionDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }

        step();
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::attachDelegateAndPreauthorizeLogin(
                                                                      IServiceIdentitySessionDelegatePtr delegate,
                                                                      const char *identityAccessToken,
                                                                      const char *identityAccessSecret,
                                                                      Time identityAccessSecretExpires
                                                                      )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ZS_THROW_INVALID_ARGUMENT_IF(!identityAccessToken)
        ZS_THROW_INVALID_ARGUMENT_IF(!identityAccessSecret)

        ZS_LOG_DEBUG(log("attach delegate and preautothorize login called") + ", access token=" + identityAccessToken + ", access secret=" + identityAccessSecret)

        AutoRecursiveLock lock(getLock());

        mDelegate = IServiceIdentitySessionDelegateProxy::createWeak(IStackForInternal::queueDelegate(), delegate);

        mIdentityInfo.mAccessToken = String(identityAccessToken);
        mIdentityInfo.mAccessSecret = String(identityAccessSecret);
        mIdentityInfo.mAccessSecretExpires = identityAccessSecretExpires;

        try {
          if (mCurrentState != mLastReportedState) {
            mDelegate->onServiceIdentitySessionStateChanged(mThisWeak.lock(), mCurrentState);
            mLastReportedState = mCurrentState;
          }
        } catch(IServiceIdentitySessionDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }

        step();
      }

      //-----------------------------------------------------------------------
      String ServiceIdentitySession::getIdentityURI() const
      {
        AutoRecursiveLock lock(getLock());
        return mIdentityInfo.mURI.hasData() ? mIdentityInfo.mURI : mIdentityInfo.mBase;
      }

      //-----------------------------------------------------------------------
      String ServiceIdentitySession::getIdentityProviderDomain() const
      {
        AutoRecursiveLock lock(getLock());
        if (mActiveBootstrappedNetwork) {
          return mActiveBootstrappedNetwork->forServices().getDomain();
        }
        return mProviderBootstrappedNetwork->forServices().getDomain();
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::getIdentityInfo(IdentityInfo &outIdentityInfo) const
      {
        AutoRecursiveLock lock(getLock());

        outIdentityInfo = mPreviousLookupInfo;
        outIdentityInfo.mergeFrom(mIdentityInfo, true);
      }

      //-----------------------------------------------------------------------
      String ServiceIdentitySession::getInnerBrowserWindowFrameURL() const
      {
        AutoRecursiveLock lock(getLock());
        if (!mActiveBootstrappedNetwork) return String();

        return mActiveBootstrappedNetwork->forServices().getServiceURI("identity", "identity-access-inner-frame");
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::notifyBrowserWindowVisible()
      {
        AutoRecursiveLock lock(getLock());
        ZS_LOG_DEBUG(log("browser window visible"))
        get(mBrowserWindowVisible) = true;
        step();
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::notifyBrowserWindowClosed()
      {
        AutoRecursiveLock lock(getLock());
        ZS_LOG_DEBUG(log("browser window close called"))
        get(mBrowserWindowClosed) = true;
        step();
      }

      //-----------------------------------------------------------------------
      DocumentPtr ServiceIdentitySession::getNextMessageForInnerBrowerWindowFrame()
      {
        AutoRecursiveLock lock(getLock());
        if (mPendingMessagesToDeliver.size() < 1) return DocumentPtr();

        DocumentPtr result = mPendingMessagesToDeliver.front();
        mPendingMessagesToDeliver.pop_front();

        if (ZS_GET_LOG_LEVEL() >= zsLib::Log::Trace) {
          GeneratorPtr generator = Generator::createJSONGenerator();
          boost::shared_array<char> jsonText = generator->write(result);
          ZS_LOG_BASIC(log(">>>>>>>>>>>>> MESSAGE TO INNER FRAME (START) >>>>>>>>>>>>>"))
          ZS_LOG_BASIC(log("sending inner frame message") + ", message=" + (CSTR)(jsonText.get()))
          ZS_LOG_BASIC(log(">>>>>>>>>>>>>  MESSAGE TO INNER FRAME (END)  >>>>>>>>>>>>>"))
        }

        if (mDelegate) {
          if (mPendingMessagesToDeliver.size() > 0) {
            try {
              ZS_LOG_DEBUG(log("notifying about another pending message for the inner frame"))
              mDelegate->onServiceIdentitySessionPendingMessageForInnerBrowserWindowFrame(mThisWeak.lock());
            } catch (IServiceIdentitySessionDelegateProxy::Exceptions::DelegateGone &) {
              ZS_LOG_WARNING(Detail, log("delegate gone"))
            }
          }
        }
        return result;
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::handleMessageFromInnerBrowserWindowFrame(DocumentPtr unparsedMessage)
      {
        if (ZS_GET_LOG_LEVEL() >= zsLib::Log::Trace) {
          GeneratorPtr generator = Generator::createJSONGenerator();
          boost::shared_array<char> jsonText = generator->write(unparsedMessage);
          ZS_LOG_BASIC(log("<<<<<<<<<<<<< MESSAGE FROM INNER FRAME (START) <<<<<<<<<<<<<"))
          ZS_LOG_TRACE(log("handling message from inner frame") + ", message=" + (CSTR)(jsonText.get()))
          ZS_LOG_BASIC(log("<<<<<<<<<<<<<  MESSAGE FROM INNER FRAME (END)  <<<<<<<<<<<<<"))
        }

        MessagePtr message = Message::create(unparsedMessage, mThisWeak.lock());
        if (IMessageMonitor::handleMessageReceived(message)) {
          ZS_LOG_DEBUG(log("message handled via message monitor"))
          return;
        }

        AutoRecursiveLock lock(getLock());

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("cannot handle message when shutdown"))
          return;
        }

        IdentityAccessWindowRequestPtr windowRequest = IdentityAccessWindowRequest::convert(message);
        if (windowRequest) {
          // send a result immediately
          IdentityAccessWindowResultPtr result = IdentityAccessWindowResult::create(windowRequest);
          sendInnerWindowMessage(result);

          if (windowRequest->ready()) {
            ZS_LOG_DEBUG(log("notified browser window ready"))
            get(mBrowserWindowReady) = true;
          }

          if (windowRequest->visible()) {
            ZS_LOG_DEBUG(log("notified browser window needs to be made visible"))
            get(mNeedsBrowserWindowVisible) = true;
          }

          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return;
        }

        IdentityAccessCompleteNotifyPtr completeNotify = IdentityAccessCompleteNotify::convert(message);
        if (completeNotify) {

          const IdentityInfo &identityInfo = completeNotify->identityInfo();
          const LockboxInfo &lockboxInfo = completeNotify->lockboxInfo();

          ZS_LOG_DEBUG(log("received complete notification") + identityInfo.getDebugValueString() + lockboxInfo.getDebugValueString())

          mIdentityInfo.mergeFrom(identityInfo, true);
          mLockboxInfo.mergeFrom(lockboxInfo, true);

          if ((mIdentityInfo.mAccessToken.isEmpty()) ||
              (mIdentityInfo.mAccessSecret.isEmpty()) ||
              (mIdentityInfo.mURI.isEmpty())) {
            ZS_LOG_ERROR(Detail, log("failed to obtain access token/secret"))
            setError(IHTTP::HTTPStatusCode_Forbidden, "Login via identity provider failed");
            cancel();
            return;
          }

          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();

          notifyLockboxStateChanged();
          return;
        }

        if ((message->isRequest()) ||
            (message->isNotify())) {
          ZS_LOG_WARNING(Debug, log("request was not understood"))
          MessageResultPtr result = MessageResult::create(message, IHTTP::HTTPStatusCode_NotImplemented);
          sendInnerWindowMessage(result);
          return;
        }

        ZS_LOG_WARNING(Detail, log("message result ignored since it was not monitored"))
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::startRolodexDownload(const char *inLastDownloadedVersion)
      {
        AutoRecursiveLock lock(getLock());

        ZS_LOG_DEBUG(log("allowing rolodex downloading to start"))

        mRolodexInfo.mVersion = String(inLastDownloadedVersion);

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::refreshRolodexContacts()
      {
        AutoRecursiveLock lock(getLock());

        ZS_LOG_DEBUG(log("forcing rolodex server to refresh its contact list"))

        mRolodexInfo.mUpdateNext = Time();  // reset when the next update is allowed to occur
        mForceRefresh = zsLib::now();
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::getDownloadedRolodexContacts(
                                                                bool &outFlushAllRolodexContacts,
                                                                String &outVersionDownloaded,
                                                                IdentityInfoListPtr &outRolodexContacts
                                                                )
      {
        AutoRecursiveLock lock(getLock());

        bool refreshed = (Time() != mFreshDownload);
        mFreshDownload = Time();

        if ((mIdentities.size() < 1) &&
            (!refreshed)) {
          ZS_LOG_DEBUG(log("no contacts downloaded"))
          return false;
        }

        ZS_LOG_DEBUG(log("returning downloaded contacts") + ", total=" + string(mIdentities.size()))

        outFlushAllRolodexContacts = refreshed;
        outVersionDownloaded = mRolodexInfo.mVersion;

        outRolodexContacts = IdentityInfoListPtr(new IdentityInfoList);

        (*outRolodexContacts) = mIdentities;
        mIdentities.clear();

        return true;
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::cancel()
      {
        AutoRecursiveLock lock(getLock());

        if (isShutdown()) {
          ZS_LOG_DEBUG(log("already shutdown"))
          return;
        }

        mGraciousShutdownReference.reset();

        mPendingMessagesToDeliver.clear();

        if (mIdentityAccessLockboxUpdateMonitor) {
          mIdentityAccessLockboxUpdateMonitor->cancel();
          mIdentityAccessLockboxUpdateMonitor.reset();
        }

        if (mIdentityLookupUpdateMonitor) {
          mIdentityLookupUpdateMonitor->cancel();
          mIdentityLookupUpdateMonitor.reset();
        }

        if (mIdentityAccessRolodexCredentialsGetMonitor) {
          mIdentityAccessRolodexCredentialsGetMonitor->cancel();
          mIdentityAccessRolodexCredentialsGetMonitor.reset();
        }

        if (mIdentityLookupMonitor) {
          mIdentityLookupMonitor->cancel();
          mIdentityLookupMonitor.reset();
        }

        if (mRolodexAccessMonitor) {
          mRolodexAccessMonitor->cancel();
          mRolodexAccessMonitor.reset();
        }
        if (mRolodexNamespaceGrantChallengeValidateMonitor) {
          mRolodexNamespaceGrantChallengeValidateMonitor->cancel();
          mRolodexNamespaceGrantChallengeValidateMonitor.reset();
        }
        if (mRolodexContactsGetMonitor) {
          mRolodexContactsGetMonitor->cancel();
          mRolodexContactsGetMonitor.reset();
        }

        if (mGrantQuery) {
          mGrantQuery->cancel();
          mGrantQuery.reset();
        }

        if (mGrantWait) {
          mGrantWait->cancel();
          mGrantWait.reset();
        }

        mTimer.reset();

        setState(SessionState_Shutdown);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IMessageSource
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IServiceIdentitySessionForServiceLockbox
      #pragma mark

      //-----------------------------------------------------------------------
      ServiceIdentitySessionPtr ServiceIdentitySession::reload(
                                                               BootstrappedNetworkPtr provider,
                                                               IServiceNamespaceGrantSessionPtr grantSession,
                                                               IServiceLockboxSessionPtr existingLockbox,
                                                               const char *identityURI,
                                                               const char *reloginKey
                                                               )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!existingLockbox)
        ZS_THROW_INVALID_ARGUMENT_IF(!provider)
        ZS_THROW_INVALID_ARGUMENT_IF(!existingLockbox)
        ZS_THROW_INVALID_ARGUMENT_IF(!identityURI)

        BootstrappedNetworkPtr identityNetwork;

        if (IServiceIdentity::isValid(identityURI)) {
          if (!IServiceIdentity::isLegacy(identityURI)) {
            String domain;
            String identifier;
            IServiceIdentity::splitURI(identityURI, domain, identifier);
            identityNetwork = IBootstrappedNetworkForServices::prepare(domain);
          }
        }

        ServiceIdentitySessionPtr pThis(new ServiceIdentitySession(
                                                                   IStackForInternal::queueStack(),
                                                                   IServiceIdentitySessionDelegatePtr(),
                                                                   provider,
                                                                   identityNetwork,
                                                                   ServiceNamespaceGrantSession::convert(grantSession),
                                                                   ServiceLockboxSession::convert(existingLockbox),
                                                                   NULL
                                                                   ));
        pThis->mThisWeak = pThis;
        pThis->mAssociatedLockbox = ServiceLockboxSession::convert(existingLockbox);
        pThis->mIdentityInfo.mURI = identityURI;
        pThis->mIdentityInfo.mReloginKey = String(reloginKey);
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::associate(ServiceLockboxSessionPtr lockbox)
      {
        AutoRecursiveLock lock(getLock());
        ZS_LOG_DEBUG(log("associate called"))
        mAssociatedLockbox = lockbox;
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::killAssociation(ServiceLockboxSessionPtr peerContact)
      {
        AutoRecursiveLock lock(getLock());

        ZS_LOG_DEBUG(log("kill associate called"))

        if (mKillAssociation) {
          ZS_LOG_WARNING(Detail, log("asssoication already killed"))
          return;
        }

        // this shutdown must be performed graciously so that there is time to clean out the associations
        mGraciousShutdownReference = mThisWeak.lock();

        mAssociatedLockbox.reset();
        get(mKillAssociation) = true;

        if (mIdentityAccessLockboxUpdateMonitor) {
          mIdentityAccessLockboxUpdateMonitor->cancel();
          mIdentityAccessLockboxUpdateMonitor.reset();
        }

        if (mIdentityLookupUpdateMonitor) {
          mIdentityLookupUpdateMonitor->cancel();
          mIdentityLookupUpdateMonitor.reset();
        }

        if (mIdentityLookupMonitor) {
          mIdentityLookupMonitor->cancel();
          mIdentityLookupMonitor.reset();
        }

        get(mLockboxUpdated) = false;
        get(mIdentityLookupUpdated) = false;

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::notifyStateChanged()
      {
        AutoRecursiveLock lock(getLock());
        ZS_LOG_DEBUG(log("notify state changed"))
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::isLoginComplete() const
      {
        AutoRecursiveLock lock(getLock());
        return mIdentityInfo.mAccessToken.hasData();
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::isShutdown() const
      {
        AutoRecursiveLock lock(getLock());
        return SessionState_Shutdown == mCurrentState;
      }

      //-----------------------------------------------------------------------
      IdentityInfo ServiceIdentitySession::getIdentityInfo() const
      {
        AutoRecursiveLock lock(getLock());
        return mIdentityInfo;
      }

      //-----------------------------------------------------------------------
      LockboxInfo ServiceIdentitySession::getLockboxInfo() const
      {
        AutoRecursiveLock lock(getLock());
        return mLockboxInfo;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IWakeDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::onWake()
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
      #pragma mark ServiceIdentitySession => IBootstrappedNetworkDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::onBootstrappedNetworkPreparationCompleted(IBootstrappedNetworkPtr bootstrappedNetwork)
      {
        ZS_LOG_DEBUG(log("bootstrapper reported complete"))

        AutoRecursiveLock lock(getLock());
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      void ServiceIdentitySession::onTimer(TimerPtr timer)
      {
        ZS_LOG_DEBUG(log("on timer fired"))
        AutoRecursiveLock lock(getLock());

        mTimer.reset();

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IServiceNamespaceGrantSessionForServicesWaitForWaitDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::onServiceNamespaceGrantSessionForServicesWaitComplete(IServiceNamespaceGrantSessionPtr session)
      {
        ZS_LOG_DEBUG(log("namespace grant waits have completed, can try again to obtain a wait (if waiting)"))

        AutoRecursiveLock lock(getLock());
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IServiceNamespaceGrantSessionForServicesQueryDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::onServiceNamespaceGrantSessionForServicesQueryComplete(
                                                                                          IServiceNamespaceGrantSessionForServicesQueryPtr query,
                                                                                          ElementPtr namespaceGrantChallengeBundleEl
                                                                                          )
      {
        ZS_LOG_DEBUG(log("namespace grant query completed"))

        AutoRecursiveLock lock(getLock());
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IMessageMonitorResultDelegate<IdentityAccessLockboxUpdateResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorResultReceived(
                                                                      IMessageMonitorPtr monitor,
                                                                      IdentityAccessLockboxUpdateResultPtr result
                                                                      )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mIdentityAccessLockboxUpdateMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        mIdentityAccessLockboxUpdateMonitor->cancel();
        mIdentityAccessLockboxUpdateMonitor.reset();

        get(mLockboxUpdated) = true;

        ZS_LOG_DEBUG(log("identity access lockbox update complete"))

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorErrorResultReceived(
                                                                           IMessageMonitorPtr monitor,
                                                                           IdentityAccessLockboxUpdateResultPtr ignore, // will always be NULL
                                                                           message::MessageResultPtr result
                                                                           )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mIdentityAccessLockboxUpdateMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        setError(result->errorCode(), result->errorReason());

        ZS_LOG_ERROR(Detail, log("identity access lockbox update failed"))

        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IMessageMonitorResultDelegate<IdentityLookupUpdateResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorResultReceived(
                                                                      IMessageMonitorPtr monitor,
                                                                      IdentityLookupUpdateResultPtr result
                                                                      )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mIdentityLookupUpdateMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        mIdentityLookupUpdateMonitor->cancel();
        mIdentityLookupUpdateMonitor.reset();

        get(mIdentityLookupUpdated) = true;

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorErrorResultReceived(
                                                                           IMessageMonitorPtr monitor,
                                                                           IdentityLookupUpdateResultPtr ignore, // will always be NULL
                                                                           message::MessageResultPtr result
                                                                           )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mIdentityLookupUpdateMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        setError(result->errorCode(), result->errorReason());

        ZS_LOG_DEBUG(log("identity login complete failed"))

        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IMessageMonitorResultDelegate<IdentityAccessRolodexCredentialsGetResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorResultReceived(
                                                                      IMessageMonitorPtr monitor,
                                                                      IdentityAccessRolodexCredentialsGetResultPtr result
                                                                      )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mIdentityAccessRolodexCredentialsGetMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        mIdentityAccessRolodexCredentialsGetMonitor->cancel();
        mIdentityAccessRolodexCredentialsGetMonitor.reset();

        mRolodexInfo = result->rolodexInfo();

        if (mRolodexInfo.mServerToken.isEmpty()) {
          setError(IHTTP::HTTPStatusCode_MethodFailure, "rolodex credentials gets did not return a server token");
          cancel();
          return true;
        }

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorErrorResultReceived(
                                                                           IMessageMonitorPtr monitor,
                                                                           IdentityAccessRolodexCredentialsGetResultPtr ignore, // will always be NULL
                                                                           message::MessageResultPtr result
                                                                           )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mIdentityAccessRolodexCredentialsGetMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }


        if (OPENPEER_STACK_SERVICE_IDENTITY_ROLODEX_NOT_SUPPORTED_FOR_IDENTITY == result->errorCode()) {
          ZS_LOG_WARNING(Detail, log("identity does not support rolodex even if identity provider supports"))
          get(mRolodexNotSupportedForIdentity) = true;

          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return true;
        }
        
        setError(result->errorCode(), result->errorReason());

        ZS_LOG_DEBUG(log("identity rolodex credentials failure"))

        cancel();
        return true;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IMessageMonitorResultDelegate<IdentityLookupResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorResultReceived(
                                                                      IMessageMonitorPtr monitor,
                                                                      IdentityLookupResultPtr result
                                                                      )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mIdentityLookupMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        // possible the identity lookup will not return any result, so we need to remember that it did in fact complete
        mPreviousLookupInfo.mURI = mIdentityInfo.mURI;

        const IdentityInfoList &infos = result->identities();
        if (infos.size() > 0) {

          const IdentityInfo &identityInfo = infos.front();

          bool validProof = false;

          if (identityInfo.mIdentityProofBundle) {
            // validate the identity proof bundle...
            String stableID;
            Time created;
            Time expires;
            validProof = IServiceIdentity::isValidIdentityProofBundle(
                                                                      identityInfo.mIdentityProofBundle,
                                                                      identityInfo.mPeerFilePublic,
                                                                      NULL, // outPeerURI
                                                                      NULL, // outIdentityURI
                                                                      &stableID,
                                                                      &created,
                                                                      &expires
                                                                      );

            Time tick = zsLib::now();
            if (tick < created) {
              tick = created; // for calculation safety
            }

            Duration consumed = (tick - created);
            Duration total = (expires - created);
            if (consumed > total) {
              consumed = total; // for calculation safety
            }

            Duration::sec_type percentageUsed = ((consumed.total_seconds() * 100) / total.total_seconds());
            if (percentageUsed > OPENPEER_STACK_SERVICE_IDENTITY_MAX_CONSUMED_TIME_PERCENTAGE_BEFORE_IDENTITY_PROOF_REFRESH) {
              ZS_LOG_WARNING(Detail, log("identity bundle proof too close to expiry, will recreate identity proof") + ", percentage used=" + string(percentageUsed) + ", consumed=" + string(consumed.total_seconds()) + ", total=" + string(total.total_seconds()))
              validProof = false;
            }

            if (stableID != identityInfo.mStableID) {
              ZS_LOG_WARNING(Detail, log("stabled ID from proof bundle does not match stable ID in identity") + ", proof stable ID=" + stableID + ", identity stable id=" + identityInfo.mStableID)
              validProof = false;
            }
          }

          mPreviousLookupInfo.mergeFrom(identityInfo, true);
          if (!validProof) {
            mPreviousLookupInfo.mIdentityProofBundle.reset(); // identity proof bundle isn't valid, later will be forced to recreated if it is missing
          }
        }

        ZS_LOG_DEBUG(log("identity lookup (of self) complete"))

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorErrorResultReceived(
                                                                           IMessageMonitorPtr monitor,
                                                                           IdentityLookupResultPtr ignore, // will always be NULL
                                                                           message::MessageResultPtr result
                                                                           )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mIdentityLookupMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        setError(result->errorCode(), result->errorReason());

        ZS_LOG_DEBUG(log("identity lookup failed"))

        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IMessageMonitorResultDelegate<RolodexAccessResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorResultReceived(
                                                                      IMessageMonitorPtr monitor,
                                                                      RolodexAccessResultPtr result
                                                                      )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mRolodexAccessMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        mRolodexAccessMonitor->cancel();
        mRolodexAccessMonitor.reset();

        mRolodexInfo.mergeFrom(result->rolodexInfo());

        if ((mRolodexInfo.mAccessToken.isEmpty()) ||
            (mRolodexInfo.mAccessSecret.isEmpty()))
        {
          setError(IHTTP::HTTPStatusCode_MethodFailure, "rolodex access did not return a proper access token/secret");
          cancel();
          return true;
        }

        NamespaceGrantChallengeInfo challengeInfo = result->namespaceGrantChallengeInfo();

        if (challengeInfo.mID.hasData()) {
          // a namespace grant challenge was issue
          NamespaceInfoMap namespaces;
          getNamespaces(namespaces);
          mGrantQuery = mGrantSession->forServices().query(mThisWeak.lock(), challengeInfo, namespaces);
        }

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorErrorResultReceived(
                                                                           IMessageMonitorPtr monitor,
                                                                           RolodexAccessResultPtr ignore, // will always be NULL
                                                                           message::MessageResultPtr result
                                                                           )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mRolodexAccessMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        setError(result->errorCode(), result->errorReason());

        ZS_LOG_DEBUG(log("rolodex access failure"))

        cancel();
        return true;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IMessageMonitorResultDelegate<RolodexNamespaceGrantChallengeValidateResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorResultReceived(
                                                                      IMessageMonitorPtr monitor,
                                                                      RolodexNamespaceGrantChallengeValidateResultPtr result
                                                                      )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mRolodexNamespaceGrantChallengeValidateMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        mRolodexNamespaceGrantChallengeValidateMonitor->cancel();
        mRolodexNamespaceGrantChallengeValidateMonitor.reset();

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorErrorResultReceived(
                                                                           IMessageMonitorPtr monitor,
                                                                           RolodexNamespaceGrantChallengeValidateResultPtr ignore, // will always be NULL
                                                                           message::MessageResultPtr result
                                                                           )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mRolodexNamespaceGrantChallengeValidateMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        setError(result->errorCode(), result->errorReason());

        ZS_LOG_DEBUG(log("rolodex namespace grant challenge failure"))

        cancel();
        return true;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IMessageMonitorResultDelegate<RolodexContactsGetResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorResultReceived(
                                                                      IMessageMonitorPtr monitor,
                                                                      RolodexContactsGetResultPtr result
                                                                      )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mRolodexContactsGetMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        mRolodexContactsGetMonitor->cancel();
        mRolodexContactsGetMonitor.reset();

        // reset the failure case
        mNextRetryAfterFailureTime = Seconds(OPENPEER_STACK_SERVICE_IDENTITY_ROLODEX_ERROR_RETRY_TIME_IN_SECONDS);
        get(mFailuresInARow) = 0;

        const IdentityInfoList &identities = result->identities();

        mRolodexInfo.mergeFrom(result->rolodexInfo());

        ZS_LOG_DEBUG(log("downloaded contacts") + ", total=" + string(identities.size()))

        for (IdentityInfoList::const_iterator iter = identities.begin(); iter != identities.end(); ++iter)
        {
          const IdentityInfo &identityInfo = (*iter);
          ZS_LOG_TRACE(log("downloaded contact") + identityInfo.getDebugValueString())
          mIdentities.push_back(identityInfo);
        }

        ZS_THROW_BAD_STATE_IF(!mDelegate)

        try {
          mDelegate->onServiceIdentitySessionRolodexContactsDownloaded(mThisWeak.lock());
        } catch(IServiceIdentitySessionDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorErrorResultReceived(
                                                                           IMessageMonitorPtr monitor,
                                                                           RolodexContactsGetResultPtr ignore, // will always be NULL
                                                                           message::MessageResultPtr result
                                                                           )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mRolodexContactsGetMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        ZS_LOG_WARNING(Detail, log("rolodex contacts get failure") + ", error code=" + string(result->errorCode()) + ", error reason=" + result->errorReason())
        ++mFailuresInARow;

        if ((mFailuresInARow < 2) &&
            (result->errorCode() == IHTTP::HTTPStatusCode_MethodFailure)) { // error 424
          ZS_LOG_DEBUG(log("performing complete rolodex refresh"))
          refreshRolodexContacts();
          step();
          return true;
        }

        // try again later
        mRolodexInfo.mUpdateNext = zsLib::now() + mNextRetryAfterFailureTime;

        // double the wait time for the next error
        mNextRetryAfterFailureTime = mNextRetryAfterFailureTime + mNextRetryAfterFailureTime;

        // cap the retry method at a maximum retrial value
        if (mNextRetryAfterFailureTime > Seconds(OPENPEER_STACK_SERVICE_IDENTITY_MAX_ROLODEX_ERROR_RETRY_TIME_IN_SECONDS)) {
          mNextRetryAfterFailureTime = Seconds(OPENPEER_STACK_SERVICE_IDENTITY_MAX_ROLODEX_ERROR_RETRY_TIME_IN_SECONDS);
        }

        step();
        return true;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      RecursiveLock &ServiceIdentitySession::getLock() const
      {
        return mLock;
      }

      //-----------------------------------------------------------------------
      String ServiceIdentitySession::log(const char *message) const
      {
        return String("ServiceIdentitySession [") + string(mID) + "] " + message;
      }

      //-----------------------------------------------------------------------
      String ServiceIdentitySession::getDebugValueString(bool includeCommaPrefix) const
      {
        AutoRecursiveLock lock(getLock());
        bool firstTime = !includeCommaPrefix;
        return
        Helper::getDebugValue("identity session id", string(mID), firstTime) +
        Helper::getDebugValue("delegate", mDelegate ? String("true") : String(), firstTime) +
        Helper::getDebugValue("state", toString(mCurrentState), firstTime) +
        Helper::getDebugValue("reported", toString(mLastReportedState), firstTime) +
        Helper::getDebugValue("error code", 0 != mLastError ? string(mLastError) : String(), firstTime) +
        Helper::getDebugValue("error reason", mLastErrorReason, firstTime) +
        Helper::getDebugValue("kill association", mKillAssociation ? String("true") : String(), firstTime) +
        (mIdentityInfo.hasData() ? mIdentityInfo.getDebugValueString() : String()) +
        IBootstrappedNetwork::toDebugString(mProviderBootstrappedNetwork) +
        IBootstrappedNetwork::toDebugString(mIdentityBootstrappedNetwork) +
        Helper::getDebugValue("active boostrapper", (mActiveBootstrappedNetwork ? (mIdentityBootstrappedNetwork == mActiveBootstrappedNetwork ? String("identity") : String("provider")) : String()), firstTime) +
        Helper::getDebugValue("grant session id", mGrantSession ? string(mGrantSession->forServices().getID()) : String(), firstTime) +
        Helper::getDebugValue("grant query id", mGrantQuery ? string(mGrantQuery->getID()) : String(), firstTime) +
        Helper::getDebugValue("grant wait id", mGrantWait ? string(mGrantWait->getID()) : String(), firstTime) +
        Helper::getDebugValue("identity access lockbox update monitor", mIdentityAccessLockboxUpdateMonitor ? String("true") : String(), firstTime) +
        Helper::getDebugValue("identity lookup update monitor", mIdentityLookupUpdateMonitor ? String("true") : String(), firstTime) +
        Helper::getDebugValue("identity access rolodex credentials get monitor", mIdentityAccessRolodexCredentialsGetMonitor ? String("true") : String(), firstTime) +
        Helper::getDebugValue("rolodex access monitor", mRolodexAccessMonitor ? String("true") : String(), firstTime) +
        Helper::getDebugValue("rolodex grant monitor", mRolodexNamespaceGrantChallengeValidateMonitor ? String("true") : String(), firstTime) +
        Helper::getDebugValue("rolodex contacts get monitor", mRolodexContactsGetMonitor ? String("true") : String(), firstTime) +
        (mLockboxInfo.hasData() ? mLockboxInfo.getDebugValueString() : String()) +
        Helper::getDebugValue("browser window ready", mBrowserWindowReady ? String("true") : String(), firstTime) +
        Helper::getDebugValue("browser window visible", mBrowserWindowVisible ? String("true") : String(), firstTime) +
        Helper::getDebugValue("browser closed", mBrowserWindowClosed ? String("true") : String(), firstTime) +
        Helper::getDebugValue("need browser window visible", mNeedsBrowserWindowVisible ? String("true") : String(), firstTime) +
        Helper::getDebugValue("identity access start notification sent", mIdentityAccessStartNotificationSent ? String("true") : String(), firstTime) +
        Helper::getDebugValue("lockbox updated", mLockboxUpdated ? String("true") : String(), firstTime) +
        Helper::getDebugValue("identity lookup updated", mIdentityLookupUpdated ? String("true") : String(), firstTime) +
        (mPreviousLookupInfo.hasData() ? mPreviousLookupInfo.getDebugValueString() : String()) +
        Helper::getDebugValue("outer frame url", mOuterFrameURLUponReload, firstTime) +
        Helper::getDebugValue("pending messages", mPendingMessagesToDeliver.size() > 0 ? string(mPendingMessagesToDeliver.size()) : String(), firstTime) +
        Helper::getDebugValue("rolodex not supported", mRolodexNotSupportedForIdentity ? String("true") : String(), firstTime) +
        (mRolodexInfo.hasData() ? mRolodexInfo.getDebugValueString() : String()) +
        Helper::getDebugValue("download timer", mTimer ? String("true") : String(), firstTime) +
        Helper::getDebugValue("force refresh", Time() != mForceRefresh ? String("true") : String(), firstTime) +
        Helper::getDebugValue("fresh download", Time() != mFreshDownload ? String("true") : String(), firstTime) +
        Helper::getDebugValue("pending identities", mIdentities.size() > 0 ? string(mIdentities.size()) : String(), firstTime) +
        Helper::getDebugValue("failures in a row", mFailuresInARow > 0 ? string(mFailuresInARow) : String(), firstTime) +
        Helper::getDebugValue("next retry (seconds)", mNextRetryAfterFailureTime.total_seconds() ? string(mNextRetryAfterFailureTime.total_seconds()) : String(), firstTime);
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::step()
      {
        if (isShutdown()) {
          ZS_LOG_DEBUG(log("step - already shutdown"))
          cancel();
          return;
        }

        ZS_LOG_DEBUG(log("step") + getDebugValueString())

        if (!mDelegate) {
          ZS_LOG_DEBUG(log("step waiting for delegate to become attached"))
          setState(SessionState_WaitingAttachmentOfDelegate);
          return;
        }

        if (!stepBootstrapper()) return;
        if (!stepGrantCheck()) return;
        if (!stepLoadBrowserWindow()) return;
        if (!stepIdentityAccessStartNotification()) return;
        if (!stepMakeBrowserWindowVisible()) return;
        if (!stepIdentityAccessCompleteNotification()) return;
        if (!stepRolodexCredentialsGet()) return;
        if (!stepRolodexAccess()) return;
        if (!stepLockboxAssociation()) return;
        if (!stepIdentityLookup()) return;
        if (!stepLockboxAccessToken()) return;
        if (!stepLockboxUpdate()) return;
        if (!stepCloseBrowserWindow()) return;
        if (!stepPreGrantChallenge()) return;
        if (!stepClearGrantWait()) return;
        if (!stepGrantChallenge()) return;
        if (!stepLockboxReady()) return;
        if (!stepLookupUpdate()) return;
        if (!stepDownloadContacts()) return;

        if (mKillAssociation) {
          ZS_LOG_DEBUG(log("association is now killed") + getDebugValueString())
          setError(IHTTP::HTTPStatusCode_Gone, "assocation is now killed");
          cancel();
          return;
        }

        // signal the object is ready
        setState(SessionState_Ready);

        ZS_LOG_TRACE(log("step complete") + getDebugValueString())
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepBootstrapper()
      {
        if (mActiveBootstrappedNetwork) {
          ZS_LOG_TRACE(log("already have an active bootstrapper"))
          return true;
        }

        if (mKillAssociation) {
          ZS_LOG_WARNING(Detail, log("association is killed"))
          setError(IHTTP::HTTPStatusCode_Gone, "association is killed");
          cancel();
          return false;
        }

        setState(SessionState_Pending);

        if (mIdentityBootstrappedNetwork) {
          if (!mIdentityBootstrappedNetwork->forServices().isPreparationComplete()) {
            ZS_LOG_TRACE(log("waiting for preparation of identity bootstrapper to complete"))
            return false;
          }

          WORD errorCode = 0;
          String reason;

          if (mIdentityBootstrappedNetwork->forServices().wasSuccessful(&errorCode, &reason)) {
            ZS_LOG_DEBUG(log("identity bootstrapper was successful thus using that as the active identity service"))
            mActiveBootstrappedNetwork = mIdentityBootstrappedNetwork;
            return true;
          }

          if (!mProviderBootstrappedNetwork) {
            ZS_LOG_ERROR(Detail, log("bootstrapped network failed for identity and there is no provider identity service specified") + ", error=" + string(errorCode) + ", reason=" + reason)

            setError(errorCode, reason);
            cancel();
            return false;
          }
        }

        if (!mProviderBootstrappedNetwork) {
          ZS_LOG_ERROR(Detail, log("provider domain not specified for identity thus identity lookup cannot complete"))
          setError(IHTTP::HTTPStatusCode_BadGateway);
          cancel();
          return false;
        }

        if (!mProviderBootstrappedNetwork->forServices().isPreparationComplete()) {
          ZS_LOG_TRACE(log("waiting for preparation of provider bootstrapper to complete"))
          return false;
        }

        WORD errorCode = 0;
        String reason;

        if (mProviderBootstrappedNetwork->forServices().wasSuccessful(&errorCode, &reason)) {
          ZS_LOG_DEBUG(log("provider bootstrapper was successful thus using that as the active identity service"))
          mActiveBootstrappedNetwork = mProviderBootstrappedNetwork;
          return true;
        }

        ZS_LOG_ERROR(Detail, log("bootstrapped network failed for provider") + ", error=" + string(errorCode) + ", reason=" + reason)

        setError(errorCode, reason);
        cancel();
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepGrantCheck()
      {
        if (mBrowserWindowClosed) {
          ZS_LOG_TRACE(log("already informed browser window closed thus no need to make sure grant wait lock is obtained"))
          return true;
        }

        if (mGrantWait) {
          ZS_LOG_TRACE(log("grant wait lock is already obtained"))
          return true;
        }

        mGrantWait = mGrantSession->forServices().obtainWaitToProceed(mThisWeak.lock());

        if (!mGrantWait) {
          ZS_LOG_TRACE(log("waiting to obtain grant wait lock"))
          return false;
        }

        ZS_LOG_TRACE(log("obtained grant wait"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepLoadBrowserWindow()
      {
        if (mBrowserWindowReady) {
          ZS_LOG_TRACE(log("browser window is ready"))
          return true;
        }

        if (mKillAssociation) {
          ZS_LOG_WARNING(Detail, log("association is killed"))
          setError(IHTTP::HTTPStatusCode_Gone, "association is killed");
          cancel();
          return false;
        }

        if (mIdentityInfo.mAccessToken.hasData()) {
          ZS_LOG_TRACE(log("already have access token, no need to load browser"))
          return true;
        }

        String url = getInnerBrowserWindowFrameURL();
        if (!url) {
          ZS_LOG_ERROR(Detail, log("bootstrapper did not return a valid inner window frame URL"))
          setError(IHTTP::HTTPStatusCode_NotFound);
          cancel();
          return false;
        }

        setState(SessionState_WaitingForBrowserWindowToBeLoaded);

        ZS_LOG_TRACE(log("waiting for browser window to report it is loaded/ready"))
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepIdentityAccessStartNotification()
      {
        if (mIdentityAccessStartNotificationSent) {
          ZS_LOG_TRACE(log("identity access start notification already sent"))
          return true;
        }

        if (mKillAssociation) {
          ZS_LOG_WARNING(Detail, log("association is killed"))
          setError(IHTTP::HTTPStatusCode_Gone, "association is killed");
          cancel();
          return false;
        }

        if (mIdentityInfo.mAccessToken.hasData()) {
          ZS_LOG_TRACE(log("already have access token, no need to load browser"))
          return true;
        }

        ZS_LOG_DEBUG(log("identity access start notification being sent"))

        setState(SessionState_Pending);

        // make sure the provider domain is set to the active bootstrapper for the identity
        mIdentityInfo.mProvider = mActiveBootstrappedNetwork->forServices().getDomain();

        IdentityAccessStartNotifyPtr request = IdentityAccessStartNotify::create();
        request->domain(mActiveBootstrappedNetwork->forServices().getDomain());
        request->identityInfo(mIdentityInfo);

        request->browserVisibility(IdentityAccessStartNotify::BrowserVisibility_VisibleOnDemand);
        request->popup(false);

        request->outerFrameURL(mOuterFrameURLUponReload);

        sendInnerWindowMessage(request);

        get(mIdentityAccessStartNotificationSent) = true;
        return true;
      }
      
      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepMakeBrowserWindowVisible()
      {
        if (mBrowserWindowVisible) {
          ZS_LOG_TRACE(log("browser window is visible"))
          return true;
        }

        if (!mNeedsBrowserWindowVisible) {
          ZS_LOG_TRACE(log("browser window was not requested to become visible"))
          return false;
        }

        if (mKillAssociation) {
          ZS_LOG_WARNING(Detail, log("association is killed"))
          setError(IHTTP::HTTPStatusCode_Gone, "association is killed");
          cancel();
          return false;
        }

        ZS_LOG_TRACE(log("waiting for browser window to become visible"))
        setState(SessionState_WaitingForBrowserWindowToBeMadeVisible);
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepIdentityAccessCompleteNotification()
      {
        if (mIdentityInfo.mAccessToken.hasData()) {
          ZS_LOG_TRACE(log("idenity access complete notification received"))
          return true;
        }

        if (mKillAssociation) {
          ZS_LOG_WARNING(Detail, log("association is killed"))
          setError(IHTTP::HTTPStatusCode_Gone, "association is killed");
          cancel();
          return false;
        }

        setState(SessionState_Pending);

        ZS_LOG_TRACE(log("waiting for identity access complete notification"))
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepRolodexCredentialsGet()
      {
        if (mRolodexNotSupportedForIdentity) {
          ZS_LOG_TRACE(log("rolodex not supported for this identity"))
          return true;
        }

        if (mRolodexInfo.mServerToken.hasData()) {
          ZS_LOG_TRACE(log("already have rolodex server token credentials"))
          return true;
        }

        if (mIdentityAccessRolodexCredentialsGetMonitor) {
          ZS_LOG_TRACE(log("rolodex credentials get pending"))
          return false;
        }

        if (!mActiveBootstrappedNetwork->forServices().supportsRolodex()) {
          ZS_LOG_WARNING(Detail, log("rolodex service not supported on this domain") + ", domain=" + mActiveBootstrappedNetwork->forServices().getDomain())
          get(mRolodexNotSupportedForIdentity) = true;
          return true;
        }

        IdentityAccessRolodexCredentialsGetRequestPtr request = IdentityAccessRolodexCredentialsGetRequest::create();
        request->domain(mActiveBootstrappedNetwork->forServices().getDomain());
        request->identityInfo(mIdentityInfo);

        ZS_LOG_DEBUG(log("fetching rolodex credentials"))

        mIdentityAccessRolodexCredentialsGetMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<IdentityAccessRolodexCredentialsGetResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_SERVICE_IDENTITY_TIMEOUT_IN_SECONDS));
        sendInnerWindowMessage(request);

        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepRolodexAccess()
      {
        if (mRolodexNotSupportedForIdentity) {
          ZS_LOG_TRACE(log("rolodex not supported for this identity"))
          return true;
        }

        if (mRolodexInfo.mAccessToken.hasData()) {
          ZS_LOG_TRACE(log("rolodex access token obtained"))
          return true;
        }

        if (mRolodexAccessMonitor) {
          ZS_LOG_TRACE(log("rolodex access still pending (continuing to next step so other steps can run in parallel)"))
          return true;
        }

        ZS_THROW_BAD_STATE_IF(!mActiveBootstrappedNetwork->forServices().supportsRolodex())

        RolodexAccessRequestPtr request = RolodexAccessRequest::create();
        request->domain(mActiveBootstrappedNetwork->forServices().getDomain());
        request->rolodexInfo(mRolodexInfo);
        request->identityInfo(mIdentityInfo);
        request->grantID(mGrantSession->forServices().getGrantID());

        ZS_LOG_DEBUG(log("accessing rolodex (continuing to next step so other steps can run in parallel)"))

        mRolodexAccessMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<RolodexAccessResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_SERVICE_IDENTITY_TIMEOUT_IN_SECONDS));

        mActiveBootstrappedNetwork->forServices().sendServiceMessage("rolodex", "rolodex-access", request);
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepLockboxAssociation()
      {
        if (mAssociatedLockbox.lock()) {
          ZS_LOG_TRACE(log("lockbox associated"))
          return true;
        }

        if (mKillAssociation) {
          ZS_LOG_TRACE(log("do not need an association to the lockbox if association is being killed"))
          return true;
        }

        setState(SessionState_WaitingForAssociationToLockbox);
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepIdentityLookup()
      {
        if (mKillAssociation) {
          ZS_LOG_TRACE(log("do not need to perform identity lookup when lockbox association is being killed"))
          return true;
        }

        if (mPreviousLookupInfo.mURI.hasData()) {
          ZS_LOG_TRACE(log("identity lookup has already completed"))
          return true;
        }

        if (mIdentityLookupMonitor) {
          ZS_LOG_TRACE(log("identity lookup already in progress (but not going to wait for it to complete to continue"))
          return true;
        }

        ZS_LOG_DEBUG(log("performing identity lookup"))

        IdentityLookupRequestPtr request = IdentityLookupRequest::create();
        request->domain(mActiveBootstrappedNetwork->forServices().getDomain());

        IdentityLookupRequest::Provider provider;

        String domain;
        String id;

        IServiceIdentity::splitURI(mIdentityInfo.mURI, domain, id);

        provider.mBase = IServiceIdentity::joinURI(domain, NULL);
        char safeChar[2];
        safeChar[0] = getSafeSplitChar(id);;
        safeChar[1] = 0;

        provider.mSeparator = String(&(safeChar[0]));
        provider.mIdentities = id;

        IdentityLookupRequest::ProviderList providers;
        providers.push_back(provider);

        request->providers(providers);

        mIdentityLookupMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<IdentityLookupResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_SERVICE_IDENTITY_TIMEOUT_IN_SECONDS));
        mActiveBootstrappedNetwork->forServices().sendServiceMessage("identity-lookup", "identity-lookup", request);

        setState(SessionState_Pending);
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepLockboxAccessToken()
      {
        if (mKillAssociation) {
          ZS_LOG_TRACE(log("do not need lockbox to be ready if association is being killed"))
          return true;
        }
        
        ServiceLockboxSessionPtr lockbox = mAssociatedLockbox.lock();
        if (!lockbox) {
          return stepLockboxAssociation();
        }

        WORD errorCode = 0;
        String reason;
        IServiceLockboxSession::SessionStates state = lockbox->forServiceIdentity().getState(&errorCode, &reason);

        switch (state) {
          case IServiceLockboxSession::SessionState_Pending:
          case IServiceLockboxSession::SessionState_PendingPeerFilesGeneration: {

            LockboxInfo lockboxInfo = lockbox->forServiceIdentity().getLockboxInfo();
            if (lockboxInfo.mAccessToken.hasData()) {
              ZS_LOG_TRACE(log("lockbox is still pending but safe to proceed because lockbox has been granted access"))
              return true;
            }

            ZS_LOG_TRACE(log("waiting for lockbox to ready"))
            return false;
          }
          case IServiceLockboxSession::SessionState_Ready: {
            ZS_LOG_TRACE(log("lockbox is ready"))
            return true;
          }
          case IServiceLockboxSession::SessionState_Shutdown: {
            ZS_LOG_ERROR(Detail, log("lockbox shutdown") + ", error=" + string(errorCode) + ", reason=" + reason)

            setError(errorCode, reason);
            cancel();
            return false;
          }
        }

        ZS_LOG_ERROR(Detail, log("unknown lockbox state") + getDebugValueString())

        ZS_THROW_BAD_STATE("unknown lockbox state")
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepLockboxUpdate()
      {
        if (mLockboxUpdated) {
          ZS_LOG_TRACE(log("lockbox update already complete"))
          return true;
        }

        if (mIdentityAccessLockboxUpdateMonitor) {
          ZS_LOG_TRACE(log("lockbox update already in progress"))
          return false;
        }

        if (!mBrowserWindowReady) {
#define WARNING_HOW_TO_UPDATE_LOCKBOX_INFO 1
#define WARNING_HOW_TO_UPDATE_LOCKBOX_INFO 2
          ZS_LOG_TRACE(log("never loaded browser window so no need to perform lockbox update"))
          return true;
        }

        if (mKillAssociation) {
          setState(SessionState_Pending);

          IdentityAccessLockboxUpdateRequestPtr request = IdentityAccessLockboxUpdateRequest::create();
          request->domain(mActiveBootstrappedNetwork->forServices().getDomain());
          request->identityInfo(mIdentityInfo);

          ZS_LOG_DEBUG(log("clearing lockbox information (but not preventing other requests from continuing)"))

          mIdentityAccessLockboxUpdateMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<IdentityAccessLockboxUpdateResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_SERVICE_IDENTITY_TIMEOUT_IN_SECONDS));
          sendInnerWindowMessage(request);

          return false;
        }

        ServiceLockboxSessionPtr lockbox = mAssociatedLockbox.lock();
        if (!lockbox) {
          return stepLockboxAssociation();
        }

        setState(SessionState_Pending);

        LockboxInfo lockboxInfo = lockbox->forServiceIdentity().getLockboxInfo();

        bool equalKeys = (((bool)lockboxInfo.mKey) == ((bool)mLockboxInfo.mKey));
        bool hasKeys = (((bool)lockboxInfo.mKey) && ((bool)mLockboxInfo.mKey));
        if (hasKeys) {
          equalKeys = (0 == IHelper::compare(*lockboxInfo.mKey, *mLockboxInfo.mKey));
        }

        if ((lockboxInfo.mDomain == mLockboxInfo.mDomain) &&
            (equalKeys)) {
          ZS_LOG_TRACE(log("lockbox info already updated correctly"))
          get(mLockboxUpdated) = true;
          return true;
        }

        mLockboxInfo.mergeFrom(lockboxInfo, true);

        IdentityAccessLockboxUpdateRequestPtr request = IdentityAccessLockboxUpdateRequest::create();
        request->domain(mActiveBootstrappedNetwork->forServices().getDomain());
        request->identityInfo(mIdentityInfo);
        request->lockboxInfo(lockboxInfo);

        ZS_LOG_DEBUG(log("updating lockbox information"))

        mIdentityAccessLockboxUpdateMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<IdentityAccessLockboxUpdateResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_SERVICE_IDENTITY_TIMEOUT_IN_SECONDS));
        sendInnerWindowMessage(request);

        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepCloseBrowserWindow()
      {
        if (!mBrowserWindowReady) {
          ZS_LOG_TRACE(log("browser window was never made visible"))
          return true;
        }

        if (mBrowserWindowClosed) {
          ZS_LOG_TRACE(log("browser window is closed"))
          return true;
        }

        ZS_LOG_DEBUG(log("waiting for browser window to close"))

        setState(SessionState_WaitingForBrowserWindowToClose);

        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepPreGrantChallenge()
      {
        if (mRolodexNotSupportedForIdentity) {
          ZS_LOG_TRACE(log("rolodex service is not supported"))
          return true;
        }

        // before continuing, make sure rolodex access is completed
        if (mRolodexInfo.mAccessToken.isEmpty()) {
          ZS_LOG_TRACE(log("rolodex access is still pending"))
          return false;
        }

        ZS_LOG_TRACE(log("rolodex access has already completed"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepClearGrantWait()
      {
        if (!mGrantWait) {
          ZS_LOG_TRACE(log("wait already cleared"))
          return true;
        }

        ZS_LOG_DEBUG(log("clearing grant wait"))

        mGrantWait->cancel();
        mGrantWait.reset();
        return true;
      }
      
      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepGrantChallenge()
      {
        if (mRolodexNotSupportedForIdentity) {
          ZS_LOG_TRACE(log("rolodex service is not supported"))
          return true;
        }

        if (mRolodexNamespaceGrantChallengeValidateMonitor) {
          ZS_LOG_TRACE(log("waiting for rolodex namespace grant challenge validate monitor to complete"))
          return false;
        }

        if (!mGrantQuery) {
          ZS_LOG_TRACE(log("no grant challenge query thus continuing..."))
          return true;
        }

        if (!mGrantQuery->isComplete()) {
          ZS_LOG_TRACE(log("waiting for the grant query to complete"))
          return false;
        }

        ElementPtr bundleEl = mGrantQuery->getNamespaceGrantChallengeBundle();
        if (!bundleEl) {
          ZS_LOG_ERROR(Detail, log("namespaces were no granted in challenge"))
          setError(IHTTP::HTTPStatusCode_Forbidden, "namespaces were not granted to access rolodex");
          cancel();
          return false;
        }

        NamespaceInfoMap namespaces;
        getNamespaces(namespaces);

        for (NamespaceInfoMap::iterator iter = namespaces.begin(); iter != namespaces.end(); ++iter)
        {
          NamespaceInfo &namespaceInfo = (*iter).second;

          if (!mGrantSession->forServices().isNamespaceURLInNamespaceGrantChallengeBundle(bundleEl, namespaceInfo.mURL)) {
            ZS_LOG_WARNING(Detail, log("rolodex was not granted required namespace") + ", namespace" + namespaceInfo.mURL)
            setError(IHTTP::HTTPStatusCode_Forbidden, "namespaces were not granted to access rolodex");
            cancel();
            return false;
          }
        }

        mGrantQuery->cancel();
        mGrantQuery.reset();

        ZS_LOG_DEBUG(log("all namespaces required were correctly granted, notify the rolodex of the newly created access"))

        RolodexNamespaceGrantChallengeValidateRequestPtr request = RolodexNamespaceGrantChallengeValidateRequest::create();
        request->domain(mActiveBootstrappedNetwork->forServices().getDomain());

        request->rolodexInfo(mRolodexInfo);
        request->namespaceGrantChallengeBundle(bundleEl);

        mRolodexNamespaceGrantChallengeValidateMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<RolodexNamespaceGrantChallengeValidateResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_SERVICE_IDENTITY_TIMEOUT_IN_SECONDS));
        mActiveBootstrappedNetwork->forServices().sendServiceMessage("rolodex", "rolodex-namespace-grant-challenge-validate", request);
        
        return true;
      }
      
      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepLockboxReady()
      {
        if (mKillAssociation) {
          ZS_LOG_TRACE(log("do not need lockbox to be ready if association is being killed"))
          return true;
        }

        ServiceLockboxSessionPtr lockbox = mAssociatedLockbox.lock();
        if (!lockbox) {
          return stepLockboxAssociation();
        }

        WORD errorCode = 0;
        String reason;
        IServiceLockboxSession::SessionStates state = lockbox->forServiceIdentity().getState(&errorCode, &reason);

        switch (state) {
          case IServiceLockboxSession::SessionState_Pending:
          case IServiceLockboxSession::SessionState_PendingPeerFilesGeneration: {

            ZS_LOG_TRACE(log("must wait for lockbox to be ready (pending with access token is not enough)"))
            return false;
          }
          case IServiceLockboxSession::SessionState_Ready: {
            ZS_LOG_TRACE(log("lockbox is fully ready"))
            return true;
          }
          case IServiceLockboxSession::SessionState_Shutdown: {
            ZS_LOG_ERROR(Detail, log("lockbox shutdown") + ", error=" + string(errorCode) + ", reason=" + reason)

            setError(errorCode, reason);
            cancel();
            return false;
          }
        }

        ZS_LOG_ERROR(Detail, log("unknown lockbox state") + getDebugValueString())

        ZS_THROW_BAD_STATE("unknown lockbox state")
        return false;
      }
      
      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepLookupUpdate()
      {
        if (mIdentityLookupUpdated) {
          ZS_LOG_TRACE(log("lookup already updated"))
          return true;
        }

        if (mIdentityLookupUpdateMonitor) {
          ZS_LOG_TRACE(log("lookup update already in progress (but does not prevent other events from completing)"))
          return false;
        }

        if (mKillAssociation) {
          ZS_LOG_DEBUG(log("clearing identity lookup information (but not preventing other requests from continuing)"))

          mIdentityInfo.mStableID.clear();
          mIdentityInfo.mPeerFilePublic.reset();
          mIdentityInfo.mPriority = 0;
          mIdentityInfo.mWeight = 0;

          ServiceLockboxSessionPtr lockbox = mAssociatedLockbox.lock();

          if (lockbox) {
            LockboxInfo lockboxInfo = lockbox->forServiceIdentity().getLockboxInfo();
            mLockboxInfo.mergeFrom(lockboxInfo, true);
          }

          IdentityLookupUpdateRequestPtr request = IdentityLookupUpdateRequest::create();
          request->domain(mActiveBootstrappedNetwork->forServices().getDomain());
          request->lockboxInfo(mLockboxInfo);
          request->identityInfo(mIdentityInfo);

          mIdentityLookupUpdateMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<IdentityLookupUpdateResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_SERVICE_IDENTITY_TIMEOUT_IN_SECONDS));
          mActiveBootstrappedNetwork->forServices().sendServiceMessage("identity", "identity-lookup-update", request);

          return false;
        }

        if (mPreviousLookupInfo.mURI.isEmpty()) {
          ZS_LOG_TRACE(log("waiting for identity lookup to complete"))
          return false;
        }

        ServiceLockboxSessionPtr lockbox = mAssociatedLockbox.lock();
        if (!lockbox) {
          return stepLockboxAssociation();
        }

        setState(SessionState_Pending);

        IPeerFilesPtr peerFiles;
        IdentityInfo identityInfo = lockbox->forServiceIdentity().getIdentityInfoForIdentity(mThisWeak.lock(), &peerFiles);
        mIdentityInfo.mergeFrom(identityInfo, true);

        if ((identityInfo.mStableID == mPreviousLookupInfo.mStableID) &&
            (isSame(identityInfo.mPeerFilePublic, mPreviousLookupInfo.mPeerFilePublic)) &&
            (identityInfo.mPriority == mPreviousLookupInfo.mPriority) &&
            (identityInfo.mWeight == mPreviousLookupInfo.mWeight) &&
            (mPreviousLookupInfo.mIdentityProofBundle)) {
          ZS_LOG_DEBUG(log("identity information already up-to-date"))
          get(mIdentityLookupUpdated) = true;
          return true;
        }

        ZS_LOG_DEBUG(log("updating identity lookup information (but not preventing other requests from continuing)") + ", lockbox: " + mLockboxInfo.getDebugValueString(false) + ", identity info: " + mIdentityInfo.getDebugValueString(false))

        LockboxInfo lockboxInfo = lockbox->forServiceIdentity().getLockboxInfo();
        mLockboxInfo.mergeFrom(lockboxInfo, true);

        IdentityLookupUpdateRequestPtr request = IdentityLookupUpdateRequest::create();
        request->domain(mActiveBootstrappedNetwork->forServices().getDomain());
        request->peerFiles(peerFiles);
        request->lockboxInfo(mLockboxInfo);
        request->identityInfo(mIdentityInfo);

        mIdentityLookupUpdateMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<IdentityLookupUpdateResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_SERVICE_IDENTITY_TIMEOUT_IN_SECONDS));
        mActiveBootstrappedNetwork->forServices().sendServiceMessage("identity", "identity-lookup-update", request);

        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepDownloadContacts()
      {
        if (mFrozenVersion == mRolodexInfo.mVersion) {
          ZS_LOG_TRACE(log("rolodex download has not been initiated yet"))
          return true;
        }

        if (mRolodexNotSupportedForIdentity) {
          ZS_LOG_TRACE(log("rolodex not supported for this identity"))

          mRolodexInfo.mVersion = mFrozenVersion;

          try {
            mDelegate->onServiceIdentitySessionRolodexContactsDownloaded(mThisWeak.lock());
          } catch(IServiceIdentitySessionDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate gone"))
          }

          return true;
        }

        if (mRolodexContactsGetMonitor) {
          ZS_LOG_TRACE(log("rolodex contact download already active"))
          return true;
        }

        bool forceRefresh = (Time() != mForceRefresh);
        mForceRefresh = Time();

        if (forceRefresh) {
          ZS_LOG_DEBUG(log("will force refresh of contact list immediately"))
          mRolodexInfo.mUpdateNext = Time();  // download immediately
          mFreshDownload = zsLib::now();
          mIdentities.clear();
          mRolodexInfo.mVersion.clear();
        }

        Time tick = zsLib::now();
        if ((Time() != mRolodexInfo.mUpdateNext) &&
            (tick < mRolodexInfo.mUpdateNext)) {
          // not ready to issue the request yet, must wait, calculate how long to wait
          Duration waitTime = mRolodexInfo.mUpdateNext - tick;
          if (waitTime < Seconds(1)) {
            waitTime = Seconds(1);
          }
          mTimer = Timer::create(mThisWeak.lock(), waitTime, false);

          ZS_LOG_TRACE(log("delaying downloading contacts") + ", wait time (seconds)=" + string(waitTime.total_seconds()))
          return true;
        }

        ZS_LOG_DEBUG(log("attempting to download contacts from rolodex"))

        RolodexContactsGetRequestPtr request = RolodexContactsGetRequest::create();
        request->domain(mActiveBootstrappedNetwork->forServices().getDomain());

        RolodexInfo rolodexInfo(mRolodexInfo);
        rolodexInfo.mRefreshFlag = forceRefresh;

        request->rolodexInfo(mRolodexInfo);

        mRolodexContactsGetMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<RolodexContactsGetResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_SERVICE_IDENTITY_TIMEOUT_IN_SECONDS));
        mActiveBootstrappedNetwork->forServices().sendServiceMessage("rolodex", "rolodex-contacts-get", request);

        return true;
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::setState(SessionStates state)
      {
        if (state == mCurrentState) return;

        ZS_LOG_DEBUG(log("state changed") + ", state=" + toString(state) + ", old state=" + toString(mCurrentState))
        mCurrentState = state;

        notifyLockboxStateChanged();

        if (mLastReportedState != mCurrentState) {
          ServiceIdentitySessionPtr pThis = mThisWeak.lock();
          if ((pThis) &&
              (mDelegate)) {
            try {
              ZS_LOG_DEBUG(log("attempting to report state to delegate") + getDebugValueString())
              mDelegate->onServiceIdentitySessionStateChanged(pThis, mCurrentState);
              mLastReportedState = mCurrentState;
            } catch (IServiceIdentitySessionDelegateProxy::Exceptions::DelegateGone &) {
              ZS_LOG_WARNING(Detail, log("delegate gone"))
            }
          }
        }
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::setError(WORD errorCode, const char *inReason)
      {
        String reason(inReason ? String(inReason) : String());
        if (reason.isEmpty()) {
          reason = IHTTP::toString(IHTTP::toStatusCode(errorCode));
        }

        if (0 != mLastError) {
          ZS_LOG_WARNING(Detail, log("error already set thus ignoring new error") + ", new error=" + string(errorCode) + ", new reason=" + reason + getDebugValueString())
          return;
        }

        get(mLastError) = errorCode;
        mLastErrorReason = reason;

        ZS_LOG_WARNING(Detail, log("error set") + ", code=" + string(mLastError) + ", reason=" + mLastErrorReason + getDebugValueString())
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::notifyLockboxStateChanged()
      {
        ServiceLockboxSessionPtr lockbox = mAssociatedLockbox.lock();
        if (!lockbox) return;

        ZS_LOG_DEBUG(log("notifying lockbox of state change"))
        lockbox->forServiceIdentity().notifyStateChanged();
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::sendInnerWindowMessage(MessagePtr message)
      {
        DocumentPtr doc = message->encode();
        mPendingMessagesToDeliver.push_back(doc);

        if (1 != mPendingMessagesToDeliver.size()) {
          ZS_LOG_DEBUG(log("already had previous messages to deliver, no need to send another notification"))
          return;
        }

        ServiceIdentitySessionPtr pThis = mThisWeak.lock();

        if ((pThis) &&
            (mDelegate)) {
          try {
            ZS_LOG_DEBUG(log("attempting to notify of message to browser window needing to be delivered"))
            mDelegate->onServiceIdentitySessionPendingMessageForInnerBrowserWindowFrame(pThis);
          } catch(IServiceIdentitySessionDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate gone"))
          }
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IdentityProofBundleQuery
      #pragma mark

      class IdentityProofBundleQuery;
      typedef boost::shared_ptr<IdentityProofBundleQuery> IdentityProofBundleQueryPtr;
      typedef boost::weak_ptr<IdentityProofBundleQuery> IdentityProofBundleQueryWeakPtr;

      class IdentityProofBundleQuery : public MessageQueueAssociator,
                                       public IServiceIdentityProofBundleQuery,
                                       public IBootstrappedNetworkDelegate
      {
      protected:
        //---------------------------------------------------------------------
        IdentityProofBundleQuery(
                                 IMessageQueuePtr queue,
                                 ElementPtr identityProofBundleEl,
                                 IServiceIdentityProofBundleQueryDelegatePtr delegate,
                                 String identityURI
                                 ) :
          MessageQueueAssociator(queue),
          mID(zsLib::createPUID()),
          mIdentityProofBundleEl(identityProofBundleEl->clone()->toElement()),
          mDelegate(IServiceIdentityProofBundleQueryDelegateProxy::createWeak(IStackForInternal::queueDelegate(), delegate)),
          mIdentityURI(identityURI),
          mErrorCode(0)
        {
        }

        //---------------------------------------------------------------------
        void init()
        {
          if (0 != mErrorReason) {
            notifyComplete();
          }

          IServiceIdentityPtr serviceIdentity = IServiceIdentity::createServiceIdentityFromIdentityProofBundle(mIdentityProofBundleEl);

          mBootstrappedNetwork = serviceIdentity->getBootstrappedNetwork();

          if (mBootstrappedNetwork->isPreparationComplete()) {
            ZS_LOG_DEBUG(log("bootstrapped network is already prepared, short-circuit answer now..."))
            onBootstrappedNetworkPreparationCompleted(mBootstrappedNetwork);
            return;
          }

          ZS_LOG_DEBUG(log("bootstrapped network is not ready yet, check for validity when ready"))
          IBootstrappedNetwork::prepare(mBootstrappedNetwork->getDomain(), mThisWeak.lock());
        }

      public:
        //---------------------------------------------------------------------
        ~IdentityProofBundleQuery()
        {
          mThisWeak.reset();
        }

        //---------------------------------------------------------------------
        static IdentityProofBundleQueryPtr create(
                                                  ElementPtr identityProofBundleEl,
                                                  IServiceIdentityProofBundleQueryDelegatePtr delegate,
                                                  String identityURI,
                                                  WORD failedErrorCode,
                                                  const String &failedReason
                                                  )
        {
          ZS_THROW_INVALID_ARGUMENT_IF(!identityProofBundleEl)
          ZS_THROW_INVALID_ARGUMENT_IF(!delegate)

          IdentityProofBundleQueryPtr pThis = IdentityProofBundleQueryPtr(new IdentityProofBundleQuery(IStackForInternal::queueStack(), identityProofBundleEl, delegate, identityURI));
          pThis->mThisWeak = pThis;
          pThis->mErrorCode = failedErrorCode;
          pThis->mErrorReason = failedReason;
          pThis->init();
          return pThis;
        }

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IdentityProofBundleQuery => IServiceIdentityProofBundleQuery
        #pragma mark

        //---------------------------------------------------------------------
        virtual bool isComplete() const
        {
          AutoRecursiveLock lock(mLock);
          if (0 != mErrorReason) return true;
          return !mDelegate;
        }

        //---------------------------------------------------------------------
        virtual bool wasSuccessful(
                                   WORD *outErrorCode = NULL,
                                   String *outErrorReason = NULL
                                   ) const
        {
          AutoRecursiveLock lock(mLock);
          if (outErrorCode) {
            *outErrorCode = mErrorCode;
          }
          if (outErrorReason) {
            *outErrorReason = mErrorReason;
          }
          if (0 != mErrorReason) {
            return false;
          }
          return !mDelegate;
        }

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IdentityProofBundleQuery => IBootstrappedNetworkDelegate
        #pragma mark

        //---------------------------------------------------------------------
        virtual void onBootstrappedNetworkPreparationCompleted(IBootstrappedNetworkPtr bootstrappedNetwork)
        {
          AutoRecursiveLock lock(mLock);
          if (isComplete()) return;

          WORD errorCode = 0;
          String reason;
          if (!bootstrappedNetwork->wasSuccessful(&errorCode, &reason)) {
            if (0 == errorCode) {
              errorCode = IHTTP::HTTPStatusCode_NoResponse;
            }
            setError(errorCode, reason);
          }

          IServiceCertificatesPtr serviceCertificates = IServiceCertificates::createServiceCertificatesFrom(mBootstrappedNetwork);
          ZS_THROW_BAD_STATE_IF(!serviceCertificates)

          ElementPtr identityProofEl = mIdentityProofBundleEl->findFirstChildElement("identityProof");
          ZS_THROW_BAD_STATE_IF(!identityProofEl)

          if (!serviceCertificates->isValidSignature(mIdentityProofBundleEl)) {
            setError(IHTTP::HTTPStatusCode_CertError, (String("identity failed to validate") + ", identity uri=" + mIdentityURI).c_str());
          }

          notifyComplete();
        }

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IdentityProofBundleQuery => (internal)
        #pragma mark

        //---------------------------------------------------------------------
        String log(const char *message) const
        {
          return String("IdentityProofBundleQuery [") + string(mID) + "] " + message;
        }

        //---------------------------------------------------------------------
        void setError(WORD errorCode, const char *reason = NULL)
        {
          if (!reason) {
            reason = IHTTP::toString(IHTTP::toStatusCode(errorCode));
          }

          if (0 != mErrorCode) {
            ZS_LOG_WARNING(Debug, log("attempting to set an error when error already set") + ", new error code=" + string(mErrorCode) + ", new reason=" + reason + ", existing error code=" + string(mErrorCode) + ", existing reason=" + mErrorReason)
            return;
          }

          mErrorCode = errorCode;
          mErrorReason = reason;

          ZS_LOG_WARNING(Debug, log("setting error code") + ", identity uri=" + mIdentityURI + ", error code=" + string(mErrorCode) + ", reason=" + reason)
        }

        //---------------------------------------------------------------------
        void notifyComplete()
        {
          if (!mDelegate) return;

          IdentityProofBundleQueryPtr pThis = mThisWeak.lock();
          if (!pThis) return;

          try {
            mDelegate->onServiceIdentityProofBundleQueryCompleted(pThis);
          } catch (IServiceIdentityProofBundleQueryDelegateProxy::Exceptions::DelegateGone &) {
          }

          mDelegate.reset();
        }

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IdentityProofBundleQuery => (data)
        #pragma mark

        mutable RecursiveLock mLock;
        PUID mID;
        IdentityProofBundleQueryWeakPtr mThisWeak;
        String mIdentityURI;

        IServiceIdentityProofBundleQueryDelegatePtr mDelegate;
        IBootstrappedNetworkPtr mBootstrappedNetwork;

        ElementPtr mIdentityProofBundleEl;

        WORD mErrorCode;
        String mErrorReason;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServiceIdentity
    #pragma mark

    //-------------------------------------------------------------------------
    bool IServiceIdentity::isValid(const char *identityURI)
    {
      if (!identityURI) {
        ZS_LOG_WARNING(Detail, String("identity URI is not valid as it is NULL, uri=(null)"))
        return false;
      }

      zsLib::RegEx e("^identity:\\/\\/([a-zA-Z0-9]([a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])?\\.)+[a-zA-Z]{2,6}\\/.+$");
      if (!e.hasMatch(identityURI)) {
        zsLib::RegEx e2("^identity:[a-zA-Z0-9\\-_]{0,61}:.+$");
        if (!e2.hasMatch(identityURI)) {
          ZS_LOG_WARNING(Detail, String("ServiceIdentity [] identity URI is not valid, uri=") + identityURI)
          return false;
        }
      }
      return true;
    }

    //-------------------------------------------------------------------------
    bool IServiceIdentity::isValidBase(const char *identityBase)
    {
      if (!identityBase) {
        ZS_LOG_WARNING(Detail, String("identity base is not valid as it is NULL, uri=(null)"))
        return false;
      }

      zsLib::RegEx e("^identity:\\/\\/([a-zA-Z0-9]([a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])?\\.)+[a-zA-Z]{2,6}\\/$");
      if (!e.hasMatch(identityBase)) {
        zsLib::RegEx e2("^identity:[a-zA-Z0-9\\-_]{0,61}:$");
        if (!e2.hasMatch(identityBase)) {
          ZS_LOG_WARNING(Detail, String("ServiceIdentity [] identity base URI is not valid, uri=") + identityBase)
          return false;
        }
      }
      return true;
    }

    //-------------------------------------------------------------------------
    bool IServiceIdentity::isLegacy(const char *identityURI)
    {
      if (!identityURI) {
        ZS_LOG_WARNING(Detail, String("identity URI is not valid as it is NULL, uri=(null)"))
        return false;
      }

      zsLib::RegEx e("^identity:[a-zA-Z0-9\\-_]{0,61}:.*$");
      if (!e.hasMatch(identityURI)) {
        return false;
      }
      return true;
    }

    //-------------------------------------------------------------------------
    bool IServiceIdentity::splitURI(
                                    const char *inIdentityURI,
                                    String &outDomainOrLegacyType,
                                    String &outIdentifier,
                                    bool *outIsLegacy
                                    )
    {
      String identityURI(inIdentityURI ? inIdentityURI : "");

      identityURI.trim();
      if (outIsLegacy) *outIsLegacy = false;

      // scope: check legacy identity
      {
        zsLib::RegEx e("^identity:[a-zA-Z0-9\\-_]{0,61}:.*$");
        if (e.hasMatch(identityURI)) {

          // find second colon
          size_t startPos = strlen("identity:");
          size_t colonPos = identityURI.find(':', identityURI.find(':')+1);

          ZS_THROW_BAD_STATE_IF(colonPos == String::npos)

          outDomainOrLegacyType = identityURI.substr(startPos, colonPos - startPos);
          outIdentifier = identityURI.substr(colonPos + 1);

          if (outIsLegacy) *outIsLegacy = true;
          return true;
        }
      }

      zsLib::RegEx e("^identity:\\/\\/([a-zA-Z0-9]([a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])?\\.)+[a-zA-Z]{2,6}\\/.*$");
      if (!e.hasMatch(identityURI)) {
        ZS_LOG_WARNING(Detail, String("ServiceIdentity [] identity URI is not valid, uri=") + identityURI)
        return false;
      }

      size_t startPos = strlen("identity://");
      size_t slashPos = identityURI.find('/', startPos);

      ZS_THROW_BAD_STATE_IF(slashPos == String::npos)

      outDomainOrLegacyType = identityURI.substr(startPos, slashPos - startPos);
      outIdentifier = identityURI.substr(slashPos + 1);

      outDomainOrLegacyType.toLower();
      return true;
    }

    //-------------------------------------------------------------------------
    String IServiceIdentity::joinURI(
                                     const char *inDomainOrType,
                                     const char *inIdentifier
                                     )
    {
      String domainOrType(inDomainOrType);
      String identifier(inIdentifier);

      domainOrType.trim();
      identifier.trim();

      if (String::npos == domainOrType.find('.')) {
        // this is legacy

        String result = "identity:" + domainOrType + ":" + identifier;
        if (identifier.hasData()) {
          if (!isValid(result)) {
            ZS_LOG_WARNING(Detail, "IServiceIdentity [] invalid identity URI created after join, URI=" + result)
            return String();
          }
        } else {
          if (!isValidBase(result)) {
            ZS_LOG_WARNING(Detail, "IServiceIdentity [] invalid identity URI created after join, URI=" + result)
            return String();
          }
        }
        return result;
      }

      domainOrType.toLower();

      String result = "identity://" + domainOrType + "/" + identifier;
      if (identifier.hasData()) {
        if (!isValid(result)) {
          ZS_LOG_WARNING(Detail, "IServiceIdentity [] invalid identity URI created after join, URI=" + result)
          return String();
        }
      } else {
        if (!isValidBase(result)) {
          ZS_LOG_WARNING(Detail, "IServiceIdentity [] invalid identity URI created after join, URI=" + result)
          return String();
        }
      }
      return result;
    }

    //-------------------------------------------------------------------------
    bool IServiceIdentity::isValidIdentityProofBundle(
                                                      ElementPtr identityProofBundleEl,
                                                      IPeerFilePublicPtr peerFilePublic,
                                                      String *outPeerURI,
                                                      String *outIdentityURI,
                                                      String *outStableID,
                                                      Time *outCreated,
                                                      Time *outExpires
                                                      )
    {
      ZS_THROW_INVALID_ARGUMENT_IF(!identityProofBundleEl)

      IServiceIdentityPtr serviceIdentity = createServiceIdentityFromIdentityProofBundle(identityProofBundleEl);
      if (!serviceIdentity) {
        ZS_LOG_WARNING(Detail, "IServiceIdentity [] failed to obtain bootstrapped network from identity proof bundle")
        return false;
      }

      IBootstrappedNetworkPtr bootstrapper = serviceIdentity->getBootstrappedNetwork();
      if (!bootstrapper->isPreparationComplete()) {
        ZS_LOG_WARNING(Detail, "IServiceIdentity [] bootstapped network isn't prepared yet")
        return false;
      }

      WORD errorCode = 0;
      String reason;
      if (!bootstrapper->wasSuccessful(&errorCode, &reason)) {
        ZS_LOG_WARNING(Detail, String("IServiceIdentity [] bootstapped network was not successful") + ", error=" + string(errorCode) + ", reason=" + reason)
        return false;
      }

      String identityURI;

      bool result = internal::extractAndVerifyProof(
                                                    identityProofBundleEl,
                                                    peerFilePublic,
                                                    outPeerURI,
                                                    &identityURI,
                                                    outStableID,
                                                    outCreated,
                                                    outExpires
                                                    );

      if (outIdentityURI) {
        *outIdentityURI = identityURI;
      }

      if (!result) {
        ZS_LOG_WARNING(Detail, String("IServiceIdentity [] signature validation failed on identity bundle") + ", identity=" + identityURI)
        return false;
      }

      IServiceCertificatesPtr serviceCertificate = IServiceCertificates::createServiceCertificatesFrom(bootstrapper);
      ZS_THROW_BAD_STATE_IF(!serviceCertificate)

      ElementPtr identityProofEl = identityProofBundleEl->findFirstChildElement("identityProof");
      ZS_THROW_BAD_STATE_IF(!identityProofEl)

      if (!serviceCertificate->isValidSignature(identityProofEl)) {
        ZS_LOG_WARNING(Detail, String("IServiceIdentity [] signature failed to validate on identity bundle") + "identity=" + identityURI)
        return false;
      }

      ZS_LOG_TRACE(String("IServiceIdentity [] signature verified for identity") + ", identity=" + identityURI)
      return true;
    }

    //-------------------------------------------------------------------------
    IServiceIdentityProofBundleQueryPtr IServiceIdentity::isValidIdentityProofBundle(
                                                                                     ElementPtr identityProofBundleEl,
                                                                                     IServiceIdentityProofBundleQueryDelegatePtr delegate,
                                                                                     IPeerFilePublicPtr peerFilePublic, // optional recommended check of associated peer file, can pass in IPeerFilePublicPtr() if not known yet
                                                                                     String *outPeerURI,
                                                                                     String *outIdentityURI,
                                                                                     String *outStableID,
                                                                                     Time *outCreated,
                                                                                     Time *outExpires
                                                                                     )
    {
      ZS_THROW_INVALID_ARGUMENT_IF(!identityProofBundleEl)
      ZS_THROW_INVALID_ARGUMENT_IF(!delegate)

      String identityURI;

      bool result = internal::extractAndVerifyProof(
                                                    identityProofBundleEl,
                                                    peerFilePublic,
                                                    outPeerURI,
                                                    &identityURI,
                                                    outStableID,
                                                    outCreated,
                                                    outExpires
                                                    );

      if (outIdentityURI) {
        *outIdentityURI = identityURI;
      }

      WORD errorCode = 0;
      String reason;
      if (!result) {
        errorCode = IHTTP::HTTPStatusCode_CertError;
        reason = "identity failed to validate, identity=" + identityURI;
      }

      return internal::IdentityProofBundleQuery::create(identityProofBundleEl, delegate, identityURI, errorCode, reason);
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServiceIdentitySession
    #pragma mark

    //-------------------------------------------------------------------------
    String IServiceIdentitySession::toDebugString(IServiceIdentitySessionPtr session, bool includeCommaPrefix)
    {
      return internal::ServiceIdentitySession::toDebugString(session, includeCommaPrefix);
    }

    //-------------------------------------------------------------------------
    const char *IServiceIdentitySession::toString(SessionStates state)
    {
      switch (state)
      {
        case SessionState_Pending:                                  return "Pending";
        case SessionState_WaitingAttachmentOfDelegate:              return "Waiting Attachment of Delegate";
        case SessionState_WaitingForBrowserWindowToBeLoaded:        return "Waiting for Browser Window to be Loaded";
        case SessionState_WaitingForBrowserWindowToBeMadeVisible:   return "Waiting for Browser Window to be Made Visible";
        case SessionState_WaitingForBrowserWindowToClose:           return "Waiting for Browser Window to Close";
        case SessionState_WaitingForAssociationToLockbox:           return "Waiting for Association to Lockbox";
        case SessionState_Ready:                                    return "Ready";
        case SessionState_Shutdown:                                 return "Shutdown";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    IServiceIdentitySessionPtr IServiceIdentitySession::loginWithIdentity(
                                                                          IServiceIdentitySessionDelegatePtr delegate,
                                                                          IServiceIdentityPtr provider,
                                                                          IServiceNamespaceGrantSessionPtr grantSession,
                                                                          IServiceLockboxSessionPtr existingLockbox,
                                                                          const char *outerFrameURLUponReload,
                                                                          const char *identityURI
                                                                          )
    {
      return internal::IServiceIdentitySessionFactory::singleton().loginWithIdentity(delegate, provider, grantSession, existingLockbox, outerFrameURLUponReload, identityURI);
    }

    //-------------------------------------------------------------------------
    IServiceIdentitySessionPtr IServiceIdentitySession::loginWithIdentityPreauthorized(
                                                                                       IServiceIdentitySessionDelegatePtr delegate,
                                                                                       IServiceIdentityPtr provider,
                                                                                       IServiceNamespaceGrantSessionPtr grantSession,
                                                                                       IServiceLockboxSessionPtr existingLockbox,  // pass NULL IServiceLockboxSessionPtr() if none exists
                                                                                       const char *identityURI,
                                                                                       const char *identityAccessToken,
                                                                                       const char *identityAccessSecret,
                                                                                       Time identityAccessSecretExpires
                                                                                       )
    {
      return internal::IServiceIdentitySessionFactory::singleton().loginWithIdentityPreauthorized(delegate, provider, grantSession, existingLockbox, identityURI, identityAccessToken, identityAccessSecret, identityAccessSecretExpires);
    }
  }
}
