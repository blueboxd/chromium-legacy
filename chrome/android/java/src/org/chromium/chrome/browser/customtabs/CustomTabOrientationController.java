// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.os.Build;

import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.webapps.SplashController;
import org.chromium.chrome.browser.webapps.SplashscreenObserver;
import org.chromium.chrome.browser.webapps.WebappExtras;
import org.chromium.content_public.browser.ScreenOrientationProvider;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.ui.base.ActivityWindowAndroid;

import javax.inject.Inject;

/**
 * Manages setting the initial screen orientation for the custom tab.
 * Delays all screen orientation requests till the activity translucency is removed.
 */
@ActivityScope
public class CustomTabOrientationController implements InflationObserver {
    private final ActivityWindowAndroid mActivityWindowAndroid;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;

    private @ScreenOrientationValues int mLockScreenOrientation = ScreenOrientationValues.DEFAULT;

    @Inject
    public CustomTabOrientationController(ActivityWindowAndroid activityWindowAndroid,
            BrowserServicesIntentDataProvider intentDataProvider,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mActivityWindowAndroid = activityWindowAndroid;
        mLifecycleDispatcher = lifecycleDispatcher;

        WebappExtras webappExtras = intentDataProvider.getWebappExtras();
        if (webappExtras != null) {
            mLockScreenOrientation = webappExtras.orientation;
            mLifecycleDispatcher.register(this);
        }
    }

    /**
     * Delays screen orientation requests if the activity window's initial translucency and the
     * Android OS version requires it.
     * Should be called:
     * - Prior to pre inflation startup occurring.
     * - Only if the splash screen is shown for the activity.
     */
    public void delayOrientationRequestsIfNeeded(
            SplashController splashController, boolean isWindowInitiallyTranslucent) {
        // Setting the screen orientation while the activity is translucent throws an exception on
        // O (but not on O MR1).
        if (!isWindowInitiallyTranslucent || Build.VERSION.SDK_INT != Build.VERSION_CODES.O) return;

        ScreenOrientationProvider.getInstance().delayOrientationRequests(mActivityWindowAndroid);

        splashController.addObserver(new SplashscreenObserver() {
            @Override
            public void onTranslucencyRemoved() {
                ScreenOrientationProvider.getInstance().runDelayedOrientationRequests(
                        mActivityWindowAndroid);
            }

            @Override
            public void onSplashscreenHidden(long startTimestamp, long endTimestamp) {}
        });
    }

    @Override
    public void onPreInflationStartup() {
        mLifecycleDispatcher.unregister(this);

        // Queue up default screen orientation request now because the web page might change it via
        // JavaScript.
        ScreenOrientationProvider.getInstance().lockOrientation(
                mActivityWindowAndroid, (byte) mLockScreenOrientation);
    }

    @Override
    public void onPostInflationStartup() {}
}
