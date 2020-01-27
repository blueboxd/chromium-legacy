// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The internal state of the overview mode except show and hide events notified in the {@link
 * OverviewModeBehavior.OverviewModeObserver}.
 */
@IntDef({OverviewModeState.NOT_SHOWN, OverviewModeState.SHOWN_HOMEPAGE,
        OverviewModeState.SHOWN_TABSWITCHER, OverviewModeState.SHOWN_TABSWITCHER_TWO_PANES,
        OverviewModeState.SHOWN_TABSWITCHER_TASKS_ONLY,
        OverviewModeState.SHOWN_TABSWITCHER_OMNIBOX_ONLY, OverviewModeState.DISABLED,
        OverviewModeState.SHOWING_TABSWITCHER, OverviewModeState.SHOWING_START,
        OverviewModeState.SHOWING_HOMEPAGE, OverviewModeState.SHOWING_PREVIOUS})
@Retention(RetentionPolicy.SOURCE)
public @interface OverviewModeState {
    int NOT_SHOWN = 0;

    // When overview is visible, it will be in one of the SHOWN states.
    int SHOWN_HOMEPAGE = 1;
    int SHOWN_TABSWITCHER = 2;
    int SHOWN_TABSWITCHER_TWO_PANES = 3;
    int SHOWN_TABSWITCHER_TASKS_ONLY = 4;
    int SHOWN_TABSWITCHER_OMNIBOX_ONLY = 5;

    int DISABLED = 6;

    // SHOWING states are intermediary states that will immediately transition
    // to one of the SHOWN states when overview is/becomes visible.
    int SHOWING_TABSWITCHER = 7;
    int SHOWING_START = 8;
    int SHOWING_HOMEPAGE = 9;
    int SHOWING_PREVIOUS = 10;
}
