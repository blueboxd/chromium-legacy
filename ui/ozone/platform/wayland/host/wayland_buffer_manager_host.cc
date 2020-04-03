// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"

#include <presentation-time-client-protocol.h>
#include <memory>

#include "base/i18n/number_formatting.h"
#include "base/message_loop/message_loop_current.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_drm.h"
#include "ui/ozone/platform/wayland/host/wayland_shm.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_zwp_linux_dmabuf.h"

namespace ui {

namespace {

uint32_t GetPresentationKindFlags(uint32_t flags) {
  // Wayland spec has different meaning of VSync. In Chromium, VSync means to
  // update the begin frame vsync timing based on presentation feedback.
  uint32_t presentation_flags = gfx::PresentationFeedback::kVSync;

  if (flags & WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK)
    presentation_flags |= gfx::PresentationFeedback::kHWClock;
  if (flags & WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION)
    presentation_flags |= gfx::PresentationFeedback::kHWCompletion;
  if (flags & WP_PRESENTATION_FEEDBACK_KIND_ZERO_COPY)
    presentation_flags |= gfx::PresentationFeedback::kZeroCopy;

  return presentation_flags;
}

base::TimeTicks GetPresentationFeedbackTimeStamp(uint32_t tv_sec_hi,
                                                 uint32_t tv_sec_lo,
                                                 uint32_t tv_nsec) {
  const int64_t seconds = (static_cast<int64_t>(tv_sec_hi) << 32) + tv_sec_lo;
  const int64_t microseconds = seconds * base::Time::kMicrosecondsPerSecond +
                               tv_nsec / base::Time::kNanosecondsPerMicrosecond;
  return base::TimeTicks() + base::TimeDelta::FromMicroseconds(microseconds);
}

std::string NumberToString(uint32_t number) {
  return base::UTF16ToUTF8(base::FormatNumber(number));
}

}  // namespace

class WaylandBufferManagerHost::Surface {
 public:
  Surface(WaylandWindow* window,
          WaylandConnection* connection,
          WaylandBufferManagerHost* buffer_manager)
      : window_(window),
        connection_(connection),
        buffer_manager_(buffer_manager) {}
  ~Surface() = default;

  bool CommitBuffer(uint32_t buffer_id, const gfx::Rect& damage_region) {
    DCHECK(!pending_buffer_);

    // The window has already been destroyed.
    if (!window_)
      return true;

    WaylandBuffer* buffer = GetBuffer(buffer_id);
    if (!buffer) {
      // Get the anonymous_wl_buffer aka the buffer that has not been attached
      // to any of the surfaces previously.
      auto anonymous_wayland_buffer =
          buffer_manager_->PassAnonymousWlBuffer(buffer_id);
      if (!anonymous_wayland_buffer)
        return false;

      buffer = anonymous_wayland_buffer.get();
      buffers_.emplace(buffer_id, std::move(anonymous_wayland_buffer));
      if (buffer->wl_buffer)
        SetupBufferReleaseListener(buffer);
    }

    buffer->damage_region = damage_region;

    // If the wl_buffer has been attached, but the wl_buffer still is null, it
    // means the Wayland server failed to create the buffer and we have to fail
    // here.
    //
    // TODO(msisov): should we ask to recreate buffers instead of failing?
    if (buffer->attached && !buffer->wl_buffer)
      return false;

    // This request may come earlier than the Wayland compositor has imported a
    // wl_buffer. Wait until the buffer is created. The wait takes place only
    // once. Though, the case when a request to attach a buffer comes earlier
    // than the wl_buffer is created does not happen often. 1) Depending on the
    // zwp linux dmabuf protocol version, the wl_buffer can be created
    // immediately without asynchronous wait 2) the wl_buffer can have been
    // created by this time.
    //
    // Another case, which always happen is waiting until the frame callback is
    // completed. Thus, wait here when the Wayland compositor fires the frame
    // callback.
    if (!buffer->wl_buffer || wl_frame_callback_) {
      pending_buffer_ = buffer;
      return true;
    }

    return CommitBufferInternal(buffer);
  }

  bool CreateBuffer(const gfx::Size& size, uint32_t buffer_id) {
    auto result = buffers_.emplace(
        buffer_id, std::make_unique<WaylandBuffer>(size, buffer_id));
    return result.second;
  }

