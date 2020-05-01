// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards.promo;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.homepage.HomepagePolicyManager;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

/**
 * Homepage related utils class.
 */
final class HomepagePromoUtils {
    // Do not instantiate.
    private HomepagePromoUtils() {}

    private static boolean sBypassHomepageEnabledForTests;
    private static boolean sBypassUrlIsNTPForTests;

    /**
     * @param tracker Tracker for feature engagement system. If the tracker is null, then ignore the
     *         feature engagement system.
     * @return True if HomepagePromo satisfied the condition to be created. False otherwise
     */
    static boolean shouldCreatePromo(@Nullable Tracker tracker) {
        boolean shouldCreateInternal = isHomepageEnabled()
                && !HomepagePolicyManager.isHomepageManagedByPolicy()
                && HomepageManager.getInstance().getPrefHomepageUseDefaultUri() && !isHomepageNTP()
                && !isPromoDismissedInSharedPreference();

        return shouldCreateInternal
                && (tracker == null
                        || tracker.shouldTriggerHelpUI(
                                FeatureConstants.HOMEPAGE_PROMO_CARD_FEATURE));
    }

    static boolean isPromoDismissedInSharedPreference() {
        return SharedPreferencesManager.getInstance().readBoolean(getDismissedKey(), false);
    }

    static void setPromoDismissedInSharedPreference(boolean isDismissed) {
        SharedPreferencesManager.getInstance().writeBoolean(getDismissedKey(), isDismissed);
    }

    static String getDismissedKey() {
        return ChromePreferenceKeys.PROMO_IS_DISMISSED.createKey(
                FeatureConstants.HOMEPAGE_PROMO_CARD_FEATURE);
    }

    // TODO(wenyufu): check finch param controlling whether the homepage needs to be enabled.
    private static boolean isHomepageEnabled() {
        return !sBypassHomepageEnabledForTests && HomepageManager.isHomepageEnabled();
    }

    private static boolean isHomepageNTP() {
        return !sBypassUrlIsNTPForTests && NewTabPage.isNTPUrl(HomepageManager.getHomepageUri());
    }

    @VisibleForTesting
    static void setBypassHomepageEnabledForTests(boolean doBypass) {
        sBypassHomepageEnabledForTests = doBypass;
    }

    @VisibleForTesting
    static void setBypassUrlIsNTPForTests(boolean doBypass) {
        sBypassUrlIsNTPForTests = doBypass;
    }
}
