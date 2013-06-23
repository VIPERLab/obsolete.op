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

#include <hookflash/services/types.h>
#include <hookflash/services/IDNS.h>
#include <zsLib/types.h>
#include <zsLib/IPAddress.h>
#include <zsLib/Proxy.h>

#include <list>

#define HOOKFLASH_SERVICES_IICESOCKET_DEFAULT_HOW_LONG_CANDIDATES_MUST_REMAIN_VALID_IN_SECONDS (10*60)

namespace hookflash
{
  namespace services
  {
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IICESocket
    #pragma mark

    interaction IICESocket
    {
      enum ICESocketStates
      {
        ICESocketState_Pending,
        ICESocketState_Ready,
        ICESocketState_GoingToSleep,
        ICESocketState_Sleeping,
        ICESocketState_ShuttingDown,
        ICESocketState_Shutdown,
      };

      static const char *toString(ICESocketStates state);

      enum Types
      {
        Type_Unknown =          1,
        Type_Local =            126,
        Type_ServerReflexive =  100,
        Type_PeerReflexive =    50,
        Type_Relayed =          0,
      };

      static const char *toString(Types type);

      struct Candidate
      {
        Types     mType;
        IPAddress mIPAddress;
        DWORD     mPriority;
        WORD      mLocalPreference;  // fill with "0" if unknown
        String    mUsernameFrag;
        String    mPassword;
        String    mProtocol;

        Candidate(): mType(Type_Unknown), mPriority(0), mLocalPreference(0) {}
        String toDebugString(bool includeCommaPrefix = true) const;
      };

      enum ICEControls
      {
        ICEControl_Controlling,
        ICEControl_Controlled
      };

      typedef std::list<Candidate> CandidateList;

      static IICESocketPtr create(
                                  IMessageQueuePtr queue,
                                  IICESocketDelegatePtr delegate,
                                  const char *turnServer,
                                  const char *turnServerUsername,
                                  const char *turnServerPassword,
                                  const char *stunServer,
                                  WORD port = 0,
                                  bool firstWORDInAnyPacketWillNotConflictWithTURNChannels = false
                                  );

      static IICESocketPtr create(
                                  IMessageQueuePtr queue,
                                  IICESocketDelegatePtr delegate,
                                  IDNS::SRVResultPtr srvTURNUDP,
                                  IDNS::SRVResultPtr srvTURNTCP,
                                  const char *turnServerUsername,
                                  const char *turnServerPassword,
                                  IDNS::SRVResultPtr srvSTUN,
                                  WORD port = 0,
                                  bool firstWORDInAnyPacketWillNotConflictWithTURNChannels = false
                                  );

      virtual PUID getID() const = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Gets the current state of the object
      virtual ICESocketStates getState() const = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Subscribe to the current socket state.
      virtual IICESocketSubscriptionPtr subscribe(IICESocketDelegatePtr delegate) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Close the socket and cause all sessions to become closed.
      virtual void shutdown() = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Call to wakeup a potentially sleeping socket so that all
      //          local candidates are prepared.
      // NOTE:    Each an every time that local candidates are to be obtained,
      //          this method must be called first to ensure that all services
      //          are ready. For example, TURN is shutdown while not in use
      //          and it must become active otherwise the TURN candidates will
      //          not be available.
      virtual void wakeup(Duration minimumTimeCandidatesMustRemainValidWhileNotUsed = Seconds(HOOKFLASH_SERVICES_IICESOCKET_DEFAULT_HOW_LONG_CANDIDATES_MUST_REMAIN_VALID_IN_SECONDS)) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Base all the usernames/passwords upon the foundation socket
      // NOTE:    Must be called before "getLocalCandidates" or creating
      //          an ICE socket session to work.
      virtual void setFoundation(IICESocketPtr foundationSocket) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Gets a local list of offered candidates
      virtual void getLocalCandidates(CandidateList &outCandidates) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Create a peer to peer connected session when the remote
      //          candidates are already known.
      virtual IICESocketSessionPtr createSessionFromRemoteCandidates(
                                                                     IICESocketSessionDelegatePtr delegate,
                                                                     const CandidateList &remoteCandidates,
                                                                     ICEControls control
                                                                     ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Enable or disable write ready notifications on all sessions
      virtual void monitorWriteReadyOnAllSessions(bool monitor = true) = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IICESocketSubscription
    #pragma mark

    interaction IICESocketSubscription
    {
      virtual void cancel() = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IICESocketDelegate
    #pragma mark

    interaction IICESocketDelegate
    {
      typedef services::IICESocketPtr IICESocketPtr;
      typedef IICESocket::ICESocketStates ICESocketStates;

      virtual void onICESocketStateChanged(
                                           IICESocketPtr socket,
                                           ICESocketStates state
                                           ) = 0;
    };
  }
}

ZS_DECLARE_PROXY_BEGIN(hookflash::services::IICESocketDelegate)
ZS_DECLARE_PROXY_METHOD_2(onICESocketStateChanged, hookflash::services::IICESocketPtr, hookflash::services::IICESocketDelegate::ICESocketStates)
ZS_DECLARE_PROXY_END()
