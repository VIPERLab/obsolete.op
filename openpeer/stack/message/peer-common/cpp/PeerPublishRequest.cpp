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

#include <hookflash/stack/message/peer-common/PeerPublishRequest.h>
#include <hookflash/stack/message/internal/stack_message_MessageHelper.h>
#include <hookflash/stack/IPublication.h>

#include <zsLib/Stringize.h>

namespace hookflash
{
  namespace stack
  {
    namespace message
    {
      namespace peer_common
      {
        //---------------------------------------------------------------------
        PeerPublishRequestPtr PeerPublishRequest::convert(MessagePtr message)
        {
          return boost::dynamic_pointer_cast<PeerPublishRequest>(message);
        }

        //---------------------------------------------------------------------
        PeerPublishRequest::PeerPublishRequest()
        {
        }

        //---------------------------------------------------------------------
        PeerPublishRequestPtr PeerPublishRequest::create()
        {
          PeerPublishRequestPtr ret(new PeerPublishRequest);
          return ret;
        }

        //---------------------------------------------------------------------
        PeerPublishRequestPtr PeerPublishRequest::create(
                                                         ElementPtr root,
                                                         IMessageSourcePtr messageSource
                                                         )
        {
          PeerPublishRequestPtr ret(new PeerPublishRequest);
          IMessageHelper::fill(*ret, root, messageSource);

          IPublicationMetaDataPtr metaData;
          internal::MessageHelper::fillFrom(messageSource, ret, root, ret->mPublication, metaData);

          if (!ret->mPublication) return ret;

          ret->mPublishedFromVersion = ret->mPublication->getBaseVersion();
          ret->mPublishedToVersion = ret->mPublication->getVersion();

          return ret;
        }

        //---------------------------------------------------------------------
        DocumentPtr PeerPublishRequest::encode()
        {
          return internal::MessageHelper::createDocument(*this, mPublication);
        }

        //---------------------------------------------------------------------
        bool PeerPublishRequest::hasAttribute(AttributeTypes type) const
        {
          switch (type) {
            case AttributeType_Publication:       return mPublication;
          }
          return false;
        }

      }
    }
  }
}
