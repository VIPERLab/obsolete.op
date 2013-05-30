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

#include <boost/uuid/random_generator.hpp>
#include <boost/smart_ptr/detail/atomic_count.hpp>
#include <boost/interprocess/detail/atomic.hpp>

#include <zsLib/helpers.h>
#include <zsLib/Log.h>

namespace zsLib { ZS_DECLARE_SUBSYSTEM(zsLib) }

using namespace boost;
using namespace boost::interprocess;

namespace zsLib
{

  namespace internal {
    ULONG &globalPUID() {
      static ULONG global = 0;
      return global;
    }
  }

  PUID createPUID()
  {
    return (PUID)atomicIncrement(zsLib::internal::globalPUID());
  }

  UUID createUUID()
  {
    boost::uuids::random_generator gen;
    return gen();
  }

  ULONG atomicIncrement(ULONG &value)
  {
#ifdef __GNUC__
    return __sync_add_and_fetch(&value, 1);
#else
    return ++(*(boost::detail::atomic_count *)((LONG *)(&value)));
#endif //__GNUC__
  }

  ULONG atomicDecrement(ULONG &value)
  {
#ifdef __GNUC__
    return __sync_sub_and_fetch(&value, 1);
#else
    return --(*(boost::detail::atomic_count *)((LONG *)(&value)));
#endif //__GNUC__
  }

  ULONG atomicGetValue(ULONG &value)
  {
    return (long)(*(boost::detail::atomic_count *)((LONG *)(&value)));
  }

  DWORD atomicGetValue32(DWORD &value)
  {
#ifdef __GNUC__
    return __sync_add_and_fetch(&value, 0);
#else
    return boost::interprocess::detail::atomic_read32((boost::uint32_t *)&value);
#endif //__GNUC__
  }

  void atomicSetValue32(DWORD &value, DWORD newValue)
  {
#ifdef __GNUC__
    __sync_lock_test_and_set(&value, newValue);
#else
	  boost::interprocess::detail::atomic_write32((boost::uint32_t *)&value, newValue);
#endif //__GNUC__
  }

  Time now()
  {
    return boost::posix_time::microsec_clock::universal_time();
  }

  time_t toEpoch(Time time)
  {
    if (Time() == time) {
      return (time_t)0;
    }

    static Time epoch(boost::gregorian::date(1970,1,1));
    Duration::sec_type x = (time - epoch).total_seconds();

    return time_t(x);
  }

  Time toTime(time_t time)
  {
    if (0 == time) {
      return Time();
    }
    static Time epoch(boost::gregorian::date(1970,1,1));
    return epoch + Seconds((Duration::sec_type)time);
  }
}
