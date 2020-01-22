// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import android.text.TextUtils;

import androidx.annotation.IntDef;

import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.homepage.HomepagePolicyManager;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.util.UrlConstants;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Provides information regarding homepage enabled states and URI.
 *
 * This class serves as a single homepage logic gateway.
 */
public class HomepageManager implements HomepagePolicyManager.HomepagePolicyStateListener {
    /**
     * Possible states for HomeButton. Used for Histogram
     * Settings.ShowHomeButtonPreferenceStateManaged. Currently {@link
     * HomeButtonPreferenceState.MANAGED_DISABLED } is not used.
     *
     * These values are persisted to logs, and should therefore never be renumbered nor reused.
     */
    @IntDef({HomeButtonPreferenceState.USER_DISABLED, HomeButtonPreferenceState.USER_ENABLED,
            HomeButtonPreferenceState.MANAGED_DISABLED, HomeButtonPreferenceState.MANAGED_ENABLED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface HomeButtonPreferenceState {
        int USER_DISABLED = 0;
        int USER_ENABLED = 1;
        int MANAGED_DISABLED = 2;
        int MANAGED_ENABLED = 3;

        int NUM_ENTRIES = 4;
    }

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
        HomepagePolicyManager.getInstance().addListener(this);
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
        return HomepagePolicyManager.isHomepageManagedByPolicy()
                || getInstance().getPrefHomepageEnabled();
    }

    /**
     * @return Whether or not current homepage is customized.
     */
    public static boolean isHomepageCustomized() {
        return !HomepagePolicyManager.isHomepageManagedByPolicy()
                && !getInstance().getPrefHomepageUseDefaultUri();
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
        String homepageUri;
        if (HomepagePolicyManager.isHomepageManagedByPolicy()) {
            homepageUri = HomepagePolicyManager.getHomepageUrl();
        } else if (manager.getPrefHomepageUseDefaultUri()) {
            homepageUri = getDefaultHomepageUri();
        } else {
            homepageUri = manager.getPrefHomepageCustomUri();
        }
        return TextUtils.isEmpty(homepageUri) ? null : homepageUri;
    }

    /**
     * @return The default homepage URI if the homepage is partner provided or the new tab page
     *         if the homepage button is force enabled via flag.
     */
    private static String getDefaultHomepageUri() {
        if (PartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled()) {
            return PartnerBrowserCustomizations.getHomePageUrl();
        }
        return UrlConstants.NTP_NON_NATIVE_URL;
    }

    /**
     * Returns the user preference for whether the homepage is enabled. This doesn't take into
     * account whether the device supports having a homepage.
     *
     * @see #isHomepageEnabled
     */
    private boolean getPrefHomepageEnabled() {
        return mSharedPreferencesManager.readBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
    }

    /**
     * Sets the user preference for whether the homepage is enabled.
     */
    public void setPrefHomepageEnabled(boolean enabled) {
        mSharedPreferencesManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, enabled);
        RecordHistogram.recordBooleanHistogram(
                "Settings.ShowHomeButtonPreferenceStateChanged", enabled);
        recordHomeButtonPreferenceState();
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
        assert !HomepagePolicyManager.isHomepageManagedByPolicy();

        recordHomepageIsCustomized(!useDefaultUri);
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, useDefaultUri);
    }

    /**
     * Get the homepage button preference state.
     */
    public static void recordHomeButtonPreferenceState() {
        if (!FeatureUtilities.isHomepageLocationPolicyEnabled()) {
            RecordHistogram.recordBooleanHistogram(
                    "Settings.ShowHomeButtonPreferenceState", HomepageManager.isHomepageEnabled());
            return;
        }

        int state = HomeButtonPreferenceState.USER_DISABLED;
        if (HomepagePolicyManager.isHomepageManagedByPolicy()) {
            state = HomeButtonPreferenceState.MANAGED_ENABLED;
        } else if (isHomepageEnabled()) {
            state = HomeButtonPreferenceState.USER_ENABLED;
        }

        RecordHistogram.recordEnumeratedHistogram("Settings.ShowHomeButtonPreferenceStateManaged",
                state, HomeButtonPreferenceState.NUM_ENTRIES);
    }

    public static void recordHomepageIsCustomized(boolean isCustomized) {
        RecordHistogram.recordBooleanHistogram("Settings.HomePageIsCustomized", isCustomized);
    }

    @Override
    public void onHomepagePolicyUpdate() {
        notifyHomepageUpdated();

        boolean isPolicyEnabled = HomepagePolicyManager.isHomepageManagedByPolicy();
        if (isPolicyEnabled) {
            recordHomepageIsCustomized(false);
        }
    }
}
