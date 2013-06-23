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
#include <hookflash/services/IICESocket.h>
#include <hookflash/services/IICESocketSession.h>

#include <boost/shared_array.hpp>

//#include <boost/test/unit_test_suite.hpp>
//#include <boost/test/unit_test.hpp>
//#include <boost/test/test_tools.hpp>

#include "config.h"
#include "boost_replacement.h"

#include <list>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <cstdio>
#include <cstring>

namespace hookflash { namespace services { namespace test { ZS_DECLARE_SUBSYSTEM(hookflash_services_test) } } }

using zsLib::BYTE;
using zsLib::WORD;
using zsLib::ULONG;
using zsLib::Socket;
using zsLib::SocketPtr;
using zsLib::ISocketPtr;
using zsLib::IPAddress;
using hookflash::services::IDNS;
using hookflash::services::IDNSQuery;
using hookflash::services::ITURNSocket;
using hookflash::services::ITURNSocketPtr;
using hookflash::services::ITURNSocketDelegate;
using hookflash::services::IICESocket;
using hookflash::services::IICESocketPtr;

namespace hookflash
{
  namespace services
  {
    namespace test
    {
      static const char *gUsername = HOOKFLASH_SERVICE_TEST_TURN_USERNAME;
      static const char *gPassword = HOOKFLASH_SERVICE_TEST_TURN_PASSWORD;

      class TestICESocketCallback;
      typedef boost::shared_ptr<TestICESocketCallback> TestICESocketCallbackPtr;
      typedef boost::weak_ptr<TestICESocketCallback> TestICESocketCallbackWeakPtr;

