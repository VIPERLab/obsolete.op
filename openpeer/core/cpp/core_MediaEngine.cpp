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

#include <openpeer/core/internal/core_MediaEngine.h>
#include <openpeer/core/internal/core_Stack.h>
#include <openpeer/core/internal/core_IConversationThreadParser.h>
#include <openpeer/core/ILogger.h>

#include <zsLib/helpers.h>

#include <boost/thread.hpp>

#include <video_capture_factory.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#ifdef _ANDROID
#include <pthread.h>
#endif

#ifdef TARGET_OS_IPHONE
#include <sys/sysctl.h>
#endif

#define OPENPEER_MEDIA_ENGINE_VOICE_CODEC_ISAC
//#define OPENPEER_MEDIA_ENGINE_VOICE_CODEC_OPUS
#define OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL (-1)
#define OPENPEER_MEDIA_ENGINE_MTU (576)

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_webrtc) } }

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      typedef zsLib::ThreadPtr ThreadPtr;
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMediaEngineForStack
      #pragma mark

      //-----------------------------------------------------------------------
      void IMediaEngineForStack::setup(IMediaEngineDelegatePtr delegate)
      {
        MediaEngine::setup(delegate);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMediaEngineForCallTransport
      #pragma mark

      //-----------------------------------------------------------------------
      MediaEnginePtr IMediaEngineForCallTransport::singleton()
      {
        return MediaEngine::singleton();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine
      #pragma mark

      //-----------------------------------------------------------------------
      MediaEngine::MediaEngine(
                               IMessageQueuePtr queue,
                               IMediaEngineDelegatePtr delegate
                               ) :
        MessageQueueAssociator(queue),
        mError(0),
        mMtu(OPENPEER_MEDIA_ENGINE_MTU),
        mID(zsLib::createPUID()),
        mDelegate(IMediaEngineDelegateProxy::createWeak(delegate)),
        mEcEnabled(false),
        mAgcEnabled(false),
        mNsEnabled(false),
        mVoiceRecordFile(""),
        mDefaultVideoOrientation(IMediaEngine::VideoOrientation_LandscapeLeft),
        mRecordVideoOrientation(IMediaEngine::VideoOrientation_LandscapeLeft),
        mVoiceChannel(OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL),
        mVoiceTransport(&mRedirectVoiceTransport),
        mVideoChannel(OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL),
        mVideoTransport(&mRedirectVideoTransport),
        mCaptureId(0),
        mCameraType(CameraType_Front),
        mVoiceEngine(NULL),
        mVoiceBase(NULL),
        mVoiceCodec(NULL),
        mVoiceNetwork(NULL),
        mVoiceRtpRtcp(NULL),
        mVoiceAudioProcessing(NULL),
        mVoiceVolumeControl(NULL),
        mVoiceHardware(NULL),
        mVoiceFile(NULL),
        mVoiceEngineReady(false),
        mVcpm(NULL),
        mVideoEngine(NULL),
        mVideoBase(NULL),
        mVideoNetwork(NULL),
        mVideoRender(NULL),
        mVideoCapture(NULL),
        mVideoRtpRtcp(NULL),
        mVideoCodec(NULL),
        mVideoFile(NULL),
        mVideoEngineReady(false),
        mFaceDetection(false),
        mCaptureRenderView(NULL),
        mChannelRenderView(NULL),
        mRedirectVoiceTransport("voice"),
        mRedirectVideoTransport("video"),
        mLifetimeWantAudio(false),
        mLifetimeWantVideoCapture(false),
        mLifetimeWantVideoChannel(false),
        mLifetimeWantRecordVideoCapture(false),
        mLifetimeHasAudio(false),
        mLifetimeHasVideoCapture(false),
        mLifetimeHasVideoChannel(false),
        mLifetimeHasRecordVideoCapture(false),
        mLifetimeInProgress(false),
        mLifetimeWantCameraType(CameraType_Front),
        mLifetimeContinuousVideoCapture(false),
        mLifetimeVideoRecordFile(""),
        mLifetimeSaveVideoToLibrary(false)
      {
#ifdef TARGET_OS_IPHONE
        int name[] = {CTL_HW, HW_MACHINE};
        size_t size;
        sysctl(name, 2, NULL, &size, NULL, 0);
        char *machine = (char *)malloc(size);
        sysctl(name, 2, machine, &size, NULL, 0);
        mMachineName = machine;
        free(machine);
#endif
      }
      
      MediaEngine::MediaEngine(Noop) :
        Noop(true),
        MessageQueueAssociator(IMessageQueuePtr()),
        mError(0),
        mMtu(OPENPEER_MEDIA_ENGINE_MTU),
        mID(zsLib::createPUID()),
        mEcEnabled(false),
        mAgcEnabled(false),
        mNsEnabled(false),
        mVoiceRecordFile(""),
        mDefaultVideoOrientation(IMediaEngine::VideoOrientation_LandscapeLeft),
        mRecordVideoOrientation(IMediaEngine::VideoOrientation_LandscapeLeft),
        mVoiceChannel(OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL),
        mVoiceTransport(NULL),
        mVideoChannel(OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL),
        mVideoTransport(NULL),
        mCaptureId(0),
        mCameraType(CameraType_Front),
        mVoiceEngine(NULL),
        mVoiceBase(NULL),
        mVoiceCodec(NULL),
        mVoiceNetwork(NULL),
        mVoiceRtpRtcp(NULL),
        mVoiceAudioProcessing(NULL),
        mVoiceVolumeControl(NULL),
        mVoiceHardware(NULL),
        mVoiceFile(NULL),
        mVoiceEngineReady(false),
        mVcpm(NULL),
        mVideoEngine(NULL),
        mVideoBase(NULL),
        mVideoNetwork(NULL),
        mVideoRender(NULL),
        mVideoCapture(NULL),
        mVideoRtpRtcp(NULL),
        mVideoCodec(NULL),
        mVideoFile(NULL),
        mVideoEngineReady(false),
        mFaceDetection(false),
        mCaptureRenderView(NULL),
        mChannelRenderView(NULL),
        mRedirectVoiceTransport("voice"),
        mRedirectVideoTransport("video"),
        mLifetimeWantAudio(false),
        mLifetimeWantVideoCapture(false),
        mLifetimeWantVideoChannel(false),
        mLifetimeWantRecordVideoCapture(false),
        mLifetimeHasAudio(false),
        mLifetimeHasVideoCapture(false),
        mLifetimeHasVideoChannel(false),
        mLifetimeHasRecordVideoCapture(false),
        mLifetimeInProgress(false),
        mLifetimeWantCameraType(CameraType_Front),
        mLifetimeContinuousVideoCapture(false),
        mLifetimeVideoRecordFile(""),
        mLifetimeSaveVideoToLibrary(false)
      {
#ifdef TARGET_OS_IPHONE
        int name[] = {CTL_HW, HW_MACHINE};
        size_t size;
        sysctl(name, 2, NULL, &size, NULL, 0);
        char *machine = (char *)malloc(size);
        sysctl(name, 2, machine, &size, NULL, 0);
        mMachineName = machine;
        free(machine);
#endif
      }

      //-----------------------------------------------------------------------
      MediaEngine::~MediaEngine()
      {
        if(isNoop()) return;
        
        destroyMediaEngine();
      }

      //-----------------------------------------------------------------------
      void MediaEngine::init()
      {
        AutoRecursiveLock lock(mLock);

        ZS_LOG_DEBUG(log("init media engine"))
#ifndef ANDROID
        mVoiceEngine = webrtc::VoiceEngine::Create();
        if (mVoiceEngine == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to create voice engine"))
          return;
        }
        mVoiceBase = webrtc::VoEBase::GetInterface(mVoiceEngine);
        if (mVoiceBase == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for voice base"))
          return;
        }
        mVoiceCodec = webrtc::VoECodec::GetInterface(mVoiceEngine);
        if (mVoiceCodec == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for voice codec"))
          return;
        }
        mVoiceNetwork = webrtc::VoENetwork::GetInterface(mVoiceEngine);
        if (mVoiceNetwork == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for voice network"))
          return;
        }
        mVoiceRtpRtcp = webrtc::VoERTP_RTCP::GetInterface(mVoiceEngine);
        if (mVoiceRtpRtcp == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for voice RTP/RTCP"))
          return;
        }
        mVoiceAudioProcessing = webrtc::VoEAudioProcessing::GetInterface(mVoiceEngine);
        if (mVoiceAudioProcessing == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for audio processing"))
          return;
        }
        mVoiceVolumeControl = webrtc::VoEVolumeControl::GetInterface(mVoiceEngine);
        if (mVoiceVolumeControl == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for volume control"))
          return;
        }
        mVoiceHardware = webrtc::VoEHardware::GetInterface(mVoiceEngine);
        if (mVoiceHardware == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for audio hardware"))
          return;
        }
        mVoiceFile = webrtc::VoEFile::GetInterface(mVoiceEngine);
        if (mVoiceFile == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for voice file"))
          return;
        }

        mError = mVoiceBase->Init();
        if (mError < 0) {
          ZS_LOG_ERROR(Detail, log("failed to initialize voice base (error: ") + string(mVideoBase->LastError()) + ")")
          return;
        } else if (mVoiceBase->LastError() > 0) {
          ZS_LOG_WARNING(Detail, log("an error has occured during voice base init (error: ") + string(mVoiceBase->LastError()) + ")")
        }
        mError = mVoiceBase->RegisterVoiceEngineObserver(*this);
        if (mError < 0) {
          ZS_LOG_ERROR(Detail, log("failed to register voice engine observer (error: ") + string(mVoiceBase->LastError()) + ")")
          return;
        }

        mVideoEngine = webrtc::VideoEngine::Create();
        if (mVideoEngine == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to create video engine"))
          return;
        }
        
        mVideoBase = webrtc::ViEBase::GetInterface(mVideoEngine);
        if (mVideoBase == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for video base"))
          return;
        }
        mVideoCapture = webrtc::ViECapture::GetInterface(mVideoEngine);
        if (mVideoCapture == NULL) {
          ZS_LOG_ERROR(Detail, log("failed get interface for video capture"))
          return;
        }
        mVideoRtpRtcp = webrtc::ViERTP_RTCP::GetInterface(mVideoEngine);
        if (mVideoRtpRtcp == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for video RTP/RTCP"))
          return;
        }
        mVideoNetwork = webrtc::ViENetwork::GetInterface(mVideoEngine);
        if (mVideoNetwork == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for video network"))
          return;
        }
        mVideoRender = webrtc::ViERender::GetInterface(mVideoEngine);
        if (mVideoRender == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for video render"))
          return;
        }
        mVideoCodec = webrtc::ViECodec::GetInterface(mVideoEngine);
        if (mVideoCodec == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for video codec"))
          return;
        }
        mVideoFile = webrtc::ViEFile::GetInterface(mVideoEngine);
        if (mVideoFile == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for video file"))
          return;
        }

        mError = mVideoBase->Init();
        if (mError < 0) {
          ZS_LOG_ERROR(Detail, log("failed to initialize video base (error: ") + string(mVideoBase->LastError()) + ")")
          return;
        } else if (mVideoBase->LastError() > 0) {
          ZS_LOG_WARNING(Detail, log("an error has occured during video base init (error: ") + string(mVideoBase->LastError()) + ")")
        }

        mError = mVideoBase->SetVoiceEngine(mVoiceEngine);
        if (mError < 0) {
          ZS_LOG_ERROR(Detail, log("failed to set voice engine for video base (error: ") + string(mVideoBase->LastError()) + ")")
          return;
        }
        
        setLogLevel();

        Log::Level logLevel = ZS_GET_LOG_LEVEL();

        unsigned int traceFilter;
        switch (logLevel) {
          case Log::None:
            traceFilter = webrtc::kTraceNone;
            break;
          case Log::Basic:
            traceFilter = webrtc::kTraceWarning | webrtc::kTraceError | webrtc::kTraceCritical;
            break;
          case Log::Detail:
            traceFilter = webrtc::kTraceStateInfo | webrtc::kTraceWarning | webrtc::kTraceError | webrtc::kTraceCritical | webrtc::kTraceApiCall;
            break;
          case Log::Debug:
            traceFilter = webrtc::kTraceDefault | webrtc::kTraceDebug | webrtc::kTraceInfo;
            break;
          case Log::Trace:
            traceFilter = webrtc::kTraceAll;
            break;
          default:
            traceFilter = webrtc::kTraceNone;
            break;
        }

        if (logLevel != Log::None) {
          mError = mVoiceEngine->SetTraceFilter(traceFilter);
          if (mError < 0) {
            ZS_LOG_ERROR(Detail, log("failed to set trace filter for voice (error: ") + string(mVoiceBase->LastError()) + ")")
            return;
          }
          mError = mVoiceEngine->SetTraceCallback(this);
          if (mError < 0) {
            ZS_LOG_ERROR(Detail, log("failed to set trace callback for voice (error: ") + string(mVoiceBase->LastError()) + ")")
            return;
          }
          mError = mVideoEngine->SetTraceFilter(traceFilter);
          if (mError < 0) {
            ZS_LOG_ERROR(Detail, log("failed to set trace filter for video (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }
          mError = mVideoEngine->SetTraceCallback(this);
          if (mError < 0) {
            ZS_LOG_ERROR(Detail, log("failed to set trace callback for video (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }
        }
#endif //ANDROID
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::destroyMediaEngine()
      {
#ifndef ANDROID
        // scope: delete voice engine
        {
          if (mVoiceBase) {
            mError = mVoiceBase->DeRegisterVoiceEngineObserver();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to deregister voice engine observer (error: ") + string(mVoiceBase->LastError()) + ")")
              return;
            }
            mError = mVoiceBase->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release voice base (error: ") + string(mVoiceBase->LastError()) + ")")
              return;
            }
          }
          
          if (mVoiceCodec) {
            mError = mVoiceCodec->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release voice codec (error: ") + string(mVoiceBase->LastError()) + ")")
              return;
            }
          }
          
          if (mVoiceNetwork) {
            mError = mVoiceNetwork->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release voice network (error: ") + string(mVoiceBase->LastError()) + ")")
              return;
            }
          }
          
          if (mVoiceRtpRtcp) {
            mError = mVoiceRtpRtcp->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release voice RTP/RTCP (error: ") + string(mVoiceBase->LastError()) + ")")
              return;
            }
          }
          
          if (mVoiceAudioProcessing) {
            mError = mVoiceAudioProcessing->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release audio processing (error: ") + string(mVoiceBase->LastError()) + ")")
              return;
            }
          }
          
          if (mVoiceVolumeControl) {
            mError = mVoiceVolumeControl->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release volume control (error: ") + string(mVoiceBase->LastError()) + ")")
              return;
            }
          }
          
          if (mVoiceHardware) {
            mError = mVoiceHardware->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release audio hardware (error: ") + string(mVoiceBase->LastError()) + ")")
              return;
            }
          }
          
          if (mVoiceFile) {
            mError = mVoiceFile->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release voice file (error: ") + string(mVoiceBase->LastError()) + ")")
              return;
            }
          }
          
          if (!VoiceEngine::Delete(mVoiceEngine)) {
            ZS_LOG_ERROR(Detail, log("failed to delete voice engine"))
            return;
          }
        }
        
        // scope; delete video engine
        {
          if (mVideoBase) {
            mError = mVideoBase->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release video base (error: ") + string(mVideoBase->LastError()) + ")")
              return;
            }
          }
          
          if (mVideoNetwork) {
            mError = mVideoNetwork->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release video network (error: ") + string(mVideoBase->LastError()) + ")")
              return;
            }
          }
          
          if (mVideoRender) {
            mError = mVideoRender->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release video render (error: ") + string(mVideoBase->LastError()) + ")")
              return;
            }
          }
          
          if (mVideoCapture) {
            mError = mVideoCapture->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release video capture (error: ") + string(mVideoBase->LastError()) + ")")
              return;
            }
          }
          
          if (mVideoRtpRtcp) {
            mError = mVideoRtpRtcp->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release video RTP/RTCP (error: ") + string(mVideoBase->LastError()) + ")")
              return;
            }
          }
          
          if (mVideoCodec) {
            mError = mVideoCodec->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release video codec (error: ") + string(mVideoBase->LastError()) + ")")
              return;
            }
          }
          
          if (mVideoFile) {
            mError = mVideoFile->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release video file (error: ") + string(mVideoBase->LastError()) + ")")
              return;
            }
          }

          if (!VideoEngine::Delete(mVideoEngine)) {
            ZS_LOG_ERROR(Detail, log("failed to delete video engine"))
            return;
          }
        }
