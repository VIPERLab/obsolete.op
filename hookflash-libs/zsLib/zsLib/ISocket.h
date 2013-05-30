/*
 *  Created by Robin Raymond.
 *  Copyright 2009-2013. Robin Raymond. All rights reserved.
 *
 * This file is part of zsLib.
 *
 * zsLib is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License (LGPL) as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * zsLib is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with zsLib; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 *
 */

#pragma once

#ifndef ZSLIB_ISOCKET_H_77b428faae5ff4a4827517546a6a6104
#define ZSLIB_ISOCKET_H_77b428faae5ff4a4827517546a6a6104

#include <zsLib/internal/zsLib_ISocket.h>

#pragma warning(push)
#pragma warning(disable: 4290)

namespace zsLib
{
  typedef Proxy<ISocketDelegate> ISocketDelegateProxy;

  interaction ISocket
  {
  public:
    struct Exceptions {
      ZS_DECLARE_CUSTOM_EXCEPTION(InvalidSocket)
      ZS_DECLARE_CUSTOM_EXCEPTION(DelegateNotSet)

      class Unspecified : public Exception
      {
      public:
        Unspecified(
                    const Subsystem &subsystem,
                    const String &inMessage,
                    CSTR inFunction,
                    CSTR inPathName,
                    ULONG inLineNumber,
                    int inErrorCode
                    ) :
          Exception(subsystem, inMessage, inFunction, inPathName, inLineNumber),
          mErrorCode(inErrorCode)
        {
        }
        ~Unspecified() throw() {}

        int getErrorCode() {return mErrorCode;}

      private:
        int mErrorCode;
      };

      ZS_DECLARE_CUSTOM_EXCEPTION_ALT_BASE_WITH_PROPERTIES_1(WouldBlock, Unspecified, int)
      ZS_DECLARE_CUSTOM_EXCEPTION_ALT_BASE_WITH_PROPERTIES_1(ConnectionReset, Unspecified, int)
      ZS_DECLARE_CUSTOM_EXCEPTION_ALT_BASE_WITH_PROPERTIES_1(AddressInUse, Unspecified, int)
      ZS_DECLARE_CUSTOM_EXCEPTION_ALT_BASE_WITH_PROPERTIES_1(ConnectionRefused, Unspecified, int)
      ZS_DECLARE_CUSTOM_EXCEPTION_ALT_BASE_WITH_PROPERTIES_1(NetworkNotReachable, Unspecified, int)
      ZS_DECLARE_CUSTOM_EXCEPTION_ALT_BASE_WITH_PROPERTIES_1(HostNotReachable, Unspecified, int)
      ZS_DECLARE_CUSTOM_EXCEPTION_ALT_BASE_WITH_PROPERTIES_1(Timeout, Unspecified, int)
      ZS_DECLARE_CUSTOM_EXCEPTION_ALT_BASE_WITH_PROPERTIES_1(Shutdown, Unspecified, int)
      ZS_DECLARE_CUSTOM_EXCEPTION_ALT_BASE_WITH_PROPERTIES_1(ConnectionAborted, Unspecified, int)
      ZS_DECLARE_CUSTOM_EXCEPTION_ALT_BASE_WITH_PROPERTIES_1(BufferTooSmall, Unspecified, int)
      ZS_DECLARE_CUSTOM_EXCEPTION_ALT_BASE_WITH_PROPERTIES_1(BufferTooBig, Unspecified, int)
      ZS_DECLARE_CUSTOM_EXCEPTION_ALT_BASE_WITH_PROPERTIES_1(UnsupportedSocketOption, Unspecified, int)
    };

    struct Receive {
      enum Options {
        None     = 0,
        Peek     = MSG_PEEK,
        OOB      = MSG_OOB,
        WaitAll  = MSG_WAITALL
      };
    };

    struct Send {
      enum Options {
        None = 0,
        OOB  = MSG_OOB
      };
    };

    struct Shutdown {
      enum Options {
        Send =      SD_SEND,
        Receive =   SD_RECEIVE,
        Both =      SD_BOTH
      };
    };

    struct SetOptionFlag {
      enum Options {
        NonBlocking          = FIONBIO,
        IgnoreSigPipe        = SO_NOSIGPIPE,
        Broadcast            = SO_BROADCAST,
        Debug                = SO_DEBUG,
        DontRoute            = SO_DONTROUTE,
        KeepAlive            = SO_KEEPALIVE,
        OOBInLine            = SO_OOBINLINE,
        ReuseAddress         = SO_REUSEADDR,
        ConditionalAccept    = SO_WINDOWS_CONDITIONAL_ACCEPT,
        ExclusiveAddressUse  = SO_WINDOWS_EXCLUSIVEADDRUSE,
        TCPNoDelay           = TCP_NODELAY,
      };
    };

    struct SetOptionValue {
      enum Options {
        ReceiverBufferSizeInBytes = SO_RCVBUF,
        SendBufferSizeInBytes     = SO_SNDBUF,
        LingerTimeInSeconds       = SO_LINGER, // specifying a time of 0 will disable linger
      };
    };

    struct GetOptionFlag {
      enum Options {
        IsListening             = SO_ACCEPTCONN,
        IsBroadcast             = SO_BROADCAST,
        IsDebugging             = SO_DEBUG,
        IsDontRoute             = SO_DONTROUTE,
        IsKeepAlive             = SO_KEEPALIVE,
        IsOOBInLine             = SO_OOBINLINE,
        IsReuseAddress          = SO_REUSEADDR,
        IsOOBAllRead            = SIOCATMARK,
        IsConditionalAccept     = SO_WINDOWS_CONDITIONAL_ACCEPT,
        IsLingering             = SO_WINDOWS_DONTLINGER,
        IsExclusiveAddressUse   = SO_EXCLUSIVEADDRUSE,
        IsTCPNoDelay            = TCP_NODELAY,
      };
    };

    struct GetOptionValue {
      enum Options {
        ErrorCode                  = SO_ERROR,
        ReceiverBufferSizeInBytes  = SO_RCVBUF,
        SendBufferSizeInBytes      = SO_SNDBUF,
        ReadyToReadSizeInBytes     = FIONREAD,    // how much data is available in a single recv method for stream types (more data may be pending), or how much data is in the buffer total for message oriented socket (however read will return the message size even if more data is available)
        Type                       = SO_TYPE,
        MaxMessageSizeInBytes      = SO_WINDOWS_MAX_MSG_SIZE
      };
    };

    struct Monitor {
      enum Options {
        Read      = 0x01,
        Write     = 0x02,
        Exception = 0x04,

        All       = (Read | Write | Exception),
      };
    };

  public:
    virtual bool isValid() const = 0;

    // socket must be valid in order to monitor the socket or an exception will be thrown
    virtual void setDelegate(ISocketDelegatePtr delegate = ISocketDelegatePtr()) throw (Exceptions::InvalidSocket) = 0;
    virtual void monitor(Monitor::Options options = Monitor::All) = 0;

    virtual void close() throw(Exceptions::WouldBlock, Exceptions::Unspecified) = 0;  // closing a socket will automaticlaly remove any socket monitor

    virtual IPAddress getLocalAddress() const throw (Exceptions::InvalidSocket, Exceptions::Unspecified) = 0;
    virtual IPAddress getRemoteAddress() const throw (Exceptions::InvalidSocket, Exceptions::Unspecified) = 0;

    virtual void bind(
                      const IPAddress &inBindIP,
                      int *noThrowErrorResult = NULL
                      ) const throw(
                                    Exceptions::InvalidSocket,
                                    Exceptions::AddressInUse,
                                    Exceptions::Unspecified
                                    ) = 0;

    virtual void listen() const throw(
                                      Exceptions::InvalidSocket,
                                      Exceptions::AddressInUse,
                                      Exceptions::Unspecified
                                      ) = 0;

    virtual ISocketPtr accept(
                              IPAddress &outRemoteIP,
                              int *noThrowErrorCode = NULL
                              ) const throw(
                                            Exceptions::InvalidSocket,
                                            Exceptions::ConnectionReset,
                                            Exceptions::Unspecified
                                            ) = 0;

    virtual void connect(
                         const IPAddress &inDestination,     // destination of the connection
                         bool *outWouldBlock = NULL,         // if this param is used, will return the "would block" as a result rather than throwing an exception
                         int *noThrowErrorResult = NULL
                         ) const throw(
                                       Exceptions::InvalidSocket,
                                       Exceptions::WouldBlock,
                                       Exceptions::AddressInUse,
                                       Exceptions::NetworkNotReachable,
                                       Exceptions::HostNotReachable,
                                       Exceptions::Timeout,
                                       Exceptions::Unspecified
                                       ) = 0;

    virtual ULONG receive(
                          BYTE *ioBuffer,
                          ULONG inBufferLengthInBytes,
                          bool *outWouldBlock = NULL,         // if this param is used, will return the "would block" as a result rather than throwing an exception
                          ULONG flags = (ULONG)(Receive::None),
                          int *noThrowErrorResult = NULL
                          ) const throw(
                                        Exceptions::InvalidSocket,
                                        Exceptions::WouldBlock,
                                        Exceptions::Shutdown,
                                        Exceptions::ConnectionReset,
                                        Exceptions::ConnectionAborted,
                                        Exceptions::Timeout,
                                        Exceptions::BufferTooSmall,
                                        Exceptions::Unspecified
                                        ) = 0;

    virtual ULONG receiveFrom(
                              IPAddress &outRemoteIP,
                              BYTE *ioBuffer,
                              ULONG inBufferLengthInBytes,
                              bool *outWouldBlock = NULL,         // if this param is used, will return the "would block" as a result rather than throwing an exception
                              ULONG flags = (ULONG)(Receive::None),
                              int *noThrowErrorResult = NULL
                              ) const throw(
                                            Exceptions::InvalidSocket,
                                            Exceptions::WouldBlock,
                                            Exceptions::Shutdown,
                                            Exceptions::ConnectionReset,
                                            Exceptions::Timeout,
                                            Exceptions::BufferTooSmall,
                                            Exceptions::Unspecified
                                            ) = 0;

    virtual ULONG send(
                       const BYTE *inBuffer,
                       ULONG inBufferLengthInBytes,
                       bool *outWouldBlock = NULL,         // if this param is used, will return the "would block" as a result rather than throwing an exception
                       ULONG flags = (ULONG)(Send::None),
                       int *noThrowErrorResult = NULL
                       ) const throw(
                                     Exceptions::InvalidSocket,
                                     Exceptions::WouldBlock,
                                     Exceptions::Shutdown,
                                     Exceptions::HostNotReachable,
                                     Exceptions::ConnectionAborted,
                                     Exceptions::ConnectionReset,
                                     Exceptions::Timeout,
                                     Exceptions::BufferTooSmall,
                                     Exceptions::Unspecified
                                     ) = 0;

    virtual ULONG sendTo(
                         const IPAddress &inDestination,
                         const BYTE *inBuffer,
                         ULONG inBufferLengthInBytes,
                         bool *outWouldBlock = NULL,         // if this param is used, will return the "would block" as a result rather than throwing an exception
                         ULONG flags = (ULONG)(Send::None),
                         int *noThrowErrorResult = NULL
                         ) const throw(
                                       Exceptions::InvalidSocket,
                                       Exceptions::WouldBlock,
                                       Exceptions::Shutdown,
                                       Exceptions::Timeout,
                                       Exceptions::HostNotReachable,
                                       Exceptions::ConnectionAborted,
                                       Exceptions::ConnectionReset,
                                       Exceptions::Timeout,
                                       Exceptions::BufferTooSmall,
                                       Exceptions::Unspecified
                                       ) = 0;

    virtual void shutdown(Shutdown::Options inOptions = Shutdown::Both) const throw(Exceptions::InvalidSocket, Exceptions::Unspecified) = 0;

    virtual void setBlocking(bool enabled) const throw(Exceptions::InvalidSocket, Exceptions::Unspecified) {setOptionFlag(SetOptionFlag::NonBlocking, !enabled);}

    virtual void setOptionFlag(SetOptionFlag::Options inOption, bool inEnabled) const throw(Exceptions::InvalidSocket, Exceptions::UnsupportedSocketOption, Exceptions::Unspecified) = 0;
    virtual void setOptionValue(SetOptionValue::Options inOption, ULONG inValue) const throw(Exceptions::InvalidSocket, Exceptions::UnsupportedSocketOption, Exceptions::Unspecified) = 0;

    virtual bool getOptionFlag(GetOptionFlag::Options inOption) const throw(Exceptions::InvalidSocket, Exceptions::UnsupportedSocketOption, Exceptions::Unspecified) = 0;
    virtual ULONG getOptionValue(GetOptionValue::Options inOption) const throw(Exceptions::InvalidSocket, Exceptions::UnsupportedSocketOption, Exceptions::Unspecified) = 0;

    virtual void onReadReadyReset() const throw(Exceptions::DelegateNotSet, Exceptions::InvalidSocket, Exceptions::Unspecified) = 0;
    virtual void onWriteReadyReset() const throw(Exceptions::DelegateNotSet, Exceptions::InvalidSocket, Exceptions::Unspecified) = 0;
    virtual void onExceptionReset() const throw(Exceptions::DelegateNotSet, Exceptions::InvalidSocket, Exceptions::Unspecified) = 0;
  };

  interaction ISocketDelegate
  {
    virtual void onReadReady(zsLib::ISocketPtr socket) = 0;
    virtual void onWriteReady(zsLib::ISocketPtr socket) = 0;
    virtual void onException(zsLib::ISocketPtr socket) = 0;
  };
}

ZS_DECLARE_PROXY_BEGIN(zsLib::ISocketDelegate)
ZS_DECLARE_PROXY_METHOD_1(onReadReady, zsLib::ISocketPtr)
ZS_DECLARE_PROXY_METHOD_1(onWriteReady, zsLib::ISocketPtr)
ZS_DECLARE_PROXY_METHOD_1(onException, zsLib::ISocketPtr)
ZS_DECLARE_PROXY_END()

#pragma warning(pop)

#endif //ZSLIB_ISOCKET_H_77b428faae5ff4a4827517546a6a6104
