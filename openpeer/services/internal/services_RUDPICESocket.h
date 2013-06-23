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

#pragma once

#include <hookflash/services/internal/types.h>
#include <hookflash/services/IRUDPICESocket.h>
#include <hookflash/services/IICESocket.h>

#include <map>

namespace hookflash
{
  namespace services
  {
    namespace internal
    {
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IRUDPICESocketForRUDPICESocketSession
      #pragma mark

      interaction IRUDPICESocketForRUDPICESocketSession
      {
        typedef IICESocket::CandidateList CandidateList;
        typedef IICESocket::ICEControls ICEControls;

        IRUDPICESocketForRUDPICESocketSession &forSession() {return *this;}
        const IRUDPICESocketForRUDPICESocketSession &forSession() const {return *this;}

        virtual IRUDPICESocketSessionPtr createSessionFromRemoteCandidates(
                                                                           IRUDPICESocketSessionDelegatePtr delegate,
                                                                           const CandidateList &remoteCandidates,
                                                                           ICEControls control
                                                                           ) = 0;

        virtual IICESocketPtr getICESocket() const = 0;
        virtual IRUDPICESocketPtr getRUDPICESocket() const = 0;

        virtual RecursiveLock &getLock() const = 0;

        virtual void onRUDPICESessionClosed(PUID sessionID) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark RUDPICESocket
      #pragma mark

      class RUDPICESocket : public Noop,
                            public MessageQueueAssociator,
                            public IRUDPICESocket,
                            public IICESocketDelegate,
                            public IRUDPICESocketForRUDPICESocketSession
      {
      public:
        friend interaction IRUDPICESocketFactory;

        typedef IICESocket::CandidateList CandidateList;
        typedef IICESocket::ICEControls ICEControls;

        class Subscription;
        typedef boost::shared_ptr<Subscription> SubscriptionPtr;
        typedef boost::weak_ptr<Subscription> SubscriptionWeakPtr;

      protected:
        RUDPICESocket(
                      IMessageQueuePtr queue,
                      IRUDPICESocketDelegatePtr delegate
                      );
        RUDPICESocket(Noop) : Noop(true), MessageQueueAssociator(IMessageQueuePtr()) {};

        void init(
                  const char *turnServer,
                  const char *turnServerUsername,
                  const char *turnServerPassword,
                  const char *stunServer,
                  WORD port = 0
                  );
        void init(
                  IDNS::SRVResultPtr srvTURNUDP,
                  IDNS::SRVResultPtr srvTURNTCP,
                  const char *turnServerUsername,
                  const char *turnServerPassword,
                  IDNS::SRVResultPtr srvSTUN,
                  WORD port = 0
                  );

      public:
        ~RUDPICESocket();

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark RUDPICESocket => IRUDPICESocket
        #pragma mark

        static RUDPICESocketPtr create(
                                       IMessageQueuePtr queue,
                                       IRUDPICESocketDelegatePtr delegate,
                                       const char *turnServer,
                                       const char *turnServerUsername,
                                       const char *turnServerPassword,
                                       const char *stunServer,
                                       WORD port = 0
                                       );

        static RUDPICESocketPtr create(
                                       IMessageQueuePtr queue,
                                       IRUDPICESocketDelegatePtr delegate,
                                       IDNS::SRVResultPtr srvTURNUDP,
                                       IDNS::SRVResultPtr srvTURNTCP,
                                       const char *turnServerUsername,
                                       const char *turnServerPassword,
                                       IDNS::SRVResultPtr srvSTUN,
                                       WORD port = 0
                                       );

        virtual PUID getID() const {return mID;}

        virtual RUDPICESocketStates getState() const;

        virtual IRUDPICESocketSubscriptionPtr subscribe(IRUDPICESocketDelegatePtr delegate);

        virtual void shutdown();

        virtual void wakeup(Duration minimumTimeCandidatesMustRemainValidWhileNotUsed);

        virtual void getLocalCandidates(CandidateList &outCandidates);

        virtual IRUDPICESocketSessionPtr createSessionFromRemoteCandidates(
                                                                           IRUDPICESocketSessionDelegatePtr delegate,
                                                                           const CandidateList &remoteCandidates,
                                                                           ICEControls control
                                                                           );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark RUDPICESocket => IRUDPICESocketForRUDPICESocketSession
        #pragma mark

        virtual RecursiveLock &getLock() const {return mLock;}

        virtual IICESocketPtr getICESocket() const;
        virtual IRUDPICESocketPtr getRUDPICESocket() const;

        virtual void onRUDPICESessionClosed(PUID sessionID);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark RUDPICESocket => IICESocketDelegate
        #pragma mark

        virtual void onICESocketStateChanged(
                                             IICESocketPtr socket,
                                             ICESocketStates state
                                             );

      protected:

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark RUDPICESocket => friend Subscription
        #pragma mark

        void cancelSubscription(Subscription &subscription);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark RUDPICESocket => (internal)
        #pragma mark

        String log(const char *message) const;
        bool isShuttingDown();
        bool isShutdown();

        void cancel();
        void setState(RUDPICESocketStates state);

      public:

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark RUDPICESocket::Subscription
        #pragma mark

        class Subscription : public IRUDPICESocketSubscription
        {
        protected:
          Subscription(RUDPICESocketPtr outer);

        public:
          ~Subscription();

          static SubscriptionPtr create(RUDPICESocketPtr outer);

          virtual PUID getID() const {return mID;}
          virtual void cancel();

        public:
          RUDPICESocketWeakPtr mOuter;
          PUID mID;
        };

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark RUDPICESocket => (data)
        #pragma mark

        mutable RecursiveLock mLock;
        RUDPICESocketWeakPtr mThisWeak;
        RUDPICESocketPtr mGracefulShutdownReference;
        PUID mID;

        RUDPICESocketStates mCurrentState;

        typedef std::map<PUID, IRUDPICESocketDelegatePtr> DelegateMap;
        DelegateMap mDelegates;

        IICESocketPtr mICESocket;

        typedef std::map<PUID, RUDPICESocketSessionPtr> SessionMap;
        SessionMap mSessions;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IRUDPICESocketFactory
      #pragma mark

      interaction IRUDPICESocketFactory
      {
        static IRUDPICESocketFactory &singleton();

        virtual RUDPICESocketPtr create(
                                        IMessageQueuePtr queue,
                                        IRUDPICESocketDelegatePtr delegate,
                                        const char *turnServer,
                                        const char *turnServerUsername,
                                        const char *turnServerPassword,
                                        const char *stunServer,
                                        WORD port = 0
                                        );

        virtual RUDPICESocketPtr create(
                                        IMessageQueuePtr queue,
                                        IRUDPICESocketDelegatePtr delegate,
                                        IDNS::SRVResultPtr srvTURNUDP,
                                        IDNS::SRVResultPtr srvTURNTCP,
                                        const char *turnServerUsername,
                                        const char *turnServerPassword,
                                        IDNS::SRVResultPtr srvSTUN,
                                        WORD port = 0
                                        );
      };
      
    }
  }
}

ZS_DECLARE_PROXY_BEGIN(hookflash::services::internal::IRUDPICESocketForRUDPICESocketSession)
ZS_DECLARE_PROXY_TYPEDEF(zsLib::RecursiveLock, RecursiveLock)
ZS_DECLARE_PROXY_TYPEDEF(zsLib::PUID, PUID)
ZS_DECLARE_PROXY_TYPEDEF(hookflash::services::IRUDPICESocketSessionPtr, IRUDPICESocketSessionPtr)
ZS_DECLARE_PROXY_TYPEDEF(hookflash::services::IRUDPICESocketSessionDelegatePtr, IRUDPICESocketSessionDelegatePtr)
ZS_DECLARE_PROXY_METHOD_SYNC_RETURN_3(createSessionFromRemoteCandidates, IRUDPICESocketSessionPtr, IRUDPICESocketSessionDelegatePtr, const CandidateList &, ICEControls)
ZS_DECLARE_PROXY_METHOD_SYNC_CONST_RETURN_0(getLock, RecursiveLock &)
ZS_DECLARE_PROXY_METHOD_SYNC_CONST_RETURN_0(getICESocket, hookflash::services::IICESocketPtr)
ZS_DECLARE_PROXY_METHOD_SYNC_CONST_RETURN_0(getRUDPICESocket, hookflash::services::IRUDPICESocketPtr)
ZS_DECLARE_PROXY_METHOD_1(onRUDPICESessionClosed, PUID)
ZS_DECLARE_PROXY_END()