  size_t DestroyBuffer(uint32_t buffer_id) {
    auto* buffer = GetBuffer(buffer_id);
    // We ack the submission of a front buffer whenever a previous front back
    // becomes the back one and receives a buffer release callback. But the
    // following can happen:
    //
    // Imagine the order:
    // the front buffer is buffer 2, then
    // Commit[1]
    // Destroy[2]
    // Commit[3]
    // Release[1]
    // Ack[3] <= this results in a wrong order of the callbacks. In reality,
    // the buffer [1] must have been acked, because the buffer 2 was
    // released.
    // But if the buffer 2 is destroyed, the buffer release callback never
    // comes for that buffer. Thus, if there is a submitted buffer, notify
    // the client about successful swap.
    // If the window has already been destroyed, no need to complete the
    // submission.
    if (buffer && !buffer->released && submitted_buffer_ && window_)
      CompleteSubmission();

    if (prev_submitted_buffer_ == buffer)
      prev_submitted_buffer_ = nullptr;

    if (pending_buffer_ == buffer)
      pending_buffer_ = nullptr;

    auto result = buffers_.erase(buffer_id);
    return result;
  }

  void AttachWlBuffer(uint32_t buffer_id, wl::Object<wl_buffer> new_buffer) {
    WaylandBuffer* buffer = GetBuffer(buffer_id);
    // It can happen that the buffer was destroyed by the client while the
    // Wayland compositor was processing the request to create a wl_buffer.
    if (!buffer)
      return;

    DCHECK(!buffer->wl_buffer);
    buffer->wl_buffer = std::move(new_buffer);
    buffer->attached = true;

    if (buffer->wl_buffer)
      SetupBufferReleaseListener(buffer);

    if (pending_buffer_ == buffer && !wl_frame_callback_)
      ProcessPendingBuffer();
  }

  void ClearState() {
    buffers_.clear();
    wl_frame_callback_.reset();
    feedback_queue_ = PresentationFeedbackQueue();

    ResetSurfaceContents();

    prev_submitted_buffer_ = nullptr;
    submitted_buffer_ = nullptr;
    pending_buffer_ = nullptr;

    connection_->ScheduleFlush();
  }

  void ResetSurfaceContents() {
    if (!window_)
      return;

    wl_surface_attach(window_->surface(), nullptr, 0, 0);
    wl_surface_commit(window_->surface());

    // We cannot reset |prev_submitted_buffer_| here as long as the surface
    // might have attached a new buffer and is about to receive a release
    // callback. Check more comments below where the variable is declared.
    contents_reset_ = true;

    connection_->ScheduleFlush();
  }

  bool BufferExists(uint32_t buffer_id) const {
    auto* buffer = GetBuffer(buffer_id);
    return !!buffer;
  }

  bool HasBuffers() const { return !buffers_.empty(); }

  void OnWindowRemoved() { window_ = nullptr; }
  bool HasWindow() const { return !!window_; }

 private:
  struct FeedbackInfo {
    // The wayland object identifying this feedback.
    wl::Object<struct wp_presentation_feedback> wp_presentation_feedback;
    // The buffer that this presentation feedback is for.
    uint32_t buffer_id;
    // The actual presentation feedback. May be missing if the callback from the
    // Wayland server has not arrived yet.
    base::Optional<gfx::PresentationFeedback> feedback;
    // True iff OnSubmission has been called.
    bool submission_completed;
  };

  using PresentationFeedbackQueue = std::vector<FeedbackInfo>;

  bool CommitBufferInternal(WaylandBuffer* buffer) {
    DCHECK(buffer && window_);
    DCHECK(!pending_buffer_);

    DCHECK(!submitted_buffer_);
    submitted_buffer_ = buffer;

    // if the same buffer has been submitted again right after the client
    // received OnSubmission for that buffer, just damage the buffer and
    // commit the surface again.
    if (prev_submitted_buffer_ != submitted_buffer_) {
      // Once the BufferRelease is called, the buffer will be released.
      DCHECK(buffer->released);
      buffer->released = false;
      AttachBuffer(buffer);
    }

    DamageBuffer(buffer);

    SetupFrameCallback();
    SetupPresentationFeedback(buffer->buffer_id);

    CommitSurface();

    connection_->ScheduleFlush();

    // If the contents were reset, there is no buffer attached. It means we have
    // to behave the same way as if it was the very first frame. Check the
    // comment below where the |contents_reset_| is declared.
    if (contents_reset_) {
      prev_submitted_buffer_ = nullptr;
      contents_reset_ = false;
    }

    // If it was the very first frame, the surface has not had a back buffer
    // before, and Wayland won't release the front buffer until next buffer is
    // attached. Thus, notify about successful submission immediately.
    //
    // As said above, if the client submits the same buffer again, we must
    // notify the client about the submission immediately as Wayland compositor
    // is not going to send a release callback for a buffer committed more than
    // once.
    if (!prev_submitted_buffer_ || prev_submitted_buffer_ == submitted_buffer_)
      CompleteSubmission();

    return true;
  }

