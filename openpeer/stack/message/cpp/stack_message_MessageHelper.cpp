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

#include <openpeer/stack/message/internal/stack_message_MessageHelper.h>
#include <openpeer/stack/message/IMessageHelper.h>
#include <openpeer/stack/message/IMessageFactory.h>
#include <openpeer/stack/message/MessageResult.h>

#include <openpeer/stack/message/peer-common/MessageFactoryPeerCommon.h>
#include <openpeer/stack/message/peer-common/PeerPublishRequest.h>
#include <openpeer/stack/message/peer-common/PeerGetResult.h>

#include <openpeer/stack/internal/stack_Location.h>
#include <openpeer/stack/internal/stack_Peer.h>
#include <openpeer/stack/internal/stack_Publication.h>
#include <openpeer/stack/internal/stack_PublicationMetaData.h>

#include <openpeer/stack/IPublicationRepository.h>
#include <openpeer/stack/IHelper.h>
#include <openpeer/stack/IPeerFilePublic.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/IRSAPublicKey.h>

#include <zsLib/Numeric.h>
#include <zsLib/XML.h>
#include <zsLib/helpers.h>

namespace openpeer { namespace stack { namespace message { ZS_DECLARE_SUBSYSTEM(openpeer_stack_message) } } }

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      using zsLib::DWORD;
      using zsLib::QWORD;
      using zsLib::Numeric;

      using namespace stack::internal;
      using services::IHelper;

      using peer_common::MessageFactoryPeerCommon;
      using peer_common::PeerPublishRequest;
      using peer_common::PeerGetResult;

      typedef stack::IPublicationMetaData::PublishToRelationshipsMap PublishToRelationshipsMap;

      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMessageHelper
      #pragma mark

      //-----------------------------------------------------------------------
      DocumentPtr IMessageHelper::createDocumentWithRoot(const Message &message)
      {
        const char *tagName = Message::toString(message.messageType());

        IMessageFactoryPtr factory = message.factory();

        if (!factory) return DocumentPtr();

        DocumentPtr ret = Document::create();
        ret->setElementNameIsCaseSensative(true);
        ret->setAttributeNameIsCaseSensative(true);

        ElementPtr rootEl = Element::create(tagName);
        ret->adoptAsFirstChild(rootEl);

        String domain = message.domain();

        if (domain.hasData()) {
          IMessageHelper::setAttribute(rootEl, "domain", domain);
        }

        String appID = message.appID();
        if (appID.hasData()) {
          IMessageHelper::setAttribute(rootEl, "appid", appID);
        }

        IMessageHelper::setAttribute(rootEl, "handler", factory->getHandler());
        IMessageHelper::setAttributeID(rootEl, message.messageID());
        IMessageHelper::setAttribute(rootEl, "method", factory->toString(message.method()));

        if (message.isResult()) {
          const message::MessageResult *msgResult = (dynamic_cast<const message::MessageResult *>(&message));
          if (msgResult->hasAttribute(MessageResult::AttributeType_Time)) {
            IMessageHelper::setAttributeTimestamp(rootEl, msgResult->time());
          }
          if ((msgResult->hasAttribute(MessageResult::AttributeType_ErrorCode)) ||
              (msgResult->hasAttribute(MessageResult::AttributeType_ErrorReason))) {

            ElementPtr errorEl;
            if (msgResult->hasAttribute(MessageResult::AttributeType_ErrorReason)) {
              errorEl = IMessageHelper::createElementWithTextAndJSONEncode("error", msgResult->errorReason());
            } else {
              errorEl = IMessageHelper::createElement("error");
            }
            if (msgResult->hasAttribute(MessageResult::AttributeType_ErrorCode)) {
              IMessageHelper:setAttributeID(errorEl, string(msgResult->errorCode()));
            }

            rootEl->adoptAsLastChild(errorEl);
          }
        }

        return ret;
      }

      //-----------------------------------------------------------------------
      Message::MessageTypes IMessageHelper::getMessageType(ElementPtr root)
      {
        if (!root) return Message::MessageType_Invalid;
        return Message::toMessageType(root->getValue());
      }

      //---------------------------------------------------------------------
      String IMessageHelper::getAttributeID(ElementPtr node)
      {
        return IMessageHelper::getAttribute(node, "id");
      }

      //---------------------------------------------------------------------
      void IMessageHelper::setAttributeID(ElementPtr elem, const String &value)
      {
        if (!value.isEmpty())
          IMessageHelper::setAttribute(elem, "id", value);
      }

      //---------------------------------------------------------------------
      Time IMessageHelper::getAttributeEpoch(ElementPtr node)
      {
        return IHelper::stringToTime(IMessageHelper::getAttribute(node, "timestamp"));
      }

      //---------------------------------------------------------------------
      void IMessageHelper::setAttributeTimestamp(ElementPtr elem, const Time &value)
      {
        if (Time() == value) return;
        elem->setAttribute("timestamp", IHelper::timeToString(value), false);
      }

      //-----------------------------------------------------------------------
      String IMessageHelper::getAttribute(
                                          ElementPtr node,
                                          const String &attributeName
                                          )
      {
        if (!node) return String();

        AttributePtr attribute = node->findAttribute(attributeName);
        if (!attribute) return String();

        return attribute->getValue();
      }

      //-----------------------------------------------------------------------
      void IMessageHelper::setAttribute(
                                        ElementPtr elem,
                                        const String &attrName,
                                        const String &value
                                        )
      {
        if (value.isEmpty()) return;

        AttributePtr attr = Attribute::create();
        attr->setName(attrName);
        attr->setValue(value);

        elem->setAttribute(attr);
      }

      //-----------------------------------------------------------------------
      ElementPtr IMessageHelper::createElement(const String &elName)
      {
        ElementPtr tmp = Element::create();
        tmp->setValue(elName);
        return tmp;
      }

      //-----------------------------------------------------------------------
      ElementPtr IMessageHelper::createElementWithText(
                                                       const String &elName,
                                                       const String &textVal
                                                       )
      {
        ElementPtr tmp = Element::create(elName);

        if (textVal.isEmpty()) return tmp;

        TextPtr tmpTxt = Text::create();
        tmpTxt->setValue(textVal, Text::Format_JSONStringEncoded);

        tmp->adoptAsFirstChild(tmpTxt);

        return tmp;
      }

      //-----------------------------------------------------------------------
      ElementPtr IMessageHelper::createElementWithNumber(
                                                         const String &elName,
                                                         const String &numberAsStringValue
                                                         )
      {
        ElementPtr tmp = Element::create(elName);

        if (numberAsStringValue.isEmpty()) return tmp;

        TextPtr tmpTxt = Text::create();
        tmpTxt->setValue(numberAsStringValue, Text::Format_JSONNumberEncoded);
        tmp->adoptAsFirstChild(tmpTxt);

        return tmp;
      }

      //-----------------------------------------------------------------------
      ElementPtr IMessageHelper::createElementWithTime(
                                                       const String &elName,
                                                       Time time
                                                       )
      {
        return createElementWithNumber(elName, IHelper::timeToString(time));
      }

      //-----------------------------------------------------------------------
      ElementPtr IMessageHelper::createElementWithTextAndJSONEncode(
                                                                    const String &elName,
                                                                    const String &textVal
                                                                    )
      {
        ElementPtr tmp = Element::create(elName);
        if (textVal.isEmpty()) return tmp;

        TextPtr tmpTxt = Text::create();
        tmpTxt->setValueAndJSONEncode(textVal);
        tmp->adoptAsFirstChild(tmpTxt);
        return tmp;
      }

      //-----------------------------------------------------------------------
      ElementPtr IMessageHelper::createElementWithID(
                                                     const String &elName,
                                                     const String &idValue
                                                     )
      {
        ElementPtr tmp = createElement(elName);

        if (idValue.isEmpty()) return tmp;

        setAttributeID(tmp, idValue);
        return tmp;
      }

      //-----------------------------------------------------------------------
      TextPtr IMessageHelper::createText(const String &textVal)
      {
        TextPtr tmpTxt = Text::create();
        tmpTxt->setValue(textVal);

        return tmpTxt;
      }

      //-----------------------------------------------------------------------
      String IMessageHelper::getElementText(ElementPtr node)
      {
        if (!node) return String();
        return node->getText();
      }

      //-----------------------------------------------------------------------
      String IMessageHelper::getElementTextAndDecode(ElementPtr node)
      {
        if (!node) return String();
        return node->getTextDecoded();
      }

      //-----------------------------------------------------------------------
      void IMessageHelper::fill(
                                Message &message,
                                ElementPtr root,
                                IMessageSourcePtr source
                                )
      {
        String id = IMessageHelper::getAttribute(root, "id");
        String domain = IMessageHelper::getAttribute(root, "domain");
        String appID = IMessageHelper::getAttribute(root, "appid");

        if (id.hasData()) {
          message.messageID(id);
        }
        if (domain.hasData()) {
          message.domain(domain);
        }
        if (appID.hasData()) {
          message.appID(appID);
        }
        if (message.isResult()) {
          Time time = IMessageHelper::getAttributeEpoch(root);
          message::MessageResult *result = (dynamic_cast<message::MessageResult *>(&message));
          result->time(time);
        }
      }

      //-----------------------------------------------------------------------
      ElementPtr IMessageHelper::createElement(
                                               const Candidate &candidate,
                                               const char *encryptionPassphrase
                                               )
      {
        return internal::MessageHelper::createElement(candidate, encryptionPassphrase);
      }

      //-----------------------------------------------------------------------
      Candidate IMessageHelper::createCandidate(
                                                ElementPtr elem,
                                                const char *encryptionPassphrase
                                                )
      {
        return internal::MessageHelper::createCandidate(elem, encryptionPassphrase);
      }

      namespace internal
      {
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageHelper
        #pragma mark

        //---------------------------------------------------------------------
        ElementPtr MessageHelper::createElement(
                                                const Candidate &candidate,
                                                const char *encryptionPassphrase
                                                )
        {
          ElementPtr candidateEl = IMessageHelper::createElement("candidate");

          if (candidate.mNamespace.hasData()) {
            candidateEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("namespace", candidate.mNamespace));
          }
          if (candidate.mTransport.hasData()) {
            candidateEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("transport", candidate.mTransport));
          }

          const char *typeAsString = NULL;
          switch (candidate.mType) {
            case IICESocket::Type_Unknown:          break;
            case IICESocket::Type_Local:            typeAsString = "host"; break;
            case IICESocket::Type_ServerReflexive:  typeAsString = "srflx";break;
            case IICESocket::Type_PeerReflexive:    typeAsString = "prflx"; break;
            case IICESocket::Type_Relayed:          typeAsString = "relay"; break;
          }

          if (typeAsString) {
            candidateEl->adoptAsLastChild(IMessageHelper::createElementWithText("type", typeAsString));
          }

          if (candidate.mFoundation.hasData()) {
            candidateEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("foundation", candidate.mFoundation));
          }

          if (0 != candidate.mComponentID) {
            candidateEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("component", string(candidate.mIPAddress.getPort())));
          }

          if (!candidate.mIPAddress.isEmpty()) {
            if (candidate.mAccessToken.hasData()) {
              candidateEl->adoptAsLastChild(IMessageHelper::createElementWithText("host", candidate.mIPAddress.string(false)));
            } else {
              candidateEl->adoptAsLastChild(IMessageHelper::createElementWithText("ip", candidate.mIPAddress.string(false)));
            }
            candidateEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("port", string(candidate.mIPAddress.getPort())));
          }

          if (0 != candidate.mPriority) {
            candidateEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("priority", string(candidate.mPriority)));
          }

          if (!candidate.mRelatedIP.isEmpty()) {
            ElementPtr relatedEl = Element::create("related");
            candidateEl->adoptAsLastChild(relatedEl);
            relatedEl->adoptAsLastChild(IMessageHelper::createElementWithText("ip", candidate.mRelatedIP.string(false)));
            relatedEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("port", string(candidate.mRelatedIP.getPort())));
          }

          if (candidate.mAccessToken.hasData()) {
            candidateEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("accessToken", candidate.mAccessToken));
          }

          if ((candidate.mAccessSecretProof.hasData()) &&
              (encryptionPassphrase)) {
            String accessSecretProofEncrypted = stack::IHelper::splitEncrypt(*IHelper::hash(encryptionPassphrase, IHelper::HashAlgorthm_SHA256), *IHelper::convertToBuffer(candidate.mAccessSecretProof));
            candidateEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("accessSecretProofEncrypted", accessSecretProofEncrypted));
          }

          return candidateEl;
        }

        //---------------------------------------------------------------------
        ElementPtr MessageHelper::createElement(
                                                const LocationInfo &locationInfo,
                                                const char *encryptionPassphrase
                                                )
        {
          if (!locationInfo.mLocation) {
            ZS_LOG_WARNING(Detail, "MessageHelper [] missing location object in location info")
            return ElementPtr();
          }

          LocationPtr location = Location::convert(locationInfo.mLocation);

          ElementPtr locationEl = IMessageHelper::createElementWithID("location", location->forMessages().getLocationID());
          ElementPtr detailEl = IMessageHelper::createElement("details");

          if (!locationInfo.mDeviceID.isEmpty()) {
            detailEl->adoptAsLastChild(IMessageHelper::createElementWithID("device", locationInfo.mDeviceID));
          }

          if (!locationInfo.mIPAddress.isAddressEmpty())
          {
            ElementPtr ipEl = IMessageHelper::createElementWithText("ip", locationInfo.mIPAddress.string(false));
            detailEl->adoptAsLastChild(ipEl);
          }

          if (!locationInfo.mUserAgent.isEmpty())
            detailEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("userAgent", locationInfo.mUserAgent));

          if (!locationInfo.mOS.isEmpty())
            detailEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("os", locationInfo.mOS));

          if (!locationInfo.mSystem.isEmpty())
            detailEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("system", locationInfo.mSystem));

          if (!locationInfo.mHost.isEmpty())
            detailEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("host", locationInfo.mHost));

          PeerPtr peer = location->forMessages().getPeer();
          if (peer) {
            locationEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("contact", peer->forMessages().getPeerURI()));
          }

          if (detailEl->hasChildren()) {
            locationEl->adoptAsLastChild(detailEl);
          }

          if (locationInfo.mCandidates.size() > 0)
          {
            ElementPtr candidates = IMessageHelper::createElement("candidates");
            locationEl->adoptAsLastChild(candidates);

            CandidateList::const_iterator it;
            for(it=locationInfo.mCandidates.begin(); it!=locationInfo.mCandidates.end(); ++it)
            {
              Candidate candidate(*it);
              candidates->adoptAsLastChild(MessageHelper::createElement(candidate, encryptionPassphrase));
            }

            locationEl->adoptAsLastChild(candidates);
          }

          return locationEl;
        }

        //---------------------------------------------------------------------
        ElementPtr MessageHelper::createElement(
                                                const IdentityInfo &identity,
                                                bool forcePriorityWeightOutput
                                                )
        {
          ElementPtr identityEl = Element::create("identity");
          if (IdentityInfo::Disposition_NA != identity.mDisposition) {
            identityEl->setAttribute("disposition", IdentityInfo::toString(identity.mDisposition));
          }

          if (!identity.mAccessToken.isEmpty()) {
            identityEl->adoptAsLastChild(IMessageHelper::createElementWithText("accessToken", identity.mAccessToken));
          }
          if (!identity.mAccessSecret.isEmpty()) {
            identityEl->adoptAsLastChild(IMessageHelper::createElementWithText("accessSecret", identity.mAccessSecret));
          }
          if (Time() != identity.mAccessSecretExpires) {
            identityEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("accessSecretExpires", IHelper::timeToString(identity.mAccessSecretExpires)));
          }
          if (!identity.mAccessSecretProof.isEmpty()) {
            identityEl->adoptAsLastChild(IMessageHelper::createElementWithText("accessSecretProof", identity.mAccessSecretProof));
          }
          if (Time() != identity.mAccessSecretProofExpires) {
            identityEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("accessSecretProofExpires", IHelper::timeToString(identity.mAccessSecretProofExpires)));
          }

          if (!identity.mReloginKey.isEmpty()) {
            identityEl->adoptAsLastChild(IMessageHelper::createElementWithText("reloginKey", identity.mReloginKey));
          }

          if (!identity.mBase.isEmpty()) {
            identityEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("base", identity.mBase));
          }
          if (!identity.mURI.isEmpty()) {
            identityEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("uri", identity.mURI));
          }
          if (!identity.mProvider.isEmpty()) {
            identityEl->adoptAsLastChild(IMessageHelper::createElementWithText("provider", identity.mProvider));
          }

          if (!identity.mStableID.isEmpty()) {
            identityEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("stableID", identity.mStableID));
          }

          if (identity.mPeerFilePublic) {
            identityEl->adoptAsLastChild(identity.mPeerFilePublic->saveToElement());
          }

          if ((0 != identity.mPriority) ||
              (forcePriorityWeightOutput)) {
            identityEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("priority", string(identity.mPriority)));
          }
          if ((0 != identity.mWeight) ||
              (forcePriorityWeightOutput)) {
            identityEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("weight", string(identity.mWeight)));
          }

          if (Time() != identity.mCreated) {
            identityEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("created", IHelper::timeToString(identity.mCreated)));
          }
          if (Time() != identity.mUpdated) {
            identityEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("updated", IHelper::timeToString(identity.mUpdated)));
          }
          if (Time() != identity.mExpires) {
            identityEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("expires", IHelper::timeToString(identity.mExpires)));
          }

          if (!identity.mName.isEmpty()) {
            identityEl->adoptAsLastChild(IMessageHelper::createElementWithText("name", identity.mName));
          }
          if (!identity.mProfile.isEmpty()) {
            identityEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("profile", identity.mProfile));
          }
          if (!identity.mVProfile.isEmpty()) {
            identityEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("vprofile", identity.mVProfile));
          }

          if (!identity.mProfile.isEmpty()) {
            identityEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("profile", identity.mProfile));
          }

          if (identity.mAvatars.size() > 0) {
            ElementPtr avatarsEl = Element::create("avatars");
            for (IdentityInfo::AvatarList::const_iterator iter = identity.mAvatars.begin(); iter != identity.mAvatars.end(); ++iter)
            {
              const IdentityInfo::Avatar &avatar = (*iter);
              ElementPtr avatarEl = Element::create("avatar");

              if (!avatar.mName.isEmpty()) {
                avatarEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("name", avatar.mName));
              }
              if (!avatar.mURL.isEmpty()) {
                avatarEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("name", avatar.mURL));
              }
              if (0 != avatar.mWidth) {
                avatarEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("width", string(avatar.mWidth)));
              }
              if (0 != avatar.mHeight) {
                avatarEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("height", string(avatar.mHeight)));
              }

              if (avatarEl->hasChildren()) {
                avatarsEl->adoptAsLastChild(avatarEl);
              }
            }
            if (avatarsEl->hasChildren()) {
              identityEl->adoptAsLastChild(avatarsEl);
            }
          }

          if (identity.mContactProofBundle) {
            identityEl->adoptAsLastChild(identity.mContactProofBundle->clone()->toElement());
          }
          if (identity.mIdentityProofBundle) {
            identityEl->adoptAsLastChild(identity.mIdentityProofBundle->clone()->toElement());
          }

          return identityEl;
        }

        //---------------------------------------------------------------------
        ElementPtr MessageHelper::createElement(const LockboxInfo &info)
        {
          ElementPtr lockboxEl = Element::create("lockbox");

          if (info.mDomain.hasData()) {
            lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithText("domain", info.mDomain));
          }
          if (info.mAccountID.hasData()) {
            lockboxEl->setAttribute("id", info.mAccountID);
          }
          if (info.mAccessToken.hasData()) {
            lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithText("accessToken", info.mAccessToken));
          }
          if (info.mAccessSecret.hasData()) {
            lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithText("accessSecret", info.mAccessSecret));
          }
          if (Time() != info.mAccessSecretExpires) {
            lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("accessSecretExpires", IHelper::timeToString(info.mAccessSecretExpires)));
          }
          if (info.mAccessSecretProof.hasData()) {
            lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithText("accessSecretProof", info.mAccessSecretProof));
          }
          if (Time() != info.mAccessSecretProofExpires) {
            lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("accessSecretProofExpires", IHelper::timeToString(info.mAccessSecretProofExpires)));
          }

          if (info.mKey) {
            lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("key", IHelper::convertToBase64(*info.mKey)));
          }
          if (info.mHash.hasData()) {
            lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("hash", info.mHash));
          }

          if (info.mResetFlag) {
            lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("reset", "true"));
          }

          return lockboxEl;
        }

        //---------------------------------------------------------------------
        ElementPtr MessageHelper::createElement(const AgentInfo &info)
        {
          ElementPtr agentEl = Element::create("agent");

          if (info.mUserAgent.hasData()) {
            agentEl->adoptAsLastChild(IMessageHelper::createElementWithText("userAgent", info.mUserAgent));
          }
          if (info.mName.hasData()) {
            agentEl->adoptAsLastChild(IMessageHelper::createElementWithText("name", info.mName));
          }
          if (info.mImageURL.hasData()) {
            agentEl->adoptAsLastChild(IMessageHelper::createElementWithText("image", info.mImageURL));
          }
          if (info.mAgentURL.hasData()) {
            agentEl->adoptAsLastChild(IMessageHelper::createElementWithText("url", info.mAgentURL));
          }

          return agentEl;
        }
        
        //---------------------------------------------------------------------
        ElementPtr MessageHelper::createElement(const NamespaceGrantChallengeInfo &info)
        {
          ElementPtr namespaceGrantChallengeEl = Element::create("namespaceGrantChallenge");

          setAttributeID(namespaceGrantChallengeEl, info.mID);
          if (info.mName.hasData()) {
            namespaceGrantChallengeEl->adoptAsLastChild(IMessageHelper::createElementWithText("name", info.mName));
          }
          if (info.mImageURL.hasData()) {
            namespaceGrantChallengeEl->adoptAsLastChild(IMessageHelper::createElementWithText("image", info.mName));
          }
          if (info.mServiceURL.hasData()) {
            namespaceGrantChallengeEl->adoptAsLastChild(IMessageHelper::createElementWithText("url", info.mName));
          }
          if (info.mDomains.hasData()) {
            namespaceGrantChallengeEl->adoptAsLastChild(IMessageHelper::createElementWithText("domains", info.mName));
          }

          return namespaceGrantChallengeEl;
        }
        
        //---------------------------------------------------------------------
        ElementPtr MessageHelper::createElement(const NamespaceInfo &info)
        {
          ElementPtr namespaceEl = Element::create("namespace");

          if (info.mURL.hasData()) {
            namespaceEl->setAttribute("id", info.mURL);
          }
          if (Time() != info.mLastUpdated) {
            namespaceEl->setAttribute("updated", IHelper::timeToString(info.mLastUpdated));
          }

          return namespaceEl;
        }

        //---------------------------------------------------------------------
        ElementPtr MessageHelper::createElement(const RolodexInfo &info)
        {
          ElementPtr lockboxEl = Element::create("rolodex");

          if (!info.mServerToken.isEmpty()) {
            lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithText("serverToken", info.mServerToken));
          }

          if (!info.mAccessToken.isEmpty()) {
            lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithText("accessToken", info.mAccessToken));
          }
          if (!info.mAccessSecret.isEmpty()) {
            lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithText("accessSecret", info.mAccessSecret));
          }
          if (Time() != info.mAccessSecretExpires) {
            lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("accessSecretExpires", IHelper::timeToString(info.mAccessSecretExpires)));
          }
          if (!info.mAccessSecretProof.isEmpty()) {
            lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithText("accessSecretProof", info.mAccessSecretProof));
          }
          if (Time() != info.mAccessSecretExpires) {
            lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("accessSecretProofExpires", IHelper::timeToString(info.mAccessSecretProofExpires)));
          }

          if (!info.mVersion.isEmpty()) {
            lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithText("version", info.mVersion));
          }
          if (Time() != info.mUpdateNext) {
            lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("updateNext", IHelper::timeToString(info.mUpdateNext)));
          }

          if (info.mRefreshFlag) {
            lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("refresh", "true"));
          }

          return lockboxEl;
        }
        
        //---------------------------------------------------------------------
        ElementPtr MessageHelper::createElement(
                                                const PublishToRelationshipsMap &relationships,
                                                const char *elementName
                                                )
        {
          ElementPtr rootEl = IMessageHelper::createElement(elementName);

          for (PublishToRelationshipsMap::const_iterator iter = relationships.begin(); iter != relationships.end(); ++iter)
          {
            String name = (*iter).first;
            const stack::IPublicationMetaData::PermissionAndPeerURIListPair &permission = (*iter).second;

            const char *permissionStr = "all";
            switch (permission.first) {
              case stack::IPublicationMetaData::Permission_All:     permissionStr = "all"; break;
              case stack::IPublicationMetaData::Permission_None:    permissionStr = "none"; break;
              case stack::IPublicationMetaData::Permission_Some:    permissionStr = "some"; break;
              case stack::IPublicationMetaData::Permission_Add:     permissionStr = "add"; break;
              case stack::IPublicationMetaData::Permission_Remove:  permissionStr = "remove"; break;
            }

            ElementPtr relationshipsEl = IMessageHelper::createElement("relationships");
            relationshipsEl->setAttribute("name", name);
            relationshipsEl->setAttribute("allow", permissionStr);

            for (stack::IPublicationMetaData::PeerURIList::const_iterator contactIter = permission.second.begin(); contactIter != permission.second.end(); ++contactIter)
            {
              ElementPtr contactEl = IMessageHelper::createElementWithTextAndJSONEncode("contact", (*contactIter));
              relationshipsEl->adoptAsLastChild(contactEl);
            }

            rootEl->adoptAsLastChild(relationshipsEl);
          }

          return rootEl;
        }

        //---------------------------------------------------------------------
        DocumentPtr MessageHelper::createDocument(
                                                  Message &msg,
                                                  IPublicationMetaDataPtr publicationMetaData,
                                                  ULONG *notifyPeerPublishMaxDocumentSizeInBytes,
                                                  IPublicationRepositoryPeerCachePtr peerCache
                                                  )
        {
          DocumentPtr ret = IMessageHelper::createDocumentWithRoot(msg);
          ElementPtr root = ret->getFirstChildElement();

          PublicationPtr publication = Publication::convert(publicationMetaData->toPublication());

          ULONG fromVersion = 0;
          ULONG toVersion = 0;

          if (publication) {
            fromVersion = publication->forMessages().getBaseVersion();
            toVersion = publication->forMessages().getVersion();
          } else {
            fromVersion = publicationMetaData->getBaseVersion();
            toVersion = publicationMetaData->getVersion();
          }

          // do not use the publication for these message types...
          switch ((MessageFactoryPeerCommon::Methods)msg.method()) {
            case MessageFactoryPeerCommon::Method_PeerPublish:
            {
              if (Message::MessageType_Request == msg.messageType()) {
                PeerPublishRequest &request = *(dynamic_cast<PeerPublishRequest *>(&msg));
                fromVersion = request.publishedFromVersion();
                toVersion = request.publishedToVersion();
              }
              if (Message::MessageType_Request != msg.messageType()) {
                // only the request can include a publication...
                publication.reset();
              }
              break;
            }
            case MessageFactoryPeerCommon::Method_PeerGet:
            {
              if (Message::MessageType_Result == msg.messageType()) {
                message::PeerGetResult &result = *(dynamic_cast<message::PeerGetResult *>(&msg));
                IPublicationMetaDataPtr originalMetaData = result.originalRequestPublicationMetaData();
                fromVersion = originalMetaData->getVersion();
                if (0 != fromVersion) {
                  // remote party already has this version so just return from that version onward...
                  ++fromVersion;
                }
                toVersion = 0;
              }

              if (Message::MessageType_Result != msg.messageType()) {
                // only the result can include a publication
                publication.reset();
              }
              break;
            }
            case MessageFactoryPeerCommon::Method_PeerDelete:
            case MessageFactoryPeerCommon::Method_PeerSubscribe:
            {
              // these messages requests/results will never include a publication (at least at this time)
              publication.reset();
              break;
            }
            case MessageFactoryPeerCommon::Method_PeerPublishNotify:
            {
              if (publication) {
                if (peerCache) {
                  ULONG bogusFillSize = 0;
                  ULONG &maxFillSize = (notifyPeerPublishMaxDocumentSizeInBytes ? *notifyPeerPublishMaxDocumentSizeInBytes : bogusFillSize);
                  if (peerCache->getNextVersionToNotifyAboutAndMarkNotified(publication, maxFillSize, fromVersion, toVersion)) {
                    break;
                  }
                }
              }
              publication.reset();
              fromVersion = 0;
              break;
            }
            default:                                  break;
          }

          // make a copy of the relationships
          PublishToRelationshipsMap relationships = publicationMetaData->getRelationships();

          ElementPtr docEl = IMessageHelper::createElement("document");
          ElementPtr detailsEl = IMessageHelper::createElement("details");
          ElementPtr publishToRelationshipsEl = MessageHelper::createElement(relationships, MessageFactoryPeerCommon::Method_PeerSubscribe == (MessageFactoryPeerCommon::Methods)msg.method() ? "subscribeToRelationships" : "publishToRelationships");
          ElementPtr dataEl = IMessageHelper::createElement("data");

          String creatorPeerURI;
          String creatorLocationID;

          LocationPtr creatorLocation = Location::convert(publicationMetaData->getCreatorLocation());
          if (creatorLocation ) {
            creatorLocationID = creatorLocation->forMessages().getLocationID();
            creatorPeerURI = creatorLocation->forMessages().getPeerURI();
          }

          ElementPtr contactEl = IMessageHelper::createElementWithTextAndJSONEncode("contact", creatorPeerURI);
          ElementPtr locationEl = IMessageHelper::createElementWithTextAndJSONEncode("location", creatorLocationID);

          NodePtr publishedDocEl;
          if (publication) {
            publishedDocEl = publication->forMessages().getDiffs(
                                                                 fromVersion,
                                                                 toVersion
                                                                 );

            if (0 == toVersion) {
              // put the version back to something more sensible
              toVersion = publicationMetaData->getVersion();
            }
          }

          ElementPtr nameEl = IMessageHelper::createElementWithText("name", publicationMetaData->getName());
          ElementPtr versionEl = IMessageHelper::createElementWithNumber("version", string(toVersion));
          ElementPtr baseVersionEl = IMessageHelper::createElementWithNumber("baseVersion", string(fromVersion));
          ElementPtr lineageEl = IMessageHelper::createElementWithNumber("lineage", string(publicationMetaData->getLineage()));
          ElementPtr chunkEl = IMessageHelper::createElementWithText("chunk", "1/1");

          ElementPtr expiresEl;
          if (publicationMetaData->getExpires() != Time()) {
            expiresEl = IMessageHelper::createElementWithNumber("expires", IHelper::timeToString(publicationMetaData->getExpires()));
          }

          ElementPtr mimeTypeEl = IMessageHelper::createElementWithText("mime", publicationMetaData->getMimeType());

          stack::IPublication::Encodings encoding = publicationMetaData->getEncoding();
          const char *encodingStr = "binary";
          switch (encoding) {
            case stack::IPublication::Encoding_Binary:  encodingStr = "binary"; break;
            case stack::IPublication::Encoding_JSON:    encodingStr = "json"; break;
          }

          ElementPtr encodingEl = IMessageHelper::createElementWithText("encoding", encodingStr);

          root->adoptAsLastChild(docEl);
          docEl->adoptAsLastChild(detailsEl);
          docEl->adoptAsLastChild(publishToRelationshipsEl);

          detailsEl->adoptAsLastChild(nameEl);
          detailsEl->adoptAsLastChild(versionEl);
          if (0 != fromVersion)
            detailsEl->adoptAsLastChild(baseVersionEl);
          detailsEl->adoptAsLastChild(lineageEl);
          detailsEl->adoptAsLastChild(chunkEl);
          detailsEl->adoptAsLastChild(contactEl);
          detailsEl->adoptAsLastChild(locationEl);
          if (expiresEl)
            detailsEl->adoptAsLastChild(expiresEl);
          detailsEl->adoptAsLastChild(mimeTypeEl);
          detailsEl->adoptAsLastChild(encodingEl);

          if (publishedDocEl) {
            dataEl->adoptAsLastChild(publishedDocEl);
            docEl->adoptAsLastChild(dataEl);
          }

          switch (msg.messageType()) {
            case Message::MessageType_Request:
            case Message::MessageType_Notify:   {
              switch ((MessageFactoryPeerCommon::Methods)msg.method()) {

                case MessageFactoryPeerCommon::Method_PeerPublish:         break;

                case MessageFactoryPeerCommon::Method_PeerGet:             {
                  if (baseVersionEl)
                    baseVersionEl->orphan();
                  if (expiresEl)
                    expiresEl->orphan();
                  mimeTypeEl->orphan();
                  encodingEl->orphan();
                  dataEl->orphan();
                  publishToRelationshipsEl->orphan();
                  break;
                }
                case MessageFactoryPeerCommon::Method_PeerDelete:          {
                  if (baseVersionEl)
                    baseVersionEl->orphan();
                  chunkEl->orphan();
                  contactEl->orphan();
                  locationEl->orphan();
                  if (expiresEl)
                    expiresEl->orphan();
                  mimeTypeEl->orphan();
                  encodingEl->orphan();
                  dataEl->orphan();
                  publishToRelationshipsEl->orphan();
                }
                case MessageFactoryPeerCommon::Method_PeerSubscribe:       {
                  versionEl->orphan();
                  if (baseVersionEl)
                    baseVersionEl->orphan();
                  lineageEl->orphan();
                  chunkEl->orphan();
                  contactEl->orphan();
                  locationEl->orphan();
                  if (expiresEl)
                    expiresEl->orphan();
                  mimeTypeEl->orphan();
                  encodingEl->orphan();
                  dataEl->orphan();
                  break;
                }
                case MessageFactoryPeerCommon::Method_PeerPublishNotify:
                {
                  if (!publication) {
                    if (baseVersionEl)
                      baseVersionEl->orphan();
                  }
                  chunkEl->orphan();
                  publishToRelationshipsEl->orphan();
                  break;
                }
                default:                                  break;
              }
              break;
            }
            case Message::MessageType_Result: {
              switch ((MessageFactoryPeerCommon::Methods)msg.method()) {
                case MessageFactoryPeerCommon::Method_PeerPublish:       {
                  if (baseVersionEl)
                    baseVersionEl->orphan();
                  locationEl->orphan();
                  contactEl->orphan();
                  dataEl->orphan();
                  break;
                }
                case MessageFactoryPeerCommon::Method_PeerGet:           {
                  break;
                }
                case MessageFactoryPeerCommon::Method_PeerDelete:        {
                  ZS_THROW_INVALID_USAGE("this method should not be used for delete result")
                  break;
                }
                case MessageFactoryPeerCommon::Method_PeerSubscribe:     {
                  versionEl->orphan();
                  if (baseVersionEl)
                    baseVersionEl->orphan();
                  lineageEl->orphan();
                  chunkEl->orphan();
                  contactEl->orphan();
                  locationEl->orphan();
                  if (expiresEl)
                    expiresEl->orphan();
                  mimeTypeEl->orphan();
                  encodingEl->orphan();
                  dataEl->orphan();
                  break;
                }
                case MessageFactoryPeerCommon::Method_PeerPublishNotify: {
                  ZS_THROW_INVALID_USAGE("this method should not be used for publication notify result")
                  break;
                }
                default:                                break;
              }
              break;
            }
            case Message::MessageType_Invalid:  break;
          }

          return ret;
        }


