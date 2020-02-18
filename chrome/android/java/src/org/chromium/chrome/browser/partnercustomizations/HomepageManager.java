// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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
     * Get the current homepage URI string, if it's enabled. Null otherwise or uninitialized.
     *
     * This function checks different source to get the current homepage, which listed below
     * according to their priority:
     *
     * <b>isManagedByPolicy > useChromeNTP > useDefaultUri > useCustomUri</b>
     *
     * @return Homepage URI string, if it's enabled. Null otherwise or uninitialized.
     *
     * @see HomepagePolicyManager#isHomepageManagedByPolicy()
     * @see #getPrefHomepageUseChromeNTP()
     * @see #getPrefHomepageUseDefaultUri()
     */
    public static String getHomepageUri() {
        if (!isHomepageEnabled()) return null;

        String homepageUri = getInstance().getHomepageUriIgnoringEnabledState();
        return TextUtils.isEmpty(homepageUri) ? null : homepageUri;
    }

    /**
     * @return The default homepage URI if the homepage is partner provided or the new tab page
     *         if the homepage button is force enabled via flag.
     */
    public static String getDefaultHomepageUri() {
        if (PartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled()) {
            return PartnerBrowserCustomizations.getHomePageUrl();
        }
        return UrlConstants.NTP_NON_NATIVE_URL;
    }

    /**
     * Get homepage URI without checking if the homepage is enabled.
     * @return Homepage URI based on policy and shared preference settings.
     */
    public @NonNull String getHomepageUriIgnoringEnabledState() {
        // TODO(wenyufu): Move this function back to #getHomepageUri after
        //  ChromeFeatureList#HOMEPAGE_SETTINGS_UI_CONVERSION 100% release
        if (HomepagePolicyManager.isHomepageManagedByPolicy()) {
            return HomepagePolicyManager.getHomepageUrl();
        }
        if (getPrefHomepageUseChromeNTP()) {
            return UrlConstants.NTP_NON_NATIVE_URL;
        }
        if (getPrefHomepageUseDefaultUri()) {
            return getDefaultHomepageUri();
        }
        return getPrefHomepageCustomUri();
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
     * True if the homepage URL is the default value. False means the homepage URL is using
     * the user customized URL. Note that this method does not take enterprise policy into account.
     * Use {@link HomepagePolicyManager#isHomepageManagedByPolicy} if policy information is needed.
     *
     * @return Whether if the homepage URL is the default value.
     */
    public boolean getPrefHomepageUseDefaultUri() {
        return mSharedPreferencesManager.readBoolean(
                ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, true);
    }

    /**
     * @return Whether the homepage is set to Chrome NTP in Homepage settings
     */
    public boolean getPrefHomepageUseChromeNTP() {
        return mSharedPreferencesManager.readBoolean(
                ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, false);
    }

    /**
     * Set homepage related shared preferences, and notify listeners for the homepage status change.
     * These shared preference values will reflect what homepage we are using.
     *
     * The priority of the input pref values during value checking:
     * useChromeNTP > useDefaultUri > customUri
     *
     * @param useChromeNtp True if homepage is set as Chrome's New tab page.
     * @param useDefaultUri True if homepage is using default URI.
     * @param customUri String value for user customized homepage URI.
     *
     * @see #getHomepageUri()
     */
    public void setHomepagePreferences(
            boolean useChromeNtp, boolean useDefaultUri, String customUri) {
        // TODO(wenyufu): Add metrics for how ofter user checks this option.
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, useChromeNtp);

        boolean wasUseDefaultUri = getPrefHomepageUseDefaultUri();
        if (wasUseDefaultUri != useDefaultUri) {
            recordHomepageIsCustomized(!useDefaultUri);
            mSharedPreferencesManager.writeBoolean(
                    ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, useDefaultUri);
        }

        mSharedPreferencesManager.writeString(ChromePreferenceKeys.HOMEPAGE_CUSTOM_URI, customUri);

        notifyHomepageUpdated();
    }

    /**
     * Get the homepage button preference state.
     */
    public static void recordHomeButtonPreferenceState() {
        if (!CachedFeatureFlags.isEnabled(ChromeFeatureList.HOMEPAGE_LOCATION_POLICY)) {
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