  void DamageBuffer(WaylandBuffer* buffer) {
    DCHECK(window_);

    gfx::Rect pending_damage_region = std::move(buffer->damage_region);
    // If the size of the damage region is empty, wl_surface_damage must be
    // supplied with the actual size of the buffer, which is going to be
    // committed.
    if (pending_damage_region.size().IsEmpty())
      pending_damage_region.set_size(buffer->size);
    DCHECK(!pending_damage_region.size().IsEmpty());

    if (connection_->compositor_version() >=
        WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION) {
      // wl_surface_damage_buffer relies on compositor API version 4. See
      // https://bit.ly/2u00lv6 for details.
      // We don't need to apply any scaling because pending_damage_region is
      // already in buffer coordinates.
      wl_surface_damage_buffer(window_->surface(), pending_damage_region.x(),
                               pending_damage_region.y(),
                               pending_damage_region.width(),
                               pending_damage_region.height());
    } else {
      // The calculation for damage region relies on two assumptions:
      // 1) The buffer is always attached at surface location (0, 0)
      // 2) The API wl_surface::set_buffer_transform is not used.
      // It's possible to write logic that accounts for both cases above, but
      // it's currently unnecessary.
      //
      // Note: The damage region may not be an integer multiple of scale. To
      // keep the implementation simple, the x() and y() coordinates round down,
      // and the width() and height() calculations always add an extra pixel.
      int scale = window_->buffer_scale();
      wl_surface_damage(window_->surface(), pending_damage_region.x() / scale,
                        pending_damage_region.y() / scale,
                        pending_damage_region.width() / scale + 1,
                        pending_damage_region.height() / scale + 1);
    }
  }

  void AttachBuffer(WaylandBuffer* buffer) {
    DCHECK(window_);

    // The logic in DamageBuffer currently relies on attachment coordinates of
    // (0, 0). If this changes, then the calculation in DamageBuffer will also
    // need to be updated.
    wl_surface_attach(window_->surface(), buffer->wl_buffer.get(), 0, 0);
  }

  void CommitSurface() {
    DCHECK(window_);
    wl_surface_commit(window_->surface());
  }

  void SetupFrameCallback() {
    DCHECK(window_);
    static const wl_callback_listener frame_listener = {
        &Surface::FrameCallbackDone};

    DCHECK(!wl_frame_callback_);
    wl_frame_callback_.reset(wl_surface_frame(window_->surface()));
    wl_callback_add_listener(wl_frame_callback_.get(), &frame_listener, this);
  }

  void SetupPresentationFeedback(uint32_t buffer_id) {
    DCHECK(window_);
    // Set up presentation feedback.
    if (!connection_->presentation())
      return;

    static const wp_presentation_feedback_listener feedback_listener = {
        &Surface::FeedbackSyncOutput, &Surface::FeedbackPresented,
        &Surface::FeedbackDiscarded};

    feedback_queue_.push_back(
        {wl::Object<struct wp_presentation_feedback>(wp_presentation_feedback(
             connection_->presentation(), window_->surface())),
         buffer_id, /*feedback=*/base::nullopt,
         /*submission_completed=*/false});
    wp_presentation_feedback_add_listener(
        feedback_queue_.back().wp_presentation_feedback.get(),
        &feedback_listener, this);
  }

  void SetupBufferReleaseListener(WaylandBuffer* buffer) {
    static struct wl_buffer_listener buffer_listener = {
        &Surface::BufferRelease,
    };
    wl_buffer_add_listener(buffer->wl_buffer.get(), &buffer_listener, this);
  }

  WaylandBuffer* GetBuffer(uint32_t buffer_id) const {
    auto it = buffers_.find(buffer_id);
    return it != buffers_.end() ? it->second.get() : nullptr;
  }

  void OnFrameCallback(struct wl_callback* callback) {
    DCHECK(wl_frame_callback_.get() == callback);
    wl_frame_callback_.reset();

    ProcessPendingBuffer();
  }

