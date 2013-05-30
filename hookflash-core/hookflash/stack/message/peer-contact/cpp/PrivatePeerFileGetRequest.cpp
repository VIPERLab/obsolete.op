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

#include <hookflash/stack/message/peer-contact/PrivatePeerFileGetRequest.h>
#include <hookflash/stack/message/internal/stack_message_MessageHelper.h>
#include <hookflash/stack/IHelper.h>
#include <hookflash/stack/IPeerFiles.h>
#include <hookflash/stack/IPeerFilePrivate.h>
#include <hookflash/stack/IPeerFilePublic.h>

#include <zsLib/XML.h>
#include <zsLib/helpers.h>

#define HOOKFLASH_STACK_MESSAGE_PRIVATE_PEER_FILE_GET_REQUEST_EXPIRES_TIME_IN_SECONDS ((60*60)*24)

namespace hookflash { namespace stack { namespace message { ZS_DECLARE_SUBSYSTEM(hookflash_stack_message) } } }

namespace hookflash
{
  namespace stack
  {
    namespace message
    {
      namespace peer_contact
      {
        using zsLib::Seconds;

        //---------------------------------------------------------------------
        PrivatePeerFileGetRequestPtr PrivatePeerFileGetRequest::convert(MessagePtr message)
        {
          return boost::dynamic_pointer_cast<PrivatePeerFileGetRequest>(message);
        }

        //---------------------------------------------------------------------
        PrivatePeerFileGetRequest::PrivatePeerFileGetRequest()
        {
        }

        //---------------------------------------------------------------------
        PrivatePeerFileGetRequestPtr PrivatePeerFileGetRequest::create()
        {
          PrivatePeerFileGetRequestPtr ret(new PrivatePeerFileGetRequest);
          return ret;
        }

        //---------------------------------------------------------------------
        bool PrivatePeerFileGetRequest::hasAttribute(AttributeTypes type) const
        {
          switch (type)
          {
            case AttributeType_ContactAccessToken:          return !mContactAccessToken.isEmpty();
            case AttributeType_ContactAccessSecret:         return !mContactAccessSecret.isEmpty();
            case AttributeType_PrivatePeerFileSecretProof:  return !mPrivatePeerFileSecretProof.isEmpty();
            default:                                        break;
          }
          return false;
        }

        //---------------------------------------------------------------------
        DocumentPtr PrivatePeerFileGetRequest::encode()
        {
          DocumentPtr ret = IMessageHelper::createDocumentWithRoot(*this);
          ElementPtr root = ret->getFirstChildElement();

          String clientNonce = IHelper::randomString(32);
          String expires = IMessageHelper::timeToString(zsLib::now() + Seconds(HOOKFLASH_STACK_MESSAGE_PRIVATE_PEER_FILE_GET_REQUEST_EXPIRES_TIME_IN_SECONDS));
          
#define MUST_REMOVE_SECURITY_HACK_ONLY_FOR_BB10_RELEASE_PURPOSES 1
#define MUST_REMOVE_SECURITY_HACK_ONLY_FOR_BB10_RELEASE_PURPOSES 2
//          String finalAccessProof = IHelper::convertToHex(*IHelper::hmac(*IHelper::hmacKey(mContactAccessSecret), "private-peer-file-get:" + clientNonce + ":" + expires + ":" + mContactAccessToken));
//          String finalPeerProof = IHelper::convertToHex(*IHelper::hash("private-peer-file-get:" + clientNonce + ":" + expires + ":" + mPrivatePeerFileSecretProof));
          
          String finalAccessProof = mContactAccessSecret;
          String finalPeerProof = mPrivatePeerFileSecretProof;

          root->adoptAsLastChild(IMessageHelper::createElementWithText("clientNonce", clientNonce));
          if (hasAttribute(AttributeType_ContactAccessToken)) {
            root->adoptAsLastChild(IMessageHelper::createElementWithText("contactAccessToken", mContactAccessToken));
          }
          if (hasAttribute(AttributeType_ContactAccessSecret)) {
            root->adoptAsLastChild(IMessageHelper::createElementWithText("contactAccessSecretProof", finalAccessProof));
          }
          root->adoptAsLastChild(IMessageHelper::createElementWithNumber("contactAccessSecretProofExpires", expires));

          if (hasAttribute(AttributeType_PrivatePeerFileSecretProof)) {
            root->adoptAsLastChild(IMessageHelper::createElementWithText("privatePeerFileSecretProof", finalPeerProof));
            root->adoptAsLastChild(IMessageHelper::createElementWithNumber("privatePeerFileSecretProofExpires", expires));
          }

          return ret;
        }
      }
    }
  }
}