#endif //ANDROID
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::setLogLevel()
      {
//        ILogger::setLogLevel("openpeer_webrtc", ILogger::Detail);
      }

      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine => IMediaEngine
      #pragma mark

      //-----------------------------------------------------------------------
      MediaEnginePtr MediaEngine::create(IMediaEngineDelegatePtr delegate)
      {
        MediaEnginePtr pThis(new MediaEngine(IStackForInternal::queueCore(), delegate));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      MediaEnginePtr MediaEngine::singleton(IMediaEngineDelegatePtr delegate)
      {
        static MediaEnginePtr engine = IMediaEngineFactory::singleton().createMediaEngine(delegate);
        return engine;
      }
      
      //-------------------------------------------------------------------------
      void MediaEngine::setDefaultVideoOrientation(VideoOrientations orientation)
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("set default video orientation - ") + IMediaEngine::toString(orientation))
        
        mDefaultVideoOrientation = orientation;
      }
      
      //-------------------------------------------------------------------------
      MediaEngine::VideoOrientations MediaEngine::getDefaultVideoOrientation()
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("get default video orientation"))
        
        return mDefaultVideoOrientation;
      }
      
      //-------------------------------------------------------------------------
      void MediaEngine::setRecordVideoOrientation(VideoOrientations orientation)
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("set record video orientation - ") + IMediaEngine::toString(orientation))
        
        mRecordVideoOrientation = orientation;
      }
      
      //-------------------------------------------------------------------------
      MediaEngine::VideoOrientations MediaEngine::getRecordVideoOrientation()
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("get record video orientation"))
        
        return mRecordVideoOrientation;
      }

      //-----------------------------------------------------------------------
      void MediaEngine::setVideoOrientation()
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("set video orientation and codec parameters"))
        
        if (mVideoChannel == OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL) {
          mError = setVideoCaptureRotation();
        } else {
          mError = setVideoCodecParameters();
        }
      }

      //-----------------------------------------------------------------------
      void MediaEngine::setCaptureRenderView(void *renderView)
      {
        AutoRecursiveLock lock(mLock);

        ZS_LOG_DEBUG(log("set capture render view"))

        mCaptureRenderView = renderView;
      }

      //-----------------------------------------------------------------------
      void MediaEngine::setChannelRenderView(void *renderView)
      {
        AutoRecursiveLock lock(mLock);

        ZS_LOG_DEBUG(log("set channel render view"))

        mChannelRenderView = renderView;
      }

      //-----------------------------------------------------------------------
      void MediaEngine::setEcEnabled(bool enabled)
      {
        AutoRecursiveLock lock(mLock);

        ZS_LOG_DEBUG(log("set EC enabled - value: ") + (enabled ? "true" : "false"))
#ifndef ANDROID
        webrtc::EcModes ecMode = getEcMode();
        if (ecMode == webrtc::kEcUnchanged) {
          return;
        }
        mError = mVoiceAudioProcessing->SetEcStatus(enabled, ecMode);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set acoustic echo canceller status (error: ") + string(mVoiceBase->LastError()) + ")")
          return;
        }
        if (ecMode == webrtc::kEcAecm && enabled) {
          mError = mVoiceAudioProcessing->SetAecmMode(webrtc::kAecmSpeakerphone);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to set acoustic echo canceller mobile mode (error: ") + string(mVoiceBase->LastError()) + ")")
            return;
          }
        }
