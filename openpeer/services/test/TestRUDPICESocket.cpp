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


#include <zsLib/MessageQueueThread.h>
#include <zsLib/Exception.h>
#include <zsLib/Socket.h>
#include <zsLib/Timer.h>
#include <openpeer/services/IRUDPICESocket.h>
#include <openpeer/services/IRUDPICESocketSession.h>
#include <openpeer/services/IRUDPMessaging.h>
#include <openpeer/services/ITransportStream.h>
#include <openpeer/services/IHelper.h>


#include "config.h"
#include "boost_replacement.h"

namespace openpeer { namespace services { namespace test { ZS_DECLARE_SUBSYSTEM(openpeer_services_test) } } }

using zsLib::BYTE;
using zsLib::WORD;
using zsLib::ULONG;
using zsLib::CSTR;
using zsLib::Socket;
using zsLib::SocketPtr;
using zsLib::ISocketPtr;
using zsLib::IPAddress;
using zsLib::AutoRecursiveLock;
using openpeer::services::IRUDPICESocket;
using openpeer::services::IRUDPICESocketPtr;
using openpeer::services::IRUDPICESocketDelegate;
using openpeer::services::IRUDPICESocketSession;
using openpeer::services::IRUDPICESocketSessionPtr;
using openpeer::services::IRUDPICESocketSessionDelegate;
using openpeer::services::IRUDPMessaging;
using openpeer::services::IRUDPMessagingPtr;
using openpeer::services::IRUDPMessagingDelegate;
using openpeer::services::IHelper;
using openpeer::services::IICESocket;
using openpeer::services::IDNS;

static const char *gUsername = OPENPEER_SERVICE_TEST_TURN_USERNAME;
static const char *gPassword = OPENPEER_SERVICE_TEST_TURN_PASSWORD;

namespace openpeer
{
  namespace services
  {
    namespace test
    {
      class TestRUDPICESocketCallback;
      typedef boost::shared_ptr<TestRUDPICESocketCallback> TestRUDPICESocketCallbackPtr;
      typedef boost::weak_ptr<TestRUDPICESocketCallback> TestRUDPICESocketCallbackWeakPtr;

