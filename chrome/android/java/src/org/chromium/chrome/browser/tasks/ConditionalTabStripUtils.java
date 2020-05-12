// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.IntCachedFieldTrialParameter;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Helper class to handle conditional tab strip related utilities.
 */
public class ConditionalTabStripUtils {
    private static final String FEATURE_STATUS =
            ChromePreferenceKeys.CONDITIONAL_TAB_STRIP_FEATURE_STATUS;
    public static final String CONDITIONAL_TAB_STRIP_SESSION_TIME_MS_PARAM =
            "conditional_tab_strip_session_time_ms";
    @VisibleForTesting
    public static final IntCachedFieldTrialParameter CONDITIONAL_TAB_STRIP_SESSION_TIME_MS =
            new IntCachedFieldTrialParameter(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID,
                    CONDITIONAL_TAB_STRIP_SESSION_TIME_MS_PARAM, 3600000);
    /**
     * A series of possible states of the conditional tab strip. FeatureStatus.DEFAULT is the
     * initial state for the feature when the previous session has expired. The strip will not show
     * in this state until being activated. FeatureStatus.ACTIVATED is the status when conditional
     * tab strip is activated and strip will show in this state. FeatureStatus.FORBIDDEN is the
     * state when user explicitly dismisses the strip, and the strip will never reshow in this
     * state until returning to FeatureStatus.Default after session expiration.
     */
    @IntDef({FeatureStatus.FORBIDDEN, FeatureStatus.ACTIVATED, FeatureStatus.DEFAULT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface FeatureStatus {
        int FORBIDDEN = 0;
        int ACTIVATED = 1;
        int DEFAULT = 2;
    }

    /**
     * Update whether the current session for conditional tab strip is expired based on the last
     * time when Chrome is in background and save the status in SharedPreference for future
     * reference.
     *
     * @param lastBackgroundedTimeMillis The last time the application was backgrounded. Set in
     *                                   ChromeTabbedActivity::onStartWithNative
     */
    public static void updateFeatureExpiration(final long lastBackgroundedTimeMillis) {
        long expirationTime =
                lastBackgroundedTimeMillis + CONDITIONAL_TAB_STRIP_SESSION_TIME_MS.getValue();
        if (lastBackgroundedTimeMillis == -1 || System.currentTimeMillis() > expirationTime) {
            setFeatureStatus(FeatureStatus.DEFAULT);
        }
    }

    /**
     * Update the status of the conditional tab strip feature in SharedPreference.
     *
     * @param featureStatus the target {@link FeatureStatus} to set.
     */
    public static void setFeatureStatus(@FeatureStatus int featureStatus) {
        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();
        sharedPreferencesManager.writeInt(FEATURE_STATUS, featureStatus);
    }

    /**
     * Get the status of the conditional tab strip feature from SharedPreference.
     *
     * @return {@link FeatureStatus} that indicates the saved status of conditional tab strip
     *         feature.
     */
    public static @FeatureStatus int getFeatureStatus() {
        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();
        return sharedPreferencesManager.readInt(FEATURE_STATUS, FeatureStatus.DEFAULT);
    }

    @VisibleForTesting
    public static void setFeatureStatusForTesting(@FeatureStatus int featureStatus) {
        setFeatureStatus(featureStatus);
    }
}
