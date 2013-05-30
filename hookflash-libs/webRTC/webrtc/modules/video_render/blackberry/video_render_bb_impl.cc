/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_render_bb_impl.h"

#include "critical_section_wrapper.h"
#include "event_wrapper.h"
#include "thread_wrapper.h"
#include "tick_util.h"

#include "video_render_bb_opengles20.h"
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

#include "bb_window_wrapper.h"

#include "trace.h"

#define LOG_ERROR(error_msg)  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id, error_msg, __FUNCTION__), false

namespace webrtc {

//----------------------------------------------------------------------------------------
BlackberryRenderCallback::BlackberryRenderCallback(VideoRenderBlackBerry* parentRenderer,
                                                   WebRtc_UWord32 streamId) :
                                                   _ptrParentRenderer(parentRenderer),
                                                   _hasFrame(false),
                                                   _frameIsRendered(false),
                                                   _isSetup(false),
                                                   _critSect(*CriticalSectionWrapper::CreateCriticalSection()) {
  _ptrOpenGLRenderer = new VideoRenderOpenGles20(streamId);
}

//----------------------------------------------------------------------------------------
WebRtc_Word32 BlackberryRenderCallback::RenderFrame(const WebRtc_UWord32 streamId, I420VideoFrame& videoFrame) {
  CriticalSectionScoped cs(&_critSect);

  _videoFrame.CopyFrame(videoFrame);
  _hasFrame = true;
  _frameIsRendered = false;
  return 0;
}

//----------------------------------------------------------------------------------------
void BlackberryRenderCallback::RenderToGL() {
  if(_hasFrame) {

    {
      CriticalSectionScoped cs(&_critSect);

      // Setup the renderer (i.e. compile shaders, set up vertex arrays, etc).
      // This must be done as part of the render cycle, when the OpenGL context is valid.
      if(!_isSetup) {
        _isSetup = true;
        _ptrOpenGLRenderer->Setup();
      }

      if(!_frameIsRendered) {
        _frameIsRendered = true;
        _ptrOpenGLRenderer->UpdateTextures(_videoFrame);
      }
    }
    _ptrOpenGLRenderer->Render();
  }
}

//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
VideoRenderBlackBerry::VideoRenderBlackBerry(
    const WebRtc_Word32 id,
    const VideoRenderType videoRenderType,
    void* window,
    const bool /*fullscreen*/):
    _id(id),
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _renderType(videoRenderType),
    _ptrWindowWrapper((BlackberryWindowWrapper*)(window)),
    _ptrGLWindow(NULL),
    _ptrDisplay(NULL),
    _eglDisplay(NULL),
    _eglConfig(NULL),
    _eglContext(NULL),
    _eglSurface(NULL),
    _GLRenderThreadPtr(NULL),
    _windowWidth(768),
    _windowHeight(1280),
    _glInitialized(false),
    _started(false),
    _stopped(false),
    _streamsMap()
{
}

VideoRenderBlackBerry::~VideoRenderBlackBerry() {
  WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id,
               "VideoRenderBlackBerry dtor");

  for (MapItem* item = _streamsMap.First(); item != NULL; item
           = _streamsMap.Next(item)) { // Delete streams
    delete static_cast<BlackberryRenderCallback*> (item->GetItem());
  }
}

WebRtc_Word32 VideoRenderBlackBerry::Init()
{



  CreateGLThread();
  //bps_initialize();

  //CreateGLWindow();

  return 0;
}

WebRtc_Word32 VideoRenderBlackBerry::ChangeUniqueId(const WebRtc_Word32 id) {
  CriticalSectionScoped cs(&_critSect);
  _id = id;

  return 0;
}

WebRtc_Word32 VideoRenderBlackBerry::ChangeWindow(void* /*window*/) {
  WEBRTC_TRACE(kTraceError, kTraceVideo, -1, "Not supported.");
  return -1;
}

