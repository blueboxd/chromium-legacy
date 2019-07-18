// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_CODEC_IMAGE_H_
#define MEDIA_GPU_ANDROID_CODEC_IMAGE_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "gpu/command_buffer/service/gl_stream_texture_image.h"
#include "media/gpu/android/codec_wrapper.h"
#include "media/gpu/android/promotion_hint_aggregator.h"
#include "media/gpu/android/surface_texture_gl_owner.h"
#include "media/gpu/media_gpu_export.h"

namespace base {
namespace android {
class ScopedHardwareBufferFenceSync;
}  // namespace android
}  // namespace base

namespace media {

// A GLImage that renders MediaCodec buffers to a TextureOwner or overlay
// as needed in order to draw them.
class MEDIA_GPU_EXPORT CodecImage : public gpu::gles2::GLStreamTextureImage {
 public:
  // Callback to notify that a codec image is now unused in the sense of not
  // being out for display.  This lets us signal interested folks once a video
  // frame is destroyed and the sync token clears, so that that CodecImage may
  // be re-used.  Once legacy mailboxes go away, SharedImageVideo can manage all
  // of this instead.
  //
  // Also note that, presently, only destruction does this.  However, with
  // pooling, there will be a way to mark a CodecImage as unused without
  // destroying it.
  using NowUnusedCB = base::OnceCallback<void(CodecImage*)>;

  // A callback for observing CodecImage destruction.
  using DestructionCB = base::OnceCallback<void(CodecImage*)>;

  CodecImage();

  // (Re-)Initialize this CodecImage to use |output_buffer| et. al.
  //
  // May be called on a random thread, but only if the CodecImage is otherwise
  // not in use.
  void Initialize(
      std::unique_ptr<CodecOutputBuffer> output_buffer,
      scoped_refptr<TextureOwner> texture_owner,
      PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb);

  void SetNowUnusedCB(NowUnusedCB now_unused_cb);
  void SetDestructionCB(DestructionCB destruction_cb);

  // gl::GLImage implementation
  gfx::Size GetSize() override;
  unsigned GetInternalFormat() override;
  BindOrCopy ShouldBindOrCopy() override;
  bool BindTexImage(unsigned target) override;
  void ReleaseTexImage(unsigned target) override;
  bool CopyTexImage(unsigned target) override;
  bool CopyTexSubImage(unsigned target,
                       const gfx::Point& offset,
                       const gfx::Rect& rect) override;
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int z_order,
                            gfx::OverlayTransform transform,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect,
                            bool enable_blend,
                            std::unique_ptr<gfx::GpuFence> gpu_fence) override;
  void SetColorSpace(const gfx::ColorSpace& color_space) override {}
  void Flush() override {}
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer() override;
  // gpu::gles2::GLStreamTextureMatrix implementation
  void GetTextureMatrix(float xform[16]) override;
  void NotifyPromotionHint(bool promotion_hint,
                           int display_x,
                           int display_y,
                           int display_width,
                           int display_height) override;

  // Whether the codec buffer has been rendered to the front buffer.
  bool was_rendered_to_front_buffer() const {
    return phase_ == Phase::kInFrontBuffer;
  }

  // Whether the TextureOwner's texture is in the front buffer and bound to the
  // latest image.
  bool was_tex_image_bound() const { return was_tex_image_bound_; }

  // Whether this image is backed by a texture owner.
  bool is_texture_owner_backed() const { return !!texture_owner_; }

  scoped_refptr<TextureOwner> texture_owner() const { return texture_owner_; }

  // Renders this image to the front buffer of its backing surface.
  // Returns true if the buffer is in the front buffer. Returns false if the
  // buffer was invalidated. After an image is invalidated it's no longer
  // possible to render it.
  bool RenderToFrontBuffer();

  // Renders this image to the back buffer of its texture owner. Only valid if
  // is_texture_owner_backed(). Returns true if the buffer is in the back
  // buffer. Returns false if the buffer was invalidated.
  bool RenderToTextureOwnerBackBuffer();

  // Release any codec buffer without rendering, if we have one.
  virtual void ReleaseCodecBuffer();

  CodecOutputBuffer* get_codec_output_buffer_for_testing() const {
    return output_buffer_.get();
  }

 protected:
  ~CodecImage() override;

 private:
  // The lifecycle phases of an image.
  // The only possible transitions are from left to right. Both
  // kInFrontBuffer and kInvalidated are terminal.
  enum class Phase { kInCodec, kInBackBuffer, kInFrontBuffer, kInvalidated };

  // Renders this image to the texture owner front buffer by first rendering
  // it to the back buffer if it's not already there, and then waiting for the
  // frame available event before calling UpdateTexImage().
  enum class BindingsMode {
    // Ensures that the TextureOwner's texture is bound to the latest image, if
    // it requires explicit binding.
    kEnsureTexImageBound,

    // Updates the current image but does not bind it. If updating the image
    // implicitly binds the texture, the current bindings will be restored.
    kRestoreIfBound,

    // Updates the current image but does not bind it. If updating the image
    // implicitly binds the texture, the current bindings will not be restored.
    kDontRestoreIfBound
  };
  bool RenderToTextureOwnerFrontBuffer(BindingsMode bindings_mode);
  void EnsureBoundIfNeeded(BindingsMode mode);

  // Renders this image to the overlay. Returns true if the buffer is in the
  // overlay front buffer. Returns false if the buffer was invalidated.
  bool RenderToOverlay();

  // The phase of the image buffer's lifecycle.
  Phase phase_ = Phase::kInvalidated;

  // The buffer backing this image.
  std::unique_ptr<CodecOutputBuffer> output_buffer_;

  // The TextureOwner that |output_buffer_| will be rendered to. Or null, if
  // this image is backed by an overlay.
  scoped_refptr<TextureOwner> texture_owner_;

  // The bounds last sent to the overlay.
  gfx::Rect most_recent_bounds_;

  // Callback to notify about promotion hints and overlay position.
  PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb_;

  NowUnusedCB now_unused_cb_;

  DestructionCB destruction_cb_;

  bool was_tex_image_bound_ = false;

  DISALLOW_COPY_AND_ASSIGN(CodecImage);
};

// Temporary helper class to prevent touching a non-threadsafe-ref-counted
// CodecImage off the gpu main thread, while still holding a reference to it.
// Passing a raw pointer around isn't safe, since stub destruction could still
// destroy the consumers of the codec image.
class MEDIA_GPU_EXPORT CodecImageHolder
    : public base::RefCountedDeleteOnSequence<CodecImageHolder> {
 public:
  CodecImageHolder(scoped_refptr<base::SequencedTaskRunner> task_runner,
                   scoped_refptr<CodecImage> codec_image);

  // Safe from any thread.
  CodecImage* codec_image_raw() const { return codec_image_.get(); }

 private:
  virtual ~CodecImageHolder();

  friend class base::RefCountedDeleteOnSequence<CodecImageHolder>;
  friend class base::DeleteHelper<CodecImageHolder>;

  scoped_refptr<CodecImage> codec_image_;

  DISALLOW_COPY_AND_ASSIGN(CodecImageHolder);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_CODEC_IMAGE_H_
