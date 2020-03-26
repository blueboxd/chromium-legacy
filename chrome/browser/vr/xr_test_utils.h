// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_XR_TEST_UTILS_H_
#define CHROME_BROWSER_VR_XR_TEST_UTILS_H_

#include "base/callback_forward.h"
#include "chrome/browser/vr/vr_export.h"

namespace vr {

// Allows tests to perform extra initialization steps on any new XR Device
// Service instance before other client code can use it. Any time a new instance
// of the service is started by |GetXRDeviceService()|, this callback (if
// non-null) is invoked.
VR_EXPORT void SetXRDeviceServiceStartupCallbackForTesting(
    base::RepeatingClosure callback);

}  // namespace vr

#endif  // CHROME_BROWSER_VR_XR_TEST_UTILS_H_
