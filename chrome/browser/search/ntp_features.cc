// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/ntp_features.h"

#include "build/build_config.h"
#include "ui/base/ui_base_features.h"

namespace features {

// If enabled, 'Chrome Colors' menu becomes visible in the customization picker.
const base::Feature kChromeColors{"ChromeColors",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, does not fade in most visited tiles on initial page load.
const base::Feature kDisableInitialMostVisitedFadeIn{
    "DisableInitialMostVisitedFadeIn", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the NTP shortcut layout will be replaced with a grid layout that
// enables better animations.
const base::Feature kGridLayoutForNtpShortcuts{
    "GridLayoutForNtpShortcuts", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the user will see the second version of the customization picker.
const base::Feature kNtpCustomizationMenuV2{"NtpCustomizationMenuV2",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the fakebox will not be shown on the NTP.
const base::Feature kRemoveNtpFakebox{"RemoveNtpFakebox",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, show a search icon (magnifier glass) in the NTP fakebox. Also
// implicitly enabled by |kFakeboxSearchIconColorOnNtp|.
const base::Feature kFakeboxSearchIconOnNtp{"FakeboxSearchIconOnNtp",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, show a blue search icon (magnifier glass) in the NTP fakebox.
const base::Feature kFakeboxSearchIconColorOnNtp{
    "FakeboxSearchIconColorOnNtp", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, show a shorter hint text in the NTP fakebox.
const base::Feature kFakeboxShortHintTextOnNtp{
    "FakeboxShortHintTextOnNtp", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, include a pre-populated Google search shortcut on the NTP during
// first run.
const base::Feature kFirstRunDefaultSearchShortcut{
    "FirstRunDefaultSearchShortcut", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the NTP fakebox will be changed to the Google search style. Also
// implicitly enabled by |kFakeboxSearchIconOnNtp|,
// |kFakeboxSearchIconColorOnNtp|, and |kUseAlternateFakeboxRectOnNtp|.
const base::Feature kUseAlternateFakeboxOnNtp{
    "UseAlternateFakeboxOnNtp", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the NTP fakebox will be changed to a rectangular Google search
// style with slightly rounded corners.
const base::Feature kUseAlternateFakeboxRectOnNtp{
    "UseAlternateFakeboxRectOnNtp", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the shortcuts will not be shown on the NTP.
const base::Feature kHideShortcutsOnNtp{"HideShortcutsOnNtp",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

bool IsFakeboxSearchIconOnNtpEnabled() {
  return base::FeatureList::IsEnabled(kFakeboxSearchIconOnNtp) ||
         base::FeatureList::IsEnabled(kFakeboxSearchIconColorOnNtp);
}

bool IsUseAlternateFakeboxOnNtpEnabled() {
  return base::FeatureList::IsEnabled(kFakeboxSearchIconOnNtp) ||
         base::FeatureList::IsEnabled(kFakeboxSearchIconColorOnNtp) ||
         base::FeatureList::IsEnabled(kUseAlternateFakeboxRectOnNtp) ||
         base::FeatureList::IsEnabled(kUseAlternateFakeboxOnNtp);
}

}  // namespace features