      class TestRUDPICESocketCallback : public zsLib::MessageQueueAssociator,
                                        public IRUDPICESocketDelegate,
                                        public IRUDPICESocketSessionDelegate,
                                        public IRUDPMessagingDelegate,
                                        public ITransportStreamWriterDelegate,
                                        public ITransportStreamReaderDelegate
      {
      private:
        //---------------------------------------------------------------------
        TestRUDPICESocketCallback(
                                  zsLib::IMessageQueuePtr queue,
                                  const zsLib::IPAddress &serverIP
                                  ) :
          zsLib::MessageQueueAssociator(queue),
          mReceiveStream(ITransportStream::create()->getReader()),
          mSendStream(ITransportStream::create()->getWriter()),
          mServerIP(serverIP),
          mSocketShutdown(false),
          mSessionShutdown(false),
          mMessagingShutdown(false)
        {
        }

        //---------------------------------------------------------------------
        void init()
        {
          zsLib::AutoRecursiveLock lock(mLock);

          mReceiveStreamSubscription = mReceiveStream->subscribe(mThisWeak.lock());
          mReceiveStream->notifyReaderReadyToRead();

          mSocket = IRUDPICESocket::create(
                                             getAssociatedMessageQueue(),
                                             mThisWeak.lock(),
                                             OPENPEER_SERVICE_TEST_TURN_SERVER_DOMAIN,
                                             gUsername,
                                             gPassword,
                                             OPENPEER_SERVICE_TEST_STUN_SERVER
                                             );
        }

      public:
        //---------------------------------------------------------------------
        static TestRUDPICESocketCallbackPtr create(
                                                   zsLib::IMessageQueuePtr queue,
                                                   zsLib::IPAddress serverIP
                                                   )
        {
          TestRUDPICESocketCallbackPtr pThis(new TestRUDPICESocketCallback(queue, serverIP));
          pThis->mThisWeak = pThis;
          pThis->init();
          return pThis;
        }

        //---------------------------------------------------------------------
        ~TestRUDPICESocketCallback()
        {
        }

        //---------------------------------------------------------------------
        void shutdown()
        {
          zsLib::AutoRecursiveLock lock(mLock);
          mSocket->shutdown();
        }

        //---------------------------------------------------------------------
        bool isShutdown()
        {
          zsLib::AutoRecursiveLock lock(mLock);
          return mSocketShutdown && mSessionShutdown && mMessagingShutdown;
        }

        //---------------------------------------------------------------------
        virtual void onRUDPICESocketStateChanged(
                                                 IRUDPICESocketPtr socket,
                                                 RUDPICESocketStates state
                                                 )
        {
          zsLib::AutoRecursiveLock lock(mLock);
          if (socket != mSocket) return;

          switch (state) {
            case IRUDPICESocket::RUDPICESocketState_Ready:
            {
              IRUDPICESocket::CandidateList candidates;
              IRUDPICESocket::Candidate candidate;
              candidate.mType = IICESocket::Type_Local;
              candidate.mIPAddress = mServerIP;
              candidate.mPriority = 0;
              candidate.mLocalPreference = 0;

              candidates.push_back(candidate);

              mSocketSession = mSocket->createSessionFromRemoteCandidates(
                                                                          mThisWeak.lock(),
                                                                          "serverUsernameFrag",
                                                                          NULL,
                                                                          candidates,
                                                                          IICESocket::ICEControl_Controlling
                                                                          );
              mSocketSession->endOfRemoteCandidates();
              break;
            }
            case IRUDPICESocket::RUDPICESocketState_Shutdown:
            {
              mSocketShutdown = true;
              break;
            }
            default: break;
          }
        }
        
        //---------------------------------------------------------------------
        virtual void onRUDPICESocketCandidatesChanged(IRUDPICESocketPtr socket)
        {
          // ignored
        }

        //---------------------------------------------------------------------
        virtual void onRUDPICESocketSessionStateChanged(
                                                        IRUDPICESocketSessionPtr session,
                                                        RUDPICESocketSessionStates state
                                                        )
        {
          zsLib::AutoRecursiveLock lock(mLock);
          if (IRUDPICESocketSession::RUDPICESocketSessionState_Ready == state) {
            mMessaging = IRUDPMessaging::openChannel(
                                                     getAssociatedMessageQueue(),
                                                     mSocketSession,
                                                     mThisWeak.lock(),
                                                     "bogus/text-bogus",
                                                     mReceiveStream->getStream(),
                                                     mSendStream->getStream()
                                                     );
          }
          if (IRUDPICESocketSession::RUDPICESocketSessionState_Shutdown == state) {
            mSessionShutdown = true;
          }
        }

        //---------------------------------------------------------------------
        virtual void onRUDPICESocketSessionChannelWaiting(IRUDPICESocketSessionPtr session)
        {
        }

        //---------------------------------------------------------------------
        virtual void onRUDPMessagingStateChanged(
                                                 IRUDPMessagingPtr session,
                                                 RUDPMessagingStates state
                                                 )
        {
          zsLib::AutoRecursiveLock lock(mLock);

          if (IRUDPMessaging::RUDPMessagingState_Connected == state) {
            mSendStream->write((const BYTE *)"*HELLO*", strlen("*HELLO*"));
          }
          if (IRUDPMessaging::RUDPMessagingState_Shutdown == state) {
            mMessagingShutdown = true;
          }
        }

        //---------------------------------------------------------------------
        virtual void onTransportStreamReaderReady(ITransportStreamReaderPtr reader)
        {
          zsLib::AutoRecursiveLock lock(mLock);
          if (reader != mReceiveStream) return;

          while (true) {
            SecureByteBlockPtr buffer = mReceiveStream->read();
            if (!buffer) return;

            zsLib::ULONG messageSize = buffer->SizeInBytes() - sizeof(char);

            zsLib::String str = (CSTR)(buffer->BytePtr());
            ZS_LOG_BASIC("-------------------------------------------------------------------------------")
            ZS_LOG_BASIC("-------------------------------------------------------------------------------")
            ZS_LOG_BASIC("-------------------------------------------------------------------------------")
            ZS_LOG_BASIC(zsLib::String("RECEIVED: \"") + str + "\"")
            ZS_LOG_BASIC("-------------------------------------------------------------------------------")
            ZS_LOG_BASIC("-------------------------------------------------------------------------------")
            ZS_LOG_BASIC("-------------------------------------------------------------------------------")

            zsLib::String add = "<SOCKET->" + IHelper::randomString(1000) + ">";

            zsLib::ULONG newMessageSize = messageSize + add.length();
            SecureByteBlockPtr newBuffer(new SecureByteBlock(newMessageSize));

            memcpy(newBuffer->BytePtr(), buffer->BytePtr(), messageSize);
            memcpy(newBuffer->BytePtr() + messageSize, (const zsLib::BYTE *)(add.c_str()), add.length());
            
            mSendStream->write(newBuffer);
          }
        }

        //---------------------------------------------------------------------
        virtual void onTransportStreamWriterReady(ITransportStreamWriterPtr reader)
        {
          // IGNORED
        }

      private:
        mutable zsLib::RecursiveLock mLock;
        TestRUDPICESocketCallbackWeakPtr mThisWeak;

        ITransportStreamReaderPtr mReceiveStream;
        ITransportStreamWriterPtr mSendStream;

        ITransportStreamReaderSubscriptionPtr mReceiveStreamSubscription;

        zsLib::IPAddress mServerIP;

        bool mSocketShutdown;
        bool mSessionShutdown;
        bool mMessagingShutdown;

        IRUDPMessagingPtr mMessaging;
        IRUDPICESocketPtr mSocket;
        IRUDPICESocketSessionPtr mSocketSession;
      };
    }
  }
}

