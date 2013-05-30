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

#include <zsLib/internal/zsLib_SocketMonitor.h>
#include <zsLib/Stringize.h>

#include <boost/thread.hpp>

namespace zsLib {ZS_DECLARE_SUBSYSTEM(zsLib)}

namespace zsLib
{
  namespace internal
  {
#ifndef _WIN32
    typedef timeval TIMEVAL;
#endif //_WIN32

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark SocketMonitorGlobalSafeReference
    #pragma mark

    class SocketMonitorGlobalSafeReference
    {
    public:
      SocketMonitorGlobalSafeReference(SocketMonitorPtr reference) :
        mSafeReference(reference)
      {
      }

      ~SocketMonitorGlobalSafeReference()
      {
        mSafeReference->cancel();
        mSafeReference.reset();
      }

    private:
      SocketMonitorPtr mSafeReference;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark SocketMonitor
    #pragma mark

    //-------------------------------------------------------------------------
    SocketMonitor::SocketMonitor() :
      mShouldShutdown(false)
    {
    }

    //-------------------------------------------------------------------------
    SocketMonitor::~SocketMonitor()
    {
      mThisWeak.reset();
      cancel();
    }

    //-------------------------------------------------------------------------
    SocketMonitorPtr SocketMonitor::create()
    {
      SocketMonitorPtr pThis = SocketMonitorPtr(new SocketMonitor);
      pThis->mThisWeak = pThis;
      return pThis;
    }

    //-------------------------------------------------------------------------
    SocketMonitorPtr SocketMonitor::singleton()
    {
      static SocketMonitorPtr singleton = SocketMonitor::create();
      static SocketMonitorGlobalSafeReference safe(singleton);
      return singleton;
    }

    //-------------------------------------------------------------------------
    void SocketMonitor::monitorBegin(
                                     SocketPtr socket,
                                     bool monitorRead,
                                     bool monitorWrite,
                                     bool monitorException
                                     )
    {
      EventPtr event;
      {
        AutoRecursiveLock lock(mLock);

        if (!mThread) {
          mThread = ThreadPtr(new boost::thread(boost::ref(*(singleton().get()))));
#ifndef _WIN32
          const int policy = SCHED_RR;
          const int maxPrio = sched_get_priority_max(policy);
          boost::thread::native_handle_type threadHandle = mThread->native_handle();
          sched_param param;
          param.sched_priority = maxPrio - 1; // maximum thread priority
          pthread_setschedparam(threadHandle, policy, &param);
#endif //_WIN32
        }

        SOCKET socketHandle = socket->getSocket();
        if (INVALID_SOCKET == socketHandle)                                             // nothing to monitor
          return;

        SocketMap::iterator found = mMonitoredSockets.find(socketHandle);
        if (found != mMonitoredSockets.end())                                           // socket already monitored?
          return;

        mMonitoredSockets[socketHandle] = socket;                                       // remember the socket is monitored

        if (monitorRead)
          mReadSet.add(socketHandle);
        if (monitorWrite)
          mWriteSet.add(socketHandle);
        if (monitorException)
          mExceptionSet.add(socketHandle);

        event = zsLib::Event::create();
        mWaitingForRebuildList.push_back(event);                                        // socket handles cane be reused so we must ensure that the socket handles are rebuilt before returning

        wakeUp();
      }
      if (event)
        event->wait();
    }

    //-------------------------------------------------------------------------
    void SocketMonitor::monitorEnd(zsLib::Socket &socket)
    {
      EventPtr event;
      {
        AutoRecursiveLock lock(mLock);

        SOCKET socketHandle = socket.getSocket();
        if (INVALID_SOCKET == socketHandle)                                             // nothing to monitor
          return;

        SocketMap::iterator found = mMonitoredSockets.find(socketHandle);
        if (found == mMonitoredSockets.end())
          return;                                                                       // socket was not being monitored

        mMonitoredSockets.erase(found);                                                 // clear out the socket since it is no longer being monitored

        mReadSet.remove(socketHandle);
        mWriteSet.remove(socketHandle);
        mExceptionSet.remove(socketHandle);

        event = zsLib::Event::create();
        mWaitingForRebuildList.push_back(event);                                        // socket handles cane be reused so we must ensure that the socket handles are rebuilt before returning

        wakeUp();
      }
      if (event)
        event->wait();
    }

