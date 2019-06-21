// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_NTP_FEATURES_H_
#define CHROME_BROWSER_SEARCH_NTP_FEATURES_H_

#include "base/feature_list.h"

namespace features {

// The features should be documented alongside the definition of their values in
// the .cc file.

extern const base::Feature kChromeColors;
extern const base::Feature kDisableInitialMostVisitedFadeIn;
extern const base::Feature kGridLayoutForNtpShortcuts;
extern const base::Feature kNtpCustomizationMenuV2;
extern const base::Feature kRemoveNtpFakebox;

extern const base::Feature kFakeboxSearchIconOnNtp;
extern const base::Feature kFakeboxSearchIconColorOnNtp;
extern const base::Feature kFakeboxShortHintTextOnNtp;
extern const base::Feature kFirstRunDefaultSearchShortcut;
extern const base::Feature kUseAlternateFakeboxOnNtp;
extern const base::Feature kUseAlternateFakeboxRectOnNtp;
extern const base::Feature kHideShortcutsOnNtp;

// Returns whether the fakebox with a search icon is enabled.
bool IsFakeboxSearchIconOnNtpEnabled();
// Returns whether the Google search style fakebox is enabled.
bool IsUseAlternateFakeboxOnNtpEnabled();

}  // namespace features

#endif  // CHROME_BROWSER_SEARCH_NTP_FEATURES_H_
