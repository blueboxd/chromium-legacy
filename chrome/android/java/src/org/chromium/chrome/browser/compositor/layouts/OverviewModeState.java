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
        OverviewModeState.SHOWN_TABSWITCHER, OverviewModeState.DISABLED})
@Retention(RetentionPolicy.SOURCE)
public @interface OverviewModeState {
    int NOT_SHOWN = 0;
    int SHOWN_HOMEPAGE = 1;
    int SHOWN_TABSWITCHER = 2;
    int SHOWN_TWO_PANES = 3;
    int SHOWN_TASKS_ONLY = 4;
    int DISABLED = 5;
}