    //-------------------------------------------------------------------------
    void SocketMonitor::monitorRead(const zsLib::Socket &socket)
    {
      AutoRecursiveLock lock(mLock);

      SOCKET socketHandle = socket.getSocket();
      if (INVALID_SOCKET == socketHandle)                                             // nothing to monitor
        return;

      if (mMonitoredSockets.end() == mMonitoredSockets.find(socketHandle))            // if the socket is not being monitored, then do nothing
        return;

      mReadSet.add(socketHandle);

      wakeUp();
    }

    //-------------------------------------------------------------------------
    void SocketMonitor::monitorWrite(const zsLib::Socket &socket)
    {
      AutoRecursiveLock lock(mLock);

      SOCKET socketHandle = socket.getSocket();
      if (INVALID_SOCKET == socketHandle)                                             // nothing to monitor
        return;

      if (mMonitoredSockets.end() == mMonitoredSockets.find(socketHandle))            // if the socket is not being monitored, then do nothing
        return;

      mWriteSet.add(socketHandle);

      wakeUp();
    }

    //-------------------------------------------------------------------------
    void SocketMonitor::monitorException(const zsLib::Socket &socket)
    {
      AutoRecursiveLock lock(mLock);

      SOCKET socketHandle = socket.getSocket();
      if (INVALID_SOCKET == socketHandle)                                             // nothing to monitor
        return;

      if (mMonitoredSockets.end() == mMonitoredSockets.find(socketHandle))            // if the socket is not being monitored, then do nothing
        return;

      mExceptionSet.add(socketHandle);

      wakeUp();
    }

    //-------------------------------------------------------------------------
    void SocketMonitor::operator()()
    {
#ifdef __QNX__
      pthread_setname_np(pthread_self(), "com.zslib.socketMonitor");
#else
#ifndef _LINUX
#ifndef _ANDROID
      pthread_setname_np("com.zslib.socketMonitor");
#endif // _ANDROID
#endif // _LINUX
#endif // __QNX__

      createWakeUpSocket();

      bool shouldShutdown = false;

      TIMEVAL timeout;
      memset(&timeout, 0, sizeof(timeout));

      ::fd_set workingReadSet;
      ::fd_set workingWriteSet;
      ::fd_set workingExceptionSet;

      FD_ZERO(&workingReadSet);
      FD_ZERO(&workingWriteSet);
      FD_ZERO(&workingExceptionSet);

      do
      {
        ::fd_set *readSet = NULL;
        ::fd_set *writeSet = NULL;
        ::fd_set *exceptionSet = NULL;

        SOCKET highestSocket = INVALID_SOCKET;

        {
          AutoRecursiveLock lock(mLock);
          processWaiting();

          readSet = mReadSet.getSet();
          writeSet = mWriteSet.getSet();
          exceptionSet = mExceptionSet.getSet();

          if (readSet) {
            memcpy(&workingReadSet, readSet, sizeof(workingReadSet));
            readSet = &workingReadSet;
          }
          if (writeSet) {
            memcpy(&workingWriteSet, writeSet, sizeof(workingWriteSet));
            writeSet = &workingWriteSet;
          }
          if (exceptionSet) {
            memcpy(&workingExceptionSet, exceptionSet, sizeof(workingExceptionSet));
            exceptionSet = &workingExceptionSet;
          }

#ifndef _WIN32
          SOCKET highest = mReadSet.getHighestSocket();
          if (INVALID_SOCKET != highest)
            highestSocket = highest;

          highest = mWriteSet.getHighestSocket();
          if (INVALID_SOCKET == highestSocket)
            highestSocket = highest;
          if (INVALID_SOCKET != highest)
            highestSocket = (highest > highestSocket ? highest : highestSocket);

          highest = mExceptionSet.getHighestSocket();
          if (INVALID_SOCKET == highestSocket)
            highestSocket = highest;
          if (INVALID_SOCKET != highest)
            highestSocket = (highest > highestSocket ? highest : highestSocket);
#endif //_WIN32
        }

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int result = select(INVALID_SOCKET == highestSocket ? 0 : (highestSocket+1), readSet, writeSet, exceptionSet, &timeout);

        // select completed, do notifications from select
        {
          AutoRecursiveLock lock(mLock);
          shouldShutdown = mShouldShutdown;
          switch (result) {
            case 0:
            case INVALID_SOCKET:  break;
            default: {
              ULONG totalToProcess = result;

              bool redoWakeupSocket = false;
              if (readSet) {
                if (FD_ISSET(mWakeUpSocket->getSocket(), readSet)) {
                  --totalToProcess;

                  bool wouldBlock = false;
                  static DWORD gBogus = 0;
                  static BYTE *bogus = (BYTE *)&gBogus;
                  int noThrowError = 0;
                  mWakeUpSocket->receive(bogus, sizeof(gBogus), &wouldBlock, 0, &noThrowError);
                  if (0 != noThrowError) redoWakeupSocket = true;
                }
              }

              if (exceptionSet) {
                if (FD_ISSET(mWakeUpSocket->getSocket(), exceptionSet)) {
                  --totalToProcess;
                  redoWakeupSocket = true;
                }
              }

              if (redoWakeupSocket) {
                mReadSet.remove(mWakeUpSocket->getSocket());
                mExceptionSet.remove(mWakeUpSocket->getSocket());
                mWakeUpSocket->close();
                mWakeUpSocket.reset();
                createWakeUpSocket();
              }

              for (SocketMap::iterator monIter = mMonitoredSockets.begin(); (monIter != mMonitoredSockets.end()) && (totalToProcess > 0); ) {
                SocketMap::iterator current = monIter;
                ++monIter;

                SOCKET socketHandle = current->first;
                SocketPtr notifySocket = (current->second).lock();

                bool removeSocket = (!notifySocket) || redoWakeupSocket;

                if (notifySocket) {
                  try {
                    if (!redoWakeupSocket) {
                      if (readSet) {
                        if (FD_ISSET(socketHandle, readSet)) {
                          mReadSet.remove(socketHandle);  // remove the socket from the read list (it will have to be put back later when the socket is actually read)
                          totalToProcess = (totalToProcess > 0 ? totalToProcess - 1 : 0);
                          notifySocket->notifyReadReady();
                        }
                      }
                      if (writeSet) {
                        if (FD_ISSET(socketHandle, writeSet)) {
                          mWriteSet.remove(socketHandle);  // remove the socket from the read list (it will have to be put back later when the socket is actually read)
                          totalToProcess = (totalToProcess > 0 ? totalToProcess - 1 : 0);
                          notifySocket->notifyWriteReady();
                        }
                      }
                      if (exceptionSet) {
                        if (FD_ISSET(socketHandle, exceptionSet)) {
                          mExceptionSet.remove(socketHandle);  // remove the socket from the read list (it will have to be put back later when the socket is actually read)
                          totalToProcess = (totalToProcess > 0 ? totalToProcess - 1 : 0);
                          notifySocket->notifyException();
                        }
                      }
                    } else {
                      notifySocket->notifyException();
                    }
                  } catch (ISocketDelegateProxy::Exceptions::DelegateGone &) {
                    removeSocket = true;
                  }
                }

                if (removeSocket) {
                  mMonitoredSockets.erase(current);
                  mReadSet.remove(socketHandle);
                  mWriteSet.remove(socketHandle);
                  mExceptionSet.remove(socketHandle);
                  continue;
                }
              }
              break;
            }
          }
        }

      } while (!shouldShutdown);

      SocketMonitorPtr gracefulReference;

      {
        AutoRecursiveLock lock(mLock);

        // transfer the reference to the thread
        gracefulReference = mGracefulReference;
        mGracefulReference.reset();

        processWaiting();
        mMonitoredSockets.clear();
        mWaitingForRebuildList.clear();
        mReadSet.clear();
        mWriteSet.clear();
        mExceptionSet.clear();
        mWakeUpSocket.reset();
      }
    }