  // wl_callback_listener
  static void FrameCallbackDone(void* data,
                                struct wl_callback* callback,
                                uint32_t time) {
    Surface* self = static_cast<Surface*>(data);
    DCHECK(self);
    self->OnFrameCallback(callback);
  }

  void OnRelease(struct wl_buffer* wl_buffer) {
    DCHECK(wl_buffer);

    WaylandBuffer* buffer = nullptr;
    // The Wayland compositor may release the buffer immediately after it has
    // been submitted. Thus, check that wl_buffer belongs to either the
    // submitted buffer or the previously submitted buffer.
    if (submitted_buffer_ && submitted_buffer_->wl_buffer.get() == wl_buffer) {
      buffer = submitted_buffer_;
      DCHECK(buffer->wl_buffer.get() == wl_buffer);
    } else {
      buffer = prev_submitted_buffer_;
      DCHECK(buffer && buffer->wl_buffer.get() == wl_buffer);
    }

    DCHECK(!buffer->released);
    buffer->released = true;

    // It may happen that the client has attached only one buffer and then
    // destroyed the window. That means that we manually called OnRelease on
    // that very first buffer attach as long as the surface has not had any
    // buffers attached before. However, the |submitted_buffer_| can be null in
    // the OnRelease and hit the DCHECK when the client does not continue
    // attaching new buffers (only one has been attached) and destroyes the
    // surface. In this case, the Wayland compositor releases the buffer and the
    // DCHECK is hit, because we have already run the OnRelease call manually.
    // Please note that the |prev_submitted_buffer_| is the buffer we have
    // released manually, and when the Wayland compositor sends OnRelease, the
    // validation of the wl_buffers succeeds because of that previous manual
    // OnRelease call.
    if (!submitted_buffer_)
      return;

    // Although, the Wayland compositor has just released the previously
    // attached buffer, which became a back buffer, we have to notify the client
    // that next buffer has been attach and become the front one. Thus, mark the
    // back buffer as released to ensure the DCHECK is not hit, and notify about
    // successful submission of the front buffer.
    CompleteSubmission();
  }

  // wl_buffer_listener
  static void BufferRelease(void* data, struct wl_buffer* wl_buffer) {
    Surface* self = static_cast<Surface*>(data);
    DCHECK(self);
    self->OnRelease(wl_buffer);
  }

  void CompleteSubmission() {
    DCHECK(submitted_buffer_);
    auto id = submitted_buffer_->buffer_id;
    prev_submitted_buffer_ = submitted_buffer_;
    submitted_buffer_ = nullptr;

    if (!window_)
      return;

    // We can now complete the latest submission. We had to wait for this
    // release because SwapCompletionCallback indicates to the client that the
    // previous buffer is available for reuse.
    buffer_manager_->OnSubmission(window_->GetWidget(), id,
                                  gfx::SwapResult::SWAP_ACK);

    // If presentation feedback is not supported, use a fake feedback. This
    // literally means there are no presentation feedback callbacks created.
    if (!connection_->presentation()) {
      DCHECK(feedback_queue_.empty());
      buffer_manager_->OnPresentation(
          window_->GetWidget(), id,
          gfx::PresentationFeedback(base::TimeTicks::Now(), base::TimeDelta(),
                                    GetPresentationKindFlags(0)));
    } else {
      for (auto& info : feedback_queue_) {
        if (info.buffer_id == id && !info.submission_completed) {
          info.submission_completed = true;
          ProcessPresentationFeedbacks();
          return;
        }
      }
      NOTREACHED() << "Did not find matching feedback for buffer_id=" << id;
    }
  }

  void OnPresentation(struct wp_presentation_feedback* wp_presentation_feedback,
                      const gfx::PresentationFeedback& feedback) {
    FeedbackInfo* feedback_info = nullptr;
    for (auto& info : feedback_queue_) {
      if (info.wp_presentation_feedback.get() == wp_presentation_feedback) {
        feedback_info = &info;
        break;
      } else if (!info.feedback.has_value()) {  // Feedback must come in order.
        info.feedback = gfx::PresentationFeedback::Failure();
      }
    }
    DCHECK(feedback_info);
    DCHECK(!feedback_info->feedback.has_value());
    feedback_info->feedback = feedback;

    ProcessPresentationFeedbacks();
  }