#endif //ANDROID
        mEcEnabled = enabled;
      }

      //-----------------------------------------------------------------------
      void MediaEngine::setAgcEnabled(bool enabled)
      {
        AutoRecursiveLock lock(mLock);

        ZS_LOG_DEBUG(log("set AGC enabled - value: ") + (enabled ? "true" : "false"))
#ifndef ANDROID
        mError = mVoiceAudioProcessing->SetAgcStatus(enabled, webrtc::kAgcAdaptiveDigital);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set automatic gain control status (error: ") + string(mVoiceBase->LastError()) + ")")
          return;
        }
#endif //ANDROID
        mAgcEnabled = enabled;
      }

      //-----------------------------------------------------------------------
      void MediaEngine::setNsEnabled(bool enabled)
      {
        AutoRecursiveLock lock(mLock);

        ZS_LOG_DEBUG(log("set NS enabled - value: ") + (enabled ? "true" : "false"))
#ifndef ANDROID
        mError = mVoiceAudioProcessing->SetNsStatus(enabled, webrtc::kNsLowSuppression);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set noise suppression status (error: ") + string(mVoiceBase->LastError()) + ")")
          return;
        }
#endif //ANDROID
        mNsEnabled = enabled;
      }
      
      //-------------------------------------------------------------------------
      void MediaEngine::setVoiceRecordFile(String fileName)
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("set voice record file - value: ") + fileName)
        
        mVoiceRecordFile = fileName;
      }

      //-------------------------------------------------------------------------
      String MediaEngine::getVoiceRecordFile() const
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("get voice record file - value: ") + mVoiceRecordFile)
        
        return mVoiceRecordFile;
      }

      //-----------------------------------------------------------------------
      void MediaEngine::setMuteEnabled(bool enabled)
      {
        AutoRecursiveLock lock(mLock);

        ZS_LOG_DEBUG(log("set microphone mute enabled - value: ") + (enabled ? "true" : "false"))
#ifndef ANDROID
        mError = mVoiceVolumeControl->SetInputMute(-1, enabled);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set microphone mute (error: ") + string(mVoiceBase->LastError()) + ")")
          return;
        }
#endif //ANDROID
      }

      //-----------------------------------------------------------------------
      bool MediaEngine::getMuteEnabled()
      {
        AutoRecursiveLock lock(mLock);

        ZS_LOG_DEBUG(log("get microphone mute enabled"))

        bool enabled;
#ifndef ANDROID
        mError = mVoiceVolumeControl->GetInputMute(-1, enabled);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set microphone mute (error: ") + string(mVoiceBase->LastError()) + ")")
          return false;
        }
#endif //ANDROID
        return enabled;
      }

      //-----------------------------------------------------------------------
      void MediaEngine::setLoudspeakerEnabled(bool enabled)
      {
        AutoRecursiveLock lock(mLock);

        ZS_LOG_DEBUG(log("set loudspeaker enabled - value: ") + (enabled ? "true" : "false"))
#ifndef ANDROID
        mError = mVoiceHardware->SetLoudspeakerStatus(enabled);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set loudspeaker (error: ") + string(mVoiceBase->LastError()) + ")")
          return;
        }
#endif //ANDROID
      }

      //-----------------------------------------------------------------------
      bool MediaEngine::getLoudspeakerEnabled()
      {
        AutoRecursiveLock lock(mLock);

        ZS_LOG_DEBUG(log("get loudspeaker enabled"))

        bool enabled;
#ifndef ANDROID
        mError = mVoiceHardware->GetLoudspeakerStatus(enabled);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to get loudspeaker (error: ") + string(mVoiceBase->LastError()) + ")")
          return false;
        }
#endif //ANDROID
        return enabled;
      }

      //-----------------------------------------------------------------------
      IMediaEngine::OutputAudioRoutes MediaEngine::getOutputAudioRoute()
      {
        AutoRecursiveLock lock(mLock);

        ZS_LOG_DEBUG(log("get output audio route"))

        OutputAudioRoute route;
#ifndef ANDROID
        mError = mVoiceHardware->GetOutputAudioRoute(route);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to get output audio route (error: ") + string(mVoiceBase->LastError()) + ")")
          return OutputAudioRoute_BuiltInSpeaker;
        }
#endif //ANDROID
        switch (route) {
          case webrtc::kOutputAudioRouteHeadphone:
            return OutputAudioRoute_Headphone;
          case webrtc::kOutputAudioRouteBuiltInReceiver:
            return OutputAudioRoute_BuiltInReceiver;
          case webrtc::kOutputAudioRouteBuiltInSpeaker:
            return OutputAudioRoute_BuiltInSpeaker;
          default:
            return OutputAudioRoute_BuiltInSpeaker;
        }
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::setContinuousVideoCapture(bool continuousVideoCapture)
      {
        AutoRecursiveLock lock(mLifetimeLock);
        
        ZS_LOG_DEBUG(log("set continuous video capture - value: ") + (continuousVideoCapture ? "true" : "false"))
        
        mLifetimeContinuousVideoCapture = continuousVideoCapture;
      }
      
      //-----------------------------------------------------------------------
      bool MediaEngine::getContinuousVideoCapture()
      {
        AutoRecursiveLock lock(mLifetimeLock);
        
        ZS_LOG_DEBUG(log("get continuous video capture"))
        
        return mLifetimeContinuousVideoCapture;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::setFaceDetection(bool faceDetection)
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("set face detection - value: ") + (faceDetection ? "true" : "false"))
        
        mFaceDetection = faceDetection;
      }
      
      //-----------------------------------------------------------------------
      bool MediaEngine::getFaceDetection()
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("get face detection"))
        
        return mFaceDetection;
      }

      //-----------------------------------------------------------------------
      IMediaEngine::CameraTypes MediaEngine::getCameraType() const
      {
        AutoRecursiveLock lock(mLifetimeLock);  // WARNING: THIS IS THE LIFETIME LOCK AND NOT THE MAIN OBJECT LOCK
        return mLifetimeWantCameraType;
      }

      //-----------------------------------------------------------------------
      void MediaEngine::setCameraType(CameraTypes type)
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantCameraType = type;
        }

        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::startVideoCapture()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantVideoCapture = true;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::stopVideoCapture()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantVideoCapture = false;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-------------------------------------------------------------------------
      void MediaEngine::startRecordVideoCapture(String fileName, bool saveToLibrary)
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantRecordVideoCapture = true;
          mLifetimeVideoRecordFile = fileName;
          mLifetimeSaveVideoToLibrary = saveToLibrary;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-------------------------------------------------------------------------
      void MediaEngine::stopRecordVideoCapture()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantRecordVideoCapture = false;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }

      //-----------------------------------------------------------------------
      int MediaEngine::getVideoTransportStatistics(RtpRtcpStatistics &stat)
      {
        AutoRecursiveLock lock(mLock);

        unsigned short fractionLost;
        unsigned int cumulativeLost;
        unsigned int extendedMax;
        unsigned int jitter;
        int rttMs;
#ifndef ANDROID
        mError = mVideoRtpRtcp->GetReceivedRTCPStatistics(mVideoChannel, fractionLost, cumulativeLost, extendedMax, jitter, rttMs);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("failed to get received RTCP statistics for video (error: ") + string(mVideoBase->LastError()) + ")")
          return mError;
        }

        unsigned int bytesSent;
        unsigned int packetsSent;
        unsigned int bytesReceived;
        unsigned int packetsReceived;

        mError = mVideoRtpRtcp->GetRTPStatistics(mVideoChannel, bytesSent, packetsSent, bytesReceived, packetsReceived);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("failed to get RTP statistics for video (error: ") + string(mVideoBase->LastError()) + ")")
          return mError;
        }

        stat.fractionLost = fractionLost;
        stat.cumulativeLost = cumulativeLost;
        stat.extendedMax = extendedMax;
        stat.jitter = jitter;
        stat.rttMs = rttMs;
        stat.bytesSent = bytesSent;
        stat.packetsSent = packetsSent;
        stat.bytesReceived = bytesReceived;
        stat.packetsReceived = packetsReceived;
#endif //ANDROID
        return 0;
      }

      //-----------------------------------------------------------------------
      int MediaEngine::getVoiceTransportStatistics(RtpRtcpStatistics &stat)
      {
        AutoRecursiveLock lock(mLock);
#ifndef ANDROID
        webrtc::CallStatistics callStat;

        mError = mVoiceRtpRtcp->GetRTCPStatistics(mVoiceChannel, callStat);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("failed to get RTCP statistics for voice (error: ") + string(mVoiceBase->LastError()) + ")")
          return mError;
        }

        stat.fractionLost = callStat.fractionLost;
        stat.cumulativeLost = callStat.cumulativeLost;
        stat.extendedMax = callStat.extendedMax;
        stat.jitter = callStat.jitterSamples;
        stat.rttMs = callStat.rttMs;
        stat.bytesSent = callStat.bytesSent;
        stat.packetsSent = callStat.packetsSent;
        stat.bytesReceived = callStat.bytesReceived;
        stat.packetsReceived = callStat.packetsReceived;