    //-------------------------------------------------------------------------
    void SocketMonitor::cancel()
    {
      ThreadPtr thread;
      {
        AutoRecursiveLock lock(mLock);
        mGracefulReference = mThisWeak.lock();
        thread = mThread;

        mShouldShutdown = true;
        wakeUp();
      }

      if (!thread)
        return;

      thread->join();

      {
        AutoRecursiveLock lock(mLock);
        mThread.reset();
      }
    }

    //-------------------------------------------------------------------------
    void SocketMonitor::processWaiting()
    {
      for (EventList::iterator iter = mWaitingForRebuildList.begin(); iter != mWaitingForRebuildList.end(); ++iter)
      {
        (*iter)->notify();
      }
      mWaitingForRebuildList.clear();
    }

    //-------------------------------------------------------------------------
    void SocketMonitor::wakeUp()
    {
      int errorCode = 0;

      {
        AutoRecursiveLock lock(mLock);

        if (!mWakeUpSocket)               // is the wakeup socket created?
          return;

        if (!mWakeUpSocket->isValid())
        {
          ZS_LOG_ERROR(Basic, "Could not wake up socket monitor as wakeup socket was closed. This will cause a delay in the socket monitor response time.")
          return;
        }

        static DWORD gBogus = 0;
        static BYTE *bogus = (BYTE *)&gBogus;

        bool wouldBlock = false;
        mWakeUpSocket->send(bogus, sizeof(gBogus), &wouldBlock, 0, &errorCode);       // send a bogus packet to its own port to wake it up
      }

      if (0 != errorCode) {
        std::cout << "SocketMonitor: Could not wake up socket monitor. This will cause a delay in the socket monitor response time, where error=" + (Stringize<int>(errorCode).string());
      }
    }

