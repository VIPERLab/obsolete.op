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

#include <openpeer/core/core.h>
#include <openpeer/core/internal/core.h>
#include <zsLib/Log.h>

namespace hookflash { namespace core { ZS_IMPLEMENT_SUBSYSTEM(hookflash_core) } }
namespace hookflash { namespace core { ZS_IMPLEMENT_SUBSYSTEM(hookflash_media) } }
namespace hookflash { namespace core { ZS_IMPLEMENT_SUBSYSTEM(hookflash_webrtc) } }
namespace hookflash { namespace core { namespace application { ZS_IMPLEMENT_SUBSYSTEM(hookflash_application) } } }

namespace hookflash
{
  namespace core
  {
    bool ContactProfileInfo::hasData() const
    {
      return ((mContact) ||
              (mProfileBundleEl));
    }

    IdentityLookupInfo::IdentityLookupInfo() :
      mPriority(0),
      mWeight(0)
    {
    }


    bool IdentityLookupInfo::hasData() const
    {
      return ((mContact) ||
              (mIdentityURI.hasData()) ||
              (mStableID.hasData()) ||
              (0 != mPriority) ||
              (0 != mWeight) ||
              (Time() != mLastUpdated) ||
              (Time() != mExpires) ||
              (mName.hasData()) ||
              (mProfileURL.hasData()) ||
              (mVProfileURL.hasData()) ||
              (mAvatars.size() > 0));
    }
    
/*
    struct IdentityLookupInfo
    {
      struct Avatar
      {
        String mName;
        String mURL;
        int mWidth;
        int mHeight;
      };
      typedef std::list<Avatar> AvatarList;

      IContactPtr mContact;

      String mIdentityURI;
      String mUserID;

      WORD mPriority;
      WORD mWeight;

      Time mLastUpdated;
      Time mExpires;

      String mName;
      String mProfileURL;
      String mVProfileURL;

      AvatarList mAvatars;

      IdentityLookupInfo();
      bool hasData()
 */
  }
}