  // We provide the guarantee to the client that:
  // 1. OnPresentation and OnSubmission will be called for each submitted buffer
  // 2. OnPresentation(buffer_id) will be called after OnSubmission(buffer_id)
  // 3. OnPresentation and OnSubmission will be called in the same order
  //    of buffer submission.
  // We make the following assumptions about the server:
  // 1. Presentation feedback will arrive in the same order of submission.
  // 2. Presentation feedback may never arrive if the buffer is destroyed.
  // 3. Presentation feedback may arrive at an arbitrary time after commit.
  // For these reasons, we can't associate feedback with a specific buffer,
  // as there may be more than one feedback in-flight for a single buffer.
  // This function ensures that we send OnPresentation for each buffer that
  // already has had OnSubmission called for it (condition #2).
  void ProcessPresentationFeedbacks() {
    if (!window_)
      return;

    while (!feedback_queue_.empty()) {
      const auto& info = feedback_queue_.front();
      if (!info.submission_completed || !info.feedback.has_value())
        break;
      buffer_manager_->OnPresentation(window_->GetWidget(), info.buffer_id,
                                      *info.feedback);
      feedback_queue_.erase(feedback_queue_.begin());
    }
    // This queue should be small - if not it's likely a bug.
    DCHECK_LE(feedback_queue_.size(), 25u);
  }

  // wp_presentation_feedback_listener
  static void FeedbackSyncOutput(
      void* data,
      struct wp_presentation_feedback* wp_presentation_feedback,
      struct wl_output* output) {}

  static void FeedbackPresented(
      void* data,
      struct wp_presentation_feedback* wp_presentation_feedback,
      uint32_t tv_sec_hi,
      uint32_t tv_sec_lo,
      uint32_t tv_nsec,
      uint32_t refresh,
      uint32_t seq_hi,
      uint32_t seq_lo,
      uint32_t flags) {
    Surface* self = static_cast<Surface*>(data);
    DCHECK(self);
    self->OnPresentation(
        wp_presentation_feedback,
        gfx::PresentationFeedback(
            GetPresentationFeedbackTimeStamp(tv_sec_hi, tv_sec_lo, tv_nsec),
            base::TimeDelta::FromNanoseconds(refresh),
            GetPresentationKindFlags(flags)));
  }

  static void FeedbackDiscarded(
      void* data,
      struct wp_presentation_feedback* wp_presentation_feedback) {
    Surface* self = static_cast<Surface*>(data);
    DCHECK(self);
    self->OnPresentation(wp_presentation_feedback,
                         gfx::PresentationFeedback::Failure());
  }

  void ProcessPendingBuffer() {
    if (!pending_buffer_ || !window_)
      return;

    auto* buffer = pending_buffer_;
    pending_buffer_ = nullptr;
    CommitBufferInternal(buffer);
  }

  // Widget this helper surface backs and has 1:1 relationship with the
  // WaylandWindow.

  // Non-owned. The window this helper surface stores and submits buffers for.
  const WaylandWindow* window_;

  // Non-owned pointer to the connection.
  WaylandConnection* const connection_;

  // Non-owned pointer to the buffer manager.
  WaylandBufferManagerHost* const buffer_manager_;

  // A container of created buffers.
  base::flat_map<uint32_t, std::unique_ptr<WaylandBuffer>> buffers_;

  // A Wayland callback, which is triggered once wl_buffer has been committed
  // and it is right time to notify the GPU that it can start a new drawing
  // operation.
  wl::Object<wl_callback> wl_frame_callback_;

  // A presentation feedback provided by the Wayland server once frame is
  // shown.
  PresentationFeedbackQueue feedback_queue_;

  // A buffer, which is pending to be submitted (look the comment in the
  // CommitBuffer method).
  WaylandBuffer* pending_buffer_ = nullptr;
  // Current submitted buffer.
  WaylandBuffer* submitted_buffer_ = nullptr;
  // Previous submitted buffer.
  WaylandBuffer* prev_submitted_buffer_ = nullptr;

  // If WaylandWindow becomes hidden, it may need to attach a null buffer to the
  // surface it backed to avoid its contents shown on screen. However, it
  // means that the Wayland compositor no longer sends new buffer release events
  // as long as there has not been buffer attached and no submission callback is
  // sent. To avoid this, |contents_reset_| can be used as an identification of
  // a need to call submission callback manually.
  bool contents_reset_ = false;