    //-------------------------------------------------------------------------
    void SocketMonitor::createWakeUpSocket()
    {
      AutoRecursiveLock lock(mLock);

      // ignore SIGPIPE
      int tries = 0;
      bool useIPv6 = true;
      while (true)
      {
        // bind only on the loopback address
        bool error = false;
        try {
          if (useIPv6)
          {
            mWakeUpAddress = IPAddress::loopbackV6();
            mWakeUpSocket = zsLib::Socket::createUDP(zsLib::Socket::Create::IPv6);
          }
          else
          {
            mWakeUpAddress = IPAddress::loopbackV4();
            mWakeUpSocket = zsLib::Socket::createUDP(zsLib::Socket::Create::IPv4);
          }
          mWakeUpSocket->resetSocketMonitorGlobalSafeReference();  // do not hold a reference to ourself

          if (((tries > 5) && (tries < 10)) ||
              (tries > 15)) {
            mWakeUpAddress.setPort(5000+(rand()%(65525-5000)));
          } else {
            mWakeUpAddress.setPort(0);
          }

          mWakeUpSocket->setOptionFlag(zsLib::Socket::SetOptionFlag::NonBlocking, true);
          mWakeUpSocket->bind(mWakeUpAddress);
          mWakeUpAddress = mWakeUpSocket->getLocalAddress();
          mWakeUpSocket->connect(mWakeUpAddress);
        } catch (zsLib::Socket::Exceptions::Unspecified &) {
          error = true;
        }
        if (!error)
        {
          break;
        }

        boost::thread::yield();       // do not hammer CPU

        if (tries > 10)
          useIPv6 = (tries%2 == 0);   // after 10 tries, start trying to bind using IPv4

        ZS_THROW_BAD_STATE_MSG_IF(tries > 500, "Unable to allocate any loopback ports for a wake-up socket")
      }

      // we are only adding the socket to the select fd_set not to the monitored map (since we aren't monitoring)
      mReadSet.add(mWakeUpSocket->getSocket());
      mExceptionSet.add(mWakeUpSocket->getSocket());
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark SocketSet
    #pragma mark
    SocketSet::SocketSet()
    {
      FD_ZERO((::fd_set *)&mSet);
      mCount = 0;
    }

    //-------------------------------------------------------------------------
   SocketSet::~SocketSet()
    {
    }

   //-------------------------------------------------------------------------
    u_int &SocketSet::count() const
    {
      return mCount;
    }

    //-------------------------------------------------------------------------
    SOCKET SocketSet::getHighestSocket() const
    {
      MonitoredSocketSet::const_reverse_iterator iter = mMonitoredSockets.rbegin();
      if (iter == mMonitoredSockets.rend())
        return INVALID_SOCKET;

      return (*iter);
    }

    //-------------------------------------------------------------------------
    ::fd_set *SocketSet::getSet() const
    {
      if (0 == count())
        return NULL;

      return (::fd_set *)&mSet;
    }

    //-------------------------------------------------------------------------
    void SocketSet::add(SOCKET inSocket)
    {
      if (INVALID_SOCKET == inSocket)
        return;

      if (mMonitoredSockets.find(inSocket) != mMonitoredSockets.end())  // already monitored?
        return;

      mMonitoredSockets.insert(inSocket);

      ++mCount;
      FD_SET(inSocket, &mSet);
    }

    //-------------------------------------------------------------------------
    void SocketSet::remove(SOCKET inSocket)
    {
      if (INVALID_SOCKET == inSocket)
        return;

      MonitoredSocketSet::iterator find = mMonitoredSockets.find(inSocket);

      if (find == mMonitoredSockets.end())  // is this socket being monitored now?
        return;

      mMonitoredSockets.erase(find);

      // NOTE: I don't shrink the fd_set allocation... oh well, who cares... it won't leak and it's limited number of sockets anyway...
      --mCount;
      FD_CLR(inSocket, &mSet);
    }

    //-------------------------------------------------------------------------
    bool SocketSet::isSet(SOCKET inSocket)
    {
      return (bool)(FD_ISSET(inSocket, &mSet) ? true : false);
    }

    //-------------------------------------------------------------------------
    void SocketSet::clear()
    {
      mMonitoredSockets.clear();
      FD_ZERO(&mSet);
    }

  }
}