VideoRenderCallback*
VideoRenderBlackBerry::AddIncomingRenderStream(const WebRtc_UWord32 streamId,
                                            const WebRtc_UWord32 zOrder,
                                            const float left, const float top,
                                            const float right,
                                            const float bottom) {
  CriticalSectionScoped cs(&_critSect);

  BlackberryRenderCallback* renderStream = NULL;
  MapItem* item = _streamsMap.Find(streamId);
  if (item) {
    renderStream = (BlackberryRenderCallback*) (item->GetItem());
    if (NULL != renderStream) {
      WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, -1,
                   "%s: Render stream already exists", __FUNCTION__);
      return renderStream;
    }
  }

  renderStream = CreateRenderChannel(streamId, zOrder, left, top, right, bottom, *this);
  if (renderStream) {
    _streamsMap.Insert(streamId, renderStream);
  }
  else {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "(%s:%d): renderStream is NULL", __FUNCTION__, __LINE__);
    return NULL;
  }
  return renderStream;
}

WebRtc_Word32 VideoRenderBlackBerry::DeleteIncomingRenderStream(
    const WebRtc_UWord32 streamId) {
  CriticalSectionScoped cs(&_critSect);

  MapItem* item = _streamsMap.Find(streamId);
  if (item) {
    _streamsMap.Erase(streamId);
  }
  else {
    WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
                 "(%s:%d): renderStream is NULL", __FUNCTION__, __LINE__);
    return -1;
  }
  return 0;
}

WebRtc_Word32 VideoRenderBlackBerry::GetIncomingRenderStreamProperties(
    const WebRtc_UWord32 streamId,
    WebRtc_UWord32& zOrder,
    float& left,
    float& top,
    float& right,
    float& bottom) const {
  return -1;
}

WebRtc_Word32 VideoRenderBlackBerry::StartRender() {
  CriticalSectionScoped cs(&_critSect);

  unsigned int tId = 0;

  //_started = true;

  //CreateGLThread();

  return 0;
}

WebRtc_Word32 VideoRenderBlackBerry::StopRender() {
  WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s:", __FUNCTION__);

  CriticalSectionScoped cs(&_critSect);

  _stopped = true;

  return 0;
}

void VideoRenderBlackBerry::ReDraw() {
  CriticalSectionScoped cs(&_critSect);
  // Allow redraw if it was more than 20ms since last.
}

void VideoRenderBlackBerry::OnBBRenderEvent() {
  CriticalSectionScoped cs(&_critSect);

  if(!_glInitialized) {

    int rc;
    EGLint attributes[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLint attrib_list[]= { EGL_RED_SIZE,        8,
                            EGL_GREEN_SIZE,      8,
                            EGL_BLUE_SIZE,       8,
                            EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
                            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                            EGL_NONE};

    _eglContext = eglCreateContext(_eglDisplay, _eglConfig, EGL_NO_CONTEXT, attributes);
    if (_eglContext == EGL_NO_CONTEXT) { LOG_ERROR("eglCreateContext"); return; }

    rc = screen_create_window_type(&_ptrGLWindow, _screen_ctx, SCREEN_CHILD_WINDOW);
    if (rc != 0) { LOG_ERROR("screen_create_window_type"); return; }

	int format = SCREEN_FORMAT_RGBA8888;
	rc = screen_set_window_property_iv(_ptrGLWindow, SCREEN_PROPERTY_FORMAT, &format);
	if (rc) { LOG_ERROR("screen_set_window_property_iv(SCREEN_PROPERTY_FORMAT)"); return; }

    int zorder = -1;
    rc = screen_set_window_property_iv(_ptrGLWindow, SCREEN_PROPERTY_ZORDER, &zorder);
    if (rc) { LOG_ERROR("screen_set_window_property_iv(SCREEN_PROPERTY_ZORDER)"); return; }

    int size[2];
    size[0] = _windowWidth;
    size[1] = _windowHeight;
    rc = screen_set_window_property_iv(_ptrGLWindow, SCREEN_PROPERTY_BUFFER_SIZE, size);
    if (rc) { LOG_ERROR("screen_set_window_property_iv"); return; }

    rc = screen_create_window_buffers(_ptrGLWindow, 2);
    if (rc) { LOG_ERROR("screen_create_window_buffers"); return; }

    int usage = SCREEN_USAGE_OPENGL_ES2 | SCREEN_USAGE_ROTATION;
    rc = screen_set_window_property_iv(_ptrGLWindow, SCREEN_PROPERTY_USAGE, &usage);
    if (rc) { LOG_ERROR("screen_set_window_property_iv(SCREEN_PROPERTY_USAGE)"); return; }

    _eglSurface = eglCreateWindowSurface(_eglDisplay, _eglConfig, _ptrGLWindow, NULL);
    if (_eglSurface == EGL_NO_SURFACE) { LOG_ERROR("eglCreateWindowSurface"); return; }

    rc = eglMakeCurrent(_eglDisplay, _eglSurface, _eglSurface, _eglContext);
    if (rc != EGL_TRUE) { LOG_ERROR("eglMakeCurrent"); return; }

    EGLint interval = 1;
    rc = eglSwapInterval(_eglDisplay, interval);
    if (rc != EGL_TRUE) { LOG_ERROR("eglSwapInterval"); return; }

    rc = screen_join_window_group(_ptrGLWindow, _ptrWindowWrapper->GetGroupId());
    if (rc != 0) { LOG_ERROR("screen_join_window_group"); return; }

    char* windowId = "viewfinder";
    rc = screen_set_window_property_cv(_ptrGLWindow,
                                  SCREEN_PROPERTY_ID_STRING,
                                  strlen(windowId),
                                  windowId);
    if (rc != 0) { LOG_ERROR("screen_set_window_property_cv"); return; }

    glViewport(0, 0, _windowWidth, _windowHeight);

    _glInitialized = true;
  }

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  MapItem* item = _streamsMap.First();
  while(item) {
    if(item->GetItem()) {
      BlackberryRenderCallback* callback = (BlackberryRenderCallback*) item->GetItem();
      callback->RenderToGL();
    }
    item = _streamsMap.Next(item);
  }

  eglSwapBuffers(_eglDisplay, _eglSurface);
}

VideoRenderType VideoRenderBlackBerry::RenderType() {
  return _renderType;
}

RawVideoType VideoRenderBlackBerry::PerferedVideoType() {
  return kVideoI420;
}

bool VideoRenderBlackBerry::FullScreen() {
  return false;
}

WebRtc_Word32 VideoRenderBlackBerry::GetGraphicsMemory(
    WebRtc_UWord64& /*totalGraphicsMemory*/,
    WebRtc_UWord64& /*availableGraphicsMemory*/) const {
  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
               "%s - not supported on Blackberry", __FUNCTION__);
  return -1;
}

