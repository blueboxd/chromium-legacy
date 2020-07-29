// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/features.h"

#include "build/build_config.h"

namespace sandbox {
namespace policy {
namespace features {

#if !defined(OS_MAC)
// Enables network service sandbox.
// (Only causes an effect when feature kNetworkService is enabled.)
const base::Feature kNetworkServiceSandbox{"NetworkServiceSandbox",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // !defined(OS_MAC)

#if defined(OS_WIN)
// Emergency "off switch" for new Windows sandbox security mitigation,
// sandbox::MITIGATION_EXTENSION_POINT_DISABLE.
const base::Feature kWinSboxDisableExtensionPoints{
    "WinSboxDisableExtensionPoint", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables GPU AppContainer sandbox on Windows.
const base::Feature kGpuAppContainer{"GpuAppContainer",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Enables GPU Low Privilege AppContainer when combined with kGpuAppContainer.
const base::Feature kGpuLPAC{"GpuLPAC", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_WIN)

#if !defined(OS_ANDROID)
// Controls whether the isolated XR service is sandboxed.
const base::Feature kXRSandbox{"XRSandbox", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // !defined(OS_ANDROID)

}  // namespace features
}  // namespace policy
}  // namespace sandbox
