// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/feature.h"

#if defined(OS_ANDROID)
const base::Feature kClickToCallReceiver{"ClickToCallReceiver",
                                         base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
const base::Feature kClickToCallUI{"ClickToCallUI",
                                   base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
