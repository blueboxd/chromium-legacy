// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zwp_linux_dmabuf.h"

#include <drm_fourcc.h>
#include <linux-dmabuf-unstable-v1-client-protocol.h>

#include "ui/ozone/common/linux/drm_util_linux.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

namespace {
constexpr uint32_t kImmedVerstion = 3;
}

WaylandZwpLinuxDmabuf::WaylandZwpLinuxDmabuf(
    zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
    WaylandConnection* connection)
    : zwp_linux_dmabuf_(zwp_linux_dmabuf), connection_(connection) {
  static const zwp_linux_dmabuf_v1_listener dmabuf_listener = {
      &WaylandZwpLinuxDmabuf::Format,
      &WaylandZwpLinuxDmabuf::Modifiers,
  };
  zwp_linux_dmabuf_v1_add_listener(zwp_linux_dmabuf_.get(), &dmabuf_listener,
                                   this);

  // A roundtrip after binding guarantees that the client has received all
  // supported formats.
  wl_display_roundtrip(connection_->display());
}

WaylandZwpLinuxDmabuf::~WaylandZwpLinuxDmabuf() = default;

void WaylandZwpLinuxDmabuf::CreateBuffer(base::ScopedFD fd,
                                         const gfx::Size& size,
                                         const std::vector<uint32_t>& strides,
                                         const std::vector<uint32_t>& offsets,
                                         const std::vector<uint64_t>& modifiers,
                                         uint32_t format,
                                         uint32_t planes_count,
                                         wl::OnRequestBufferCallback callback) {
  static const struct zwp_linux_buffer_params_v1_listener params_listener = {
      &WaylandZwpLinuxDmabuf::CreateSucceeded,
      &WaylandZwpLinuxDmabuf::CreateFailed};

  struct zwp_linux_buffer_params_v1* params =
      zwp_linux_dmabuf_v1_create_params(zwp_linux_dmabuf_.get());

  for (size_t i = 0; i < planes_count; i++) {
    uint32_t modifier_lo = 0;
    uint32_t modifier_hi = 0;
    if (modifiers[i] != DRM_FORMAT_MOD_INVALID) {
      modifier_lo = modifiers[i] & UINT32_MAX;
      modifier_hi = modifiers[i] >> 32;
    } else {
      DCHECK_EQ(planes_count, 1u) << "Invalid modifier may be passed only in "
                                     "case of single plane format being used";
    }
    zwp_linux_buffer_params_v1_add(params, fd.get(), i /* plane id */,
                                   offsets[i], strides[i], modifier_hi,
                                   modifier_lo);
  }

  // It's possible to avoid waiting until the buffer is created and have it
  // immediately. This method is only available since the protocol version 3.
  if (zwp_linux_dmabuf_v1_get_version(zwp_linux_dmabuf_.get()) >=
      kImmedVerstion) {
    wl::Object<wl_buffer> buffer(zwp_linux_buffer_params_v1_create_immed(
        params, size.width(), size.height(), format, 0));
    std::move(callback).Run(std::move(buffer));
  } else {
    // Store the |params| with the corresponding |callback| to identify newly
    // created buffer and notify the client about it via the |callback|.
    pending_params_.emplace(params, std::move(callback));

    zwp_linux_buffer_params_v1_add_listener(params, &params_listener, this);
    zwp_linux_buffer_params_v1_create(params, size.width(), size.height(),
                                      format, 0);
  }
  connection_->ScheduleFlush();
}

void WaylandZwpLinuxDmabuf::AddSupportedFourCCFormat(uint32_t fourcc_format) {
  // Return on not the supported fourcc format.
  if (!IsValidBufferFormat(fourcc_format))
    return;

  // It can happen that ::Format or ::Modifiers call can have already added
  // such a format. Thus, we can ignore that format.
  gfx::BufferFormat format = GetBufferFormatFromFourCCFormat(fourcc_format);
  auto it = std::find(supported_buffer_formats_.begin(),
                      supported_buffer_formats_.end(), format);
  if (it != supported_buffer_formats_.end())
    return;
  supported_buffer_formats_.push_back(format);
}

void WaylandZwpLinuxDmabuf::NotifyRequestCreateBufferDone(
    struct zwp_linux_buffer_params_v1* params,
    struct wl_buffer* new_buffer) {
  auto it = pending_params_.find(params);
  DCHECK(it != pending_params_.end());

  std::move(it->second).Run(wl::Object<struct wl_buffer>(new_buffer));

  pending_params_.erase(it);
  zwp_linux_buffer_params_v1_destroy(params);

  connection_->ScheduleFlush();
}

// static
void WaylandZwpLinuxDmabuf::Modifiers(
    void* data,
    struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
    uint32_t format,
    uint32_t modifier_hi,
    uint32_t modifier_lo) {
  WaylandZwpLinuxDmabuf* self = static_cast<WaylandZwpLinuxDmabuf*>(data);
  if (self)
    self->AddSupportedFourCCFormat(format);
}

// static
void WaylandZwpLinuxDmabuf::Format(void* data,
                                   struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
                                   uint32_t format) {
  WaylandZwpLinuxDmabuf* self = static_cast<WaylandZwpLinuxDmabuf*>(data);
  if (self)
    self->AddSupportedFourCCFormat(format);
}

// static
void WaylandZwpLinuxDmabuf::CreateSucceeded(
    void* data,
    struct zwp_linux_buffer_params_v1* params,
    struct wl_buffer* new_buffer) {
  WaylandZwpLinuxDmabuf* self = static_cast<WaylandZwpLinuxDmabuf*>(data);
  if (self)
    self->NotifyRequestCreateBufferDone(params, new_buffer);
}

// static
void WaylandZwpLinuxDmabuf::CreateFailed(
    void* data,
    struct zwp_linux_buffer_params_v1* params) {
  WaylandZwpLinuxDmabuf* self = static_cast<WaylandZwpLinuxDmabuf*>(data);
  if (self)
    self->NotifyRequestCreateBufferDone(params, nullptr);
}

}  // namespace ui
