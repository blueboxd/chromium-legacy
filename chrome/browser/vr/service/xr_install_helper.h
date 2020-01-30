// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_XR_INSTALL_HELPER_H_
#define CHROME_BROWSER_VR_SERVICE_XR_INSTALL_HELPER_H_

#include "base/callback_forward.h"

namespace vr {

typedef base::OnceCallback<void(bool)> OnInstallFinishedCallback;

class XrInstallHelper {
 public:
  virtual ~XrInstallHelper() = default;

  virtual void EnsureInstalled(int render_process_id,
                               int render_frame_id,
                               OnInstallFinishedCallback install_callback) = 0;

 protected:
  XrInstallHelper() = default;

 private:
  XrInstallHelper(const XrInstallHelper&) = delete;
  XrInstallHelper& operator=(const XrInstallHelper&) = delete;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_XR_INSTALL_HELPER_H_