  DISALLOW_COPY_AND_ASSIGN(Surface);
};

WaylandBuffer::WaylandBuffer(const gfx::Size& size, uint32_t buffer_id)
    : size(size), buffer_id(buffer_id) {}
WaylandBuffer::~WaylandBuffer() = default;

WaylandBufferManagerHost::WaylandBufferManagerHost(
    WaylandConnection* connection)
    : connection_(connection), receiver_(this), weak_factory_(this) {
  connection_->wayland_window_manager()->AddObserver(this);
}

WaylandBufferManagerHost::~WaylandBufferManagerHost() {
  DCHECK(surfaces_.empty());
  DCHECK(anonymous_buffers_.empty());
}

void WaylandBufferManagerHost::OnWindowAdded(WaylandWindow* window) {
  DCHECK(window);
  surfaces_[window->GetWidget()] =
      std::make_unique<Surface>(window, connection_, this);
}

void WaylandBufferManagerHost::OnWindowRemoved(WaylandWindow* window) {
  DCHECK(window);
  auto it = surfaces_.find(window->GetWidget());
  DCHECK(it != surfaces_.end());
  if (it->second->HasBuffers())
    it->second->OnWindowRemoved();
  else
    surfaces_.erase(it);
}

void WaylandBufferManagerHost::SetTerminateGpuCallback(
    base::OnceCallback<void(std::string)> terminate_callback) {
  terminate_gpu_cb_ = std::move(terminate_callback);
}

mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost>
WaylandBufferManagerHost::BindInterface() {
  DCHECK(!receiver_.is_bound());
  mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost>
      buffer_manager_host;
  receiver_.Bind(buffer_manager_host.InitWithNewPipeAndPassReceiver());
  return buffer_manager_host;
}

void WaylandBufferManagerHost::OnChannelDestroyed() {
  buffer_manager_gpu_associated_.reset();
  receiver_.reset();

  for (auto& surface_pair : surfaces_)
    surface_pair.second->ClearState();

  anonymous_buffers_.clear();
}

wl::BufferFormatsWithModifiersMap
WaylandBufferManagerHost::GetSupportedBufferFormats() const {
  if (connection_->zwp_dmabuf())
    return connection_->zwp_dmabuf()->supported_buffer_formats();
  else if (connection_->drm())
    return connection_->drm()->supported_buffer_formats();
  else
    return {};
}

bool WaylandBufferManagerHost::SupportsDmabuf() const {
  return !!connection_->zwp_dmabuf() ||
         (connection_->drm() && connection_->drm()->SupportsDrmPrime());
}

void WaylandBufferManagerHost::SetWaylandBufferManagerGpu(
    mojo::PendingAssociatedRemote<ozone::mojom::WaylandBufferManagerGpu>
        buffer_manager_gpu_associated) {
  buffer_manager_gpu_associated_.Bind(std::move(buffer_manager_gpu_associated));
}

void WaylandBufferManagerHost::CreateDmabufBasedBuffer(
    mojo::PlatformHandle dmabuf_fd,
    const gfx::Size& size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  DCHECK(error_message_.empty());

  TRACE_EVENT2("wayland", "WaylandBufferManagerHost::CreateDmabufBasedBuffer",
               "Format", format, "Buffer id", buffer_id);

  base::ScopedFD fd = dmabuf_fd.TakeFD();

  // Validate data and ask surface to create a buffer associated with the
  // |buffer_id|.
  if (!ValidateDataFromGpu(fd, size, strides, offsets, modifiers, format,
                           planes_count, buffer_id) ||
      !CreateBuffer(size, buffer_id)) {
    TerminateGpuProcess();
    return;
  }

  // Create wl_buffer associated with the internal Buffer.
  auto callback =
      base::BindOnce(&WaylandBufferManagerHost::OnCreateBufferComplete,
                     weak_factory_.GetWeakPtr(), buffer_id);
  if (connection_->zwp_dmabuf()) {
    connection_->zwp_dmabuf()->CreateBuffer(std::move(fd), size, strides,
                                            offsets, modifiers, format,
                                            planes_count, std::move(callback));
  } else if (connection_->drm()) {
    connection_->drm()->CreateBuffer(std::move(fd), size, strides, offsets,
                                     modifiers, format, planes_count,
                                     std::move(callback));
  } else {
    // This method must never be called if neither zwp_linux_dmabuf or wl_drm
    // are supported.
    NOTREACHED();
  }
}

void WaylandBufferManagerHost::CreateShmBasedBuffer(mojo::PlatformHandle shm_fd,
                                                    uint64_t length,
                                                    const gfx::Size& size,
                                                    uint32_t buffer_id) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  DCHECK(error_message_.empty());