WebRtc_Word32 VideoRenderBlackBerry::GetScreenResolution(
    WebRtc_UWord32& /*screenWidth*/,
    WebRtc_UWord32& /*screenHeight*/) const {
  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
               "%s - not supported on Blackberry", __FUNCTION__);
  return -1;
}

WebRtc_UWord32 VideoRenderBlackBerry::RenderFrameRate(
    const WebRtc_UWord32 /*streamId*/) {
  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
               "%s - not supported on Blackberry", __FUNCTION__);
  return -1;
}

WebRtc_Word32 VideoRenderBlackBerry::SetStreamCropping(
    const WebRtc_UWord32 /*streamId*/,
    const float /*left*/,
    const float /*top*/,
    const float /*right*/,
    const float /*bottom*/) {
  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
               "%s - not supported on Blackberry", __FUNCTION__);
  return -1;
}

WebRtc_Word32 VideoRenderBlackBerry::SetTransparentBackground(const bool enable) {
  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
               "%s - not supported on Blackberry", __FUNCTION__);
  return -1;
}

WebRtc_Word32 VideoRenderBlackBerry::ConfigureRenderer(
    const WebRtc_UWord32 streamId,
    const unsigned int zOrder,
    const float left,
    const float top,
    const float right,
    const float bottom) {

  BlackberryRenderCallback* renderStream = NULL;
  MapItem* item = _streamsMap.Find(streamId);
  if (item) {
    renderStream = (BlackberryRenderCallback*) (item->GetItem());
    if (NULL != renderStream) {
      renderStream->GetRenderer()->SetCoordinates(zOrder, left, top, right, bottom);
      return 0;
    }
  }
  WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, -1,
               "%s: Render stream %d does not exist", __FUNCTION__, streamId);
  return -1;
}

WebRtc_Word32 VideoRenderBlackBerry::SetText(
    const WebRtc_UWord8 textId,
    const WebRtc_UWord8* text,
    const WebRtc_Word32 textLength,
    const WebRtc_UWord32 textColorRef,
    const WebRtc_UWord32 backgroundColorRef,
    const float left, const float top,
    const float rigth, const float bottom) {
  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
               "%s - not supported on Blackberry", __FUNCTION__);
  return -1;
}

