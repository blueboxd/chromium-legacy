// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.actionparser;

import android.support.annotation.IntDef;

/** Possible action types. */
@IntDef({ActionSource.UNKNOWN, ActionSource.VIEW, ActionSource.CLICK, ActionSource.LONG_CLICK,
        ActionSource.SWIPE, ActionSource.CONTEXT_MENU})
public @interface ActionSource {
    int UNKNOWN = 0;
    /** View action */
    int VIEW = 1;

    /** Click action */
    int CLICK = 2;

    /** Long click action */
    int LONG_CLICK = 3;

    /* Swipe action */
    int SWIPE = 4;

    /* Action performed from context menu*/
    int CONTEXT_MENU = 5;
}