/*
<request xmlns="http://www.openpeer.com/openpeer/1.0/message" id=”abc123” method=”peer-publish”>

 <document>
  <details>
   <name>/openpeer.com/presence/1.0/bd520f1dbaa13c0cc9b7ff528e83470e/883fa7...9533609131</name>
   <version>12</version>
   <!-- <baseVersion>10</baseVersion> -->
   <lineage>5849943</lineage>
   <chunk>1/12</chunk>
   <scope>location</scope>
   <lifetime>session</lifetime>
   <expires>2002-01-20 23:59:59.000</expires>
   <mime>text/xml</mime>
   <encoding>xml</encoding>
  </details>
  <publishToRelationships>
   <relationships name=”/openpeer.com/authorization-list/1.0/whitelist” allow=”all” />
   <relationships name=”/openpeer.com/authorization-list/1.0/adhoc” allow=”all” />
   <relationships name=”/openpeer.com/shared-groups/1.0/foobar” allow=”all” />
  </publishToRelationships>
  <data>
   ...
  </data>
 </document>

</request>
*/

/*
<result xmlns="http://www.openpeer.com/openpeer/1.0/message" id=”abc123” method=”peer-publish” epoch=”13494934”>

 <document>
  <details>
   <name>/openpeer.com/presence/1.0/bd520f1dbaa13c0cc9b7ff528e83470e/883fa7...9533609131</name>
   <version>12</version>
   <lineage>5849943</lineage>
   <chunk>1/12</chunk>
   <scope>location</scope>
   <lifetime>session</lifetime>
   <expires>2002-01-20 23:59:59.000</expires>
   <mime>text/xml</mime>
   <encoding>xml</encoding>
  </details>
  <publishToRelationships>
   <relationships name=”/openpeer.com/authorization-list/1.0/whitelist” allow=”all” />
   <relationships name=”/openpeer.com/authorization-list/1.0/adhoc” allow=”all” />
   <relationships name=”/openpeer.com/shared-groups/1.0/foobar” allow=”all” />
  </publishToRelationships>
 </document>

</result>
*/

        /*
<request xmlns="http://www.openpeer.com/openpeer/1.0/message" id=”abc123” method=”peer-get”>

 <document>
  <details>
   <name>/openpeer.com/presence/1.0/bd520f1dbaa13c0cc9b7ff528e83470e/883fa7...9533609131</name>
   <version>12</version>
   <lineage>39239392</lineage>
   <scope>location</scope>
   <contact>peer://domain.com/ea00ede4405c99be9ae45739ebfe57d5<contact/>
   <location id=”524e609f337663bdbf54f7ef47d23ca9” />
   <chunk>1/1</chunk>
  </details>
 </document>

</request>
         */

