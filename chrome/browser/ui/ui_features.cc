// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ui_features.h"

namespace features {

// Enables tabs to change pinned state when dragging in the tabstrip.
// https://crbug.com/965681
const base::Feature kDragToPinTabs{"DragToPinTabs",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enables showing the EV certificate details in the Page Info bubble.
const base::Feature kEvDetailsInPageInfo{"EvDetailsInPageInfo",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Enables an extension menu in the toolbar. See https://crbug.com/943702
const base::Feature kExtensionsToolbarMenu{"ExtensionsToolbarMenu",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables tabs to scroll in the tabstrip. https://crbug.com/951078
const base::Feature kScrollableTabStrip{"ScrollableTabStrip",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables grouping tabs together in the tab strip. https://crbug.com/905491
const base::Feature kTabGroups{"TabGroups", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables popup cards containing tab information when hovering over a tab.
// https://crbug.com/910739
const base::Feature kTabHoverCards{"TabHoverCards",
                                   base::FEATURE_DISABLED_BY_DEFAULT};
// Parameter name used for tab hover cards user study.
// TODO(corising): Removed this after tab hover cards user study.
const char kTabHoverCardsFeatureParameterName[] = "setting";

// Enables preview images in hover cards. See kTabHoverCards.
// https://crbug.com/928954
const base::Feature kTabHoverCardImages{"TabHoverCardImages",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