#endif //ANDROID
        return 0;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine => IMediaEngineForStack
      #pragma mark

      //-----------------------------------------------------------------------
      void MediaEngine::setup(IMediaEngineDelegatePtr delegate)
      {
        singleton(delegate);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine => IMediaEngineForCallTransport
      #pragma mark

      //-----------------------------------------------------------------------
      void MediaEngine::startVoice()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantAudio = true;
        }

        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }

      //-----------------------------------------------------------------------
      void MediaEngine::stopVoice()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantAudio = false;
        }

        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }

      //-----------------------------------------------------------------------
      void MediaEngine::startVideoChannel()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantVideoChannel = true;
        }

        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }

      //-----------------------------------------------------------------------
      void MediaEngine::stopVideoChannel()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantVideoChannel = false;
        }

        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }

      //-----------------------------------------------------------------------
      int MediaEngine::registerVoiceExternalTransport(Transport &transport)
      {
        AutoRecursiveLock lock(mLock);

        ZS_LOG_DEBUG(log("register voice external transport"))
#ifndef ANDROID
        mRedirectVoiceTransport.redirect(&transport);
#endif
        return 0;
      }

      //-----------------------------------------------------------------------
      int MediaEngine::deregisterVoiceExternalTransport()
      {
        AutoRecursiveLock lock(mLock);

        ZS_LOG_DEBUG(log("deregister voice external transport"))
#ifndef ANDROID
        mRedirectVoiceTransport.redirect(NULL);
#endif
        return 0;
      }

      //-----------------------------------------------------------------------
      int MediaEngine::receivedVoiceRTPPacket(const void *data, unsigned int length)
      {
#ifndef ANDROID
        int channel = OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL;
        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          if (mVoiceEngineReady)
            channel = mVoiceChannel;
        }

        if (OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL == channel) {
          ZS_LOG_WARNING(Debug, log("voice channel is not ready yet"))
          return -1;
        }

        mError = mVoiceNetwork->ReceivedRTPPacket(channel, data, length);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("received voice RTP packet failed (error: ") + string(mVoiceBase->LastError()) + ")")
          return mError;
        }
#endif
        return 0;
      }

      //-----------------------------------------------------------------------
      int MediaEngine::receivedVoiceRTCPPacket(const void* data, unsigned int length)
      {
#ifndef ANDROID
        int channel = OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL;
        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          if (mVoiceEngineReady)
            channel = mVoiceChannel;
        }

        if (OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL == channel) {
          ZS_LOG_WARNING(Debug, log("voice channel is not ready yet"))
          return -1;
        }

        mError = mVoiceNetwork->ReceivedRTCPPacket(channel, data, length);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("received voice RTCP packet failed (error: ") + string(mVoiceBase->LastError()) + ")")
          return mError;
        }
#endif //ANDROID
        return 0;
      }

      //-----------------------------------------------------------------------
      int MediaEngine::registerVideoExternalTransport(Transport &transport)
      {
        AutoRecursiveLock lock(mLock);

        ZS_LOG_DEBUG(log("register video external transport"))
#ifndef ANDROID
        mRedirectVideoTransport.redirect(&transport);
#endif //ANDROID
        return 0;
      }

      //-----------------------------------------------------------------------
      int MediaEngine::deregisterVideoExternalTransport()
      {
        AutoRecursiveLock lock(mLock);

        ZS_LOG_DEBUG(log("deregister video external transport"))
#ifndef ANDROID
        mRedirectVideoTransport.redirect(NULL);
#endif //ANDROID
        return 0;
      }

      //-----------------------------------------------------------------------
      int MediaEngine::receivedVideoRTPPacket(const void *data, const int length)
      {
#ifndef ANDROID
        int channel = OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL;
        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          if (mVideoEngineReady)
            channel = mVideoChannel;
        }

        if (OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL == channel) {
          ZS_LOG_WARNING(Debug, log("video channel is not ready yet"))
          return -1;
        }

        mError = mVideoNetwork->ReceivedRTPPacket(channel, data, length);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("received video RTP packet failed (error: ") + string(mVideoBase->LastError()) + ")")
          return mError;
        }
#endif
        return 0;
      }

      //-----------------------------------------------------------------------
      int MediaEngine::receivedVideoRTCPPacket(const void *data, const int length)
      {
#ifndef ANDROID
        int channel = OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL;
        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          if (mVideoEngineReady)
            channel = mVideoChannel;
        }

        if (OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL == channel) {
          ZS_LOG_WARNING(Debug, log("video channel is not ready yet"))
          return -1;
        }

        mError = mVideoNetwork->ReceivedRTCPPacket(channel, data, length);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("received video RTCP packet failed (error: ") + string(mVideoBase->LastError()) + ")")
          return mError;
        }
#endif //ANDROID
        return 0;
      }

      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine => TraceCallback
      #pragma mark
      //-----------------------------------------------------------------------
      void MediaEngine::Print(const webrtc::TraceLevel level, const char *traceString, const int length)
      {
#ifndef ANDROID
        switch (level) {
          case webrtc::kTraceApiCall:
          case webrtc::kTraceStateInfo:
            ZS_LOG_DETAIL(log(traceString))
            break;
          case webrtc::kTraceDebug:
          case webrtc::kTraceInfo:
            ZS_LOG_DEBUG(log(traceString))
            break;
          case webrtc::kTraceWarning:
            ZS_LOG_WARNING(Detail, log(traceString))
            break;
          case webrtc::kTraceError:
            ZS_LOG_ERROR(Detail, log(traceString))
            break;
          case webrtc::kTraceCritical:
            ZS_LOG_FATAL(Detail, log(traceString))
            break;
          case webrtc::kTraceModuleCall:
          case webrtc::kTraceMemory:
          case webrtc::kTraceTimer:
          case webrtc::kTraceStream:
            ZS_LOG_TRACE(log(traceString))
            break;
          default:
            ZS_LOG_TRACE(log(traceString))
            break;
        }
#endif //ANDROID
      }

      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine => VoiceEngineObserver
      #pragma mark
      //-----------------------------------------------------------------------
      void MediaEngine::CallbackOnError(const int errCode, const int channel)
      {
        ZS_LOG_ERROR(Detail, log("Voice engine error: ") + string(errCode) + ")")
      }

      //-----------------------------------------------------------------------
      void MediaEngine::CallbackOnOutputAudioRouteChange(const webrtc::OutputAudioRoute inRoute)
      {
        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("audio route change callback igored as delegate was not specified"))
          return;
        }

        OutputAudioRoutes route = IMediaEngine::OutputAudioRoute_Headphone;

        switch (inRoute) {
          case webrtc::kOutputAudioRouteHeadphone:        route = IMediaEngine::OutputAudioRoute_Headphone;  break;
          case webrtc::kOutputAudioRouteBuiltInReceiver:  route = IMediaEngine::OutputAudioRoute_BuiltInReceiver; break;
          case webrtc::kOutputAudioRouteBuiltInSpeaker:   route = IMediaEngine::OutputAudioRoute_BuiltInSpeaker; break;
          default: {
            ZS_LOG_WARNING(Basic, log("media route changed to unknown type") + ", value=" + string(inRoute))
            break;
          }
        }

        try {
          if (mDelegate)
            mDelegate->onMediaEngineAudioRouteChanged(route);
        } catch (IMediaEngineDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }

        ZS_LOG_DEBUG(log("Audio output route changed") + ", route=" + IMediaEngine::toString(route))
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine => ViECaptureObserver
      #pragma mark
      //-------------------------------------------------------------------------
      void MediaEngine::BrightnessAlarm(const int capture_id, const webrtc::Brightness brightness)
      {
        
      }
      
      //-------------------------------------------------------------------------
      void MediaEngine::CapturedFrameRate(const int capture_id, const unsigned char frame_rate)
      {
        
      }
      
      //-------------------------------------------------------------------------
      void MediaEngine::NoPictureAlarm(const int capture_id, const webrtc::CaptureAlarm alarm)
      {
        
      }
      
      //-------------------------------------------------------------------------
      void MediaEngine::FaceDetected(const int capture_id)
      {
        try {
          if (mDelegate)
            mDelegate->onMediaEngineFaceDetected();
        } catch (IMediaEngineDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      void MediaEngine::operator()()
      {
  #if !defined(_ANDROID) && !defined(_LINUX)
  # ifdef __QNX__
        pthread_setname_np(pthread_self(), "org.openpeer.core.mediaEngine");
  # else
        pthread_setname_np("org.openpeer.core.mediaEngine");
  # endif
  #endif
        ZS_LOG_DEBUG(log("media engine lifetime thread spawned"))

        bool repeat = false;

        bool firstAttempt = true;

        bool wantAudio = false;
        bool wantVideoCapture = false;
        bool wantVideoChannel = false;
        bool wantRecordVideoCapture = false;
        bool hasAudio = false;
        bool hasVideoCapture = false;
        bool hasVideoChannel = false;
        bool hasRecordVideoCapture = false;
        CameraTypes wantCameraType = IMediaEngine::CameraType_None;
        String videoRecordFile;
        bool saveVideoToLibrary;

        // attempt to get the lifetime lock
        while (true)
        {
          if (!firstAttempt) {
            boost::thread::yield();       // do not hammer CPU
          }
          firstAttempt = false;

          AutoRecursiveLock lock(mLifetimeLock);
          if (mLifetimeInProgress) {
            ZS_LOG_WARNING(Debug, log("could not obtain media lifetime lock"))
            continue;
          }

          mLifetimeInProgress = true;

          if (mLifetimeWantVideoChannel)
            mLifetimeWantVideoCapture = true;
          else if (mLifetimeHasVideoChannel && !mLifetimeContinuousVideoCapture)
            mLifetimeWantVideoCapture = false;
          if (!mLifetimeWantVideoCapture)
            mLifetimeWantRecordVideoCapture = false;
          wantAudio = mLifetimeWantAudio;
          wantVideoCapture = mLifetimeWantVideoCapture;
          wantVideoChannel = mLifetimeWantVideoChannel;
          wantRecordVideoCapture = mLifetimeWantRecordVideoCapture;
          hasAudio = mLifetimeHasAudio;
          hasVideoCapture = mLifetimeHasVideoCapture;
          hasVideoChannel = mLifetimeHasVideoChannel;
          hasRecordVideoCapture = mLifetimeHasRecordVideoCapture;
          wantCameraType = mLifetimeWantCameraType;
          videoRecordFile = mLifetimeVideoRecordFile;
          saveVideoToLibrary = mLifetimeSaveVideoToLibrary;
          break;
        }

        {
          AutoRecursiveLock lock(mLock);

          if (wantVideoCapture) {
            if (wantCameraType != mCameraType) {
              ZS_LOG_DEBUG(log("camera type needs to change") + ", was=" + IMediaEngine::toString(mCameraType) + ", desired=" + IMediaEngine::toString(wantCameraType))
              mCameraType = wantCameraType;
              if (hasVideoCapture) {
                ZS_LOG_DEBUG(log("video capture must be stopped first before camera type can be swapped (will try again)"))
                wantVideoCapture = false;  // pretend that we don't want video so it will be stopped
                repeat = true;      // repeat this thread operation again to start video back up again after
                if (hasVideoChannel) {
                  ZS_LOG_DEBUG(log("video channel must be stopped first before camera type can be swapped (will try again)"))
                  wantVideoChannel = false;  // pretend that we don't want video so it will be stopped
                }
              }
            }
          }
          
          if (wantVideoCapture) {
            if (!hasVideoCapture) {
              internalStartVideoCapture();
            }
          }
          
          if (wantRecordVideoCapture) {
            if (!hasRecordVideoCapture) {
              internalStartRecordVideoCapture(videoRecordFile, saveVideoToLibrary);
            }
          } else {
            if (hasRecordVideoCapture) {
              internalStopRecordVideoCapture();
            }
          }

          if (wantAudio) {
            if (!hasAudio) {
              internalStartVoice();
            }
          } else {
            if (hasAudio) {
              internalStopVoice();
            }
          }
          
          if (wantVideoChannel) {
            if (!hasVideoChannel) {
              internalStartVideoChannel();
            }
          } else {
            if (hasVideoChannel) {
              internalStopVideoChannel();
            }
          }
          
          if (!wantVideoCapture) {
            if (hasVideoCapture) {
              internalStopVideoCapture();
            }
          }
        }

        {
          AutoRecursiveLock lock(mLifetimeLock);

          mLifetimeHasAudio = wantAudio;
          mLifetimeHasVideoCapture = wantVideoCapture;
          mLifetimeHasVideoChannel = wantVideoChannel;
          mLifetimeHasRecordVideoCapture = wantRecordVideoCapture;

          mLifetimeInProgress = false;
        }

        if (repeat) {
          ZS_LOG_DEBUG(log("repeating media thread operation again"))
          (*this)();
          return;
        }

        ZS_LOG_DEBUG(log("media engine lifetime thread completed"))
      }

      //-----------------------------------------------------------------------
      void MediaEngine::internalStartVoice()
      {
#ifndef ANDROID
        {
          AutoRecursiveLock lock(mLock);

          ZS_LOG_DEBUG(log("start voice"))

          mVoiceChannel = mVoiceBase->CreateChannel();
          if (mVoiceChannel < 0) {
            ZS_LOG_ERROR(Detail, log("could not create voice channel (error: ") + string(mVoiceBase->LastError()) + ")")
            mVoiceChannel = OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL;
            return;
          }

          mError = registerVoiceTransport();
          if (mError != 0)
            return;

          webrtc::EcModes ecMode = getEcMode();
          if (ecMode == webrtc::kEcUnchanged) {
            return;
          }
          mError = mVoiceAudioProcessing->SetEcStatus(mEcEnabled, ecMode);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to set acoustic echo canceller status (error: ") + string(mVoiceBase->LastError()) + ")")
            return;
          }
          if (ecMode == webrtc::kEcAecm && mEcEnabled) {
            mError = mVoiceAudioProcessing->SetAecmMode(webrtc::kAecmSpeakerphone);
            if (mError != 0) {
              ZS_LOG_ERROR(Detail, log("failed to set acoustic echo canceller mobile mode (error: ") + string(mVoiceBase->LastError()) + ")")
              return;
            }
          }
          mError = mVoiceAudioProcessing->SetAgcStatus(mAgcEnabled, webrtc::kAgcAdaptiveDigital);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to set automatic gain control status (error: ") + string(mVoiceBase->LastError()) + ")")
            return;
          }
          mError = mVoiceAudioProcessing->SetNsStatus(mNsEnabled, webrtc::kNsLowSuppression);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to set noise suppression status (error: ") + string(mVoiceBase->LastError()) + ")")
            return;
          }

          mError = mVoiceVolumeControl->SetInputMute(-1, false);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to set microphone mute (error: ") + string(mVoiceBase->LastError()) + ")")
            return;
          }