      class TestICESocketCallback : public zsLib::MessageQueueAssociator,
                                    public IICESocketDelegate,
                                    public IICESocketSessionDelegate,
                                    public zsLib::ITimerDelegate
      {
      protected:
        typedef std::list<IICESocketSessionPtr> SessionList;

      private:
        TestICESocketCallback(zsLib::IMessageQueuePtr queue) :
          zsLib::MessageQueueAssociator(queue),
          mExpectConnected(false),
          mExpectGracefulShutdown(false),
          mExpectErrorShutdown(false),
          mExpectedSessionConnected(false),
          mExpectedSessionClosed(false),
          mConnected(false),
          mGracefulShutdown(false),
          mErrorShutdown(false),
          mShutdownCalled(false),
          mSessionConnected(false),
          mSessionClosed(false)
        {
        }

        void init(
                  WORD port,
                  const char *srvName
                  )
        {
          zsLib::AutoRecursiveLock lock(mLock);

          mICESocket = IICESocket::create(
                                          getAssociatedMessageQueue(),
                                          mThisWeak.lock(),
                                          srvName,
                                          gUsername,
                                          gPassword,
                                          srvName,
                                          port
                                          );

          mTimer = zsLib::Timer::create(mThisWeak.lock(), zsLib::Milliseconds(rand()%400+200));
        }

      public:
        static TestICESocketCallbackPtr create(
                                               zsLib::IMessageQueuePtr queue,
                                               WORD port,
                                               const char *srvName,
                                               bool expectConnected = true,
                                               bool expectGracefulShutdown = true,
                                               bool expectErrorShutdown = false,
                                               bool expectedSessionConnected = true,
                                               bool expectedSessionClosed = true
                                               )
        {
          TestICESocketCallbackPtr pThis(new TestICESocketCallback(queue));
          pThis->mThisWeak = pThis;
          pThis->mExpectConnected = expectConnected;
          pThis->mExpectGracefulShutdown = expectGracefulShutdown;
          pThis->mExpectErrorShutdown = expectErrorShutdown;
          pThis->mExpectedSessionConnected = expectedSessionConnected;
          pThis->mExpectedSessionClosed = expectedSessionClosed;
          pThis->init(port, srvName);
          return pThis;
        }

        ~TestICESocketCallback()
        {
          if (mTimer) {
            mTimer->cancel();
            mTimer.reset();
          }
          mSessions.clear();
          mICESocket.reset();
        }

        virtual void onICESocketStateChanged(
                                             IICESocketPtr socket,
                                             ICESocketStates state
                                             )
        {
          zsLib::AutoRecursiveLock lock(mLock);
          switch (state) {
            case IICESocket::ICESocketState_Ready:
            {
              BOOST_CHECK(mExpectConnected);
              mConnected = true;
              break;
            }
            case IICESocket::ICESocketState_Shutdown:
            {
              if (mShutdownCalled) {
                BOOST_CHECK(mExpectGracefulShutdown);
                mGracefulShutdown = true;
              } else {
                BOOST_CHECK(mExpectErrorShutdown);
                mErrorShutdown = true;
              }
              mICESocket.reset();
              break;
            }
            default:  break;
          }
        }

        virtual void onICESocketSessionStateChanged(
                                                    IICESocketSessionPtr session,
                                                    ICESocketSessionStates state
                                                    )
        {
          zsLib::AutoRecursiveLock lock(mLock);

          switch(state) {
            case IICESocketSession::ICESocketSessionState_Nominated:
            {
              BOOST_CHECK(mExpectedSessionConnected);
              mSessionConnected = true;

              SessionList::iterator found = find(mSessions.begin(), mSessions.end(), session);
              BOOST_CHECK(found != mSessions.end())
              break;
            }
            case IICESocketSession::ICESocketSessionState_Shutdown:
            {
              BOOST_CHECK(mExpectedSessionClosed);
              mSessionClosed = true;

              SessionList::iterator found = find(mSessions.begin(), mSessions.end(), session);
              BOOST_CHECK(found != mSessions.end())
              mSessions.erase(found);
            }
            default: break;
          }
        }

        virtual void handleICESocketSessionReceivedPacket(
                                                          IICESocketSessionPtr session,
                                                          const zsLib::BYTE *buffer,
                                                          zsLib::ULONG bufferLengthInBytes
                                                          )
        {
          zsLib::AutoRecursiveLock lock(mLock);
        }

        virtual bool handleICESocketSessionReceivedSTUNPacket(
                                                              IICESocketSessionPtr session,
                                                              STUNPacketPtr stun,
                                                              const zsLib::String &localUsernameFrag,
                                                              const zsLib::String &remoteUsernameFrag
                                                              )
        {
          zsLib::AutoRecursiveLock lock(mLock);
          return false;
        }

        virtual void onICESocketSessionWriteReady(IICESocketSessionPtr session)
        {
          zsLib::AutoRecursiveLock lock(mLock);
        }

        virtual void onTimer(zsLib::TimerPtr timer)
        {
          zsLib::AutoRecursiveLock lock(mLock);
          if (timer != mTimer) return;
        }

        void shutdown()
        {
          zsLib::AutoRecursiveLock lock(mLock);
          if (!mICESocket) return;
          if (mShutdownCalled) return;
          mShutdownCalled = true;
          for (SessionList::iterator iter = mSessions.begin(); iter != mSessions.end(); ++iter) {
            IICESocketSessionPtr &session = (*iter);
            session->close();
          }
          mICESocket->shutdown();
          if (mTimer) {
            mTimer->cancel();
            mTimer.reset();
          }
        }

        bool isComplete()
        {
          return (mExpectConnected == mConnected) &&
                 (mExpectGracefulShutdown == mGracefulShutdown) &&
                 (mExpectErrorShutdown == mErrorShutdown) &&
                 (mExpectedSessionConnected == mSessionConnected) &&
                 (mExpectedSessionClosed == mSessionClosed);
        }

        void expectationsOkay() {
          zsLib::AutoRecursiveLock lock(mLock);
          if (mExpectConnected) {
            BOOST_CHECK(mConnected);
          } else {
            BOOST_CHECK(!mConnected);
          }

          if (mExpectGracefulShutdown) {
            BOOST_CHECK(mGracefulShutdown);
          } else {
            BOOST_CHECK(!mGracefulShutdown);
          }

          if (mExpectErrorShutdown) {
            BOOST_CHECK(mErrorShutdown);
          } else {
            BOOST_CHECK(!mErrorShutdown);
          }

          if (mExpectedSessionConnected) {
            BOOST_CHECK(mSessionConnected);
          } else {
            BOOST_CHECK(!mSessionConnected);
          }

          if (mExpectedSessionClosed) {
            BOOST_CHECK(mSessionClosed);
          } else {
            BOOST_CHECK(!mSessionClosed);
          }
        }

        void getLocalCandidates(IICESocket::CandidateList &outCandidates)
        {
          zsLib::AutoRecursiveLock lock(mLock);
          if (!mICESocket) return;
          BOOST_CHECK(mConnected);
          mICESocket->getLocalCandidates(outCandidates);
        }

        void createSessionFromRemoteCandidates(
                                               const IICESocket::CandidateList &remoteCandidates,
                                               IICESocket::ICEControls control
                                               )
        {
          zsLib::AutoRecursiveLock lock(mLock);
          if (!mICESocket) return;
          IICESocketSessionPtr session = mICESocket->createSessionFromRemoteCandidates(mThisWeak.lock(), remoteCandidates, control);
          mSessions.push_back(session);
        }

      private:
        mutable zsLib::RecursiveLock mLock;
        TestICESocketCallbackWeakPtr mThisWeak;

        zsLib::TimerPtr mTimer;

        IICESocketPtr mICESocket;
        SessionList mSessions;

        bool mExpectConnected;
        bool mExpectGracefulShutdown;
        bool mExpectErrorShutdown;
        bool mExpectedSessionConnected;
        bool mExpectedSessionClosed;

        bool mConnected;
        bool mGracefulShutdown;
        bool mErrorShutdown;
        bool mSessionConnected;
        bool mSessionClosed;

        bool mShutdownCalled;
      };
    }
  }
}

