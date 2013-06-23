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

#include <hookflash/stack/message/peer-finder/PeerLocationFindReply.h>
#include <hookflash/stack/message/peer-finder/PeerLocationFindRequest.h>
#include <hookflash/stack/message/internal/stack_message_MessageHelper.h>
#include <hookflash/stack/internal/stack_Location.h>
#include <hookflash/stack/internal/stack_Peer.h>
#include <hookflash/stack/IPeerFiles.h>
#include <hookflash/stack/IPeerFilePublic.h>
#include <hookflash/stack/IPeerFilePrivate.h>

#include <zsLib/XML.h>
#include <zsLib/helpers.h>

namespace hookflash { namespace stack { namespace message { ZS_DECLARE_SUBSYSTEM(hookflash_stack_message) } } }

namespace hookflash
{
  namespace stack
  {
    namespace message
    {
      namespace peer_finder
      {
        typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

        using message::internal::MessageHelper;

        using namespace stack::internal;

        //---------------------------------------------------------------------
        PeerLocationFindReplyPtr PeerLocationFindReply::convert(MessagePtr message)
        {
          return boost::dynamic_pointer_cast<PeerLocationFindReply>(message);
        }

        //---------------------------------------------------------------------
        PeerLocationFindReply::PeerLocationFindReply()
        {
        }

        //---------------------------------------------------------------------
        PeerLocationFindReplyPtr PeerLocationFindReply::create(
                                                               ElementPtr root,
                                                               IMessageSourcePtr messageSource
                                                               )
        {
          PeerLocationFindReplyPtr ret(new PeerLocationFindReply);
          IMessageHelper::fill(*ret, root, messageSource);

          try {

            ElementPtr findProofEl = root->findFirstChildElementChecked("findProofBundle")->findFirstChildElementChecked("findProof");

            ElementPtr locationEl = findProofEl->findFirstChildElement("location");
            if (locationEl) {
              ret->mLocationInfo = internal::MessageHelper::createLocation(locationEl, messageSource, (ret->mPeerSecret).get());
            }

            if (!ret->mLocationInfo.mLocation) {
              ZS_LOG_ERROR(Detail, "PeerLocationFindReply [] missing location information in find request")
              return PeerLocationFindReplyPtr();
            }

            PeerPtr peer = Location::convert(ret->mLocationInfo.mLocation)->forMessages().getPeer();

            if (!peer) {
              ZS_LOG_WARNING(Detail, "PeerLocationFindReply [] expected element is missing")
              return PeerLocationFindReplyPtr();
            }

            if (!peer->forMessages().verifySignature(findProofEl)) {
              ZS_LOG_WARNING(Detail, "PeerLocationFindReply [] could not validate signature of find proof request")
              return PeerLocationFindReplyPtr();
            }

            ret->mRequestfindProofBundleDigestValue = findProofEl->findFirstChildElementChecked("requestFindProofBundleDigestValue")->getText();

            ElementPtr routes = root->findFirstChildElement("routes");
            if (routes)
            {
              RouteList routeLst;
              ElementPtr route = routes->findFirstChildElement("route");
              while (route)
              {
                String id = IMessageHelper::getAttributeID(route);
                routeLst.push_back(id);

                route = route->getNextSiblingElement();
              }

              if (routeLst.size() > 0)
                ret->mRoutes = routeLst;
            }
          } catch(CheckFailed &) {
            ZS_LOG_WARNING(Detail, "PeerLocationFindReply [] expected element is missing")
            return PeerLocationFindReplyPtr();
          }

          return ret;
        }

        //---------------------------------------------------------------------
        PeerLocationFindReplyPtr PeerLocationFindReply::create(PeerLocationFindRequestPtr request)
        {
          PeerLocationFindReplyPtr ret(new PeerLocationFindReply);

          ret->mDomain = request->domain();
          ret->mID = request->messageID();

          if (request->hasAttribute(PeerLocationFindRequest::AttributeType_PeerSecret)) {
            ret->mPeerSecret = request->peerSecret();
          }
          if (request->hasAttribute(PeerLocationFindRequest::AttributeType_RequestfindProofBundleDigestValue)) {
            ret->mRequestfindProofBundleDigestValue = request->mRequestfindProofBundleDigestValue;
          }
          if (request->hasAttribute(PeerLocationFindRequest::AttributeType_Routes)) {
            ret->mRoutes = request->mRoutes;
          }

          return ret;
        }

        //---------------------------------------------------------------------
        bool PeerLocationFindReply::hasAttribute(AttributeTypes type) const
        {
          switch (type)
          {
            case AttributeType_RequestfindProofBundleDigestValue: return mRequestfindProofBundleDigestValue.hasData();
            case AttributeType_PeerSecret:                        return mPeerSecret;
            case AttributeType_LocationInfo:                      return mLocationInfo.hasData();
            case AttributeType_Routes:                            return mRoutes.size() > 0;
            case AttributeType_PeerFiles:                         return mPeerFiles;
            default:
              break;
          }
          return MessageReply::hasAttribute((MessageReply::AttributeTypes)type);
        }

        //---------------------------------------------------------------------
        DocumentPtr PeerLocationFindReply::encode()
        {
          DocumentPtr ret = IMessageHelper::createDocumentWithRoot(*this);
          ElementPtr root = ret->getFirstChildElement();

          if (!mPeerFiles) {
            ZS_LOG_ERROR(Detail, "PeerLocationFindRequest [] peer files was null")
            return DocumentPtr();
          }

          IPeerFilePrivatePtr peerFilePrivate = mPeerFiles->getPeerFilePrivate();
          if (!peerFilePrivate) {
            ZS_LOG_ERROR(Detail, "PeerLocationFindRequest [] peer file private was null")
            return DocumentPtr();
          }
          IPeerFilePublicPtr peerFilePublic = mPeerFiles->getPeerFilePublic();
          if (!peerFilePublic) {
            ZS_LOG_ERROR(Detail, "PeerLocationFindRequest [] peer file public was null")
            return DocumentPtr();
          }


          ElementPtr findProofBundleEl = Element::create("findProofBundle");
          ElementPtr findProofEl = Element::create("findProof");

          if (hasAttribute(AttributeType_RequestfindProofBundleDigestValue))
          {
            findProofEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("requestFindProofBundleDigestValue", mRequestfindProofBundleDigestValue));
          }

          if (hasAttribute(AttributeType_LocationInfo))
          {
            findProofEl->adoptAsLastChild(MessageHelper::createElement(mLocationInfo, mPeerSecret.get()));
          }

          findProofBundleEl->adoptAsLastChild(findProofEl);
          peerFilePrivate->signElement(findProofEl);
          root->adoptAsLastChild(findProofBundleEl);

          if (hasAttribute(AttributeType_Routes))
          {
            ElementPtr routesEl = IMessageHelper::createElement("routes");
            root->adoptAsLastChild(routesEl);

            for (RouteList::const_iterator it = mRoutes.begin(); it != mRoutes.end(); ++it)
            {
              const String &routeID = (*it);
              routesEl->adoptAsLastChild(IMessageHelper::createElementWithID("route", routeID));
            }
          }

          return ret;
        }

      }
    }
  }
}