WebRtc_Word32 VideoRenderBlackBerry::SetBitmap(const void* bitMap,
                                            const WebRtc_UWord8 pictureId,
                                            const void* colorKey,
                                            const float left, const float top,
                                            const float right,
                                            const float bottom) {
  WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id,
               "%s - not supported on Blackberry", __FUNCTION__);
  return -1;
}

BlackberryRenderCallback* VideoRenderBlackBerry::CreateRenderChannel(
    WebRtc_Word32 streamId,
    WebRtc_Word32 zOrder,
    const float left,
    const float top,
    const float right,
    const float bottom,
    VideoRenderBlackBerry& renderer) {

  BlackberryRenderCallback* callback = new BlackberryRenderCallback(this, streamId);
  callback->GetRenderer()->SetCoordinates(zOrder, left, top, right, bottom);
  return callback;
}

bool VideoRenderBlackBerry::CreateGLThread() {
//  pthread_t threadId;
//  pthread_create(&threadId, NULL, VideoRenderBlackBerry::GLThread, (void*) this);
//  return true;

  _GLRenderThreadPtr = ThreadWrapper::CreateThread(GLThread, this,
                                                    kRealtimePriority,
                                                    "BlackberryGLRenderThread");
    if (!_GLRenderThreadPtr) {
      return -1;
    }

    unsigned int tId = 0;
    _GLRenderThreadPtr->Start(tId);

    return true;

}

bool VideoRenderBlackBerry::GLThread(void* arg) {
  VideoRenderBlackBerry* pThis = (VideoRenderBlackBerry*) arg;
  pThis->GLThreadRun();
  return 0;
}

bool VideoRenderBlackBerry::GLThreadRun() {

	{
	  CriticalSectionScoped cs(&_critSect);
	  bps_initialize();

	  CreateGLWindow();
	}

	if (!_started) {
	  sleep(1);
	  _started = true;
	}

  screen_request_events(_screen_ctx);

  bps_event_t *event = NULL;

  while (!_stopped) {
    do {

      //Request and process BPS next available event
      event = NULL;
      int returnCode = bps_get_event(&event, 0);

      if (event) {
        int domain = bps_event_get_domain(event);

        if (domain == screen_get_domain()) {
        }
      }
    } while (event);

    if (!_started) {
  	  sleep(1);
  	  continue;
    }

    if (_stopped) {
      break;
    }

    OnBBRenderEvent();

    usleep(5);
  }

  // remove and cleanup each view
  CleanUpGLWindow();

  // Stop requesting events from libscreen
  screen_stop_events(_screen_ctx);

  // Shut down BPS library for this process
  bps_shutdown();

  // Destroy libscreen context
  screen_destroy_context(_screen_ctx);

  return true;
}

bool VideoRenderBlackBerry::CreateGLWindow() {

  int rc;

  rc = screen_create_context(&_screen_ctx, SCREEN_APPLICATION_CONTEXT);
  if (rc != 0) { return LOG_ERROR("screen_create_context"); }

  EGLint attrib_list[]= { EGL_RED_SIZE,        8,
                          EGL_GREEN_SIZE,      8,
                          EGL_BLUE_SIZE,       8,
                          EGL_DEPTH_SIZE, 	   16,
                          EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
                          EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                          EGL_NONE};

  _eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (_eglDisplay == EGL_NO_DISPLAY) { return LOG_ERROR("eglGetDisplay"); }

  rc = eglInitialize(_eglDisplay, NULL, NULL);
  if (rc != EGL_TRUE) { return LOG_ERROR("eglInitialize"); }

  rc = eglBindAPI(EGL_OPENGL_ES_API);
  if (rc != EGL_TRUE) { return LOG_ERROR("eglBindAPI"); }

  int num_configs;
  rc = eglChooseConfig(_eglDisplay, attrib_list, &_eglConfig, 1, &num_configs);
  if (rc != EGL_TRUE) { return LOG_ERROR("eglChooseConfig"); }

  return true;
}

bool VideoRenderBlackBerry::CleanUpGLWindow() {
  return true;
}

}  // namespace webrtc