  TRACE_EVENT1("wayland", "WaylandBufferManagerHost::CreateShmBasedBuffer",
               "Buffer id", buffer_id);

  base::ScopedFD fd = shm_fd.TakeFD();
  // Validate data and create a buffer associated with the |buffer_id|.
  if (!ValidateDataFromGpu(fd, length, size, buffer_id) ||
      !CreateBuffer(size, buffer_id)) {
    TerminateGpuProcess();
    return;
  }

  // Create a shm based wl_buffer and attach it to the created buffer.
  auto buffer = connection_->shm()->CreateBuffer(std::move(fd), length, size);
  OnCreateBufferComplete(buffer_id, std::move(buffer));

  connection_->ScheduleFlush();
}

void WaylandBufferManagerHost::CommitBuffer(gfx::AcceleratedWidget widget,
                                            uint32_t buffer_id,
                                            const gfx::Rect& damage_region) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());

  TRACE_EVENT1("wayland", "WaylandBufferManagerHost::CommitBuffer", "Buffer id",
               buffer_id);

  DCHECK(error_message_.empty());

  if (widget == gfx::kNullAcceleratedWidget) {
    error_message_ = "Invalid widget.";
  } else if (ValidateBufferIdFromGpu(buffer_id)) {
    Surface* surface = GetSurface(widget);
    if (!surface)
      return;

    if (!surface->CommitBuffer(buffer_id, damage_region)) {
      error_message_ =
          base::StrCat({"Buffer with ", NumberToString(buffer_id),
                        " id does not exist or failed to be created."});
    }
  }

  if (!error_message_.empty())
    TerminateGpuProcess();
}

void WaylandBufferManagerHost::DestroyBuffer(gfx::AcceleratedWidget widget,
                                             uint32_t buffer_id) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());

  TRACE_EVENT1("wayland", "WaylandBufferManagerHost::DestroyBuffer",
               "Buffer id", buffer_id);

  DCHECK(error_message_.empty());
  if (!ValidateBufferIdFromGpu(buffer_id)) {
    TerminateGpuProcess();
    return;
  }

  // We allow creating buffers and attaching them to surfaces later. Thus, we
  // must pay attention to the following things during the destruction of the
  // buffers.
  // 1) the |widget| is basically a hint where we must search for buffers. If no
  // such surface exists (has already been destroyed), check if the buffer still
  // has been stored in the |anonymous_buffers_|.
  // 2) if the |widget| is null, always search a buffer with the |buffer_id| in
  // the |anonymous_buffers_|.

  uint32_t destroyed_count = 0u;

  Surface* surface = GetSurface(widget);
  if (surface) {
    destroyed_count = surface->DestroyBuffer(buffer_id);
    if (!surface->HasBuffers() && !surface->HasWindow())
      surfaces_.erase(widget);
  }

  // Ensure that we can't destroy more than 1 buffer. This can be 0 as well
  // if no buffers are destroyed.
  DCHECK_LE(destroyed_count, 1u);

  if (destroyed_count == 1u || DestroyAnonymousBuffer(buffer_id))
    return;

  error_message_ = base::StrCat(
      {"Buffer with ", NumberToString(buffer_id), " id does not exist"});
  TerminateGpuProcess();
}

void WaylandBufferManagerHost::ResetSurfaceContents(
    gfx::AcceleratedWidget widget) {
  auto* surface = GetSurface(widget);
  DCHECK(surface);
  surface->ResetSurfaceContents();
}

std::unique_ptr<WaylandBuffer> WaylandBufferManagerHost::PassAnonymousWlBuffer(
    uint32_t buffer_id) {
  auto it = anonymous_buffers_.find(buffer_id);
  if (it == anonymous_buffers_.end())
    return nullptr;
  auto buffer = std::move(it->second);
  anonymous_buffers_.erase(it);
  return buffer;
}

bool WaylandBufferManagerHost::CreateBuffer(const gfx::Size& size,
                                            uint32_t buffer_id) {
  // First check if any of the surfaces has already had a buffer with the same
  // id.
  for (auto const& surface : surfaces_) {
    if (surface.second->BufferExists(buffer_id)) {
      error_message_ = base::StrCat(
          {"A buffer with id= ", NumberToString(buffer_id), " already exists"});
      return false;
    }
  }

  auto result = anonymous_buffers_.emplace(
      buffer_id, std::make_unique<WaylandBuffer>(size, buffer_id));
  if (!result.second)
    error_message_ = base::StrCat(
        {"A buffer with id= ", NumberToString(buffer_id), " already exists"});
  return result.second;
}

