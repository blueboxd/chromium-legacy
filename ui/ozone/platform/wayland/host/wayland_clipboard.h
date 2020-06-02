// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CLIPBOARD_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CLIPBOARD_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "ui/ozone/public/platform_clipboard.h"

namespace ui {

class GtkPrimarySelectionSource;
class WaylandConnection;
class WaylandDataDeviceBase;
class WaylandDataDeviceManager;
class WaylandDataSourceBase;
class WaylandDataSource;

// Handles clipboard operations.
//
// WaylandConnection's wl_data_device_manager wrapper object is required to be
// non-null for objects of this class so it can provide basic functionality.
class WaylandClipboard : public PlatformClipboard {
 public:
  WaylandClipboard(WaylandConnection* connection,
                   WaylandDataDeviceManager* data_device_manager);
  ~WaylandClipboard() override;

  // PlatformClipboard.
  void OfferClipboardData(
      ClipboardBuffer buffer,
      const PlatformClipboard::DataMap& data_map,
      PlatformClipboard::OfferDataClosure callback) override;
  void RequestClipboardData(
      ClipboardBuffer buffer,
      const std::string& mime_type,
      PlatformClipboard::DataMap* data_map,
      PlatformClipboard::RequestDataClosure callback) override;
  void GetAvailableMimeTypes(
      ClipboardBuffer buffer,
      PlatformClipboard::GetMimeTypesClosure callback) override;
  bool IsSelectionOwner(ClipboardBuffer buffer) override;
  void SetSequenceNumberUpdateCb(
      PlatformClipboard::SequenceNumberUpdateCb cb) override;

  void DataSourceCancelled(ClipboardBuffer buffer);
  void SetData(const std::vector<uint8_t>& contents,
               const std::string& mime_type);
  void UpdateSequenceNumber(ClipboardBuffer buffer);

 private:
  bool IsPrimarySelectionSupported() const;
  WaylandDataDeviceBase* GetDataDevice(ClipboardBuffer buffer) const;
  WaylandDataSourceBase* GetDataSource(ClipboardBuffer buffer);

  // WaylandConnection providing optional data device managers, e.g: gtk
  // primary selection.
  WaylandConnection* const connection_;

  // Owned by WaylandConnection and required to be non-null so that
  // WaylandConnection can be of some usefulness.
  WaylandDataDeviceManager* const data_device_manager_;

  // Holds a temporary instance of the client's clipboard content
  // so that we can asynchronously write to it.
  PlatformClipboard::DataMap* data_map_ = nullptr;

  // Notifies whenever clipboard sequence number is changed. Can be empty if
  // not set.
  PlatformClipboard::SequenceNumberUpdateCb update_sequence_cb_;

  // Stores the callback to be invoked upon data reading from clipboard.
  PlatformClipboard::RequestDataClosure read_clipboard_closure_;

  std::unique_ptr<WaylandDataSource> clipboard_data_source_;
  std::unique_ptr<GtkPrimarySelectionSource> primary_data_source_;

  DISALLOW_COPY_AND_ASSIGN(WaylandClipboard);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CLIPBOARD_H_
