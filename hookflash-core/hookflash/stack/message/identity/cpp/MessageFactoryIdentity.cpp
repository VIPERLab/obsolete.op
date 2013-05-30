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

#include <hookflash/stack/message/identity/MessageFactoryIdentity.h>
#include <hookflash/stack/message/identity/IdentityLoginStartResult.h>
#include <hookflash/stack/message/identity/IdentityLoginBrowserWindowControlNotify.h>
#include <hookflash/stack/message/identity/IdentityLoginCompleteResult.h>
#include <hookflash/stack/message/identity/IdentityAssociateResult.h>
#include <hookflash/stack/message/identity/IdentitySignResult.h>
#include <hookflash/stack/message/Message.h>
#include <hookflash/stack/message/IMessageFactoryManager.h>

#include <hookflash/stack/message/internal/stack_message_MessageHelper.h>

#include <hookflash/stack/IHelper.h>

#define HOOKFLASH_STACK_MESSAGE_MESSAGE_FACTORY_IDENTITY_HANDLER "identity"


namespace hookflash
{
  namespace stack
  {
    namespace message
    {
      namespace identity
      {
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageFactoryIdentity
        #pragma mark

        //---------------------------------------------------------------------
        MessageFactoryIdentityPtr MessageFactoryIdentity::create()
        {
          MessageFactoryIdentityPtr pThis(new MessageFactoryIdentity);
          IMessageFactoryManager::registerFactory(pThis);
          return pThis;
        }

        //---------------------------------------------------------------------
        MessageFactoryIdentityPtr MessageFactoryIdentity::singleton()
        {
          static MessageFactoryIdentityPtr pThis = create();
          return pThis;
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageFactoryIdentity => IMessageFactory
        #pragma mark

        //---------------------------------------------------------------------
        const char *MessageFactoryIdentity::getHandler() const
        {
          return HOOKFLASH_STACK_MESSAGE_MESSAGE_FACTORY_IDENTITY_HANDLER;
        }

        //---------------------------------------------------------------------
        Message::Methods MessageFactoryIdentity::toMethod(const char *inMethod) const
        {
          typedef zsLib::ULONG ULONG;
          String methodStr(inMethod ? inMethod : "");

          for (ULONG loop = (ULONG)(MessageFactoryIdentity::Method_Invalid+1); loop <= ((ULONG)MessageFactoryIdentity::Method_Last); ++loop)
          {
            if (methodStr == toString((Message::Methods)loop)) {
              return (Message::Methods)loop;
            }
          }
          return Message::Method_Invalid;
        }

        //---------------------------------------------------------------------
        const char *MessageFactoryIdentity::toString(Message::Methods method) const
        {
          switch ((MessageFactoryIdentity::Methods)method)
          {
            case Method_Invalid:                            return "";

            case Method_IdentityLoginStart:                 return "identity-login-start";
            case Method_IdentityLoginNotify:                return "identity-login-notify";
            case Method_IdentityLoginBrowserWindowControl:  return "identity-login-browser-window-control";
            case Method_IdentityLoginComplete:              return "identity-login-complete";
            case Method_IdentityAssociate:                  return "identity-associate";
            case Method_IdentitySign:                       return "identity-sign";
          }
          return "";
        }

        //---------------------------------------------------------------------
        MessagePtr MessageFactoryIdentity::create(
                                                  ElementPtr root,
                                                  IMessageSourcePtr messageSource
                                                  )
        {
          if (!root) return MessagePtr();

          Message::MessageTypes msgType = IMessageHelper::getMessageType(root);
          Methods msgMethod = (MessageFactoryIdentity::Methods)toMethod(IMessageHelper::getAttribute(root, "method"));

          switch (msgType) {
            case Message::MessageType_Invalid:                  return MessagePtr();

            case Message::MessageType_Request:
            {
              switch (msgMethod) {
                case Method_Invalid:                            return MessagePtr();

                case Method_IdentityLoginStart:                 return MessagePtr();
                case Method_IdentityLoginNotify:                return MessagePtr();
                case Method_IdentityLoginBrowserWindowControl:  return MessagePtr();
                case Method_IdentityLoginComplete:              return MessagePtr();
                case Method_IdentityAssociate:                  return MessagePtr();
                case Method_IdentitySign:                       return MessagePtr();
              }
              break;
            }
            case Message::MessageType_Result:
            {
              switch (msgMethod) {
                case Method_Invalid:                            return MessagePtr();

                case Method_IdentityLoginStart:                 return IdentityLoginStartResult::create(root, messageSource);
                case Method_IdentityLoginNotify:                return MessagePtr();
                case Method_IdentityLoginBrowserWindowControl:  return MessagePtr();
                case Method_IdentityLoginComplete:              return IdentityLoginCompleteResult::create(root, messageSource);
                case Method_IdentityAssociate:                  return IdentityAssociateResult::create(root, messageSource);
                case Method_IdentitySign:                       return IdentitySignResult::create(root, messageSource);
              }
              break;
            }
            case Message::MessageType_Notify:                   {
              switch (msgMethod) {
                case Method_Invalid:                            return MessagePtr();

                case Method_IdentityLoginStart:                 return MessagePtr();
                case Method_IdentityLoginNotify:                return MessagePtr();
                case Method_IdentityLoginBrowserWindowControl:  return IdentityLoginBrowserWindowControlNotify::create(root, messageSource);
                case Method_IdentityLoginComplete:              return MessagePtr();
                case Method_IdentityAssociate:                  return MessagePtr();
                case Method_IdentitySign:                       return MessagePtr();
              }
              break;
            }
            case Message::MessageType_Reply:                    return MessagePtr();
          }

          return MessagePtr();
        }

      }
    }
  }
}
