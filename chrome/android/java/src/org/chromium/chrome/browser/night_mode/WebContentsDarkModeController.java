// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import static org.chromium.chrome.browser.preferences.ChromePreferenceManager.DARKEN_WEBSITES_ENABLED_KEY;

import android.text.TextUtils;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ApplicationStateListener;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;

/**
 * A controller class could enable or disable web content dark mode feature based on the night mode
 * and the user preference
 */
public class WebContentsDarkModeController implements ApplicationStateListener {
    private NightModeStateProvider.Observer mNightModeObserver;
    private ChromePreferenceManager.Observer mPreferenceObserver;

    private static WebContentsDarkModeController sController;

    private WebContentsDarkModeController() {
        enableWebContentsDarkMode(shouldEnableWebContentsDarkMode());
        final int applicationState = ApplicationStatus.getStateForApplication();
        if (applicationState == ApplicationState.HAS_RUNNING_ACTIVITIES
                || applicationState == ApplicationState.HAS_PAUSED_ACTIVITIES) {
            start();
        }
        ApplicationStatus.registerApplicationStateListener(this);
    }

    /**
     * @return The instance can enable or disable the feature. Call the start method to listen
     * the user setting and app night mode change so that the instance can automatically apply the
     * change. Call the stop method to stop the listening.
     */
    public static WebContentsDarkModeController createInstance() {
        if (sController == null) {
            sController = new WebContentsDarkModeController();
        }
        return sController;
    }

    /**
     * Enable or Disable web content dark mode
     * @param enabled the new state of the web content dark mode
     */
    private static void enableWebContentsDarkMode(boolean enabled) {
        PrefServiceBridge.getInstance().setForceWebContentsDarkModeEnabled(enabled);
    }

    private static boolean shouldEnableWebContentsDarkMode() {
        return GlobalNightModeStateProviderHolder.getInstance().isInNightMode()
                && ChromePreferenceManager.getInstance().readBoolean(
                        DARKEN_WEBSITES_ENABLED_KEY, false);
    }

    /**
     * start listening to any event can enable or disable web content dark mode
     */
    private void start() {
        if (mNightModeObserver != null) return;
        mNightModeObserver = () -> enableWebContentsDarkMode(shouldEnableWebContentsDarkMode());
        mPreferenceObserver = (key) -> {
            if (TextUtils.equals(key, DARKEN_WEBSITES_ENABLED_KEY)) {
                enableWebContentsDarkMode(shouldEnableWebContentsDarkMode());
            }
        };
        enableWebContentsDarkMode(shouldEnableWebContentsDarkMode());
        GlobalNightModeStateProviderHolder.getInstance().addObserver(mNightModeObserver);
        ChromePreferenceManager.getInstance().addObserver(mPreferenceObserver);
    }

    /**
     * stop listening to any event can enable or disable web content dark mode
     */
    private void stop() {
        if (mNightModeObserver == null) return;
        GlobalNightModeStateProviderHolder.getInstance().removeObserver(mNightModeObserver);
        ChromePreferenceManager.getInstance().removeObserver(mPreferenceObserver);
        mNightModeObserver = null;
        mPreferenceObserver = null;
    }

    @Override
    public void onApplicationStateChange(int newState) {
        if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES) {
            start();
        } else if (newState == ApplicationState.HAS_STOPPED_ACTIVITIES) {
            stop();
        }
    }
}
