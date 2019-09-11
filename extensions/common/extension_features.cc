// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_features.h"

namespace extensions_features {

// Forces to handle event listeners as it specifies the "extraHeaders" option.
// TODO(crbug.com/1000982, 1000984): Run a field trial, and convert to a
// short-term enterprise policy.
const base::Feature kForceWebRequestExtraHeaders{
    "ForceWebRequestExtraHeaders", base::FEATURE_DISABLED_BY_DEFAULT};

// Forces requests to go through WebRequestProxyingURLLoaderFactory.
const base::Feature kForceWebRequestProxyForTest{
    "ForceWebRequestProxyForTest", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace extensions_features