WaylandBufferManagerHost::Surface* WaylandBufferManagerHost::GetSurface(
    gfx::AcceleratedWidget widget) const {
  auto it = surfaces_.find(widget);
  return it != surfaces_.end() ? it->second.get() : nullptr;
}

bool WaylandBufferManagerHost::ValidateDataFromGpu(
    const base::ScopedFD& fd,
    const gfx::Size& size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  if (!ValidateBufferIdFromGpu(buffer_id))
    return false;

  std::string reason;
  if (!fd.is_valid())
    reason = "Buffer fd is invalid";

  if (size.IsEmpty())
    reason = "Buffer size is invalid";

  if (planes_count < 1)
    reason = "Planes count cannot be less than 1";

  if (planes_count != strides.size() || planes_count != offsets.size() ||
      planes_count != modifiers.size()) {
    reason = base::StrCat({"Number of strides(", NumberToString(strides.size()),
                           ")/offsets(", NumberToString(offsets.size()),
                           ")/modifiers(", NumberToString(modifiers.size()),
                           ") does not correspond to the number of planes(",
                           NumberToString(planes_count), ")"});
  }

  for (auto stride : strides) {
    if (stride == 0)
      reason = "Strides are invalid";
  }

  if (!IsValidBufferFormat(format))
    reason = "Buffer format is invalid";

  if (!reason.empty()) {
    error_message_ = std::move(reason);
    return false;
  }
  return true;
}

bool WaylandBufferManagerHost::ValidateBufferIdFromGpu(uint32_t buffer_id) {
  std::string reason;
  if (buffer_id < 1)
    reason = base::StrCat({"Invalid buffer id: ", NumberToString(buffer_id)});

  if (!reason.empty()) {
    error_message_ = std::move(reason);
    return false;
  }

  return true;
}

bool WaylandBufferManagerHost::ValidateDataFromGpu(
    const base::ScopedFD& fd,
    size_t length,
    const gfx::Size& size,
    uint32_t buffer_id) {
  if (!ValidateBufferIdFromGpu(buffer_id))
    return false;

  std::string reason;
  if (!fd.is_valid())
    reason = "Buffer fd is invalid";

  if (length == 0)
    reason = "The shm pool length cannot be less than 1";

  if (size.IsEmpty())
    reason = "Buffer size is invalid";

  if (!reason.empty()) {
    error_message_ = std::move(reason);
    return false;
  }

  return true;
}

void WaylandBufferManagerHost::OnCreateBufferComplete(
    uint32_t buffer_id,
    wl::Object<struct wl_buffer> new_buffer) {
    auto it = anonymous_buffers_.find(buffer_id);
    // It might have already been destroyed or stored by any of the surfaces.
    if (it != anonymous_buffers_.end()) {
      it->second->wl_buffer = std::move(new_buffer);
    } else {
      for (auto& surface : surfaces_) {
        if (surface.second->BufferExists(buffer_id)) {
          surface.second.get()->AttachWlBuffer(buffer_id,
                                               std::move(new_buffer));
          break;
        }
      }
    }
    // There is no need for the buffer anymore. Let it go out of the scope and
    // be destroyed.
}

void WaylandBufferManagerHost::OnSubmission(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::SwapResult& swap_result) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());

  DCHECK(buffer_manager_gpu_associated_);
  buffer_manager_gpu_associated_->OnSubmission(widget, buffer_id, swap_result);
}

void WaylandBufferManagerHost::OnPresentation(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());

  DCHECK(buffer_manager_gpu_associated_);
  buffer_manager_gpu_associated_->OnPresentation(widget, buffer_id, feedback);
}

void WaylandBufferManagerHost::TerminateGpuProcess() {
  DCHECK(!error_message_.empty());
  std::move(terminate_gpu_cb_).Run(std::move(error_message_));
  // The GPU process' failure results in calling ::OnChannelDestroyed.
}

bool WaylandBufferManagerHost::DestroyAnonymousBuffer(uint32_t buffer_id) {
  auto it = anonymous_buffers_.find(buffer_id);
  if (it == anonymous_buffers_.end())
    return false;

  anonymous_buffers_.erase(it);
  return true;
}

}  // namespace ui
