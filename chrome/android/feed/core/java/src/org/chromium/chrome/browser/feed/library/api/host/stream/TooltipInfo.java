// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.stream;

import androidx.annotation.StringDef;

/** All the information necessary to render a tooltip. */
public interface TooltipInfo {
    /** The list of features that the tooltip can highlight. */
    @StringDef({FeatureName.UNKNOWN, FeatureName.CARD_MENU_TOOLTIP})
    @interface FeatureName {
        String UNKNOWN = "unknown";
        String CARD_MENU_TOOLTIP = "card_menu_tooltip";
    }

    /** Returns the text to display in the tooltip. */
    String getLabel();

    /** Returns the talkback string to attach to tooltip. */
    String getAccessibilityLabel();

    /** Returns the Feature that the tooltip should reference. */
    @FeatureName
    String getFeatureName();

    /**
     * Returns the number of dp units that the tooltip arrow should point to below the center of the
     * top of the view it is referencing.
     */
    int getTopInset();

    /**
     * Returns the number of dp units that the tooltip arrow should point to above the center of the
     * bottom of the view it is referencing.
     */
    int getBottomInset();
}