using namespace openpeer::services::test;
using openpeer::services::test::TestRUDPICESocketCallback;
using openpeer::services::test::TestRUDPICESocketCallbackPtr;

void doTestRUDPICESocket()
{
  if (!OPENPEER_SERVICE_TEST_DO_RUDPICESOCKET_CLIENT_TO_SERVER_TEST) return;
  if (!OPENPEER_SERVICE_TEST_RUNNING_AS_CLIENT) return;

  BOOST_INSTALL_LOGGER();

  zsLib::MessageQueueThreadPtr thread(zsLib::MessageQueueThread::createBasic());

  TestRUDPICESocketCallbackPtr testObject1 = TestRUDPICESocketCallback::create(thread, IPAddress(OPENPEER_SERVICE_TEST_RUDP_SERVER_IP, OPENPEER_SERVICE_TEST_RUDP_SERVER_PORT));

  ZS_LOG_BASIC("WAITING:      Waiting for RUDP ICE socket testing to complete (max wait is 60 minutes).");

  {
    int expecting = 1;
    int found = 0;

    ULONG totalWait = 0;
    do
    {
      boost::this_thread::sleep(zsLib::Seconds(1));
      ++totalWait;
      if (totalWait >= (10*60))
        break;

      if ((4*60 + 50) == totalWait) {
        testObject1->shutdown();
      }

      found = 0;
      if (testObject1->isShutdown()) ++found;

      if (found == expecting)
        break;

    } while(true);
    BOOST_CHECK(found == expecting)
  }

  testObject1.reset();

  ZS_LOG_BASIC("WAITING:      All RUDP sockets have finished. Waiting for 'bogus' events to process (10 second wait).");

  boost::this_thread::sleep(zsLib::Seconds(10));

  // wait for shutdown
  {
    ULONG count = 0;
    do
    {
      count = thread->getTotalUnprocessedMessages();
      //    count += mThreadNeverCalled->getTotalUnprocessedMessages();
      if (0 != count)
        boost::this_thread::yield();

    } while (count > 0);

    thread->waitForShutdown();
  }
  BOOST_UNINSTALL_LOGGER();
  zsLib::proxyDump();
  BOOST_EQUAL(zsLib::proxyGetTotalConstructed(), 0);
}
