// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/enterprise_clipboard_dlp_controller.h"
#include "ui/base/clipboard/clipboard.h"

namespace chromeos {

bool EnterpriseClipboardDlpController::IsDataReadAllowed(
    const ui::ClipboardDataEndpoint* const data_src,
    const ui::ClipboardDataEndpoint* const data_dst) const {
  // TODO(crbug.com/1102332): all the policy logic should be added later.
  return true;
}

}  // namespace chromeos
