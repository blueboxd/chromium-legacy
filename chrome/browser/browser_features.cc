// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_features.h"

namespace features {

#if defined(OS_CHROMEOS)
// Enables being able to zoom a web page by double tapping in Chrome OS tablet
// mode.
const base::Feature kDoubleTapToZoomInTabletMode{
    "DoubleTapToZoomInTabletMode", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if !defined(OS_ANDROID)
// Enables Nearby Sharing functionality. Android already has a native
// implementation.
const base::Feature kNearbySharing{"NearbySharing",
                                   base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
// Enables taking snapshots of the user data directory after a major
// milestone update and restoring them after a version rollback.
const base::Feature kUserDataSnapshot{"UserDataSnapshot",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

}  // namespace features