using hookflash::services::test::TestICESocketCallback;
using hookflash::services::test::TestICESocketCallbackPtr;

void doTestICESocket()
{
  if (!HOOKFLASH_SERVICE_TEST_DO_ICE_SOCKET_TEST) return;

  BOOST_INSTALL_LOGGER();

  zsLib::MessageQueueThreadPtr thread(zsLib::MessageQueueThread::createBasic());

  TestICESocketCallbackPtr testObject1;
  TestICESocketCallbackPtr testObject2;
  TestICESocketCallbackPtr testObject3;
  TestICESocketCallbackPtr testObject4;

  IICESocket::CandidateList candidates1;
  IICESocket::CandidateList candidates2;

  IICESocket::CandidateList candidates3;
  IICESocket::CandidateList candidates4;

  BOOST_STDOUT() << "WAITING:      Waiting for ICE testing to complete (max wait is 180 seconds).\n";

  // check to see if all DNS routines have resolved
  {
    ULONG step = 0;
    ULONG totalSteps = 2;

    do
    {
      BOOST_STDOUT() << "STEP:         ---------->>>>>>>>>> " << step << " <<<<<<<<<<----------\n";

      ULONG expecting = 0;
      switch (step) {
        case 0: {
          expecting = 2;
          testObject1 = TestICESocketCallback::create(thread, 0, HOOKFLASH_SERVICE_TEST_TURN_SERVER_DOMAIN);
          testObject2 = TestICESocketCallback::create(thread, 0, HOOKFLASH_SERVICE_TEST_TURN_SERVER_DOMAIN);
          break;
        }
        case 1: {
          expecting = 2;
          testObject1 = TestICESocketCallback::create(thread, 0, HOOKFLASH_SERVICE_TEST_TURN_SERVER_DOMAIN, true, false, false, true, false);
          testObject2 = TestICESocketCallback::create(thread, 0, HOOKFLASH_SERVICE_TEST_TURN_SERVER_DOMAIN, true, false, false, true, false);
          break;
        }
      }

      ULONG found = 0;
      ULONG lastFound = 0;
      ULONG totalWait = 0;

      while (found < expecting)
      {
        boost::this_thread::sleep(zsLib::Seconds(1));
        ++totalWait;
        if (totalWait >= 30)
          break;

        found = 0;

        switch (step) {
          case 0: {
            if (10 == totalWait) {
              testObject1->getLocalCandidates(candidates1);
              testObject2->getLocalCandidates(candidates2);

              BOOST_CHECK(candidates1.size() > 0);
              BOOST_CHECK(candidates2.size() > 0);
              testObject1->createSessionFromRemoteCandidates(candidates2, IICESocket::ICEControl_Controlling);
              testObject2->createSessionFromRemoteCandidates(candidates1, IICESocket::ICEControl_Controlled);
            }

            if (20 == totalWait) {
              testObject1->shutdown();
              testObject2->shutdown();
            }
            break;
          }
          case 1: {
            if (10 == totalWait) {
              testObject1->getLocalCandidates(candidates1);
              testObject2->getLocalCandidates(candidates2);
              BOOST_CHECK(candidates1.size() > 0);
              BOOST_CHECK(candidates2.size() > 0);
              testObject1->createSessionFromRemoteCandidates(candidates2, IICESocket::ICEControl_Controlling);
              testObject2->createSessionFromRemoteCandidates(candidates1, IICESocket::ICEControl_Controlling);
            }

            if (20 == totalWait) {
              found = 2;
            }

            break;
          }
        }

        found += (testObject1 ? (testObject1->isComplete() ? 1 : 0) : 0);
        found += (testObject2 ? (testObject2->isComplete() ? 1 : 0) : 0);
        found += (testObject3 ? (testObject3->isComplete() ? 1 : 0) : 0);
        found += (testObject4 ? (testObject4->isComplete() ? 1 : 0) : 0);

        if (lastFound != found) {
          lastFound = found;
          BOOST_STDOUT() << "FOUND:        [" << found << "].\n";
        }
      }
      BOOST_EQUAL(found, expecting);

      switch (step) {
        case 0: {
          testObject1->expectationsOkay();
          testObject2->expectationsOkay();

          break;
        }
        case 1: {
          testObject1->expectationsOkay();
          testObject2->expectationsOkay();
          break;
        }
      }
      testObject1.reset();
      testObject2.reset();
      testObject3.reset();
      testObject4.reset();

      ++step;
    } while (step < totalSteps);
  }

  BOOST_STDOUT() << "WAITING:      All ICE sockets have finished. Waiting for 'bogus' events to process (10 second wait).\n";
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
