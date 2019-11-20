// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.support.annotation.Nullable;
import android.view.WindowManager;

import org.chromium.base.CommandLine;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.incognito.IncognitoTabHost;
import org.chromium.chrome.browser.incognito.IncognitoTabHostRegistry;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;

import javax.inject.Inject;

/**
 * Implements incognito tab host for the given instance of Custom Tab activity.
 */
@ActivityScope
public class CustomTabIncognitoManager implements NativeInitObserver, Destroyable {
    private static final String TAG = "CctIncognito";

    private final ChromeActivity mChromeActivity;
    private final CustomTabActivityNavigationController mNavigationController;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;

    @Nullable
    private IncognitoTabHost mIncognitoTabHost;

    @Inject
    public CustomTabIncognitoManager(ChromeActivity customTabActivity,
            BrowserServicesIntentDataProvider intentDataProvider,
            CustomTabActivityNavigationController navigationController,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mChromeActivity = customTabActivity;
        mIntentDataProvider = intentDataProvider;
        mNavigationController = navigationController;
        lifecycleDispatcher.register(this);
    }

    @Override
    public void onFinishNativeInitialization() {
        assert ChromeFeatureList.isInitialized();
        if (mIntentDataProvider.isIncognito()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_INCOGNITO)) {
            initializeIncognito();
        }
    }

    @Override
    public void destroy() {
        if (mIncognitoTabHost != null) {
            IncognitoTabHostRegistry.getInstance().unregister(mIncognitoTabHost);
        }
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