#ifdef TARGET_OS_IPHONE
          mError = mVoiceHardware->SetLoudspeakerStatus(false);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to set loudspeaker (error: ") + string(mVoiceBase->LastError()) + ")")
            return;
          }
#endif
          webrtc::CodecInst cinst;
          memset(&cinst, 0, sizeof(webrtc::CodecInst));
          for (int idx = 0; idx < mVoiceCodec->NumOfCodecs(); idx++) {
            mError = mVoiceCodec->GetCodec(idx, cinst);
            if (mError != 0) {
              ZS_LOG_ERROR(Detail, log("failed to get voice codec (error: ") + string(mVoiceBase->LastError()) + ")")
              return;
            }
#ifdef OPENPEER_MEDIA_ENGINE_VOICE_CODEC_ISAC
            if (strcmp(cinst.plname, "ISAC") == 0) {
              strcpy(cinst.plname, "ISAC");
              cinst.pltype = 103;
              cinst.rate = 32000;
              cinst.pacsize = 480; // 30ms
              cinst.plfreq = 16000;
              cinst.channels = 1;
              mError = mVoiceCodec->SetSendCodec(mVoiceChannel, cinst);
              if (mError != 0) {
                ZS_LOG_ERROR(Detail, log("failed to set send voice codec (error: ") + string(mVoiceBase->LastError()) + ")")
                return;
              }
              break;
            }
#elif defined OPENPEER_MEDIA_ENGINE_VOICE_CODEC_OPUS
            if (strcmp(cinst.plname, "OPUS") == 0) {
              strcpy(cinst.plname, "OPUS");
              cinst.pltype = 110;
              cinst.rate = 20000;
              cinst.pacsize = 320; // 20ms
              cinst.plfreq = 16000;
              cinst.channels = 1;
              mError = mVoiceCodec->SetSendCodec(mVoiceChannel, cinst);
              if (mError != 0) {
                ZS_LOG_ERROR(Detail, log("failed to set send voice codec (error: ") + string(mVoiceBase->LastError()) + ")")
                return;
              }
              break;
            }
#endif
          }

          webrtc::CodecInst cfinst;
          memset(&cfinst, 0, sizeof(webrtc::CodecInst));
          for (int idx = 0; idx < mVoiceCodec->NumOfCodecs(); idx++) {
            mError = mVoiceCodec->GetCodec(idx, cfinst);
            if (mError != 0) {
              ZS_LOG_ERROR(Detail, log("failed to get voice codec (error: ") + string(mVoiceBase->LastError()) + ")")
              return;
            }
            if (strcmp(cfinst.plname, "VORBIS") == 0) {
              strcpy(cfinst.plname, "VORBIS");
              cfinst.pltype = 109;
              cfinst.rate = 32000;
              cfinst.pacsize = 480; // 30ms
              cfinst.plfreq = 16000;
              cfinst.channels = 1;
              break;
            }
          }

          mError = setVoiceTransportParameters();
          if (mError != 0)
            return;
          
          mError = mVoiceBase->StartSend(mVoiceChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to start sending voice (error: ") + string(mVoiceBase->LastError()) + ")")
            return;
          }

          mError = mVoiceBase->StartReceive(mVoiceChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to start receiving voice (error: ") + string(mVoiceBase->LastError()) + ")")
            return;
          }
          mError = mVoiceBase->StartPlayout(mVoiceChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to start playout (error: ") + string(mVoiceBase->LastError()) + ")")
            return;
          }
          if (!mVoiceRecordFile.empty()) {
            mError = mVoiceFile->StartRecordingCall(mVoiceRecordFile, &cfinst);
            if (mError != 0) {
              ZS_LOG_ERROR(Detail, log("failed to start call recording (error: ") + string(mVoiceBase->LastError()) + ")")
            }
          }
        }