/*
<result xmlns="http://www.openpeer.com/openpeer/1.0/message" id=”abc123” method=”peer-get” epoch=”13494934”>

 <document>
  <details>
   <name>/openpeer.com/presence/1.0/bd520f1dbaa13c0cc9b7ff528e83470e/883fa7...9533609131</name>
   <version>12</version>
   <!-- <baseVersion>10</baseVersion> -->
   <lineage>39239392</lineage>
   <chunk>1/10</chunk>
   <scope>location</scope>
   <contact>peer://domain.com/ea00ede4405c99be9ae45739ebfe57d5<contact/>
   <location id=”524e609f337663bdbf54f7ef47d23ca9” />
   <lifetime>session</lifetime>
   <expires>2002-01-20 23:59:59.000</expires>
   <mime>text/xml</mime>
   <encoding>xml</encoding>
  </details>
  <publishToRelationships>
   <relationships name=”/openpeer.com/authorization-list/1.0/whitelist” allow=”all” />
   <relationships name=”/openpeer.com/authorization-list/1.0/adhoc” allow=”all” />
   <relationships name=”/openpeer.com/shared-groups/1.0/foobar” allow=”all” />
  </publishToRelationships>
  <data>
   ...
  </data>
 </document>

</result>
 */

        /*
<request xmlns="http://www.openpeer.com/openpeer/1.0/message" id=”abc123” method=”peer-delete”>

 <document>
  <details>
   <name>/openpeer.com/presence/1.0/bd520f1dbaa13c0cc9b7ff528e83470e/883fa7...9533609131</name>
   <version>12</version>
   <lineage>39239392</lineage>
   <scope>location</scope>
  </details>
 </document>

</request>
         */

        /*
<request xmlns="http://www.openpeer.com/openpeer/1.0/message" id=”abc123” method=”peer-subscribe”>

 <document>
  <name>/openpeer.com/presence/1.0/</name>
  <subscribeToRelationships>
   <relationships name=”/openpeer.com/authorization-list/1.0/whitelist” subscribe=”all” />
   <relationships name=”/openpeer.com/authorization-list/1.0/adhoc” subscribe =”add”>
    <contact>peer://domain.com/bd520f1dbaa13c0cc9b7ff528e83470e</contact>
   </relationships>
   <relationships name=”/openpeer.com/shared-groups/1.0/foobar” subscribe =”all” />
  </subscribeToRelationships>
 </document>

</request>
         */

        /*
<result xmlns="http://www.openpeer.com/openpeer/1.0/message" id=”abc123” method=”peer-subscribe” epoch=”13494934”>

 <document>
   <name>/openpeer.com/presence/1.0/</name>
   <subscribeToRelationships>
    <relationships name=”/openpeer.com/authorization-list/1.0/whitelist” subscribe =”all” />
    <relationships name=”/openpeer.com/authorization-list/1.0/adhoc” subscribe =”some”>
     <contact>peer://domain.com/bd520f1dbaa13c0cc9b7ff528e83470e</contact>
     <contact>peer://domain.com/8d17a88e8d42ffbd138f3895ec45375c</contact>
    </relationships>
    <relationships name=”/openpeer.com/shared-groups/1.0/foobar” subscribe =”all” />
   </subscribeToRelationships>
 </document>

</result>
         */
        /*
<request xmlns="http://www.openpeer.com/openpeer/1.0/message" id=”abc123” method=”peer-publish-notify”>

 <documents>
  <document>
   <details>
    <name>/openpeer.com/presence/1.0/bd520f1dbaa13c0cc9b7ff528e83470e/883fa7...9533609131</name>
    <version>12</version>
    <lineage>43493943</lineage>
    <scope>location</scope>
    <contact>peer://domain.com/ea00ede4405c99be9ae45739ebfe57d5</contact>
    <location id=”524e609f337663bdbf54f7ef47d23ca9” />
    <lifetime>session</lifetime>
    <expires>49494393</expires>
    <mime>text/xml</mime>
    <encoding>xml</encoding>
   </details>
  </document>
  <!-- <data> ... </data> -->
 </documents>

</request>
         */

        //---------------------------------------------------------------------
        void MessageHelper::fillFrom(
                                     IMessageSourcePtr messageSource,
                                     MessagePtr msg,
                                     ElementPtr rootEl,
                                     IPublicationPtr &outPublication,
                                     IPublicationMetaDataPtr &outPublicationMetaData
                                     )
        {
          try {
            ElementPtr docEl = rootEl->findFirstChildElementChecked("document");
            ElementPtr detailsEl = docEl->findFirstChildElementChecked("details");

            ElementPtr nameEl = detailsEl->findFirstChildElementChecked("name");
            ElementPtr versionEl = detailsEl->findFirstChildElement("version");
            ElementPtr baseVersionEl = detailsEl->findFirstChildElement("baseVersion");
            ElementPtr lineageEl = detailsEl->findFirstChildElement("lineage");
            ElementPtr scopeEl = detailsEl->findFirstChildElement("scope");
            ElementPtr lifetimeEl = detailsEl->findFirstChildElement("lifetime");
            ElementPtr expiresEl = detailsEl->findFirstChildElement("expires");
            ElementPtr mimeTypeEl = detailsEl->findFirstChildElement("mime");
            ElementPtr encodingEl = detailsEl->findFirstChildElement("encoding");

            String contact;
            ElementPtr contactEl = detailsEl->findFirstChildElement("contact");
            if (contactEl) {
              contact = contactEl->getTextDecoded();
            }

            String locationID;
            ElementPtr locationEl = detailsEl->findFirstChildElement("location");
            if (locationEl) {
              locationID = locationEl->getTextDecoded();
            }

            LocationPtr location = ILocationForMessages::create(messageSource, contact, locationID);

            ElementPtr dataEl = docEl->findFirstChildElement("data");

            ULONG version = 0;
            if (versionEl) {
              String versionStr = versionEl->getText();
              try {
                version = Numeric<ULONG>(versionStr);
              } catch(Numeric<ULONG>::ValueOutOfRange &) {
              }
            }

            ULONG baseVersion = 0;
            if (baseVersionEl) {
              String baseVersionStr = baseVersionEl->getText();
              try {
                baseVersion = Numeric<ULONG>(baseVersionStr);
              } catch(Numeric<ULONG>::ValueOutOfRange &) {
              }
            }

            ULONG lineage = 0;
            if (lineageEl) {
              String lineageStr = lineageEl->getText();
              try {
                lineage = Numeric<ULONG>(lineageStr);
              } catch(Numeric<ULONG>::ValueOutOfRange &) {
              }
            }

            IPublicationMetaData::Encodings encoding = IPublicationMetaData::Encoding_Binary;
            if (encodingEl) {
              String encodingStr = encodingEl->getText();
              if (encodingStr == "json") encoding = IPublicationMetaData::Encoding_JSON;
              else if (encodingStr == "binary") encoding = IPublicationMetaData::Encoding_Binary;
            }

            Time expires;
            if (expiresEl) {
              String expiresStr = expiresEl->getText();
              expires = IHelper::stringToTime(expiresStr);
            }

            String mimeType;
            if (mimeTypeEl) {
              mimeType = mimeTypeEl->getText();
            }

            IPublicationMetaData::PublishToRelationshipsMap relationships;

            ElementPtr publishToRelationshipsEl = docEl->findFirstChildElement(MessageFactoryPeerCommon::Method_PeerSubscribe == (MessageFactoryPeerCommon::Methods)msg->method() ? "subscribeToRelationships" : "publishToRelationships");
            if (publishToRelationshipsEl) {
              ElementPtr relationshipsEl = publishToRelationshipsEl->findFirstChildElement("relationships");
              while (relationshipsEl)
              {
                String name = relationshipsEl->getAttributeValue("name");
                String allowStr = relationshipsEl->getAttributeValue("allow");

                IPublicationMetaData::PeerURIList contacts;
                ElementPtr contactEl = relationshipsEl->findFirstChildElement("contact");
                while (contactEl)
                {
                  String contact = contactEl->getTextDecoded();
                  if (contact.size() > 0) {
                    contacts.push_back(contact);
                  }
                  contactEl = contactEl->findNextSiblingElement("contact");
                }

                IPublicationMetaData::Permissions permission = IPublicationMetaData::Permission_All;
                if (allowStr == "all") permission = IPublicationMetaData::Permission_All;
                else if (allowStr == "none") permission = IPublicationMetaData::Permission_None;
                else if (allowStr == "some") permission = IPublicationMetaData::Permission_Some;
                else if (allowStr == "add") permission = IPublicationMetaData::Permission_Add;
                else if (allowStr == "remove") permission = IPublicationMetaData::Permission_Remove;

                if (name.size() > 0) {
                  relationships[name] = IPublicationMetaData::PermissionAndPeerURIListPair(permission, contacts);
                }

                relationshipsEl = relationshipsEl->findNextSiblingElement("relationships");
              }
            }

            bool hasPublication = false;

            switch (msg->messageType()) {
              case Message::MessageType_Request:
              case Message::MessageType_Notify:   {
                switch ((MessageFactoryPeerCommon::Methods)msg->method()) {
                  case MessageFactoryPeerCommon::Method_PeerPublish:
                  case MessageFactoryPeerCommon::Method_PeerPublishNotify: {
                    hasPublication = true;
                    break;
                  }
                  case MessageFactoryPeerCommon::Method_PeerGet:
                  case MessageFactoryPeerCommon::Method_PeerDelete:
                  case MessageFactoryPeerCommon::Method_PeerSubscribe: {
                    hasPublication = false;
                  }
                  default: break;
                }
                break;
              }
              case Message::MessageType_Result:   {
                switch ((MessageFactoryPeerCommon::Methods)msg->method()) {
                  case MessageFactoryPeerCommon::Method_PeerPublish:
                  case MessageFactoryPeerCommon::Method_PeerSubscribe: {
                    hasPublication = false;
                  }
                  case MessageFactoryPeerCommon::Method_PeerGet:
                  {
                    hasPublication = true;
                    break;
                  }
                  default: break;
                }
                break;
              }
              case Message::MessageType_Invalid:  break;
            }

            if (!dataEl) {
              hasPublication = false;
            }

            if (hasPublication) {
              PublicationPtr publication = IPublicationForMessages::create(
                                                                           version,
                                                                           baseVersion,
                                                                           lineage,
                                                                           location,
                                                                           nameEl->getText(),
                                                                           mimeType,
                                                                           dataEl,
                                                                           encoding,
                                                                           relationships,
                                                                           location,
                                                                           expires
                                                                           );
              outPublicationMetaData = publication->forMessages().toPublicationMetaData();
              outPublication = publication;
            } else {
              PublicationMetaDataPtr metaData = IPublicationMetaDataForMessages::create(
                                                                                        version,
                                                                                        baseVersion,
                                                                                        lineage,
                                                                                        location,
                                                                                        nameEl->getText(),
                                                                                        mimeType,
                                                                                        encoding,
                                                                                        relationships,
                                                                                        location,
                                                                                        expires
                                                                                        );
              outPublicationMetaData = metaData;
            }
          } catch (CheckFailed &) {
          }
        }

        //---------------------------------------------------------------------
        int MessageHelper::stringToInt(const String &s)
        {
          if (s.isEmpty()) return 0;

          try {
            return Numeric<int>(s);
          } catch (Numeric<int>::ValueOutOfRange &) {
            ZS_LOG_WARNING(Detail, "unable to convert value to int, value=" + s)
          }
          return 0;
        }


        //---------------------------------------------------------------------
        UINT MessageHelper::stringToUint(const String &s)
        {
          if (s.isEmpty()) return 0;

          try {
            return Numeric<UINT>(s);
          } catch (Numeric<UINT>::ValueOutOfRange &) {
            ZS_LOG_WARNING(Detail, "unable to convert value to unsigned int, value=" + s)
          }
          return 0;
        }

        //---------------------------------------------------------------------
        WORD MessageHelper::getErrorCode(ElementPtr root)
        {
          if (!root) return 0;

          ElementPtr errorEl = root->findFirstChildElement("error");
          if (!errorEl) return 0;

          String ec = IMessageHelper::getAttributeID(errorEl);
          if (ec.isEmpty()) return 0;

          try {
            return (Numeric<WORD>(ec));
          } catch(Numeric<WORD>::ValueOutOfRange &) {
            ZS_LOG_WARNING(Detail, "unable to convert value to error code, value=" + ec)
          }
          return 0;
        }

        //---------------------------------------------------------------------
        String MessageHelper::getErrorReason(ElementPtr root)
        {
          if (!root) return String();

          ElementPtr errorEl = root->findFirstChildElement("error");
          if (!errorEl) return String();

          return IMessageHelper::getElementText(errorEl);
        }

        //---------------------------------------------------------------------
        LocationInfo MessageHelper::createLocation(
                                                   ElementPtr elem,
                                                   IMessageSourcePtr messageSource,
                                                   const char *encryptionPassphrase
                                                   )
        {
          LocationInfo ret;
          if (!elem) return ret;

          String id = IMessageHelper::getAttributeID(elem);

          ElementPtr contact = elem->findFirstChildElement("contact");
          if (contact)
          {
            String peerURI = IMessageHelper::getElementTextAndDecode(contact);
            ret.mLocation = ILocationForMessages::create(messageSource, peerURI, id);
          }

          ElementPtr candidates = elem->findFirstChildElement("candidates");
          if (candidates)
          {
            CandidateList candidateLst;
            ElementPtr candidate = candidates->findFirstChildElement("candidate");
            while (candidate)
            {
              Candidate c = MessageHelper::createCandidate(candidate, encryptionPassphrase);
              candidateLst.push_back(c);

              candidate = candidate->getNextSiblingElement();
            }

            if (candidateLst.size() > 0)
              ret.mCandidates = candidateLst;
          }

          if (elem->getValue() == "location")
            elem = elem->findFirstChildElement("details");

          if (elem)
          {
            ElementPtr device = elem->findFirstChildElement("device");
            ElementPtr ip = elem->findFirstChildElement("ip");
            ElementPtr ua = elem->findFirstChildElement("userAgent");
            ElementPtr os = elem->findFirstChildElement("os");
            ElementPtr system = elem->findFirstChildElement("system");
            ElementPtr host = elem->findFirstChildElement("host");
            if (device) {
              ret.mDeviceID = IMessageHelper::getAttribute(device, "id");
            }
            if (ip) {
              IPAddress ipOriginal(IMessageHelper::getElementText(ip), 0);

              ret.mIPAddress.mIPAddress = ipOriginal.mIPAddress;
            }
            if (ua) ret.mUserAgent = IMessageHelper::getElementTextAndDecode(ua);
            if (os) ret.mOS = IMessageHelper::getElementTextAndDecode(os);
            if (system) ret.mSystem = IMessageHelper::getElementTextAndDecode(system);
            if (host) ret.mHost = IMessageHelper::getElementTextAndDecode(host);
          }
          return ret;
        }

        //---------------------------------------------------------------------
        Candidate MessageHelper::createCandidate(
                                                 ElementPtr elem,
                                                 const char *encryptionPassphrase
                                                 )
        {
          Candidate ret;
          if (!elem) return ret;

          ElementPtr namespaceEl = elem->findFirstChildElement("namespace");
          ElementPtr transportEl = elem->findFirstChildElement("transport");
          ElementPtr typeEl = elem->findFirstChildElement("type");
          ElementPtr foundationEl = elem->findFirstChildElement("foundation");
          ElementPtr componentEl = elem->findFirstChildElement("component");
          ElementPtr hostEl = elem->findFirstChildElement("host");
          ElementPtr ipEl = elem->findFirstChildElement("ip");
          ElementPtr portEl = elem->findFirstChildElement("port");
          ElementPtr priorityEl = elem->findFirstChildElement("priority");
          ElementPtr accessTokenEl = elem->findFirstChildElement("accessToken");
          ElementPtr accessSecretProofEncryptedEl = elem->findFirstChildElement("accessSecretProofEncrypted");
          ElementPtr relatedEl = elem->findFirstChildElement("related");
          ElementPtr relatedIPEl;
          ElementPtr relatedPortEl;
          if (relatedEl) {
            relatedIPEl = relatedEl->findFirstChildElement("ip");
            relatedPortEl = relatedEl->findFirstChildElement("port");
          }

          ret.mNamespace = IMessageHelper::getElementTextAndDecode(namespaceEl);
          ret.mTransport = IMessageHelper::getElementTextAndDecode(transportEl);

          String type = IMessageHelper::getElementTextAndDecode(typeEl);
          if ("host" == type) {
            ret.mType = IICESocket::Type_Local;
          } else if ("srflx" == type) {
            ret.mType = IICESocket::Type_ServerReflexive;
          } else if ("prflx" == type) {
            ret.mType = IICESocket::Type_PeerReflexive;
          } else if ("relay" == type) {
            ret.mType = IICESocket::Type_Relayed;
          }

          ret.mFoundation = IMessageHelper::getElementTextAndDecode(foundationEl);

          if (componentEl) {
            try {
              ret.mComponentID = Numeric<typeof(ret.mComponentID)>(IMessageHelper::getElementText(componentEl));
            } catch(Numeric<typeof(ret.mComponentID)>::ValueOutOfRange &) {
            }
          }

          if ((ipEl) ||
              (hostEl))
          {
            WORD port = 0;
            if (portEl) {
              try {
                port = Numeric<WORD>(IMessageHelper::getElementText(portEl));
              } catch(Numeric<WORD>::ValueOutOfRange &) {
              }
            }

            if (ipEl)
              ret.mIPAddress = IPAddress(IMessageHelper::getElementText(ipEl), port);
            if (hostEl)
              ret.mIPAddress = IPAddress(IMessageHelper::getElementText(hostEl), port);

            ret.mIPAddress.setPort(port);
          }

          if (priorityEl) {
            try {
              ret.mPriority = Numeric<typeof(ret.mPriority)>(IMessageHelper::getElementText(priorityEl));
            } catch(Numeric<DWORD>::ValueOutOfRange &) {
            }
          }

          if (relatedIPEl) {
            WORD port = 0;
            if (relatedPortEl) {
              try {
                port = Numeric<WORD>(IMessageHelper::getElementText(relatedPortEl));
              } catch(Numeric<WORD>::ValueOutOfRange &) {
              }
            }

            ret.mRelatedIP = IPAddress(IMessageHelper::getElementText(ipEl), port);
            ret.mRelatedIP.setPort(port);
          }

          ret.mAccessToken = IMessageHelper::getElementTextAndDecode(accessTokenEl);

          String accessSecretProofEncrypted = IMessageHelper::getElementTextAndDecode(accessSecretProofEncryptedEl);
          if ((accessSecretProofEncrypted.hasData()) &&
              (encryptionPassphrase)) {

            SecureByteBlockPtr accessSeretProof = stack::IHelper::splitDecrypt(*IHelper::hash(encryptionPassphrase, IHelper::HashAlgorthm_SHA256), accessSecretProofEncrypted);
            if (accessSeretProof) {
              ret.mAccessSecretProof = IHelper::convertToString(*accessSeretProof);
            }
          }

          return ret;
        }

        //---------------------------------------------------------------------
        Finder MessageHelper::createFinder(ElementPtr elem)
        {
          Finder ret;
          if (!elem) return ret;

          ret.mID = IMessageHelper::getAttributeID(elem);

          ElementPtr protocolsEl = elem->findFirstChildElement("protocols");
          if (protocolsEl) {
            ElementPtr protocolEl = protocolsEl->findFirstChildElement("protocol");
            while (protocolEl) {
              Finder::Protocol protocol;
              protocol.mTransport = IMessageHelper::getElementText(protocolEl->findFirstChildElement("transport"));
              protocol.mHost = IMessageHelper::getElementText(protocolEl->findFirstChildElement("host"));

              if ((protocol.mTransport.hasData()) ||
                  (protocol.mHost.hasData())) {
                ret.mProtocols.push_back(protocol);
              }

              protocolEl = protocolEl->findNextSiblingElement("protocol");
            }
          }

          ret.mRegion = IMessageHelper::getElementText(elem->findFirstChildElement("region"));
          ret.mCreated = IHelper::stringToTime(IMessageHelper::getElementText(elem->findFirstChildElement("created")));
          ret.mExpires = IHelper::stringToTime(IMessageHelper::getElementText(elem->findFirstChildElement("expires")));

          try
          {
            ret.mPublicKey = IRSAPublicKey::load(*IHelper::convertFromBase64(IMessageHelper::getElementText(elem->findFirstChildElementChecked("key")->findFirstChildElementChecked("x509Data"))));
            try {
              ret.mPriority = Numeric<WORD>(IMessageHelper::getElementText(elem->findFirstChildElementChecked("priority")));
            } catch(Numeric<WORD>::ValueOutOfRange &) {
            }
            try {
              ret.mWeight = Numeric<WORD>(IMessageHelper::getElementText(elem->findFirstChildElementChecked("weight")));
            } catch(Numeric<WORD>::ValueOutOfRange &) {
            }
          }
          catch(CheckFailed &) {
            ZS_LOG_BASIC("createFinder XML check failure")
          }

          return ret;
        }

        //---------------------------------------------------------------------
        Service MessageHelper::createService(ElementPtr serviceEl)
        {
          Service service;

          if (!serviceEl) return service;

          service.mID = IMessageHelper::getAttributeID(serviceEl);
          service.mType = IMessageHelper::getElementText(serviceEl->findFirstChildElement("type"));
          service.mVersion = IMessageHelper::getElementText(serviceEl->findFirstChildElement("version"));

          ElementPtr methodsEl = serviceEl->findFirstChildElement("methods");
          if (methodsEl) {
            ElementPtr methodEl = methodsEl->findFirstChildElement("method");
            while (methodEl) {
              Service::Method method;
              method.mName = IMessageHelper::getElementText(methodEl->findFirstChildElement("name"));

              String uri = IMessageHelper::getElementText(methodEl->findFirstChildElement("uri"));
              String host = IMessageHelper::getElementText(methodEl->findFirstChildElement("host"));

              method.mURI = (host.hasData() ? host : uri);
              method.mUsername = IMessageHelper::getElementText(methodEl->findFirstChildElement("username"));
              method.mPassword = IMessageHelper::getElementText(methodEl->findFirstChildElement("password"));

              if (method.hasData()) {
                service.mMethods[method.mName] = method;
              }

              methodEl = methodEl->findNextSiblingElement("method");
            }
          }
          return service;
        }

        //---------------------------------------------------------------------
        IdentityInfo MessageHelper::createIdentity(ElementPtr elem)
        {
          IdentityInfo info;

          if (!elem) return info;

          info.mDisposition = IdentityInfo::toDisposition(elem->getAttributeValue("disposition"));

          info.mAccessToken = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessToken"));
          info.mAccessSecret = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecret"));
          info.mAccessSecretExpires = IHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecretExpires")));
          info.mAccessSecretProof = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecretProof"));
          info.mAccessSecretProofExpires = IHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecretProofExpires")));

          info.mReloginKey = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("reloginKey"));

          info.mBase = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("base"));
          info.mURI = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("uri"));
          info.mProvider = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("provider"));

          info.mStableID = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("stableID"));
          ElementPtr peerEl = elem->findFirstChildElement("peer");
          if (peerEl) {
            info.mPeerFilePublic = IPeerFilePublic::loadFromElement(peerEl);
          }

          try {
            info.mPriority = Numeric<WORD>(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("priority")));
          } catch(Numeric<WORD>::ValueOutOfRange &) {
          }
          try {
            info.mWeight = Numeric<WORD>(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("weight")));
          } catch(Numeric<WORD>::ValueOutOfRange &) {
          }

          info.mUpdated = IHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("created")));
          info.mUpdated = IHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("updated")));
          info.mExpires = IHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("expires")));

          info.mName = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("name"));
          info.mProfile = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("profile"));
          info.mVProfile = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("vprofile"));

          ElementPtr avatarsEl = elem->findFirstChildElement("avatars");
          if (avatarsEl) {
            ElementPtr avatarEl = elem->findFirstChildElement("avatar");
            while (avatarEl) {
              IdentityInfo::Avatar avatar;
              avatar.mName = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("name"));
              avatar.mURL = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("url"));
              try {
                avatar.mWidth = Numeric<int>(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("width")));
              } catch(Numeric<int>::ValueOutOfRange &) {
              }
              try {
                avatar.mHeight = Numeric<int>(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("height")));
              } catch(Numeric<int>::ValueOutOfRange &) {
              }

              if (avatar.hasData()) {
                info.mAvatars.push_back(avatar);
              }
              avatarEl = elem->findNextSiblingElement("avatar");
            }
          }

          ElementPtr contactProofBundleEl = elem->findFirstChildElement("contactProofBundle");
          if (contactProofBundleEl) {
            info.mContactProofBundle = contactProofBundleEl->clone()->toElement();
          }
          ElementPtr identityProofBundleEl = elem->findFirstChildElement("identityProofBundle");
          if (contactProofBundleEl) {
            info.mIdentityProofBundle = identityProofBundleEl->clone()->toElement();
          }

          return info;
        }

        //---------------------------------------------------------------------
        LockboxInfo MessageHelper::createLockbox(ElementPtr elem)
        {
          LockboxInfo info;

          if (!elem) return info;

          info.mDomain = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("domain"));
          info.mAccountID = MessageHelper::getAttributeID(elem);
          info.mAccessToken = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessToken"));
          info.mAccessSecret = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecret"));
          info.mAccessSecretExpires = IHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecretExpires")));
          info.mAccessSecretProof = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecretProof"));
          info.mAccessSecretProofExpires = IHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecretProofExpires")));

          String key = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("key"));

          if (key.hasData()) {
            info.mKey = IHelper::convertFromBase64(key);
          }
          info.mHash = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("hash"));

          try {
            info.mResetFlag = Numeric<bool>(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("reset")));
          } catch(Numeric<bool>::ValueOutOfRange &) {
          }

          return info;
        }

        //---------------------------------------------------------------------
        AgentInfo MessageHelper::createAgent(ElementPtr elem)
        {
          AgentInfo info;

          if (!elem) return info;

          info.mUserAgent = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("userAgent"));
          info.mName = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("name"));
          info.mImageURL = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("image"));
          info.mAgentURL = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("url"));

          return info;
        }

        //---------------------------------------------------------------------
        NamespaceGrantChallengeInfo MessageHelper::createNamespaceGrantChallenge(ElementPtr elem)
        {
          NamespaceGrantChallengeInfo info;

          if (!elem) return info;

          info.mID = IMessageHelper::getAttributeID(elem);
          info.mName = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("name"));
          info.mImageURL = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("image"));
          info.mServiceURL = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("url"));
          info.mDomains = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("domains"));

          return info;
        }

        //---------------------------------------------------------------------
        RolodexInfo MessageHelper::createRolodex(ElementPtr elem)
        {
          RolodexInfo info;

          if (!elem) return info;

          info.mServerToken = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("serverToken"));

          info.mAccessToken = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessToken"));
          info.mAccessSecret = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecret"));
          info.mAccessSecretExpires = IHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecretExpires")));
          info.mAccessSecretProof = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecretProof"));
          info.mAccessSecretProofExpires = IHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecretProofExpires")));

          info.mVersion = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("version"));
          info.mUpdateNext = IHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("updateNext")));

          try {
            info.mRefreshFlag = Numeric<bool>(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("refresh")));
          } catch(Numeric<bool>::ValueOutOfRange &) {
          }
          
          return info;
        }
        
      }
    }
  }
}

