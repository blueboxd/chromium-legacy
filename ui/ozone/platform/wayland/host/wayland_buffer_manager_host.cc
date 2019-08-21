// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"

#include <presentation-time-client-protocol.h>
#include <memory>

#include "base/i18n/number_formatting.h"
#include "base/message_loop/message_loop_current.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/ozone/common/linux/drm_util_linux.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
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

    WaylandBuffer* buffer = GetBuffer(buffer_id);
    if (!buffer)
      return false;

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
    if (buffer && !buffer->released && submitted_buffer_)
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

    if (pending_buffer_ == buffer)
      ProcessPendingBuffer();
  }

  void ClearState() {
    buffers_.clear();
    wl_frame_callback_.reset();
    presentation_feedbacks_ = PresentationFeedbackQueue();

    ResetSurfaceContents();

    prev_submitted_buffer_ = nullptr;
    submitted_buffer_ = nullptr;

    connection_->ScheduleFlush();
  }

  void ResetSurfaceContents() {
    wl_surface_attach(window_->surface(), nullptr, 0, 0);
    wl_surface_commit(window_->surface());

    // We cannot reset |prev_submitted_buffer_| here as long as the surface
    // might have attached a new buffer and is about to receive a release
    // callback. Check more comments below where the variable is declared.
    contents_reset_ = true;

    connection_->ScheduleFlush();
  }

 private:
  using PresentationFeedbackQueue = base::queue<
      std::pair<uint32_t, wl::Object<struct wp_presentation_feedback>>>;

  // This is an internal helper representation of a wayland buffer object, which
  // the GPU process creates when CreateBuffer is called. It's used for
  // asynchronous buffer creation and stores |params| parameter to find out,
  // which Buffer the wl_buffer corresponds to when CreateSucceeded is called.
  // What is more, the Buffer stores such information as a widget it is attached
  // to, its buffer id for simpler buffer management and other members specific
  // to this Buffer object on run-time.
  struct WaylandBuffer {
    WaylandBuffer() = delete;
    WaylandBuffer(const gfx::Size& size, uint32_t buffer_id);
    ~WaylandBuffer();

    // Actual buffer size.
    const gfx::Size size;

    // Damage region this buffer describes. Must be emptied once buffer is
    // submitted.
    gfx::Rect damage_region;

    // The id of this buffer.
    const uint32_t buffer_id;

    // A wl_buffer backed by a dmabuf created on the GPU side.
    wl::Object<struct wl_buffer> wl_buffer;

    // Tells if the buffer has the wl_buffer attached. This can be used to
    // identify potential problems, when the Wayland compositor fails to create
    // wl_buffers.
    bool attached = false;

    // Tells if the buffer has already been released aka not busy, and the
    // surface can tell the gpu about successful swap.
    bool released = true;

    // In some cases, a presentation feedback can come earlier than we fire a
    // submission callback. Thus, instead of sending it immediately to the GPU
    // process, we store it and fire as soon as the submission callback is
    // fired.
    bool needs_send_feedback = false;

    gfx::PresentationFeedback feedback;

    DISALLOW_COPY_AND_ASSIGN(WaylandBuffer);
  };

  bool CommitBufferInternal(WaylandBuffer* buffer) {
    DCHECK(buffer);
    DCHECK(!pending_buffer_);

    // Once the BufferRelease is called, the buffer will be released.
    DCHECK(buffer->released);
    buffer->released = false;

    DCHECK(!submitted_buffer_);
    submitted_buffer_ = buffer;

    AttachAndDamageBuffer(buffer);

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
    if (!prev_submitted_buffer_)
      CompleteSubmission();

    return true;
  }

  void AttachAndDamageBuffer(WaylandBuffer* buffer) {
    gfx::Rect pending_damage_region = std::move(buffer->damage_region);
    // If the size of the damage region is empty, wl_surface_damage must be
    // supplied with the actual size of the buffer, which is going to be
    // committed.
    if (pending_damage_region.size().IsEmpty())
      pending_damage_region.set_size(buffer->size);
    DCHECK(!pending_damage_region.size().IsEmpty());

    auto* surface = window_->surface();
    wl_surface_damage_buffer(
        surface, pending_damage_region.x(), pending_damage_region.y(),
        pending_damage_region.width(), pending_damage_region.height());
    wl_surface_attach(surface, buffer->wl_buffer.get(), 0, 0);
  }

  void CommitSurface() { wl_surface_commit(window_->surface()); }

  void SetupFrameCallback() {
    static const wl_callback_listener frame_listener = {
        &Surface::FrameCallbackDone};

    DCHECK(!wl_frame_callback_);
    wl_frame_callback_.reset(wl_surface_frame(window_->surface()));
    wl_callback_add_listener(wl_frame_callback_.get(), &frame_listener, this);
  }

  void SetupPresentationFeedback(uint32_t buffer_id) {
    // Set up presentation feedback.
    if (!connection_->presentation())
      return;

    static const wp_presentation_feedback_listener feedback_listener = {
        &Surface::FeedbackSyncOutput, &Surface::FeedbackPresented,
        &Surface::FeedbackDiscarded};

    presentation_feedbacks_.push(std::make_pair(
        buffer_id,
        wl::Object<struct wp_presentation_feedback>(wp_presentation_feedback(
            connection_->presentation(), window_->surface()))));
    wp_presentation_feedback_add_listener(
        presentation_feedbacks_.back().second.get(), &feedback_listener, this);
  }

  void SetupBufferReleaseListener(WaylandBuffer* buffer) {
    static struct wl_buffer_listener buffer_listener = {
        &Surface::BufferRelease,
    };
    wl_buffer_add_listener(buffer->wl_buffer.get(), &buffer_listener, this);
  }

  WaylandBuffer* GetBuffer(uint32_t buffer_id) {
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

    auto feedback = std::move(submitted_buffer_->feedback);
    bool needs_send_feedback = submitted_buffer_->needs_send_feedback;
    submitted_buffer_->needs_send_feedback = false;

    prev_submitted_buffer_ = submitted_buffer_;
    submitted_buffer_ = nullptr;
    // We can now complete the latest submission. We had to wait for this
    // release because SwapCompletionCallback indicates to the client that the
    // previous buffer is available for reuse.
    buffer_manager_->OnSubmission(window_->GetWidget(), id,
                                  gfx::SwapResult::SWAP_ACK);

    // If presentation feedback is not supported, use a fake feedback. This
    // literally means there are no presentation feedback callbacks created.
    if (!connection_->presentation()) {
      DCHECK(presentation_feedbacks_.empty());
      OnPresentation(id, gfx::PresentationFeedback(
                             base::TimeTicks::Now(), base::TimeDelta(),
                             GetPresentationKindFlags(0)));
    } else if (needs_send_feedback) {
      OnPresentation(id, std::move(feedback));
    }
  }

  void OnPresentation(uint32_t buffer_id,
                      const gfx::PresentationFeedback& feedback) {
    // The order of submission and presentation callbacks cannot be controlled.
    // Some Wayland compositors may fire presentation callbacks earlier than we
    // are able to send submission callbacks and this is bad. Thus, handle it
    // here.
    if (submitted_buffer_ && submitted_buffer_->buffer_id == buffer_id) {
      submitted_buffer_->needs_send_feedback = true;
      submitted_buffer_->feedback = feedback;
      return;
    }

    buffer_manager_->OnPresentation(window_->GetWidget(), buffer_id, feedback);
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
    auto presentation = std::move(self->presentation_feedbacks_.front());
    DCHECK(presentation.second.get() == wp_presentation_feedback);
    self->presentation_feedbacks_.pop();
    self->OnPresentation(
        presentation.first,
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
    auto presentation = std::move(self->presentation_feedbacks_.front());
    DCHECK(presentation.second.get() == wp_presentation_feedback);
    self->presentation_feedbacks_.pop();
    self->OnPresentation(presentation.first,
                         gfx::PresentationFeedback::Failure());
  }

  void ProcessPendingBuffer() {
    if (!pending_buffer_)
      return;

    auto* buffer = pending_buffer_;
    pending_buffer_ = nullptr;
    CommitBufferInternal(buffer);
  }

  // Widget this helper surface backs and has 1:1 relationship with the
  // WaylandWindow.

  // Non-owned. The window this helper surface stores and submits buffers for.
  const WaylandWindow* const window_;

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
  PresentationFeedbackQueue presentation_feedbacks_;

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
  // sent. To avoid this, |contents_reset_| can be used as an identification of a
  // need to call submission callback manually.
  bool contents_reset_ = false;

  DISALLOW_COPY_AND_ASSIGN(Surface);
};

