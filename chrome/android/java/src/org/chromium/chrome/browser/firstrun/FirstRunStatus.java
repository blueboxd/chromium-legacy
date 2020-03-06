// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import org.chromium.base.CommandLine;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * Gets and sets preferences related to the status of the first run experience.
 */
public class FirstRunStatus {
    // Whether the first run flow is triggered in the current browser session.
    private static boolean sFirstRunTriggered;

    /**
     * Sets the "main First Run Experience flow complete" preference.
     * @param isComplete Whether the main First Run Experience flow is complete
     */
    public static void setFirstRunFlowComplete(boolean isComplete) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE, isComplete);
    }

    /**
     * Returns whether the main First Run Experience flow is complete.
     * Note: that might NOT include "intro"/"what's new" pages, but it always
     * includes ToS and Sign In pages if necessary.
     */
    public static boolean getFirstRunFlowComplete() {
        if (SharedPreferencesManager.getInstance().readBoolean(
                    ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE, false)) {
            return true;
        }
        return CommandLine.getInstance().hasSwitch(
                ChromeSwitches.FORCE_FIRST_RUN_FLOW_COMPLETE_FOR_TESTING);
    }

    /**
     * Sets whether the first run flow is triggered in the current browser session.
     * @param triggered
     * @return
     */
    public static void setFirstRunTriggered(boolean triggered) {
        sFirstRunTriggered = triggered;
    }

    /**
     * Returns whether first run flow is triggered in the current browser session.
     */
    public static boolean isFirstRunTriggered() {
        return sFirstRunTriggered;
    }

    /**
    * Sets the preference to skip the welcome page from the main First Run Experience.
     * @param isSkip Whether the welcome page should be skpped
    */
    public static void setSkipWelcomePage(boolean isSkip) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FIRST_RUN_SKIP_WELCOME_PAGE, isSkip);
    }

    /**
    * Checks whether the welcome page should be skipped from the main First Run Experience.
    */
    public static boolean shouldSkipWelcomePage() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.FIRST_RUN_SKIP_WELCOME_PAGE, false);
    }

    /**
     * Sets the "lightweight First Run Experience flow complete" preference.
     * @param isComplete Whether the lightweight First Run Experience flow is complete
     */
    public static void setLightweightFirstRunFlowComplete(boolean isComplete) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FIRST_RUN_LIGHTWEIGHT_FLOW_COMPLETE, isComplete);
    }

    /**
     * Returns whether the "lightweight First Run Experience flow" is complete.
     */
    public static boolean getLightweightFirstRunFlowComplete() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.FIRST_RUN_LIGHTWEIGHT_FLOW_COMPLETE, false);
    }
}
