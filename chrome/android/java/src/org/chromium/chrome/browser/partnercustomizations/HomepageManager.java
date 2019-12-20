// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import android.text.TextUtils;

import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.util.UrlConstants;

/**
 * Provides information regarding homepage enabled states and URI.
 *
 * This class serves as a single homepage logic gateway.
 */
public class HomepageManager {

    /**
     * An interface to use for getting homepage related updates.
     */
    public interface HomepageStateListener {
        /**
         * Called when the homepage is enabled or disabled or the homepage URL changes.
         */
        void onHomepageStateUpdated();
    }

    private static HomepageManager sInstance;

    private final SharedPreferencesManager mSharedPreferencesManager;
    private final ObserverList<HomepageStateListener> mHomepageStateListeners;

    private HomepageManager() {
        mSharedPreferencesManager = SharedPreferencesManager.getInstance();
        mHomepageStateListeners = new ObserverList<>();
    }

    /**
     * Returns the singleton instance of HomepageManager, creating it if needed.
     */
    public static HomepageManager getInstance() {
        if (sInstance == null) {
            sInstance = new HomepageManager();
        }
        return sInstance;
    }

    /**
     * Adds a HomepageStateListener to receive updates when the homepage state changes.
     */
    public void addListener(HomepageStateListener listener) {
        mHomepageStateListeners.addObserver(listener);
    }

    /**
     * Removes the given listener from the state listener list.
     * @param listener The listener to remove.
     */
    public void removeListener(HomepageStateListener listener) {
        mHomepageStateListeners.removeObserver(listener);
    }

    /**
     * Notify any listeners about a homepage state change.
     */
    public void notifyHomepageUpdated() {
        for (HomepageStateListener listener : mHomepageStateListeners) {
            listener.onHomepageStateUpdated();
        }
    }

    /**
     * @return Whether or not homepage is enabled.
     */
    public static boolean isHomepageEnabled() {
        return getInstance().getPrefHomepageEnabled();
    }

    /**
     * @return Whether to close the app when the user has zero tabs.
     */
    public static boolean shouldCloseAppWithZeroTabs() {
        return HomepageManager.isHomepageEnabled()
                && !NewTabPage.isNTPUrl(HomepageManager.getHomepageUri());
    }

    /**
     * @return Homepage URI string, if it's enabled. Null otherwise or uninitialized.
     */
    public static String getHomepageUri() {
        if (!isHomepageEnabled()) return null;

        HomepageManager manager = getInstance();
        String homepageUri = manager.getPrefHomepageUseDefaultUri()
                ? getDefaultHomepageUri()
                : manager.getPrefHomepageCustomUri();
        return TextUtils.isEmpty(homepageUri) ? null : homepageUri;
    }

    /**
     * @return The default homepage URI if the homepage is partner provided or the new tab page
     *         if the homepage button is force enabled via flag.
     */
    public static String getDefaultHomepageUri() {
        return PartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled()
                ? PartnerBrowserCustomizations.getHomePageUrl()
                : UrlConstants.NTP_NON_NATIVE_URL;
    }

    /**
     * Returns the user preference for whether the homepage is enabled. This doesn't take into
     * account whether the device supports having a homepage.
     *
     * @see #isHomepageEnabled
     */
    public boolean getPrefHomepageEnabled() {
        return mSharedPreferencesManager.readBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
    }

    /**
     * Sets the user preference for whether the homepage is enabled.
     */
    public void setPrefHomepageEnabled(boolean enabled) {
        mSharedPreferencesManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, enabled);
        RecordHistogram.recordBooleanHistogram(
                "Settings.ShowHomeButtonPreferenceStateChanged", enabled);
        RecordHistogram.recordBooleanHistogram("Settings.ShowHomeButtonPreferenceState", enabled);
        notifyHomepageUpdated();
    }

    /**
     * @return User specified homepage custom URI string.
     */
    public String getPrefHomepageCustomUri() {
        return mSharedPreferencesManager.readString(ChromePreferenceKeys.HOMEPAGE_CUSTOM_URI, "");
    }

    /**
     * Sets custom homepage URI
     */
    public void setPrefHomepageCustomUri(String customUri) {
        mSharedPreferencesManager.writeString(ChromePreferenceKeys.HOMEPAGE_CUSTOM_URI, customUri);
    }

    /**
     * @return Whether the homepage URL is the default value.
     */
    public boolean getPrefHomepageUseDefaultUri() {
        return mSharedPreferencesManager.readBoolean(
                ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, true);
    }

    /**
     * Sets whether the homepage URL is the default value.
     */
    public void setPrefHomepageUseDefaultUri(boolean useDefaultUri) {
        RecordHistogram.recordBooleanHistogram("Settings.HomePageIsCustomized", !useDefaultUri);
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, useDefaultUri);
    }
}
