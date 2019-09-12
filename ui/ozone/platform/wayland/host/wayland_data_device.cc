// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_data_device.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aura.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

namespace {

constexpr char kMimeTypeText[] = "text/plain";
constexpr char kMimeTypeTextUTF8[] = "text/plain;charset=utf-8";

int GetOperation(uint32_t source_actions, uint32_t dnd_action) {
  uint32_t action = dnd_action != WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE
                        ? dnd_action
                        : source_actions;

  int operation = DragDropTypes::DRAG_NONE;
  if (action & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
    operation |= DragDropTypes::DRAG_COPY;
  if (action & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE)
    operation |= DragDropTypes::DRAG_MOVE;
  // TODO(jkim): Implement branch for WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK
  if (action & WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK)
    operation |= DragDropTypes::DRAG_COPY;
  return operation;
}

void AddStringToOSExchangeData(const std::string& data,
                               OSExchangeData* os_exchange_data) {
  DCHECK(os_exchange_data);
  if (data.empty())
    return;

  base::string16 string16 = base::UTF8ToUTF16(data);
  os_exchange_data->SetString(string16);
}

void AddToOSExchangeData(const std::string& data,
                         const std::string& mime_type,
                         OSExchangeData* os_exchange_data) {
  DCHECK(os_exchange_data);
  if ((mime_type == kMimeTypeText || mime_type == kMimeTypeTextUTF8)) {
    DCHECK(!os_exchange_data->HasString());
    AddStringToOSExchangeData(data, os_exchange_data);
    return;
  }
  // TODO(crbug.com/875164): Fix mime types support.
  NOTREACHED();
}

}  // namespace

// static
WaylandDataDevice::WaylandDataDevice(WaylandConnection* connection,
                                     wl_data_device* data_device)
    : data_device_(data_device), connection_(connection) {
  static const struct wl_data_device_listener kDataDeviceListener = {
      WaylandDataDevice::OnDataOffer, WaylandDataDevice::OnEnter,
      WaylandDataDevice::OnLeave,     WaylandDataDevice::OnMotion,
      WaylandDataDevice::OnDrop,      WaylandDataDevice::OnSelection};
  wl_data_device_add_listener(data_device_.get(), &kDataDeviceListener, this);
}

WaylandDataDevice::~WaylandDataDevice() = default;

bool WaylandDataDevice::RequestSelectionData(const std::string& mime_type) {
  if (!selection_offer_)
    return false;

  base::ScopedFD fd = selection_offer_->Receive(mime_type);
  if (!fd.is_valid()) {
    LOG(ERROR) << "Failed to open file descriptor.";
    return false;
  }

  // Ensure there is not pending operation to be performed by the compositor,
  // otherwise read(..) can block awaiting data to be sent to pipe.
  deferred_read_closure_ =
      base::BindOnce(&WaylandDataDevice::ReadClipboardDataFromFD,
                     base::Unretained(this), std::move(fd), mime_type);
  RegisterDeferredReadCallback();
  return true;
}

void WaylandDataDevice::RequestDragData(
    const std::string& mime_type,
    base::OnceCallback<void(const std::string&)> callback) {
  base::ScopedFD fd = drag_offer_->Receive(mime_type);
  if (!fd.is_valid()) {
    LOG(ERROR) << "Failed to open file descriptor.";
    return;
  }

  // Ensure there is not pending operation to be performed by the compositor,
  // otherwise read(..) can block awaiting data to be sent to pipe.
  deferred_read_closure_ = base::BindOnce(
      &WaylandDataDevice::ReadDragDataFromFD, base::Unretained(this),
      std::move(fd), std::move(callback));
  RegisterDeferredReadCallback();
}

void WaylandDataDevice::DeliverDragData(const std::string& mime_type,
                                        std::string* buffer) {
  DCHECK(buffer);
  DCHECK(source_data_);

  if (mime_type != kMimeTypeText && mime_type != kMimeTypeTextUTF8)
    return;

  const OSExchangeData::FilenameToURLPolicy policy =
      OSExchangeData::FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES;
  // TODO(jkim): Handle other data format as well.
  if (source_data_->HasURL(policy)) {
    GURL url;
    base::string16 title;
    source_data_->GetURLAndTitle(policy, &url, &title);
    buffer->append(url.spec());
    return;
  }

  if (source_data_->HasString()) {
    base::string16 data;
    source_data_->GetString(&data);
    buffer->append(base::UTF16ToUTF8(data));
    return;
  }
}

void WaylandDataDevice::StartDrag(wl_data_source* data_source,
                                  const ui::OSExchangeData& data) {
  DCHECK(data_source);

  WaylandWindow* window =
      connection_->wayland_window_manager()->GetCurrentFocusedWindow();
  if (!window) {
    LOG(ERROR) << "Failed to get focused window.";
    return;
  }
  const SkBitmap* icon = PrepareDragIcon(data);
  source_data_ = std::make_unique<ui::OSExchangeData>(data.provider().Clone());
  wl_data_device_start_drag(data_device_.get(), data_source, window->surface(),
                            icon_surface_.get(), connection_->serial());
  if (icon)
    DrawDragIcon(icon);
  connection_->ScheduleFlush();
}

void WaylandDataDevice::ResetSourceData() {
  source_data_.reset();
}

std::vector<std::string> WaylandDataDevice::GetAvailableMimeTypes() {
  if (selection_offer_)
    return selection_offer_->GetAvailableMimeTypes();

  return std::vector<std::string>();
}

void WaylandDataDevice::ReadClipboardDataFromFD(base::ScopedFD fd,
                                                const std::string& mime_type) {
  std::string contents;
  ReadDataFromFD(std::move(fd), &contents);
  connection_->clipboard()->SetData(contents, mime_type);
}

void WaylandDataDevice::ReadDragDataFromFD(
    base::ScopedFD fd,
    base::OnceCallback<void(const std::string&)> callback) {
  std::string contents;
  ReadDataFromFD(std::move(fd), &contents);
  std::move(callback).Run(contents);
}

void WaylandDataDevice::ReadDataFromFD(base::ScopedFD fd,
                                       std::string* contents) {
  DCHECK(contents);
  char buffer[1 << 10];  // 1 kB in bytes.
  ssize_t length;
  while ((length = read(fd.get(), buffer, sizeof(buffer))) > 0)
    contents->append(buffer, length);
}

void WaylandDataDevice::HandleDeferredLeaveIfNeeded() {
  if (!is_leaving_)
    return;

  OnLeave(this, data_device_.get());
}

// static
void WaylandDataDevice::OnDataOffer(void* data,
                                    wl_data_device* data_device,
                                    wl_data_offer* offer) {
  auto* self = static_cast<WaylandDataDevice*>(data);

  self->connection_->clipboard()->UpdateSequenceNumber(
      ClipboardBuffer::kCopyPaste);

  DCHECK(!self->new_offer_);
  self->new_offer_ = std::make_unique<WaylandDataOffer>(offer);
}

void WaylandDataDevice::OnEnter(void* data,
                                wl_data_device* data_device,
                                uint32_t serial,
                                wl_surface* surface,
                                wl_fixed_t x,
                                wl_fixed_t y,
                                wl_data_offer* offer) {
  WaylandWindow* window =
      static_cast<WaylandWindow*>(wl_surface_get_user_data(surface));
  if (!window) {
    LOG(ERROR) << "Failed to get window.";
    return;
  }

  auto* self = static_cast<WaylandDataDevice*>(data);
  DCHECK(self->new_offer_);
  DCHECK(!self->drag_offer_);
  self->drag_offer_ = std::move(self->new_offer_);
  self->window_ = window;

  // TODO(crbug.com/875164): Set mime type the client can accept. Now it sets
  // all mime types offered because current implementation doesn't decide
  // action based on mime type.
  self->unprocessed_mime_types_.clear();
  for (auto mime : self->drag_offer_->GetAvailableMimeTypes()) {
    self->unprocessed_mime_types_.push_back(mime);
    self->drag_offer_->Accept(serial, mime);
  }

  int operation = GetOperation(self->drag_offer_->source_actions(),
                               self->drag_offer_->dnd_action());
  gfx::PointF point(wl_fixed_to_double(x), wl_fixed_to_double(y));

  // If |source_data_| is set, it means that dragging is started from the
  // same window and it's not needed to read data through Wayland.
  std::unique_ptr<OSExchangeData> pdata;
  if (!self->IsDraggingExternalData())
    pdata = std::make_unique<OSExchangeData>(
        self->source_data_->provider().Clone());
  self->window_->OnDragEnter(point, std::move(pdata), operation);
}

void WaylandDataDevice::OnMotion(void* data,
                                 wl_data_device* data_device,
                                 uint32_t time,
                                 wl_fixed_t x,
                                 wl_fixed_t y) {
  auto* self = static_cast<WaylandDataDevice*>(data);
  if (!self->window_) {
    LOG(ERROR) << "Failed to get window.";
    return;
  }

  int operation = GetOperation(self->drag_offer_->source_actions(),
                               self->drag_offer_->dnd_action());
  gfx::PointF point(wl_fixed_to_double(x), wl_fixed_to_double(y));
  int client_operation = self->window_->OnDragMotion(point, time, operation);
  self->SetOperation(client_operation);
}

void WaylandDataDevice::OnDrop(void* data, wl_data_device* data_device) {
  auto* self = static_cast<WaylandDataDevice*>(data);
  if (!self->window_) {
    LOG(ERROR) << "Failed to get window.";
    return;
  }
  if (!self->IsDraggingExternalData()) {
    // When the drag session started from a chromium window, source_data_
    // already holds the data and already forwarded it to delegate through
    // OnDragEnter, so at this point (onDragDrop) the delegate expects a
    // nullptr and the data will be read internally with no need to read it
    // through Wayland pipe and so on.
    self->HandleReceivedData(nullptr);
  } else {
    // Creates buffer to receive data from Wayland.
    self->received_data_ = std::make_unique<OSExchangeData>(
        std::make_unique<OSExchangeDataProviderAura>());
    // In order to guarantee all data received, it sets
    // |is_handling_dropped_data_| and defers OnLeave event handling if it gets
    // OnLeave event before completing to read the data.
    self->is_handling_dropped_data_ = true;
    // Starts to read the data on Drop event because read(..) API blocks
    // awaiting data to be sent to pipe if we try to read the data on OnEnter.
    // 'Weston' also reads data on OnDrop event and other examples do as well.
    self->HandleUnprocessedMimeTypes();
  }
}

void WaylandDataDevice::OnLeave(void* data, wl_data_device* data_device) {
  // While reading data, it could get OnLeave event. We don't handle OnLeave
  // event directly if |is_handling_dropped_data_| is set.
  auto* self = static_cast<WaylandDataDevice*>(data);
  if (!self->window_) {
    LOG(ERROR) << "Failed to get window.";
    return;
  }

  if (self->is_handling_dropped_data_) {
    self->is_leaving_ = true;
    return;
  }

  self->window_->OnDragLeave();
  self->window_ = nullptr;
  self->drag_offer_.reset();
  self->is_handling_dropped_data_ = false;
  self->is_leaving_ = false;
}

// static
void WaylandDataDevice::OnSelection(void* data,
                                    wl_data_device* data_device,
                                    wl_data_offer* offer) {
  auto* self = static_cast<WaylandDataDevice*>(data);
  DCHECK(self);

  // 'offer' will be null to indicate that the selection is no longer valid,
  // i.e. there is no longer clipboard data available to paste.
  if (!offer) {
    self->selection_offer_.reset();

    // Clear Clipboard cache.
    self->connection_->clipboard()->SetData(std::string(), std::string());
    return;
  }

  DCHECK(self->new_offer_);
  self->selection_offer_ = std::move(self->new_offer_);

  self->selection_offer_->EnsureTextMimeTypeIfNeeded();
}

void WaylandDataDevice::RegisterDeferredReadCallback() {
  static const wl_callback_listener kDeferredReadListener = {
      WaylandDataDevice::DeferredReadCallback};

  DCHECK(!deferred_read_callback_);
  deferred_read_callback_.reset(wl_display_sync(connection_->display()));
  wl_callback_add_listener(deferred_read_callback_.get(),
                           &kDeferredReadListener, this);
  connection_->ScheduleFlush();
}

void WaylandDataDevice::DeferredReadCallback(void* data,
                                             struct wl_callback* cb,
                                             uint32_t time) {
  auto* data_device = static_cast<WaylandDataDevice*>(data);
  DCHECK(data_device);
  DCHECK(!data_device->deferred_read_closure_.is_null());
  std::move(data_device->deferred_read_closure_).Run();
  data_device->deferred_read_callback_.reset();
}

const SkBitmap* WaylandDataDevice::PrepareDragIcon(const OSExchangeData& data) {
  const SkBitmap* icon_bitmap = data.provider().GetDragImage().bitmap();
  if (!icon_bitmap || icon_bitmap->empty())
    return nullptr;
  icon_surface_.reset(wl_compositor_create_surface(connection_->compositor()));
  DCHECK(icon_surface_);
  return icon_bitmap;
}

void WaylandDataDevice::DrawDragIcon(const SkBitmap* icon_bitmap) {
  DCHECK(icon_bitmap);
  DCHECK(!icon_bitmap->empty());
  gfx::Size size(icon_bitmap->width(), icon_bitmap->height());

  if (!shm_buffer_ || shm_buffer_->size() != size) {
    shm_buffer_ = std::make_unique<WaylandShmBuffer>(connection_->shm(), size);
    if (!shm_buffer_->IsValid()) {
      LOG(ERROR) << "Failed to create drag icon buffer.";
      return;
    }
  }
  wl::DrawBitmap(*icon_bitmap, shm_buffer_.get());

  wl_surface* surface = icon_surface_.get();
  wl_surface_attach(surface, shm_buffer_->get(), 0, 0);
  wl_surface_damage(surface, 0, 0, size.width(), size.height());
  wl_surface_commit(surface);
}

void WaylandDataDevice::HandleUnprocessedMimeTypes() {
  std::string mime_type = SelectNextMimeType();
  if (mime_type.empty()) {
    HandleReceivedData(std::move(received_data_));
  } else {
    RequestDragData(mime_type,
                    base::BindOnce(&WaylandDataDevice::OnDragDataReceived,
                                   base::Unretained(this)));
  }
}

void WaylandDataDevice::OnDragDataReceived(const std::string& contents) {
  if (!contents.empty()) {
    AddToOSExchangeData(contents, unprocessed_mime_types_.front(),
                        received_data_.get());
  }

  unprocessed_mime_types_.pop_front();

  // Continue reading data for other negotiated mime types.
  HandleUnprocessedMimeTypes();
}

void WaylandDataDevice::HandleReceivedData(
    std::unique_ptr<ui::OSExchangeData> received_data) {
  // TODO(crbug.com/875164): Fix mime types support.
  unprocessed_mime_types_.clear();

  window_->OnDragDrop(std::move(received_data));
  drag_offer_->FinishOffer();
  is_handling_dropped_data_ = false;
  HandleDeferredLeaveIfNeeded();
}

std::string WaylandDataDevice::SelectNextMimeType() {
  while (!unprocessed_mime_types_.empty()) {
    std::string& mime_type = unprocessed_mime_types_.front();
    if ((mime_type == kMimeTypeText || mime_type == kMimeTypeTextUTF8) &&
        !received_data_->HasString()) {
      return mime_type;
    }
    // TODO(crbug.com/875164): Fix mime types support.
    unprocessed_mime_types_.pop_front();
  }
  return {};
}

void WaylandDataDevice::SetOperation(const int operation) {
  uint32_t dnd_actions = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
  uint32_t preferred_action = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;

  if (operation & DragDropTypes::DRAG_COPY) {
    dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
    preferred_action = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
  }

  if (operation & DragDropTypes::DRAG_MOVE) {
    dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
    if (preferred_action == WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE)
      preferred_action = WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
  }
  drag_offer_->SetAction(dnd_actions, preferred_action);
}

}  // namespace ui
