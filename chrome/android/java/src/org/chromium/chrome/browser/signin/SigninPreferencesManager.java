// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.support.annotation.Nullable;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

import java.util.Set;

/**
 * SigninPreferencesManager stores the state of the SignInPromo.
 */
public class SigninPreferencesManager {
    static final SigninPreferencesManager INSTANCE = new SigninPreferencesManager();

    private final SharedPreferencesManager mManager;

    private SigninPreferencesManager() {
        mManager = SharedPreferencesManager.getInstance();
    }

    /**
     * @return the SignInPromoStore singleton
     */
    public static SigninPreferencesManager getInstance() {
        return INSTANCE;
    }

    /**
     * Returns Chrome major version number when signin promo was last shown, or 0 if version number
     * isn't known.
     */
    int getSigninPromoLastShownVersion() {
        return mManager.readInt(ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION);
    }

    /**
     * Sets Chrome major version number when signin promo was last shown.
     */
    void setSigninPromoLastShownVersion(int majorVersion) {
        mManager.writeInt(ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION, majorVersion);
    }

    /**
     * Returns a set of account names on the device when signin promo was last shown,
     * or null if promo hasn't been shown yet.
     */
    @Nullable
    Set<String> getSigninPromoLastAccountNames() {
        return mManager.readStringSet(
                ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES, null);
    }

    /**
     * Stores a set of account names on the device when signin promo is shown.
     */
    void setSigninPromoLastAccountNames(Set<String> accountNames) {
        mManager.writeStringSet(
                ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES, accountNames);
    }

    /**
     * Returns timestamp of the suppression period start if signin promos in the New Tab Page are
     * temporarily suppressed; zero otherwise.
     * @return the epoch time in milliseconds (see {@link System#currentTimeMillis()}).
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public long getNewTabPageSigninPromoSuppressionPeriodStart() {
        return mManager.readLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_SUPPRESSION_PERIOD_START);
    }

    /**
     * Sets timestamp of the suppression period start if signin promos in the New Tab Page are
     * temporarily suppressed.
     * @param timeMillis the epoch time in milliseconds (see {@link System#currentTimeMillis()}).
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void setNewTabPageSigninPromoSuppressionPeriodStart(long timeMillis) {
        mManager.writeLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_SUPPRESSION_PERIOD_START, timeMillis);
    }

    /**
     * Removes the stored timestamp of the suppression period start when signin promos in the New
     * Tab Page are no longer suppressed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void clearNewTabPageSigninPromoSuppressionPeriodStart() {
        mManager.removeKey(ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_SUPPRESSION_PERIOD_START);
    }
}
