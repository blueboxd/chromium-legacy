// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_CLIPBOARD_H_
#define UI_OZONE_PLATFORM_X11_X11_CLIPBOARD_H_

#include <string>

#include "base/callback.h"
#include "ui/ozone/public/platform_clipboard.h"

namespace ui {

// Handles clipboard operations for X11 Platform
class X11Clipboard : public PlatformClipboard {
 public:
  X11Clipboard();
  ~X11Clipboard() override;

  // PlatformClipboard.
  void OfferClipboardData(
      const PlatformClipboard::DataMap& data_map,
      PlatformClipboard::OfferDataClosure callback) override;
  void RequestClipboardData(
      const std::string& mime_type,
      PlatformClipboard::DataMap* data_map,
      PlatformClipboard::RequestDataClosure callback) override;
  void GetAvailableMimeTypes(
      PlatformClipboard::GetMimeTypesClosure callback) override;
  bool IsSelectionOwner() override;
  void SetSequenceNumberUpdateCb(
      PlatformClipboard::SequenceNumberUpdateCb cb) override;

 private:
  void UpdateSequenceNumber();

  // Notifies whenever clipboard sequence number is changed.
  PlatformClipboard::SequenceNumberUpdateCb update_sequence_cb_;

  DISALLOW_COPY_AND_ASSIGN(X11Clipboard);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_CLIPBOARD_H_
