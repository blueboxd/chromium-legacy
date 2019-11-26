// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar.PrefObserver;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;

/**
 * Provides information for the home page related policies.
 * Monitors changes for the homepage preference.
 */
public class HomepagePolicyManager implements PrefObserver {
    private static HomepagePolicyManager sInstance;

    private boolean mIsHomepageLocationPolicyEnabled;
    private String mHomepage;

    private final PrefChangeRegistrar mPrefChangeRegistrar;

    /**
     * Get the singleton instance of HomepagePolicyManager.
     * @return HomepagePolicyManager
     */
    public static HomepagePolicyManager getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new HomepagePolicyManager();
        }
        return sInstance;
    }

    @VisibleForTesting
    static void setInstanceForTests(HomepagePolicyManager instance) {
        assert instance != null;
        sInstance = instance;
    }

    private HomepagePolicyManager() {
        this(new PrefChangeRegistrar());
    }

    @VisibleForTesting
    HomepagePolicyManager(@NonNull PrefChangeRegistrar prefChangeRegistrar) {
        if (!isFeatureFlagEnabled()) {
            mPrefChangeRegistrar = null;
            return;
        }

        mPrefChangeRegistrar = prefChangeRegistrar;
        mPrefChangeRegistrar.addObserver(Pref.HOME_PAGE, this);
        refresh();
    }

    @Override
    public void onPreferenceChange() {
        assert isFeatureFlagEnabled();
        refresh();
    }

    /**
     * Stop observing pref changes and destroy the singleton instance.
     * Will be called from {@link org.chromium.chrome.browser.ChromeActivitySessionTracker}
     */
    public static void destroy() {
        sInstance.destroyInternal();
        sInstance = null;
    }

    protected void destroyInternal() {
        if (mPrefChangeRegistrar != null) mPrefChangeRegistrar.destroy();
    }

    private void refresh() {
        PrefServiceBridge prefServiceBridge = PrefServiceBridge.getInstance();
        mIsHomepageLocationPolicyEnabled = prefServiceBridge.isManagedPreference(Pref.HOME_PAGE);

        if (!mIsHomepageLocationPolicyEnabled) {
            mHomepage = "";
            return;
        }

        mHomepage = prefServiceBridge.getString(Pref.HOME_PAGE);
        assert mHomepage != null;
    }

    @VisibleForTesting
    boolean isHomepageLocationPolicyEnabled() {
        return mIsHomepageLocationPolicyEnabled;
    }

    @VisibleForTesting
    @NonNull
    String getHomepagePreference() {
        assert mIsHomepageLocationPolicyEnabled;
        return mHomepage;
    }

    /**
     * If Policies such as HomepageLocation / HomepageIsNewTabPage is enabled on this device,
     * the home page will be marked as managed.
     * @return True if the current home page is managed by enterprice policy.
     */
    public static boolean isHomepageManagedByPolicy() {
        return isFeatureFlagEnabled() && getInstance().isHomepageLocationPolicyEnabled();
    }

    private static boolean isFeatureFlagEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.HOMEPAGE_LOCATION_POLICY);
    }

    /**
     * @return The homepage URL from native
     */
    @NonNull
    public static String getHomepageUrl() {
        return getInstance().getHomepagePreference();
    }
}
