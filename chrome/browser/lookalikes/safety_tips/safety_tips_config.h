// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_SAFETY_TIPS_CONFIG_H_
#define CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_SAFETY_TIPS_CONFIG_H_

#include <memory>

#include "chrome/browser/lookalikes/safety_tips/safety_tips.pb.h"

namespace safety_tips {

// Sets the global configuration for Safety Tips retrieved from the component
// updater. The configuration proto contains the list of URLs that can trigger
// a safety tip.
void SetProto(
    std::unique_ptr<chrome_browser_safety_tips::SafetyTipsConfig> proto);

}  // namespace safety_tips

#endif  // CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_SAFETY_TIPS_CONFIG_H_
