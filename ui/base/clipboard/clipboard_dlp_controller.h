// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_DLP_CONTROLLER_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_DLP_CONTROLLER_H_

#include "ui/base/clipboard/clipboard_data_endpoint.h"

namespace ui {

// The Clipboard Data Leak Prevention controller is used to control clipboard
// read operations. It should allow/disallow clipboard data read given the
// source of the data and the destination trying to access the data and a set of
// rules controlling these source and destination.
class ClipboardDlpController {
 public:
  ClipboardDlpController() = default;
  virtual ~ClipboardDlpController() = default;

  virtual bool IsDataReadAllowed(
      const ClipboardDataEndpoint* const data_src,
      const ClipboardDataEndpoint* const data_dst) const = 0;
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_DLP_CONTROLLER_H_
