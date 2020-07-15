// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_ENTERPRISE_CLIPBOARD_DLP_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_ENTERPRISE_CLIPBOARD_DLP_CONTROLLER_H_

#include "ui/base/clipboard/clipboard_data_endpoint.h"
#include "ui/base/clipboard/clipboard_dlp_controller.h"

namespace chromeos {

// EnterpriseClipboardDlpController is responsible for preventing leaks of
// confidential clipboard data by controlling read operations according to the
// policy rules set by the admin.
class EnterpriseClipboardDlpController : public ui::ClipboardDlpController {
 public:
  EnterpriseClipboardDlpController() = default;
  ~EnterpriseClipboardDlpController() override = default;

  EnterpriseClipboardDlpController(const EnterpriseClipboardDlpController&) =
      delete;
  void operator=(const EnterpriseClipboardDlpController&) = delete;

  // nullptr can be passed instead of |data_src| or |data_dst|.
  bool IsDataReadAllowed(
      const ui::ClipboardDataEndpoint* const data_src,
      const ui::ClipboardDataEndpoint* const data_dst) const override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_ENTERPRISE_CLIPBOARD_DLP_CONTROLLER_H_
