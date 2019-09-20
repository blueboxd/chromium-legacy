// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/features.h"

#include "base/feature_list.h"

namespace content_settings {

// Enables an improved UI for third-party cookie blocking in incognito mode.
const base::Feature kImprovedCookieControls{"ImprovedCookieControls",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables an improved UI for existing third-party cookie blocking users.
const base::Feature kImprovedCookieControlsForThirdPartyCookieBlocking{
    "ImprovedCookieControlsForThirdPartyCookieBlocking",
    base::FEATURE_DISABLED_BY_DEFAULT};
}