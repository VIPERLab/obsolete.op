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

#include <openpeer/core/internal/types.h>
#include <openpeer/core/internal/core_MediaStream.h>

#include <zsLib/MessageQueueAssociator.h>

#include <voe_base.h>
#include <voe_codec.h>
#include <voe_network.h>
#include <voe_rtp_rtcp.h>
#include <voe_audio_processing.h>
#include <voe_volume_control.h>
#include <voe_hardware.h>
#include <voe_file.h>

#include <vie_base.h>
#include <vie_network.h>
#include <vie_render.h>
#include <vie_capture.h>
#include <vie_codec.h>
#include <vie_rtp_rtcp.h>
#include <vie_file.h>

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMediaEngine
      #pragma mark
      
      interaction IMediaEngine
      {
      public:
        typedef webrtc::CapturedFrameOrientation CapturedFrameOrientation;
        typedef webrtc::OutputAudioRoute OutputAudioRoute;
        typedef webrtc::CallStatistics CallStatistics;
        typedef webrtc::Transport Transport;

        static IMediaEnginePtr singleton();
        
        static void setup(IMediaEngineDelegatePtr delegate);

        virtual void setDefaultVideoOrientation(CapturedFrameOrientation orientation) = 0;
        virtual CapturedFrameOrientation getDefaultVideoOrientation() = 0;
        virtual void setRecordVideoOrientation(CapturedFrameOrientation orientation) = 0;
        virtual CapturedFrameOrientation getRecordVideoOrientation() = 0;
        virtual void setVideoOrientation() = 0;
        
        virtual void setRenderView(int sourceId, void *renderView) = 0;
        
        virtual void setEcEnabled(int channelId, bool enabled) = 0;
        virtual void setAgcEnabled(int channelId, bool enabled) = 0;
        virtual void setNsEnabled(int channelId, bool enabled) = 0;
        virtual void setVoiceRecordFile(String fileName) = 0;
        virtual String getVoiceRecordFile() const = 0;
        
        virtual void setMuteEnabled(bool enabled) = 0;
        virtual bool getMuteEnabled() = 0;
        virtual void setLoudspeakerEnabled(bool enabled) = 0;
        virtual bool getLoudspeakerEnabled() = 0;
        virtual OutputAudioRoute getOutputAudioRoute() = 0;
        
        virtual void setContinuousVideoCapture(bool continuousVideoCapture) = 0;
        virtual bool getContinuousVideoCapture() = 0;
        
        virtual void setFaceDetection(int captureId, bool faceDetection) = 0;
        virtual bool getFaceDetection(int captureId) = 0;
        
        virtual uint32_t getCameraType(int captureId) const = 0;
        virtual void setCameraType(int captureId, uint32_t captureIdx) = 0;
        
        virtual void startVideoCapture(int captureId) = 0;
        virtual void stopVideoCapture(int captureId) = 0;
        
        virtual void startVideoChannel(int captureId) = 0;
        virtual void stopVideoChannel(int captureId) = 0;

        virtual void startVoice(int channelId) = 0;
        virtual void stopVoice(int channelId) = 0;

        virtual void startRecordVideoCapture(int captureId, String fileName, bool saveToLibrary = false) = 0;
        virtual void stopRecordVideoCapture(int captureId) = 0;
        
        virtual int getVideoTransportStatistics(int channelId, CallStatistics &stat) = 0;
        virtual int getVoiceTransportStatistics(int channelId, CallStatistics &stat) = 0;
        
        virtual int registerExternalTransport(int channelId, Transport &transport) = 0;
        virtual int deregisterExternalTransport(int channelId) = 0;
        virtual int receivedRTPPacket(int channelId, const void *data, unsigned int length) = 0;
        virtual int receivedRTCPPacket(int channelId, const void *data, unsigned int length) = 0;
      };
      
      interaction IMediaEngineDelegate
      {
        typedef IMediaEngine::OutputAudioRoute OutputAudioRoute;
        
        virtual void onMediaEngineAudioRouteChanged(OutputAudioRoute audioRoute) = 0;
        virtual void onMediaEngineFaceDetected(int captureId) = 0;
        virtual void onMediaEngineVideoCaptureRecordStopped(int captureId) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine
      #pragma mark
      
      class MediaEngine : public Noop,
                          public MessageQueueAssociator,
                          public IMediaEngine,
                          public webrtc::TraceCallback,
                          public webrtc::VoiceEngineObserver,
                          public webrtc::ViECaptureObserver
      {
      public:
        friend interaction IMediaEngineFactory;
        friend interaction IMediaEngine;
        
        typedef webrtc::TraceLevel TraceLevel;
        typedef webrtc::VoiceEngine VoiceEngine;
        typedef webrtc::VoEBase VoiceBase;
        typedef webrtc::VoECodec VoiceCodec;
        typedef webrtc::VoENetwork VoiceNetwork;
        typedef webrtc::VoERTP_RTCP VoiceRtpRtcp;
        typedef webrtc::VoEAudioProcessing VoiceAudioProcessing;
        typedef webrtc::VoEVolumeControl VoiceVolumeControl;
        typedef webrtc::VoEHardware VoiceHardware;
        typedef webrtc::VoEFile VoiceFile;
        typedef webrtc::OutputAudioRoute OutputAudioRoute;
        typedef webrtc::EcModes EcModes;
        typedef webrtc::VideoCaptureModule VideoCaptureModule;
        typedef webrtc::VideoEngine VideoEngine;
        typedef webrtc::ViEBase VideoBase;
        typedef webrtc::ViENetwork VideoNetwork;
        typedef webrtc::ViERender VideoRender;
        typedef webrtc::ViECapture VideoCapture;
        typedef webrtc::ViERTP_RTCP VideoRtpRtcp;
        typedef webrtc::ViECodec VideoCodec;
        typedef webrtc::ViEFile VideoFile;
        
      protected:
        MediaEngine(
                    IMessageQueuePtr queue,
                    IMediaEngineDelegatePtr delegate
                    );
        
        MediaEngine(Noop);
        
        void init();
        
        static MediaEnginePtr create(IMediaEngineDelegatePtr delegate);

        void destroyMediaEngine();
        
      public:
        ~MediaEngine();
        
      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MediaEngine => IMediaEngine
        #pragma mark
        
        static MediaEnginePtr singleton(IMediaEngineDelegatePtr delegate = IMediaEngineDelegatePtr());
        
        static void setup(IMediaEngineDelegatePtr delegate);

        virtual void setDefaultVideoOrientation(CapturedFrameOrientation orientation);
        virtual CapturedFrameOrientation getDefaultVideoOrientation();
        virtual void setRecordVideoOrientation(CapturedFrameOrientation orientation);
        virtual CapturedFrameOrientation getRecordVideoOrientation();
        virtual void setVideoOrientation();
        
        virtual void setRenderView(int sourceId, void *renderView);
        
        virtual void setEcEnabled(int channelId, bool enabled);
        virtual void setAgcEnabled(int channelId, bool enabled);
        virtual void setNsEnabled(int channelId, bool enabled);
        virtual void setVoiceRecordFile(String fileName);
        virtual String getVoiceRecordFile() const;

        virtual void setMuteEnabled(bool enabled);
        virtual bool getMuteEnabled();
        virtual void setLoudspeakerEnabled(bool enabled);
        virtual bool getLoudspeakerEnabled();
        virtual OutputAudioRoute getOutputAudioRoute();
        
        virtual void setContinuousVideoCapture(bool continuousVideoCapture);
        virtual bool getContinuousVideoCapture();
        
        virtual void setFaceDetection(int captureId, bool faceDetection);
        virtual bool getFaceDetection(int captureId);
        
        virtual uint32_t getCameraType(int captureId) const;
        virtual void setCameraType(int captureId, uint32_t captureIdx);
        
        virtual void startVideoCapture(int captureId);
        virtual void stopVideoCapture(int captureId);
        
        virtual void startVideoChannel(int channelId);
        virtual void stopVideoChannel(int channelId);
        
        virtual void startVoice(int channelId);
        virtual void stopVoice(int channelId);

        virtual void startRecordVideoCapture(int captureId, String fileName, bool saveToLibrary = false);
        virtual void stopRecordVideoCapture(int captureId);
        
        virtual int getVideoTransportStatistics(int channelId, CallStatistics &stat);
        virtual int getVoiceTransportStatistics(int channelId, CallStatistics &stat);
        
        virtual int registerExternalTransport(int channelId, Transport &transport);
        virtual int deregisterExternalTransport(int channelId);
        virtual int receivedRTPPacket(int channelId, const void *data, unsigned int length);
        virtual int receivedRTCPPacket(int channelId, const void *data, unsigned int length);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MediaEngine => TraceCallback
        #pragma mark
        
        virtual void Print(const TraceLevel level, const char *traceString, const int length);
        
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MediaEngine => VoiceEngineObserver
        #pragma mark
        
        void CallbackOnError(const int errCode, const int channel);
        void CallbackOnOutputAudioRouteChange(const OutputAudioRoute route);
        
        //-----------------------------------------------------------------------
        #pragma mark
        #pragma mark MediaEngine => ViECaptureObserver
        #pragma mark
        
        void BrightnessAlarm(const int capture_id, const webrtc::Brightness brightness);
        void CapturedFrameRate(const int capture_id, const unsigned char frame_rate);
        void NoPictureAlarm(const int capture_id, const webrtc::CaptureAlarm alarm);
        void FaceDetected(const int capture_id);
        
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MediaEngine => (internal)
        #pragma mark
        
      public:
        void operator()();
        
      protected:
        virtual void internalStartVoice();
        virtual void internalStopVoice();
        
        virtual void internalStartVideoCapture();
        virtual void internalStopVideoCapture();
        virtual void internalStartVideoChannel();
        virtual void internalStopVideoChannel();
        virtual void internalStartRecordVideoCapture(String videoRecordFile, bool saveVideoToLibrary);
        virtual void internalStopRecordVideoCapture();
        
        virtual int registerVoiceTransport();
        virtual int deregisterVoiceTransport();
        virtual int setVoiceTransportParameters();
        virtual int registerVideoTransport();
        virtual int deregisterVideoTransport();
        virtual int setVideoTransportParameters();
        
      protected:
        int getVideoCaptureParameters(webrtc::RotateCapturedFrame orientation, int& width, int& height,
                                      int& maxFramerate, int& maxBitrate);
        int setVideoCodecParameters();
        int setVideoCaptureRotation();
        EcModes getEcMode();

      private:
        String log(const char *message) const;
        
      protected:
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MediaEngine::RedirectTransport
        #pragma mark
        
        class RedirectTransport : public Transport
        {
        public:
          RedirectTransport(const char *transportType);
          
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark MediaEngine::RedirectTransport => webrtc::Transport
          #pragma mark
          
          virtual int SendPacket(int channel, const void *data, int len);
          virtual int SendRTCPPacket(int channel, const void *data, int len);
          
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark MediaEngine::RedirectTransport => friend MediaEngine
          #pragma mark
          
          void redirect(Transport *transport);
          
        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark MediaEngine::RedirectTransport => (internal)
          #pragma mark
          
          String log(const char *message);
          
        private:
          PUID mID;
          mutable RecursiveLock mLock;
          
          const char *mTransportType;
          
          Transport *mTransport;
        };
        

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MediaEngine => (data)
        #pragma mark
        
      protected:
        PUID mID;
        mutable RecursiveLock mLock;
        MediaEngineWeakPtr mThisWeak;
        IMediaEngineDelegatePtr mDelegate;
        
        int mError;
        unsigned int mMtu;
        String mMachineName;
        
        bool mEcEnabled;
        bool mAgcEnabled;
        bool mNsEnabled;
        String mVoiceRecordFile;
        CapturedFrameOrientation mDefaultVideoOrientation;
        CapturedFrameOrientation mRecordVideoOrientation;
        
        int mVoiceChannel;
        Transport *mVoiceTransport;
        VoiceEngine *mVoiceEngine;
        VoiceBase *mVoiceBase;
        VoiceCodec *mVoiceCodec;
        VoiceNetwork *mVoiceNetwork;
        VoiceRtpRtcp *mVoiceRtpRtcp;
        VoiceAudioProcessing *mVoiceAudioProcessing;
        VoiceVolumeControl *mVoiceVolumeControl;
        VoiceHardware *mVoiceHardware;
        VoiceFile *mVoiceFile;
        bool mVoiceEngineReady;
        bool mFaceDetection;
        
        int mVideoChannel;
        Transport *mVideoTransport;
        int mCaptureId;
        char mDeviceUniqueId[512];
        uint32_t mCaptureIdx;
        VideoCaptureModule *mVcpm;
        VideoEngine *mVideoEngine;
        VideoBase *mVideoBase;
        VideoNetwork *mVideoNetwork;
        VideoRender *mVideoRender;
        VideoCapture *mVideoCapture;
        VideoRtpRtcp *mVideoRtpRtcp;
        VideoCodec *mVideoCodec;
        VideoFile *mVideoFile;
        void *mCaptureRenderView;
        void *mChannelRenderView;
        bool mVideoEngineReady;
        
        RedirectTransport mRedirectVoiceTransport;
        RedirectTransport mRedirectVideoTransport;
        
        // lifetime start / stop state
        mutable RecursiveLock mLifetimeLock;
        
        bool mLifetimeWantAudio;
        bool mLifetimeWantVideoCapture;
        bool mLifetimeWantVideoChannel;
        bool mLifetimeWantRecordVideoCapture;
        
        bool mLifetimeHasAudio;
        bool mLifetimeHasVideoCapture;
        bool mLifetimeHasVideoChannel;
        bool mLifetimeHasRecordVideoCapture;
        
        bool mLifetimeInProgress;
        uint32_t mLifetimeWantCaptureIdx;
        bool mLifetimeContinuousVideoCapture;
        
        String mLifetimeVideoRecordFile;
        bool mLifetimeSaveVideoToLibrary;

        mutable RecursiveLock mMediaEngineReadyLock;
      };
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMediaEngineFactory
      #pragma mark
      
      interaction IMediaEngineFactory
      {
        static IMediaEngineFactory &singleton();
        
        virtual MediaEnginePtr createMediaEngine(IMediaEngineDelegatePtr delegate);
      };
    }
  }
}

ZS_DECLARE_PROXY_BEGIN(openpeer::core::internal::IMediaEngineDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::core::internal::IMediaEngine::OutputAudioRoute, OutputAudioRoute)
ZS_DECLARE_PROXY_METHOD_1(onMediaEngineAudioRouteChanged, OutputAudioRoute)
ZS_DECLARE_PROXY_METHOD_1(onMediaEngineFaceDetected, int)
ZS_DECLARE_PROXY_METHOD_1(onMediaEngineVideoCaptureRecordStopped, int)
ZS_DECLARE_PROXY_END()
