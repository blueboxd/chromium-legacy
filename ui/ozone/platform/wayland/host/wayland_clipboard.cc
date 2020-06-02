// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_clipboard.h"

#include <string>

#include "base/notreached.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/gtk_primary_selection_device.h"
#include "ui/ozone/platform/wayland/host/gtk_primary_selection_device_manager.h"
#include "ui/ozone/platform/wayland/host/gtk_primary_selection_source.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device_base.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source_base.h"

namespace ui {

WaylandClipboard::WaylandClipboard(
    WaylandConnection* connection,
    WaylandDataDeviceManager* data_device_manager)
    : connection_(connection), data_device_manager_(data_device_manager) {
  DCHECK(connection_);
  DCHECK(data_device_manager_);
}

WaylandClipboard::~WaylandClipboard() = default;

void WaylandClipboard::OfferClipboardData(
    ClipboardBuffer buffer,
    const PlatformClipboard::DataMap& data_map,
    PlatformClipboard::OfferDataClosure callback) {
  if (auto* data_source = GetDataSource(buffer)) {
    data_source->WriteToClipboard(data_map);
    data_source->set_data_map(data_map);
  }
  std::move(callback).Run();
}

void WaylandClipboard::RequestClipboardData(
    ClipboardBuffer buffer,
    const std::string& mime_type,
    PlatformClipboard::DataMap* data_map,
    PlatformClipboard::RequestDataClosure callback) {
  read_clipboard_closure_ = std::move(callback);
  DCHECK(data_map);
  data_map_ = data_map;
  auto* device = GetDataDevice(buffer);
  if (!device || !device->RequestSelectionData(mime_type))
    SetData({}, mime_type);
}

bool WaylandClipboard::IsSelectionOwner(ClipboardBuffer buffer) {
  if (buffer == ClipboardBuffer::kCopyPaste)
    return !!clipboard_data_source_;
  else
    return !!primary_data_source_;
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
  auto* device = GetDataDevice(buffer);
  std::move(callback).Run(!device ? device->GetAvailableMimeTypes()
                                  : std::vector<std::string>{});
}

void WaylandClipboard::DataSourceCancelled(ClipboardBuffer buffer) {
  if (buffer == ClipboardBuffer::kCopyPaste) {
    DCHECK(clipboard_data_source_);
    SetData({}, {});
    clipboard_data_source_.reset();
  } else {
    DCHECK(primary_data_source_);
    SetData({}, {});
    primary_data_source_.reset();
  }
}

void WaylandClipboard::SetData(const std::vector<uint8_t>& contents,
                               const std::string& mime_type) {
  if (!data_map_)
    return;

  (*data_map_)[mime_type] = contents;

  if (!read_clipboard_closure_.is_null()) {
    auto it = data_map_->find(mime_type);
    DCHECK(it != data_map_->end());
    std::move(read_clipboard_closure_).Run(it->second);
  }
  data_map_ = nullptr;
}

void WaylandClipboard::UpdateSequenceNumber(ClipboardBuffer buffer) {
  if (!update_sequence_cb_.is_null())
    update_sequence_cb_.Run(buffer);
}

bool WaylandClipboard::IsPrimarySelectionSupported() const {
  return !!GetDataDevice(ClipboardBuffer::kSelection);
}

WaylandDataDeviceBase* WaylandClipboard::GetDataDevice(
    ClipboardBuffer buffer) const {
  switch (buffer) {
    case ClipboardBuffer::kCopyPaste: {
      return connection_->data_device_manager()->GetDevice();
    }
    case ClipboardBuffer::kSelection: {
      return connection_->primary_selection_device_manager()
                 ? connection_->primary_selection_device_manager()->GetDevice()
                 : nullptr;
    }
    default: {
      NOTREACHED();
      return nullptr;
    }
  }
}

WaylandDataSourceBase* WaylandClipboard::GetDataSource(ClipboardBuffer buffer) {
  switch (buffer) {
    case ClipboardBuffer::kCopyPaste: {
      if (!clipboard_data_source_) {
        clipboard_data_source_ =
            connection_->data_device_manager()->CreateSource();
      }
      return clipboard_data_source_.get();
    }
    case ClipboardBuffer::kSelection: {
      if (!IsPrimarySelectionSupported())
        return nullptr;
      if (!primary_data_source_) {
        primary_data_source_ =
            connection_->primary_selection_device_manager()->CreateSource();
      }
      return primary_data_source_.get();
    }
    default: {
      NOTREACHED();
      return nullptr;
    }
  }
}

}  // namespace ui
