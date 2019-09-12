// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_clipboard.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device_manager.h"

namespace ui {

WaylandClipboard::WaylandClipboard(
    WaylandDataDeviceManager* data_device_manager,
    WaylandDataDevice* data_device)
    : data_device_manager_(data_device_manager), data_device_(data_device) {
  DCHECK(data_device_manager_);
  DCHECK(data_device_);
}

WaylandClipboard::~WaylandClipboard() = default;

void WaylandClipboard::OfferClipboardData(
    ClipboardBuffer buffer,
    const PlatformClipboard::DataMap& data_map,
    PlatformClipboard::OfferDataClosure callback) {
  // TODO(https://crbug.com/921950): Implement primary selection.
  if (buffer != ClipboardBuffer::kCopyPaste) {
    std::move(callback).Run();
    return;
  }

  if (!clipboard_data_source_) {
    clipboard_data_source_ = data_device_manager_->CreateSource();
    clipboard_data_source_->WriteToClipboard(data_map);
  }
  clipboard_data_source_->UpdateDataMap(data_map);
  std::move(callback).Run();
}

void WaylandClipboard::RequestClipboardData(
    ClipboardBuffer buffer,
    const std::string& mime_type,
    PlatformClipboard::DataMap* data_map,
    PlatformClipboard::RequestDataClosure callback) {
  // TODO(https://crbug.com/921950): Implement primary selection.
  if (buffer != ClipboardBuffer::kCopyPaste) {
    std::move(callback).Run({});
    return;
  }

  read_clipboard_closure_ = std::move(callback);

  DCHECK(data_map);
  data_map_ = data_map;
  if (!data_device_->RequestSelectionData(mime_type))
    SetData({}, mime_type);
}

bool WaylandClipboard::IsSelectionOwner(ClipboardBuffer buffer) {
  // TODO(https://crbug.com/921950): Implement primary selection.
  if (buffer != ClipboardBuffer::kCopyPaste)
    return false;

  return !!clipboard_data_source_;
}

void WaylandClipboard::SetSequenceNumberUpdateCb(
    PlatformClipboard::SequenceNumberUpdateCb cb) {
  CHECK(update_sequence_cb_.is_null())
      << " The callback can be installed only once.";
  update_sequence_cb_ = std::move(cb);
}

void WaylandClipboard::GetAvailableMimeTypes(
    ClipboardBuffer buffer,
    PlatformClipboard::GetMimeTypesClosure callback) {
  // TODO(https://crbug.com/921950): Implement primary selection.
  if (buffer != ClipboardBuffer::kCopyPaste) {
    std::move(callback).Run({});
    return;
  }

  std::move(callback).Run(data_device_->GetAvailableMimeTypes());
}

void WaylandClipboard::DataSourceCancelled() {
  DCHECK(clipboard_data_source_);
  SetData({}, {});
  clipboard_data_source_.reset();
}

void WaylandClipboard::SetData(const std::string& contents,
                               const std::string& mime_type) {
  if (!data_map_)
    return;

  (*data_map_)[mime_type] =
      std::vector<uint8_t>(contents.begin(), contents.end());

  if (!read_clipboard_closure_.is_null()) {
    auto it = data_map_->find(mime_type);
    DCHECK(it != data_map_->end());
    std::move(read_clipboard_closure_).Run(it->second);
  }
  data_map_ = nullptr;
}

void WaylandClipboard::UpdateSequenceNumber(ClipboardBuffer buffer) {
  // TODO(https://crbug.com/921950): Implement primary selection.
  if (buffer != ClipboardBuffer::kCopyPaste)
    return;

  if (!update_sequence_cb_.is_null())
    update_sequence_cb_.Run(buffer);
}

}  // namespace ui