#endif //ANDROID
        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          mVoiceEngineReady = true;
        }
      }

      //-----------------------------------------------------------------------
      void MediaEngine::internalStopVoice()
      {
        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          mVoiceEngineReady = false;
        }

        {
          AutoRecursiveLock lock(mLock);

          ZS_LOG_DEBUG(log("stop voice"))
#ifndef ANDROID
          mError = mVoiceBase->StopSend(mVoiceChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to stop sending voice (error: ") + string(mVoiceBase->LastError()) + ")")
            return;
          }
          mError = mVoiceBase->StopPlayout(mVoiceChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to stop playout (error: ") + string(mVoiceBase->LastError()) + ")")
            return;
          }
          mError = mVoiceBase->StopReceive(mVoiceChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to stop receiving voice (error: ") + string(mVoiceBase->LastError()) + ")")
            return;
          }
          if (!mVoiceRecordFile.empty()) {
            mError = mVoiceFile->StopRecordingCall();
            if (mError != 0) {
              ZS_LOG_ERROR(Detail, log("failed to stop call recording (error: ") + string(mVoiceBase->LastError()) + ")")
            }
            mVoiceRecordFile.erase();
          }
          mError = mVoiceNetwork->DeRegisterExternalTransport(mVoiceChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to deregister voice external transport (error: ") + string(mVoiceBase->LastError()) + ")")
            return;
          }
          mError = mVoiceBase->DeleteChannel(mVoiceChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to delete voice channel (error: ") + string(mVoiceBase->LastError()) + ")")
            return;
          }
#endif //ANDROID
          mVoiceChannel = OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL;
        }
      }
      
      //-----------------------------------------------------------------------
      int MediaEngine::registerVoiceTransport()
      {
#ifndef ANDROID
        if (NULL != mVoiceTransport) {
          mError = mVoiceNetwork->RegisterExternalTransport(mVoiceChannel, *mVoiceTransport);
          if (0 != mError) {
            ZS_LOG_ERROR(Detail, log("failed to register voice external transport (error: ") + string(mVoiceBase->LastError()) + ")")
            return mError;
          }
        } else {
          ZS_LOG_ERROR(Detail, log("external voice transport is not set"))
          return -1;
        }
#endif //ANDROID
        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngine::setVoiceTransportParameters()
      {
        // No transport parameters for external transport.
        return 0;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::internalStartVideoCapture()
      {
#ifndef ANDROID
        {
          AutoRecursiveLock lock(mLock);
          
          ZS_LOG_DEBUG(log("start video capture - camera type: ") + (mCameraType == CameraType_Back ? "back" : "front"))

          const unsigned int KMaxDeviceNameLength = 128;
          const unsigned int KMaxUniqueIdLength = 256;
          char deviceName[KMaxDeviceNameLength];
          memset(deviceName, 0, KMaxDeviceNameLength);
          char uniqueId[KMaxUniqueIdLength];
          memset(uniqueId, 0, KMaxUniqueIdLength);
          uint32_t captureIdx;
          
          if (mCameraType == CameraType_Back)
          {
            captureIdx = 0;
          }
          else if (mCameraType == CameraType_Front)
          {
            captureIdx = 1;
          }
          else
          {
            ZS_LOG_ERROR(Detail, log("camera type is not set"))
            return;
          }
          
#if defined(TARGET_OS_IPHONE) || defined(__QNX__)
          void *captureView = mCaptureRenderView;
#else
          void *captureView = NULL;
#endif
#ifndef __QNX__
          if (captureView == NULL) {
            ZS_LOG_ERROR(Detail, log("capture view is not set"))
            return;
          }
#endif
          
          webrtc::VideoCaptureModule::DeviceInfo *devInfo = webrtc::VideoCaptureFactory::CreateDeviceInfo(0);
          if (devInfo == NULL) {
            ZS_LOG_ERROR(Detail, log("failed to create video capture device info"))
            return;
          }
          
          mError = devInfo->GetDeviceName(captureIdx, deviceName,
                                          KMaxDeviceNameLength, uniqueId,
                                          KMaxUniqueIdLength);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to get video device name"))
            return;
          }
          
          strcpy(mDeviceUniqueId, uniqueId);

          mVcpm = webrtc::VideoCaptureFactory::Create(1, uniqueId);
          if (mVcpm == NULL) {
            ZS_LOG_ERROR(Detail, log("failed to create video capture module"))
            return;
          }
          
          mError = mVideoCapture->AllocateCaptureDevice(*mVcpm, mCaptureId);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to allocate video capture device (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }
          mVcpm->AddRef();
          delete devInfo;
          
          mError = mVideoCapture->RegisterObserver(mCaptureId, *this);
          if (mError < 0) {
            ZS_LOG_ERROR(Detail, log("failed to register video capture observer (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }
          
#ifdef TARGET_OS_IPHONE
          webrtc::CapturedFrameOrientation defaultOrientation;
          switch (mDefaultVideoOrientation) {
            case IMediaEngine::VideoOrientation_LandscapeLeft:
              defaultOrientation = webrtc::CapturedFrameOrientation_LandscapeLeft;
              break;
            case IMediaEngine::VideoOrientation_PortraitUpsideDown:
              defaultOrientation = webrtc::CapturedFrameOrientation_PortraitUpsideDown;
              break;
            case IMediaEngine::VideoOrientation_LandscapeRight:
              defaultOrientation = webrtc::CapturedFrameOrientation_LandscapeRight;
              break;
            case IMediaEngine::VideoOrientation_Portrait:
              defaultOrientation = webrtc::CapturedFrameOrientation_Portrait;
              break;
            default:
              defaultOrientation = webrtc::CapturedFrameOrientation_LandscapeLeft;
              break;
          }
          mError = mVideoCapture->SetDefaultCapturedFrameOrientation(mCaptureId, defaultOrientation);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to set default orientation on video capture device (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }
          
          setVideoCaptureRotation();
          
          webrtc::RotateCapturedFrame orientation;
          mError = mVideoCapture->GetOrientation(mDeviceUniqueId, orientation);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to get orientation from video capture device (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }
#else
          webrtc::RotateCapturedFrame orientation = webrtc::RotateCapturedFrame_0;
#endif
          
          int width = 0, height = 0, maxFramerate = 0, maxBitrate = 0;
          mError = getVideoCaptureParameters(orientation, width, height, maxFramerate, maxBitrate);
          if (mError != 0)
            return;

          webrtc::CaptureCapability capability;
          capability.width = width;
          capability.height = height;
          capability.maxFPS = maxFramerate;
          capability.rawType = webrtc::kVideoI420;
          capability.faceDetection = mFaceDetection;
          mError = mVideoCapture->StartCapture(mCaptureId, capability);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to start capturing (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }

#ifndef __QNX__
          mError = mVideoRender->AddRenderer(mCaptureId, captureView, 0, 0.0F, 0.0F, 1.0F,
                                             1.0F);
          if (0 != mError) {
            ZS_LOG_ERROR(Detail, log("failed to add renderer for video capture (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }
          
          mError = mVideoRender->StartRender(mCaptureId);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to start rendering video capture (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }
#endif
        }
#endif //ANDROID
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::internalStopVideoCapture()
      {
#ifndef ANDROID
        {
          AutoRecursiveLock lock(mLock);
          
          ZS_LOG_DEBUG(log("stop video capture"))

#ifndef __QNX__
          mError = mVideoRender->StopRender(mCaptureId);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to stop rendering video capture (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }
          mError = mVideoRender->RemoveRenderer(mCaptureId);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to remove renderer for video capture (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }
#endif
          mError = mVideoCapture->StopCapture(mCaptureId);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to stop video capturing (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }
          mError = mVideoCapture->ReleaseCaptureDevice(mCaptureId);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to release video capture device (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }
          
          if (mVcpm != NULL)
            mVcpm->Release();
          
          mVcpm = NULL;
        }
#endif //ANDROID
      }

      //-----------------------------------------------------------------------
      void MediaEngine::internalStartVideoChannel()
      {
#ifndef ANDROID
        {
          AutoRecursiveLock lock(mLock);
          
          ZS_LOG_DEBUG(log("start video channel"))
          
#if defined(TARGET_OS_IPHONE) || defined(__QNX__)
          void *channelView = mChannelRenderView;
#else
          void *channelView = NULL;
#endif
          if (channelView == NULL) {
            ZS_LOG_ERROR(Detail, log("channel view is not set"))
            return;
          }

          mError = mVideoBase->CreateChannel(mVideoChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("could not create video channel (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }

          mError = registerVideoTransport();
          if (0 != mError)
            return;
          
          mError = mVideoNetwork->SetMTU(mVideoChannel, mMtu);
          if (0 != mError) {
            ZS_LOG_ERROR(Detail, log("failed to set MTU for video channel (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }

          mError = mVideoCapture->ConnectCaptureDevice(mCaptureId, mVideoChannel);
          if (0 != mError) {
            ZS_LOG_ERROR(Detail, log("failed to connect capture device to video channel (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }

          mError = mVideoRtpRtcp->SetRTCPStatus(mVideoChannel, webrtc::kRtcpCompound_RFC4585);
          if (0 != mError) {
            ZS_LOG_ERROR(Detail, log("failed to set video RTCP status (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }

          mError = mVideoRtpRtcp->SetKeyFrameRequestMethod(mVideoChannel,
                                                           webrtc::kViEKeyFrameRequestPliRtcp);
          if (0 != mError) {
            ZS_LOG_ERROR(Detail, log("failed to set key frame request method (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }

          mError = mVideoRtpRtcp->SetTMMBRStatus(mVideoChannel, true);
          if (0 != mError) {
            ZS_LOG_ERROR(Detail, log("failed to set temporary max media bit rate status (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }

#ifdef TARGET_OS_IPHONE
          OutputAudioRoute route;
          mError = mVoiceHardware->GetOutputAudioRoute(route);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to get output audio route (error: ") + string(mVoiceBase->LastError()) + ")")
            return;
          }
          if (route != webrtc::kOutputAudioRouteHeadphone)
          {
            mError = mVoiceHardware->SetLoudspeakerStatus(true);
            if (mError != 0) {
              ZS_LOG_ERROR(Detail, log("failed to set loudspeaker (error: ") + string(mVoiceBase->LastError()) + ")")
              return;
            }
          }
#endif

          mError = mVideoRender->AddRenderer(mVideoChannel, channelView, 0, 0.0F, 0.0F, 1.0F,
                                             1.0F);
          if (0 != mError) {
            ZS_LOG_ERROR(Detail, log("failed to add renderer for video channel (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }

          webrtc::VideoCodec videoCodec;
          memset(&videoCodec, 0, sizeof(VideoCodec));
          for (int idx = 0; idx < mVideoCodec->NumberOfCodecs(); idx++) {
            mError = mVideoCodec->GetCodec(idx, videoCodec);
            if (mError != 0) {
              ZS_LOG_ERROR(Detail, log("failed to get video codec (error: ") + string(mVideoBase->LastError()) + ")")
              return;
            }
            if (videoCodec.codecType == webrtc::kVideoCodecVP8) {
              mError = mVideoCodec->SetSendCodec(mVideoChannel, videoCodec);
              if (mError != 0) {
                ZS_LOG_ERROR(Detail, log("failed to set send video codec (error: ") + string(mVideoBase->LastError()) + ")")
                return;
              }
              break;
            }
          }

          mError = setVideoCodecParameters();
          if (mError != 0) {
            return;
          }
          
          mError = setVideoTransportParameters();
          if (mError != 0)
            return;
          
          mError = mVideoBase->StartSend(mVideoChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to start sending video (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }

          mError = mVideoBase->StartReceive(mVideoChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to start receiving video (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }
          mError = mVideoRender->StartRender(mVideoChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to start rendering video channel (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }
        }

        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          mVideoEngineReady = true;
        }
#endif //ANDROID
        
        sleep(1);
        
        mError = mVideoRtpRtcp->RequestKeyFrame(mVideoChannel);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("failed to request key frame (error: ") + string(mVideoBase->LastError()) + ")")
          return;
        }
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::internalStopVideoChannel()
      {
#ifndef ANDROID
        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          mVideoEngineReady = false;
        }

        {
          AutoRecursiveLock lock(mLock);

          ZS_LOG_DEBUG(log("stop video channel"))

          mError = mVideoRender->StopRender(mVideoChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to stop rendering video channel (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }
          mError = mVideoRender->RemoveRenderer(mVideoChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to remove renderer for video channel (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }
          mError = mVideoBase->StopSend(mVideoChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to stop sending video (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }
          mError = mVideoBase->StopReceive(mVideoChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to stop receiving video (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }
          mError = mVideoCapture->DisconnectCaptureDevice(mVideoChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to disconnect capture device from video channel (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }
          mError = deregisterVideoTransport();
          if (0 != mError)
            return;
          mError = mVideoBase->DeleteChannel(mVideoChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to delete video channel (error: ") + string(mVideoBase->LastError()) + ")")
            return;
          }

          mVideoChannel = OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL;
        }
#endif //ANDROID
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::internalStartRecordVideoCapture(String videoRecordFile, bool saveVideoToLibrary)
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("start video capture recording"))
#ifndef ANDROID
        webrtc::CapturedFrameOrientation recordOrientation;
        switch (mRecordVideoOrientation) {
          case IMediaEngine::VideoOrientation_LandscapeLeft:
            recordOrientation = webrtc::CapturedFrameOrientation_LandscapeLeft;
            break;
          case IMediaEngine::VideoOrientation_PortraitUpsideDown:
            recordOrientation = webrtc::CapturedFrameOrientation_PortraitUpsideDown;
            break;
          case IMediaEngine::VideoOrientation_LandscapeRight:
            recordOrientation = webrtc::CapturedFrameOrientation_LandscapeRight;
            break;
          case IMediaEngine::VideoOrientation_Portrait:
            recordOrientation = webrtc::CapturedFrameOrientation_Portrait;
            break;
          default:
            recordOrientation = webrtc::CapturedFrameOrientation_LandscapeLeft;
            break;
        }
        mError = mVideoCapture->SetCapturedFrameLockedOrientation(mCaptureId, recordOrientation);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set record orientation on video capture device (error: ") + string(mVideoBase->LastError()) + ")")
          return;
        }
        mError = mVideoCapture->EnableCapturedFrameOrientationLock(mCaptureId, true);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to enable orientation lock on video capture device (error: ") + string(mVideoBase->LastError()) + ")")
          return;
        }
        
        webrtc::RotateCapturedFrame orientation;
        mError = mVideoCapture->GetOrientation(mDeviceUniqueId, orientation);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to get orientation from video capture device (error: ") + string(mVideoBase->LastError()) + ")")
          return;
        }
        
        if (mVideoChannel == OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL)
          setVideoCaptureRotation();
        else
          setVideoCodecParameters();
        
        int width = 0, height = 0, maxFramerate = 0, maxBitrate = 0;
        mError = getVideoCaptureParameters(orientation, width, height, maxFramerate, maxBitrate);
        if (mError != 0)
          return;
        
        webrtc::CodecInst audioCodec;
        memset(&audioCodec, 0, sizeof(webrtc::CodecInst));
        strcpy(audioCodec.plname, "AAC");
        audioCodec.rate = 32000;
        audioCodec.plfreq = 16000;
        audioCodec.channels = 1;
        
        webrtc::VideoCodec videoCodec;
        memset(&videoCodec, 0, sizeof(VideoCodec));
        videoCodec.codecType = webrtc::kVideoCodecH264;
        videoCodec.width = width;
        videoCodec.height = height;
        videoCodec.maxFramerate = maxFramerate;
        videoCodec.maxBitrate = maxBitrate;
        
        mError = mVideoFile->StartRecordCaptureVideo(mCaptureId, videoRecordFile, webrtc::MICROPHONE, audioCodec, videoCodec, webrtc::kFileFormatMP4File, saveVideoToLibrary);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to start video capture recording (error: ") + string(mVoiceBase->LastError()) + ")")
          return;
        }
#endif //ANDROID
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::internalStopRecordVideoCapture()
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("stop video capture recording"))
#ifndef ANDROID
        mError = mVideoFile->StopRecordCaptureVideo(mCaptureId);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to stop video capture recording (error: ") + string(mVoiceBase->LastError()) + ")")
          return;
        }
        
        mError = mVideoCapture->EnableCapturedFrameOrientationLock(mCaptureId, false);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to disable orientation lock on video capture device (error: ") + string(mVideoBase->LastError()) + ")")
          return;
        }
#endif //ANDROID
        try {
          if (mDelegate)
            mDelegate->onMediaEngineVideoCaptureRecordStopped();
        } catch (IMediaEngineDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      int MediaEngine::registerVideoTransport()
      {
#ifndef ANDROID
        if (NULL != mVideoTransport) {
          mError = mVideoNetwork->RegisterSendTransport(mVideoChannel, *mVideoTransport);
          if (0 != mError) {
            ZS_LOG_ERROR(Detail, log("failed to register video external transport (error: ") + string(mVideoBase->LastError()) + ")")
            return mError;
          }
        } else {
          ZS_LOG_ERROR(Detail, log("external video transport is not set"))
          return -1;
        }
#endif //ANDROID
        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngine::deregisterVideoTransport()
      {
#ifndef ANDROID
        mError = mVideoNetwork->DeregisterSendTransport(mVideoChannel);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to deregister video external transport (error: ") + string(mVideoBase->LastError()) + ")")
          return mError;
        }
#endif
        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngine::setVideoTransportParameters()
      {
        // No transport parameters for external transport.
        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngine::getVideoCaptureParameters(webrtc::RotateCapturedFrame orientation, int& width, int& height, int& maxFramerate, int& maxBitrate)
      {
#ifdef TARGET_OS_IPHONE
        String iPadString("iPad");
        String iPad2String("iPad2");
        String iPadMiniString("iPad2,5");
        String iPad3String("iPad3");
        String iPad4String("iPad3,4");
        String iPhoneString("iPhone");
        String iPhone4SString("iPhone4,1");
        String iPhone5String("iPhone5");
        String iPodString("iPod");
        String iPod4String("iPod4,1");
        if (mCameraType == CameraType_Back) {
          if (orientation == webrtc::RotateCapturedFrame_0 || orientation == webrtc::RotateCapturedFrame_180) {
            if (mMachineName.compare(0, iPod4String.size(), iPod4String) >= 0) {
              width = 320;
              height = 180;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPodString.size(), iPodString) >= 0) {
              width = 160;
              height = 90;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPhone5String.size(), iPhone5String) >= 0) {
              width = 480;
              height = 270;
              maxFramerate = 15;
              maxBitrate = 300;
            } else if (mMachineName.compare(0, iPhone4SString.size(), iPhone4SString) >= 0) {
              width = 320;
              height = 180;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPhoneString.size(), iPhoneString) >= 0) {
              width = 160;
              height = 90;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPadMiniString.size(), iPadMiniString) >= 0) {
              width = 480;
              height = 270;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPad2String.size(), iPad2String) >= 0) {
              width = 320;
              height = 180;
              maxFramerate = 15;
              maxBitrate = 250;
            } else {
              ZS_LOG_ERROR(Detail, log("machine name is not supported"))
              return -1;
            }
          } else if (orientation == webrtc::RotateCapturedFrame_90 || orientation == webrtc::RotateCapturedFrame_270) {
            if (mMachineName.compare(0, iPod4String.size(), iPod4String) >= 0) {
              width = 180;
              height = 320;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPodString.size(), iPodString) >= 0) {
              width = 90;
              height = 160;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPhone5String.size(), iPhone5String) >= 0) {
              width = 270;
              height = 480;
              maxFramerate = 15;
              maxBitrate = 300;
            } else if (mMachineName.compare(0, iPhone4SString.size(), iPhone4SString) >= 0) {
              width = 180;
              height = 320;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPhoneString.size(), iPhoneString) >= 0) {
              width = 90;
              height = 160;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPadMiniString.size(), iPadMiniString) >= 0) {
              width = 270;
              height = 480;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPad2String.size(), iPad2String) >= 0) {
              width = 180;
              height = 320;
              maxFramerate = 15;
              maxBitrate = 250;
            } else {
              ZS_LOG_ERROR(Detail, log("machine name is not supported"))
              return -1;
            }
          }
        } else if (mCameraType == CameraType_Front) {
          if (orientation == webrtc::RotateCapturedFrame_0 || orientation == webrtc::RotateCapturedFrame_180) {
            if (mMachineName.compare(0, iPod4String.size(), iPod4String) >= 0) {
              width = 320;
              height = 240;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPodString.size(), iPodString) >= 0) {
              width = 160;
              height = 120;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPhone5String.size(), iPhone5String) >= 0) {
              width = 320;
              height = 180;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPhone4SString.size(), iPhone4SString) >= 0) {
              width = 320;
              height = 240;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPhoneString.size(), iPhoneString) >= 0) {
              width = 160;
              height = 120;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPad4String.size(), iPad4String) >= 0) {
              width = 320;
              height = 180;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPad3String.size(), iPad3String) >= 0) {
              width = 320;
              height = 240;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPadMiniString.size(), iPadMiniString) >= 0) {
              width = 320;
              height = 180;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPadString.size(), iPadString) >= 0) {
              width = 320;
              height = 240;
              maxFramerate = 15;
              maxBitrate = 250;
            } else {
              ZS_LOG_ERROR(Detail, log("machine name is not supported"))
              return -1;
            }
          } else if (orientation == webrtc::RotateCapturedFrame_90 || orientation == webrtc::RotateCapturedFrame_270) {
            if (mMachineName.compare(0, iPod4String.size(), iPod4String) >= 0) {
              width = 240;
              height = 320;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPodString.size(), iPodString) >= 0) {
              width = 120;
              height = 160;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPhone5String.size(), iPhone5String) >= 0) {
              width = 180;
              height = 320;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPhone4SString.size(), iPhone4SString) >= 0) {
              width = 240;
              height = 320;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPhoneString.size(), iPhoneString) >= 0) {
              width = 120;
              height = 160;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPad4String.size(), iPad4String) >= 0) {
              width = 180;
              height = 320;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPad3String.size(), iPad3String) >= 0) {
              width = 240;
              height = 320;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPadMiniString.size(), iPadMiniString) >= 0) {
              width = 180;
              height = 320;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPadString.size(), iPadString) >= 0) {
              width = 240;
              height = 320;
              maxFramerate = 15;
              maxBitrate = 250;
            } else {
              ZS_LOG_ERROR(Detail, log("machine name is not supported"))
              return -1;
            }
          }
        } else {
          ZS_LOG_ERROR(Detail, log("camera type is not set"))
          return -1;
        }
#else
        width = 180;
        height = 320;
        maxFramerate = 15;
        maxBitrate = 250;
#endif
        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngine::setVideoCaptureRotation()
      {
#ifndef ANDROID
        webrtc::RotateCapturedFrame orientation;
        mError = mVideoCapture->GetOrientation(mDeviceUniqueId, orientation);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to get orientation from video capture device (error: ") + string(mVideoBase->LastError()) + ")")
          return mError;
        }
        mError = mVideoCapture->SetRotateCapturedFrames(mCaptureId, orientation);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set rotation for video capture device (error: ") + string(mVideoBase->LastError()) + ")")
          return mError;
        }
        
        const char *rotationString = NULL;
        switch (orientation) {
          case webrtc::RotateCapturedFrame_0:
            rotationString = "0 degrees";
            break;
          case webrtc::RotateCapturedFrame_90:
            rotationString = "90 degrees";
            break;
          case webrtc::RotateCapturedFrame_180:
            rotationString = "180 degrees";
            break;
          case webrtc::RotateCapturedFrame_270:
            rotationString = "270 degrees";
            break;
          default:
            break;
        }
        
        if (rotationString) {
          ZS_LOG_DEBUG(log("video capture rotation set - rotation: ") + rotationString)
        }
#endif //ANDROID
        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngine::setVideoCodecParameters()
      {
#ifndef ANDROID
#ifdef TARGET_OS_IPHONE
        webrtc::RotateCapturedFrame orientation;
        mError = mVideoCapture->GetOrientation(mDeviceUniqueId, orientation);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to get orientation from video capture device (error: ") + string(mVideoBase->LastError()) + ")")
          return mError;
        }
#else
        webrtc::RotateCapturedFrame orientation = webrtc::RotateCapturedFrame_0;
#endif
        
        int width = 0, height = 0, maxFramerate = 0, maxBitrate = 0;
        mError = getVideoCaptureParameters(orientation, width, height, maxFramerate, maxBitrate);
        if (mError != 0)
          return mError;
        
        webrtc::VideoCodec videoCodec;
        memset(&videoCodec, 0, sizeof(VideoCodec));
        mError = mVideoCodec->GetSendCodec(mVideoChannel, videoCodec);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to get video codec (error: ") + string(mVideoBase->LastError()) + ")")
          return mError;
        }
        videoCodec.width = width;
        videoCodec.height = height;
        videoCodec.maxFramerate = maxFramerate;
        videoCodec.maxBitrate = maxBitrate;
        mError = mVideoCodec->SetSendCodec(mVideoChannel, videoCodec);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set send video codec (error: ") + string(mVideoBase->LastError()) + ")")
          return mError;
        }
        
        ZS_LOG_DEBUG(log("video codec size - width: ") + string(width) + ", height: " + string(height))
        
