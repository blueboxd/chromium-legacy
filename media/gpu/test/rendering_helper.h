// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_RENDERING_HELPER_H_
#define MEDIA_GPU_TEST_RENDERING_HELPER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "media/base/video_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace base {
class WaitableEvent;
}

namespace media {
namespace test {
class TextureRef;
}  // namespace test

// TODO(dstaessens@) Most functionality can be removed from this file when the
// video_decode_accelerator_unittests are deprecated in favor of the new
// video_decode_accelerator_test.
class VideoFrameTexture : public base::RefCounted<VideoFrameTexture> {
 public:
  uint32_t texture_id() const { return texture_id_; }
  uint32_t texture_target() const { return texture_target_; }

  VideoFrameTexture(uint32_t texture_target,
                    uint32_t texture_id,
                    const base::Closure& no_longer_needed_cb);

 private:
  friend class base::RefCounted<VideoFrameTexture>;

  uint32_t texture_target_;
  uint32_t texture_id_;
  base::Closure no_longer_needed_cb_;

  ~VideoFrameTexture();
};

struct RenderingHelperParams {
  // The target rendering FPS. A value of 0 makes the RenderingHelper return
  // frames immediately.
  int rendering_fps = 0;

  // The number of windows. We play each stream in its own window
  // on the screen.
  int num_windows = 0;

  // The members below are only used for the thumbnail mode where all frames
  // are rendered in sequence onto one FBO for comparison/verification purposes.

  // Whether the frames are rendered as scaled thumbnails within a
  // larger FBO that is in turn rendered to the window.
  bool render_as_thumbnails = false;
  // The size of the FBO containing all visible thumbnails.
  gfx::Size thumbnails_page_size;
  // The size of each thumbnail within the FBO.
  gfx::Size thumbnail_size;
};

// Creates and draws textures used by the video decoder.
// This class is not thread safe and thus all the methods of this class
// (except for ctor/dtor) ensure they're being run on a single thread.
class RenderingHelper {
 public:
  RenderingHelper();
  ~RenderingHelper();

  // Initialize GL. This method must be called on the rendering thread.
  static void InitializeOneOff(bool use_gl, base::WaitableEvent* done);

  // Create the render context and windows by the specified
  // dimensions. This method must be called on the rendering thread.
  void Initialize(const RenderingHelperParams& params,
                  base::WaitableEvent* done);

  // Undo the effects of Initialize() and signal |*done|. This method
  // must be called on the rendering thread.
  void UnInitialize(base::WaitableEvent* done);

  // Return a newly-created media::test::TextureRef of the specified size and
  // pixel format. If pre_allocate is true, NativePixmap is allocated in this
  // function.
  scoped_refptr<media::test::TextureRef> CreateTexture(
      uint32_t texture_target,
      bool pre_allocate,
      VideoPixelFormat pixel_format,
      const gfx::Size& size);

  // If |render_as_thumbnails_| is true, renders |video_frame| as thumbnail.
  // Otherwise, queues |video_frame| to |pending_frames|.
  void ConsumeVideoFrame(size_t window_id,
                         scoped_refptr<VideoFrameTexture> video_frame);

  // Flushes the pending frames. Notify the rendering_helper there won't be
  // more video frames.
  void Flush(size_t window_id);

  // Get the GL context.
  gl::GLContext* GetGLContext();

  // Get rendered thumbnails as RGBA.
  void GetThumbnailsAsRGBA(std::vector<unsigned char>* rgba,
                           base::WaitableEvent* done);

  // Delete the texture with specified |texture_id|.
  static void DeleteTexture(uint32_t texture_id);

  // Set the GL viewport to the specified |area|.
  static void GLSetViewPort(const gfx::Rect& area);

  // Create a shader with specified |program| id and |type| by compiling the
  // shader |source| code with length |size|.
  static void CreateShader(GLuint program,
                           GLenum type,
                           const char* source,
                           int size);

  // Render |texture_id| to the current view port of the screen using target
  // |texture_target|.
  static void RenderTexture(uint32_t texture_target, uint32_t texture_id);

 private:
  struct RenderedVideo {
    // True if there won't be any new video frames comming.
    bool is_flushing = false;

    // The number of frames need to be dropped to catch up the rendering. We
    // always keep the last remaining frame in pending_frames even after it
    // has been rendered, so that we have something to display if the client
    // is falling behind on providing us with new frames during timer-driven
    // playback.
    int frames_to_drop = 0;

    // The video frames pending for rendering.
    base::queue<scoped_refptr<VideoFrameTexture>> pending_frames;

    RenderedVideo();
    RenderedVideo(const RenderedVideo& other);
    ~RenderedVideo();
  };

  // Render thumbnail in the |texture_id| to the FBO buffer using target
  // |texture_target|.
  void RenderThumbnail(uint32_t texture_target, uint32_t texture_id);

  // Queues the |video_frame| for rendering.
  void QueueVideoFrame(size_t window_id,
                       scoped_refptr<VideoFrameTexture> video_frame);

  // Return a newly-created GLES2 texture id of the specified size.
  uint32_t CreateTextureId(uint32_t texture_target, const gfx::Size& size);

  void Clear();
  void RenderContent();
  void DropOneFrameForAllVideos();
  void ScheduleNextRenderContent();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  scoped_refptr<gl::GLContext> gl_context_;
  scoped_refptr<gl::GLSurface> gl_surface_;

  std::unique_ptr<gpu::GpuMemoryBufferFactory> gpu_memory_buffer_factory_;

  std::vector<RenderedVideo> videos_;

  bool render_as_thumbnails_;
  int frame_count_;
  GLuint thumbnails_fbo_id_;
  GLuint thumbnails_texture_id_;
  gfx::Size thumbnails_fbo_size_;
  gfx::Size thumbnail_size_;
  GLuint vertex_buffer_;
  GLuint program_;
  static bool use_gl_;
  base::TimeDelta frame_duration_;
  base::TimeTicks scheduled_render_time_;
  base::CancelableClosure render_task_;
  base::TimeTicks vsync_timebase_;

  DISALLOW_COPY_AND_ASSIGN(RenderingHelper);
};

}  // namespace media

#endif  // MEDIA_GPU_TEST_RENDERING_HELPER_H_