WaylandBufferManagerHost::Surface::WaylandBuffer::WaylandBuffer(
    const gfx::Size& size,
    uint32_t buffer_id)
    : size(size), buffer_id(buffer_id) {}
WaylandBufferManagerHost::Surface::WaylandBuffer::~WaylandBuffer() = default;

WaylandBufferManagerHost::WaylandBufferManagerHost(
    WaylandConnection* connection)
    : connection_(connection), binding_(this), weak_factory_(this) {
  connection_->wayland_window_manager()->AddObserver(this);
}

WaylandBufferManagerHost::~WaylandBufferManagerHost() {
  DCHECK(surfaces_.empty());
}

void WaylandBufferManagerHost::OnWindowAdded(WaylandWindow* window) {
  DCHECK(window);
  surfaces_[window->GetWidget()] =
      std::make_unique<Surface>(window, connection_, this);
}

void WaylandBufferManagerHost::OnWindowRemoved(WaylandWindow* window) {
  DCHECK(window);
  DCHECK(surfaces_.erase(window->GetWidget()));
}

void WaylandBufferManagerHost::SetTerminateGpuCallback(
    base::OnceCallback<void(std::string)> terminate_callback) {
  terminate_gpu_cb_ = std::move(terminate_callback);
}

ozone::mojom::WaylandBufferManagerHostPtr
WaylandBufferManagerHost::BindInterface() {
  DCHECK(!binding_.is_bound());
  ozone::mojom::WaylandBufferManagerHostPtr buffer_manager_host_ptr;
  binding_.Bind(MakeRequest(&buffer_manager_host_ptr));
  return buffer_manager_host_ptr;
}

