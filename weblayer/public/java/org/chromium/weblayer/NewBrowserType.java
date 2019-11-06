// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({NewBrowserType.FOREGROUND_TAB, NewBrowserType.BACKGROUND_TAB, NewBrowserType.NEW_POPUP,
        NewBrowserType.NEW_WINDOW})
@Retention(RetentionPolicy.SOURCE)
public @interface NewBrowserType {
    /**
     * The page requested a new browser to be shown active.
     */
    int FOREGROUND_TAB = org.chromium.weblayer_private.aidl.NewBrowserType.FOREGROUND_TAB;

    /**
     * The page requested a new browser in the background. Generally, this is only encountered when
     * keyboard modifiers are used.
     */
    int BACKGROUND_TAB = org.chromium.weblayer_private.aidl.NewBrowserType.BACKGROUND_TAB;
    /**
     * The page requested the browser to open a new popup. A popup generally shows minimal ui
     * affordances, such as no tabstrip. On a phone, this is generally the same as
     * NEW_BROWSER_MODE_FOREGROUND_TAB.
     */
    int NEW_POPUP = org.chromium.weblayer_private.aidl.NewBrowserType.NEW_POPUP;

    /**
     * The page requested the browser to open in a new window. On a phone, this is generally the
     * same as NEW_BROWSER_MODE_FOREGROUND_TAB.
     */
    int NEW_WINDOW = org.chromium.weblayer_private.aidl.NewBrowserType.NEW_WINDOW;
}
