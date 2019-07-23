// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.support.customtabs.CustomTabsSessionToken;

import org.chromium.chrome.browser.ChromeApplication;

/**
 * Remnants of the old class that ensure downstream doesn't get broken. To be removed soon.
 */
@Deprecated
public class BrowserSessionContentUtils {

    /** See {@link SessionDataHolder#isActiveSession} */
    public static boolean isActiveSession(CustomTabsSessionToken session) {
        return getHolder().isActiveSession(session);
    }

    /** See {@link SessionDataHolder#getCurrentUrlForActiveBrowserSession} */
    public static String getCurrentUrlForActiveBrowserSession() {
        return getHolder().getCurrentUrlForActiveBrowserSession();
    }

    /** See {@link SessionDataHolder#getPendingUrlForActiveBrowserSession} */
    public static String getPendingUrlForActiveBrowserSession() {
        return getHolder().getPendingUrlForActiveBrowserSession();
    }

    private static SessionDataHolder getHolder() {
        return ChromeApplication.getComponent().resolveSessionDataHolder();
    }
}
