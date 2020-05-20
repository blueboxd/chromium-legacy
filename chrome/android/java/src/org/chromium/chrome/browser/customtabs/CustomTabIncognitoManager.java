// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.view.WindowManager;

import androidx.annotation.Nullable;

import org.chromium.base.CommandLine;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoTabHost;
import org.chromium.chrome.browser.incognito.IncognitoTabHostRegistry;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;

import javax.inject.Inject;

/**
 * Implements incognito tab host for the given instance of Custom Tab activity.
 * This class exists for every custom tab, but its only active if
 * |isEnabledIncognitoCCT| returns true.
 */
@ActivityScope
public class CustomTabIncognitoManager implements NativeInitObserver, Destroyable {
    private static final String TAG = "CctIncognito";

    private final ChromeActivity<?> mChromeActivity;
    private final CustomTabActivityNavigationController mNavigationController;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final CustomTabActivityTabProvider mTabProvider;
    private OTRProfileID mOTRProfileID;

    @Nullable
    private IncognitoTabHost mIncognitoTabHost;

    @Inject
    public CustomTabIncognitoManager(ChromeActivity<?> customTabActivity,
            BrowserServicesIntentDataProvider intentDataProvider,
            CustomTabActivityNavigationController navigationController,
            CustomTabActivityTabProvider tabProvider,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mChromeActivity = customTabActivity;
        mIntentDataProvider = intentDataProvider;
        mNavigationController = navigationController;
        mTabProvider = tabProvider;
        lifecycleDispatcher.register(this);
    }

    public boolean isEnabledIncognitoCCT() {
        return mIntentDataProvider.isIncognito()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_INCOGNITO);
    }

    public Profile getProfile() {
        if (mOTRProfileID == null) {
            // TODO(https://crbug.com/1023759): This creates a new OffTheRecord
            // profile for each call to open an incognito CCT.
            // To be updated for apps which have introduced themselves, are
            // resurrecting, and prefer to use their previous profile if it
            // still alive.
            mOTRProfileID = OTRProfileID.createUnique("CCT:Incognito");
        }
        return Profile.getLastUsedRegularProfile().getOffTheRecordProfile(mOTRProfileID);
    }

    @Override
    public void onFinishNativeInitialization() {
        if (isEnabledIncognitoCCT()) {
            initializeIncognito();
        }
    }

    @Override
    public void destroy() {
        if (mIncognitoTabHost != null) {
            IncognitoTabHostRegistry.getInstance().unregister(mIncognitoTabHost);
        }
        if (mOTRProfileID != null) {
            Profile.getLastUsedRegularProfile()
                    .getOffTheRecordProfile(mOTRProfileID)
                    .destroyWhenAppropriate();
            mOTRProfileID = null;
        }
    }

    // TODO(crbug.com/1023759): Remove this function.
    public static boolean hasIsolatedProfile() {
        return true;
    }

    private void initializeIncognito() {
        mIncognitoTabHost = new IncognitoCustomTabHost();
        IncognitoTabHostRegistry.getInstance().register(mIncognitoTabHost);
        if (!CommandLine.getInstance().hasSwitch(
                    ChromeSwitches.ENABLE_INCOGNITO_SNAPSHOTS_IN_ANDROID_RECENTS)) {
            // Disable taking screenshots and seeing snapshots in recents.
            mChromeActivity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        }
    }

    private class IncognitoCustomTabHost implements IncognitoTabHost {
        public IncognitoCustomTabHost() {
            assert mIntentDataProvider.isIncognito();
        }
        @Override
        public boolean hasIncognitoTabs() {
            return !mChromeActivity.isFinishing();
        }
        @Override
        public void closeAllIncognitoTabs() {
            mNavigationController.finish(CustomTabActivityNavigationController.FinishReason.OTHER);
        }
    }
}
