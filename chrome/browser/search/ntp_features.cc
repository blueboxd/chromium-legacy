// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/ntp_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "ui/base/ui_base_features.h"

namespace features {

// If enabled, 'Chrome Colors' menu becomes visible in the customization picker.
const base::Feature kChromeColors{"ChromeColors",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, custom color picker becomes visible in 'Chrome Colors' menu.
const base::Feature kChromeColorsCustomColorPicker{
    "ChromeColorsCustomColorPicker", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the NTP shortcut layout will be replaced with a grid layout that
// enables better animations.
const base::Feature kGridLayoutForNtpShortcuts{
    "GridLayoutForNtpShortcuts", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the user will see the second version of the customization picker.
const base::Feature kNtpCustomizationMenuV2{"NtpCustomizationMenuV2",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the search box in the middle of the NTP will accept input
// directly (i.e. not be a "fake" box) and search results will show directly
// below the non-fake input ("realbox").
const base::Feature kNtpRealbox{"NtpRealbox",
                                base::FEATURE_DISABLED_BY_DEFAULT};

bool IsNtpRealboxEnabled() {
  return base::FeatureList::IsEnabled(kNtpRealbox) ||
         base::FeatureList::IsEnabled(omnibox::kZeroSuggestionsOnNTPRealbox) ||
         (base::FeatureList::IsEnabled(omnibox::kOnFocusSuggestions) &&
          !OmniboxFieldTrial::GetZeroSuggestVariants(
               metrics::OmniboxEventProto::NTP_REALBOX)
               .empty());
}

// If enabled, "middle slot" promos on the bottom of the NTP will show a dismiss
// UI that allows users to close them and not see them again.
const base::Feature kDismissNtpPromos{"DismissNtpPromos",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