void WaylandBufferManagerHost::OnChannelDestroyed() {
  buffer_manager_gpu_associated_ptr_.reset();
  binding_.Close();

  for (auto& surface_pair : surfaces_)
    surface_pair.second->ClearState();
}

bool WaylandBufferManagerHost::CanCreateDmabufBasedBuffer() const {
  return !!connection_->zwp_dmabuf();
}

void WaylandBufferManagerHost::SetWaylandBufferManagerGpuPtr(
    ozone::mojom::WaylandBufferManagerGpuAssociatedPtrInfo
        buffer_manager_gpu_associated_ptr) {
  buffer_manager_gpu_associated_ptr_.Bind(
      std::move(buffer_manager_gpu_associated_ptr));
}

void WaylandBufferManagerHost::CreateDmabufBasedBuffer(
    gfx::AcceleratedWidget widget,
    mojo::ScopedHandle dmabuf_fd,
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

  base::ScopedFD fd = mojo::UnwrapPlatformHandle(std::move(dmabuf_fd)).TakeFD();

  // Validate data and ask surface to create a buffer associated with the
  // |buffer_id|.
  if (!ValidateDataFromGpu(widget, fd, size, strides, offsets, modifiers,
                           format, planes_count, buffer_id) ||
      !CreateBuffer(widget, size, buffer_id)) {
    TerminateGpuProcess();
    return;
  }

  // Create wl_buffer associated with the internal Buffer.
  auto callback =
      base::BindOnce(&WaylandBufferManagerHost::OnCreateBufferComplete,
                     weak_factory_.GetWeakPtr(), widget, buffer_id);
  connection_->zwp_dmabuf()->CreateBuffer(std::move(fd), size, strides, offsets,
                                          modifiers, format, planes_count,
                                          std::move(callback));
}

void WaylandBufferManagerHost::CreateShmBasedBuffer(
    gfx::AcceleratedWidget widget,
    mojo::ScopedHandle shm_fd,
    uint64_t length,
    const gfx::Size& size,
    uint32_t buffer_id) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  DCHECK(error_message_.empty());

  TRACE_EVENT1("wayland", "WaylandBufferManagerHost::CreateShmBasedBuffer",
               "Buffer id", buffer_id);

  base::ScopedFD fd = mojo::UnwrapPlatformHandle(std::move(shm_fd)).TakeFD();
  // Validate data and create a buffer associated with the |buffer_id|.
  if (!ValidateDataFromGpu(widget, fd, length, size, buffer_id) ||
      !CreateBuffer(widget, size, buffer_id)) {
    TerminateGpuProcess();
    return;
  }

  // Create a shm based wl_buffer and attach it to the created buffer.
  auto buffer = connection_->shm()->CreateBuffer(std::move(fd), length, size);
  OnCreateBufferComplete(widget, buffer_id, std::move(buffer));

  connection_->ScheduleFlush();
}

void WaylandBufferManagerHost::CommitBuffer(gfx::AcceleratedWidget widget,
                                            uint32_t buffer_id,
                                            const gfx::Rect& damage_region) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());

  TRACE_EVENT1("wayland", "WaylandBufferManagerHost::ScheduleSwapBuffer",
               "Buffer id", buffer_id);

  DCHECK(error_message_.empty());

  if (ValidateDataFromGpu(widget, buffer_id)) {
    Surface* surface = GetSurface(widget);
    if (!surface) {
      error_message_ = "Surface does not exist";
    } else if (!surface->CommitBuffer(buffer_id, damage_region)) {
      error_message_ = "Buffer with " + NumberToString(buffer_id) +
                       " id does not exist or failed to be created.";
    }
  }

  if (!error_message_.empty())
    TerminateGpuProcess();
}