#endif //ANDROID
        return 0;
      }

      //-----------------------------------------------------------------------
      webrtc::EcModes MediaEngine::getEcMode()
      {
#ifdef TARGET_OS_IPHONE
        String iPadString("iPad");
        String iPad2String("iPad2");
        String iPad3String("iPad3");
        String iPhoneString("iPhone");
        String iPhone5String("iPhone5");
        String iPhone4SString("iPhone4,1");
        String iPodString("iPod");
        String iPod4String("iPod4,1");

        if (mMachineName.compare(0, iPod4String.size(), iPod4String) >= 0) {
          return webrtc::kEcAec;
        } else if (mMachineName.compare(0, iPodString.size(), iPodString) >= 0) {
          return webrtc::kEcAecm;
        } else if (mMachineName.compare(0, iPhone5String.size(), iPhone5String) >= 0) {
          return webrtc::kEcAec;
        } else if (mMachineName.compare(0, iPhone4SString.size(), iPhone4SString) >= 0) {
          return webrtc::kEcAec;
        } else if (mMachineName.compare(0, iPhoneString.size(), iPhoneString) >= 0) {
          return webrtc::kEcAecm;
        } else if (mMachineName.compare(0, iPad3String.size(), iPad3String) >= 0) {
          return webrtc::kEcAec;
        } else if (mMachineName.compare(0, iPad2String.size(), iPad2String) >= 0) {
          return webrtc::kEcAec;
        } else if (mMachineName.compare(0, iPadString.size(), iPadString) >= 0) {
          return webrtc::kEcAecm;
        } else {
          ZS_LOG_ERROR(Detail, log("machine name is not supported"))
          return webrtc::kEcUnchanged;
        }
#elif defined(__QNX__)
        return webrtc::kEcAec;
#else
        return webrtc::kEcUnchanged;
#endif
      }

      //-----------------------------------------------------------------------
      String MediaEngine::log(const char *message) const
      {
        return String("MediaEngine [") + string(mID) + "] " + message;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine::RedirectTransport
      #pragma mark

      //-----------------------------------------------------------------------
      MediaEngine::RedirectTransport::RedirectTransport(const char *transportType) :
        mID(zsLib::createPUID()),
        mTransportType(transportType),
        mTransport(0)
      {
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine::RedirectTransport => webrtc::Transport
      #pragma mark

      //-----------------------------------------------------------------------
      int MediaEngine::RedirectTransport::SendPacket(int channel, const void *data, int len)
      {
        Transport *transport = NULL;
        {
          AutoRecursiveLock lock(mLock);
          transport = mTransport;
        }
        if (!transport) {
          ZS_LOG_WARNING(Debug, log("RTP packet cannot be sent as no transport is not registered") + ", channel=" + string(channel) + ", length=" + string(len))
          return 0;
        }

        return transport->SendPacket(channel, data, len);
      }

      //-----------------------------------------------------------------------
      int MediaEngine::RedirectTransport::SendRTCPPacket(int channel, const void *data, int len)
      {
        Transport *transport = NULL;
        {
          AutoRecursiveLock lock(mLock);
          transport = mTransport;
        }
        if (!transport) {
          ZS_LOG_WARNING(Debug, log("RTCP packet cannot be sent as no transport is not registered") + ", channel=" + string(channel) + ", length=" + string(len))
          return 0;
        }

        return transport->SendRTCPPacket(channel, data, len);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine::RedirectTransport => friend MediaEngine
      #pragma mark

      //-----------------------------------------------------------------------
      void MediaEngine::RedirectTransport::redirect(Transport *transport)
      {
        AutoRecursiveLock lock(mLock);
        mTransport = transport;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine::RedirectTransport => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      String MediaEngine::RedirectTransport::log(const char *message)
      {
        return String("MediaEngine::RedirectTransport (") + mTransportType + ") [" + string(mID) + "] " + message;
      }
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IMediaEngine
    #pragma mark

    //-------------------------------------------------------------------------
    const char *IMediaEngine::toString(CameraTypes type)
    {
      switch (type) {
        case CameraType_None:   return "None";
        case CameraType_Front:  return "Front";
        case CameraType_Back:   return "Back";
      }
      return "UNDEFINED";
    }
    
    //---------------------------------------------------------------------------
    const char *IMediaEngine::toString(VideoOrientations orientation)
    {
      switch (orientation) {
        case VideoOrientation_LandscapeLeft:        return "Landscape left";
        case VideoOrientation_PortraitUpsideDown:   return "Portrait upside down";
        case VideoOrientation_LandscapeRight:       return "Landscape right";
        case VideoOrientation_Portrait:             return "Portrait";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    const char *IMediaEngine::toString(OutputAudioRoutes route)
    {
      switch (route) {
        case OutputAudioRoute_Headphone:        return "Headphone";
        case OutputAudioRoute_BuiltInReceiver:  return "Built in receiver";
        case OutputAudioRoute_BuiltInSpeaker:   return "Built in speaker";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    IMediaEnginePtr IMediaEngine::singleton()
    {
      return internal::MediaEngine::singleton();
    }
  }
}
