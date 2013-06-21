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

#include <hookflash/stack/message/identity-lockbox/LockboxIdentitiesUpdateRequest.h>
#include <hookflash/stack/message/internal/stack_message_MessageHelper.h>
#include <hookflash/stack/IHelper.h>
#include <hookflash/stack/IPeerFiles.h>
#include <hookflash/stack/IPeerFilePrivate.h>
#include <hookflash/stack/IPeerFilePublic.h>

#include <zsLib/XML.h>
#include <zsLib/helpers.h>

#define HOOKFLASH_STACK_MESSAGE_LOCKBOX_IDENTITIES_UPDATE_REQUEST_EXPIRES_TIME_IN_SECONDS ((60*60)*24)

namespace hookflash { namespace stack { namespace message { ZS_DECLARE_SUBSYSTEM(hookflash_stack_message) } } }

namespace hookflash
{
  namespace stack
  {
    namespace message
    {
      namespace identity_lockbox
      {
        using zsLib::Seconds;
        using internal::MessageHelper;

        //---------------------------------------------------------------------
        LockboxIdentitiesUpdateRequestPtr LockboxIdentitiesUpdateRequest::convert(MessagePtr message)
        {
          return boost::dynamic_pointer_cast<LockboxIdentitiesUpdateRequest>(message);
        }

        //---------------------------------------------------------------------
        LockboxIdentitiesUpdateRequest::LockboxIdentitiesUpdateRequest()
        {
        }

        //---------------------------------------------------------------------
        LockboxIdentitiesUpdateRequestPtr LockboxIdentitiesUpdateRequest::create()
        {
          LockboxIdentitiesUpdateRequestPtr ret(new LockboxIdentitiesUpdateRequest);
          return ret;
        }

        //---------------------------------------------------------------------
        bool LockboxIdentitiesUpdateRequest::hasAttribute(AttributeTypes type) const
        {
          switch (type)
          {
            case AttributeType_LockboxInfo:      return mLockboxInfo.hasData();
            default:                             break;
          }
          return false;
        }

        //---------------------------------------------------------------------
        DocumentPtr LockboxIdentitiesUpdateRequest::encode()
        {
          DocumentPtr ret = IMessageHelper::createDocumentWithRoot(*this);
          ElementPtr root = ret->getFirstChildElement();

          String clientNonce = IHelper::randomString(32);

          LockboxInfo lockboxInfo;

          lockboxInfo.mAccessToken = mLockboxInfo.mAccessToken;
          if (mLockboxInfo.mAccessSecret.hasData()) {
            lockboxInfo.mAccessSecretProofExpires = zsLib::now() + Seconds(HOOKFLASH_STACK_MESSAGE_LOCKBOX_IDENTITIES_UPDATE_REQUEST_EXPIRES_TIME_IN_SECONDS);
            lockboxInfo.mAccessSecretProof = IHelper::convertToHex(*IHelper::hmac(*IHelper::convertToBuffer(mLockboxInfo.mAccessSecret), "lockbox-access-validate:" + clientNonce + ":" + IMessageHelper::timeToString(lockboxInfo.mAccessSecretProofExpires) + ":" + lockboxInfo.mAccessToken + ":lockbox-identities-update"));
          }

          IdentityInfoList identities;

          for (IdentityInfoList::iterator iter = mIdentitiesToUpdate.begin(); iter != mIdentitiesToUpdate.end(); ++iter)
          {
            IdentityInfo &listIdentity = (*iter);

            IdentityInfo identityInfo;
            identityInfo.mURI = listIdentity.mURI;
            identityInfo.mProvider = listIdentity.mProvider;

            identityInfo.mAccessToken = listIdentity.mAccessToken;
            if (listIdentity.mAccessSecret.hasData()) {
              identityInfo.mAccessSecretProofExpires = zsLib::now() + Seconds(HOOKFLASH_STACK_MESSAGE_LOCKBOX_IDENTITIES_UPDATE_REQUEST_EXPIRES_TIME_IN_SECONDS);
              identityInfo.mAccessSecretProof = IHelper::convertToHex(*IHelper::hmac(*IHelper::convertToBuffer(listIdentity.mAccessSecret), "identity-access-validate:" + identityInfo.mURI + ":" + clientNonce + ":" + IMessageHelper::timeToString(identityInfo.mAccessSecretProofExpires) + ":" + identityInfo.mAccessToken + ":lockbox-access-update"));
            }

            if (identityInfo.hasData()) {
              identityInfo.mDisposition = IdentityInfo::Disposition_Update;
              identities.push_back(identityInfo);
            }
          }

          for (IdentityInfoList::iterator iter = mIdentitiesToRemove.begin(); iter != mIdentitiesToRemove.end(); ++iter)
          {
            IdentityInfo &listIdentity = (*iter);

            IdentityInfo identityInfo;
            identityInfo.mURI = listIdentity.mURI;
            identityInfo.mProvider = listIdentity.mProvider;

            if (identityInfo.hasData()) {
              identityInfo.mDisposition = IdentityInfo::Disposition_Remove;
              identities.push_back(identityInfo);
            }
          }

          root->adoptAsLastChild(IMessageHelper::createElementWithText("clientNonce", clientNonce));
          if (lockboxInfo.hasData()) {
            root->adoptAsLastChild(MessageHelper::createElement(lockboxInfo));
          }

          ElementPtr identitiesEl = IMessageHelper::createElement("identities");
          for (IdentityInfoList::iterator iter = identities.begin(); iter != identities.end(); ++iter)
          {
            IdentityInfo &listIdentity = (*iter);
            identitiesEl->adoptAsLastChild(MessageHelper::createElement(listIdentity));
          }

          if (identitiesEl->hasChildren()) {
            root->adoptAsLastChild(identitiesEl);
          }

          return ret;
        }
      }
    }
  }
}