void WaylandBufferManagerHost::DestroyBuffer(gfx::AcceleratedWidget widget,
                                             uint32_t buffer_id) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());

  TRACE_EVENT1("wayland", "WaylandBufferManagerHost::DestroyZwpLinuxDmabuf",
               "Buffer id", buffer_id);

  DCHECK(error_message_.empty());

  Surface* surface = GetSurface(widget);
  // On browser shutdown, the surface might have already been destroyed.
  if (!surface)
    return;

  if (surface->DestroyBuffer(buffer_id) != 1u) {
    error_message_ =
        "Buffer with " + NumberToString(buffer_id) + " id does not exist";
    TerminateGpuProcess();
    return;
  }

  connection_->ScheduleFlush();
}

void WaylandBufferManagerHost::ResetSurfaceContents(
    gfx::AcceleratedWidget widget) {
  auto* surface = GetSurface(widget);
  DCHECK(surface);
  surface->ResetSurfaceContents();
}

bool WaylandBufferManagerHost::CreateBuffer(gfx::AcceleratedWidget& widget,
                                            const gfx::Size& size,
                                            uint32_t buffer_id) {
  WaylandBufferManagerHost::Surface* surface = GetSurface(widget);
  DCHECK(surface);

  if (!surface->CreateBuffer(size, buffer_id)) {
    error_message_ =
        "A buffer with id= " + NumberToString(buffer_id) + " already exists";
  }
  return error_message_.empty();
}

WaylandBufferManagerHost::Surface* WaylandBufferManagerHost::GetSurface(
    gfx::AcceleratedWidget widget) const {
  auto it = surfaces_.find(widget);
  return it != surfaces_.end() ? it->second.get() : nullptr;
}

bool WaylandBufferManagerHost::ValidateDataFromGpu(
    const gfx::AcceleratedWidget& widget,
    const base::ScopedFD& fd,
    const gfx::Size& size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  if (!ValidateDataFromGpu(widget, buffer_id))
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
    reason = "Number of strides(" + NumberToString(strides.size()) +
             ")/offsets(" + NumberToString(offsets.size()) + ")/modifiers(" +
             NumberToString(modifiers.size()) +
             ") does not correspond to the number of planes(" +
             NumberToString(planes_count) + ")";
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

bool WaylandBufferManagerHost::ValidateDataFromGpu(
    const gfx::AcceleratedWidget& widget,
    uint32_t buffer_id) {
  std::string reason;
  if (widget == gfx::kNullAcceleratedWidget)
    reason = "Invalid accelerated widget";

  if (buffer_id < 1)
    reason = "Invalid buffer id: " + NumberToString(buffer_id);

  if (!reason.empty()) {
    error_message_ = std::move(reason);
    return false;
  }

  return true;
}

bool WaylandBufferManagerHost::ValidateDataFromGpu(
    const gfx::AcceleratedWidget& widget,
    const base::ScopedFD& fd,
    size_t length,
    const gfx::Size& size,
    uint32_t buffer_id) {
  if (!ValidateDataFromGpu(widget, buffer_id))
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
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    wl::Object<struct wl_buffer> new_buffer) {
  Surface* surface = GetSurface(widget);
  // A surface can have been destroyed if it does not have buffers left.
  if (!surface)
    return;

  surface->AttachWlBuffer(buffer_id, std::move(new_buffer));
}

void WaylandBufferManagerHost::OnSubmission(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::SwapResult& swap_result) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());

  DCHECK(buffer_manager_gpu_associated_ptr_);
  buffer_manager_gpu_associated_ptr_->OnSubmission(widget, buffer_id,
                                                   swap_result);
}

void WaylandBufferManagerHost::OnPresentation(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());

  DCHECK(buffer_manager_gpu_associated_ptr_);
  buffer_manager_gpu_associated_ptr_->OnPresentation(widget, buffer_id,
                                                     feedback);
}

void WaylandBufferManagerHost::TerminateGpuProcess() {
  DCHECK(!error_message_.empty());
  std::move(terminate_gpu_cb_).Run(std::move(error_message_));
  // The GPU process' failure results in calling ::OnChannelDestroyed.
}

}  // namespace ui
