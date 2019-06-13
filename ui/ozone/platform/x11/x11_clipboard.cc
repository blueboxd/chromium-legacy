// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "ui/ozone/platform/x11/x11_clipboard.h"

namespace ui {

X11Clipboard::X11Clipboard() = default;
X11Clipboard::~X11Clipboard() = default;

void X11Clipboard::OfferClipboardData(
    const PlatformClipboard::DataMap& data_map,
    PlatformClipboard::OfferDataClosure callback) {
  // TODO(crbug.com/973295): Implement X11Clipboard
  NOTIMPLEMENTED_LOG_ONCE();
  std::move(callback).Run();
}

void X11Clipboard::RequestClipboardData(
    const std::string& mime_type,
    PlatformClipboard::DataMap* data_map,
    PlatformClipboard::RequestDataClosure callback) {
  // TODO(crbug.com/973295): Implement X11Clipboard
  NOTIMPLEMENTED_LOG_ONCE();
  std::move(callback).Run({});
}

void X11Clipboard::GetAvailableMimeTypes(
    PlatformClipboard::GetMimeTypesClosure callback) {
  // TODO(crbug.com/973295): Implement X11Clipboard
  NOTIMPLEMENTED_LOG_ONCE();
  std::move(callback).Run({});
}

bool X11Clipboard::IsSelectionOwner() {
  // TODO(crbug.com/973295): Implement X11Clipboard
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void X11Clipboard::SetSequenceNumberUpdateCb(
    PlatformClipboard::SequenceNumberUpdateCb cb) {
  DCHECK(update_sequence_cb_.is_null())
      << " The callback can be installed only once.";
  update_sequence_cb_ = std::move(cb);
}

void X11Clipboard::UpdateSequenceNumber() {
  if (!update_sequence_cb_.is_null())
    update_sequence_cb_.Run();
}

}  // namespace ui
