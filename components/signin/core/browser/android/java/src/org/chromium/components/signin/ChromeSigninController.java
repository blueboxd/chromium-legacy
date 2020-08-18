// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import org.chromium.base.ContextUtils;

/**
 * Caches the signed-in username in the app prefs.
 */
@Deprecated
public class ChromeSigninController {
    public static final String TAG = "ChromeSigninController";

    private static final String SIGNED_IN_ACCOUNT_KEY = "google.services.username";

    private static final Object LOCK = new Object();

    private static ChromeSigninController sChromeSigninController;

    private ChromeSigninController() {}

    /**
     * A factory method for the ChromeSigninController.
     *
     * @return a singleton instance of the ChromeSigninController
     */
    public static ChromeSigninController get() {
        synchronized (LOCK) {
            if (sChromeSigninController == null) {
                sChromeSigninController = new ChromeSigninController();
            }
        }
        return sChromeSigninController;
    }

    // TODO(https://crbug.com/1046412): Remove after migrating downstream usages to
    //                                  SigninPreferencesManager.
    public String getSignedInAccountName() {
        return ContextUtils.getAppSharedPreferences().getString(SIGNED_IN_ACCOUNT_KEY, null);
    }
}
