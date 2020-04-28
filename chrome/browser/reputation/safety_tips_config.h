// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REPUTATION_SAFETY_TIPS_CONFIG_H_
#define CHROME_BROWSER_REPUTATION_SAFETY_TIPS_CONFIG_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/reputation/safety_tips.pb.h"
#include "components/security_state/core/security_state.h"

class GURL;

// Sets the global configuration for Safety Tips retrieved from the component
// updater. The configuration proto contains the list of URLs that can trigger
// a safety tip.
void SetSafetyTipsRemoteConfigProto(
    std::unique_ptr<chrome_browser_safety_tips::SafetyTipsConfig> proto);

// Gets the global configuration for Safety Tips as retrieved from the component
// updater. The configuration proto contains the list of URLs that can trigger
// a safety tip.
const chrome_browser_safety_tips::SafetyTipsConfig*
GetSafetyTipsRemoteConfigProto();

// Checks SafeBrowsing-style permutations of |url| against the component updater
// allowlist and returns whether the URL is explicitly allowed.
bool IsUrlAllowlistedBySafetyTipsComponent(
    const chrome_browser_safety_tips::SafetyTipsConfig* proto,
    const GURL& url);

// Checks the hostname of |url| against the component updater target allowlist
// and returns whether the URL is explicitly allowed.
bool IsTargetUrlAllowlistedBySafetyTipsComponent(
    const chrome_browser_safety_tips::SafetyTipsConfig* proto,
    const GURL& target_url);

// Checks SafeBrowsing-style permutations of |url| against the component updater
// blocklist and returns the match type. kNone means the URL is not blocked.
// This method assumes that the flagged pages in the safety tip config proto are
// in sorted order.
security_state::SafetyTipStatus GetSafetyTipUrlBlockType(const GURL& url);

#endif  // CHROME_BROWSER_REPUTATION_SAFETY_TIPS_CONFIG_H_
