// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ui_features.h"

namespace features {

// Enables showing the EV certificate details in the Page Info bubble.
const base::Feature kEvDetailsInPageInfo{"EvDetailsInPageInfo",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Enables an extension menu in the toolbar. See https://crbug.com/943702
const base::Feature kExtensionsToolbarMenu{"ExtensionsToolbarMenu",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables tabs from different browser types (NORMAL vs APP) and different apps
// to mix via dragging.
// https://crbug.com/1012169
const base::Feature kMixBrowserTypeTabs{"MixBrowserTypeTabs",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the new profile picker.
// https:://crbug.com/1063856
const base::Feature kNewProfilePicker{"NewProfilePicker",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enables updated tabstrip animations, required for a scrollable tabstrip.
// https://crbug.com/958173
const base::Feature kNewTabstripAnimation{"NewTabstripAnimation",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables an experimental permission prompt that uses a chip in the location
// bar.
const base::Feature kPermissionChip{"PermissionChip",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a more prominent active tab title in dark mode to aid with
// accessibility.
const base::Feature kProminentDarkModeActiveTabTitle{
    "ProminentDarkModeActiveTabTitle", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables tabs to scroll in the tabstrip. https://crbug.com/951078
const base::Feature kScrollableTabStrip{"ScrollableTabStrip",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables grouping tabs together in the tab strip. https://crbug.com/905491
const base::Feature kTabGroups{"TabGroups", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables tab groups to be collapsed and expanded. https://crbug.com/1018230
const base::Feature kTabGroupsCollapse{"TabGroupsCollapse",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the feedback through the tab group editor bubble.
// https://crbug.com/1067062
const base::Feature kTabGroupsFeedback{"TabGroupsFeedback",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// Enables popup cards containing tab information when hovering over a tab.
// https://crbug.com/910739
const base::Feature kTabHoverCards {
  "TabHoverCards",
#if defined(OS_MACOSX)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif  // defined(OS_MACOSX)
};

// Parameter name used for tab hover cards user study.
// TODO(corising): Removed this after tab hover cards user study.
const char kTabHoverCardsFeatureParameterName[] = "setting";

// Enables preview images in hover cards. See kTabHoverCards.
// https://crbug.com/928954
const base::Feature kTabHoverCardImages{"TabHoverCardImages",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables tab outlines in additional situations for accessibility.
const base::Feature kTabOutlinesInLowContrastThemes{
    "TabOutlinesInLowContrastThemes", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables showing text next to the 3-dot menu when an update is available.
// See https://crbug.com/1001731
const base::Feature kUseTextForUpdateButton{"UseTextForUpdateButton",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a web-based separator that's only used for performance testing. See
// https://crbug.com/993502.
const base::Feature kWebFooterExperiment{"WebFooterExperiment",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a web-based tab strip. See https://crbug.com/989131. Note this
// feature only works when the ENABLE_WEBUI_TAB_STRIP buildflag is enabled.
const base::Feature kWebUITabStrip{"WebUITabStrip",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the demo options for the WebUI Tab Strip. This flag will only work
// if kWebUITabStrip is enabled.
const base::Feature kWebUITabStripDemoOptions{
    "WebUITabStripDemoOptions", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables friendly settings for the |chrome://settings/syncSetup| page.
// https://crbug.com/1035421.
const base::Feature kSyncSetupFriendlySettings{
    "SyncSetupFriendlySettings", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_CHROMEOS)
// Enables a warning about connecting to hidden WiFi networks.
// https://crbug.com/903908
const base::Feature kHiddenNetworkWarning{"HiddenNetworkWarning",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_CHROMEOS)
}  // namespace features
