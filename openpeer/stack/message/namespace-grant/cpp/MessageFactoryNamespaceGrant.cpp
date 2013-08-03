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

#include <openpeer/stack/message/namespace-grant/MessageFactoryNamespaceGrant.h>
#include <openpeer/stack/message/namespace-grant/NamespaceGrantWindowRequest.h>
#include <openpeer/stack/message/namespace-grant/NamespaceGrantCompleteNotify.h>
#include <openpeer/stack/message/Message.h>
#include <openpeer/stack/message/IMessageFactoryManager.h>

#include <openpeer/stack/message/internal/stack_message_MessageHelper.h>

#include <openpeer/stack/IHelper.h>

#define OPENPEER_STACK_MESSAGE_MESSAGE_FACTORY_NAMESPACE_GRANT_HANDLER "namespace-grant"


namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      namespace namespace_grant
      {
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageFactoryNamespaceGrant
        #pragma mark

        //---------------------------------------------------------------------
        MessageFactoryNamespaceGrantPtr MessageFactoryNamespaceGrant::create()
        {
          MessageFactoryNamespaceGrantPtr pThis(new MessageFactoryNamespaceGrant);
          IMessageFactoryManager::registerFactory(pThis);
          return pThis;
        }

        //---------------------------------------------------------------------
        MessageFactoryNamespaceGrantPtr MessageFactoryNamespaceGrant::singleton()
        {
          static MessageFactoryNamespaceGrantPtr pThis = create();
          return pThis;
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageFactoryNamespaceGrant => IMessageFactory
        #pragma mark

        //---------------------------------------------------------------------
        const char *MessageFactoryNamespaceGrant::getHandler() const
        {
          return OPENPEER_STACK_MESSAGE_MESSAGE_FACTORY_NAMESPACE_GRANT_HANDLER;
        }

        //---------------------------------------------------------------------
        Message::Methods MessageFactoryNamespaceGrant::toMethod(const char *inMethod) const
        {
          typedef zsLib::ULONG ULONG;
          String methodStr(inMethod ? inMethod : "");

          for (ULONG loop = (ULONG)(MessageFactoryNamespaceGrant::Method_Invalid+1); loop <= ((ULONG)MessageFactoryNamespaceGrant::Method_Last); ++loop)
          {
            if (methodStr == toString((Message::Methods)loop)) {
              return (Message::Methods)loop;
            }
          }
          return Message::Method_Invalid;
        }

        //---------------------------------------------------------------------
        const char *MessageFactoryNamespaceGrant::toString(Message::Methods method) const
        {
          switch ((MessageFactoryNamespaceGrant::Methods)method)
          {
            case Method_Invalid:                            return "";

            case Method_NamespaceGrantWindow:               return "namespace-grant-window";
            case Method_NamespaceGrantStart:                return "namespace-grant-start";
            case Method_NamespaceGrantComplete:             return "namespace-grant-complete";
          }
          return "";
        }

        //---------------------------------------------------------------------
        MessagePtr MessageFactoryNamespaceGrant::create(
                                                        ElementPtr root,
                                                        IMessageSourcePtr messageSource
                                                        )
        {
          if (!root) return MessagePtr();

          Message::MessageTypes msgType = IMessageHelper::getMessageType(root);
          Methods msgMethod = (MessageFactoryNamespaceGrant::Methods)toMethod(IMessageHelper::getAttribute(root, "method"));

          switch (msgType) {
            case Message::MessageType_Invalid:                  return MessagePtr();

            case Message::MessageType_Request:
            {
              switch (msgMethod) {
                case Method_Invalid:                            return MessagePtr();

                case Method_NamespaceGrantWindow:               return NamespaceGrantWindowRequest::create(root, messageSource);
                case Method_NamespaceGrantStart:                return MessagePtr();
                case Method_NamespaceGrantComplete:             return MessagePtr();
              }
              break;
            }
            case Message::MessageType_Result:
            {
              switch (msgMethod) {
                case Method_Invalid:                            return MessagePtr();

                case Method_NamespaceGrantWindow:               return MessagePtr();
                case Method_NamespaceGrantStart:                return MessagePtr();
                case Method_NamespaceGrantComplete:             return MessagePtr();
              }
              break;
            }
            case Message::MessageType_Notify:
            {
              switch (msgMethod) {
                case Method_Invalid:                            return MessagePtr();

                case Method_NamespaceGrantWindow:               return MessagePtr();
                case Method_NamespaceGrantStart:                return MessagePtr();
                case Method_NamespaceGrantComplete:             return NamespaceGrantCompleteNotify::create(root, messageSource);
              }
              break;
            }
          }

          return MessagePtr();
        }

      }
    }
  }
}
