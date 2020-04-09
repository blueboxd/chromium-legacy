// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dependency_injection;

import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TwaIntentHandlingStrategy;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TwaVerifier;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.Verifier;
import org.chromium.chrome.browser.customtabs.CustomTabNightModeStateController;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler.IntentIgnoringCriterion;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandlingStrategy;
import org.chromium.chrome.browser.customtabs.content.DefaultCustomTabIntentHandlingStrategy;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.webapps.AddToHomescreenVerifier;
import org.chromium.chrome.browser.webapps.WebApkPostShareTargetNavigator;
import org.chromium.chrome.browser.webapps.WebApkVerifier;

import dagger.Lazy;
import dagger.Module;
import dagger.Provides;
import dagger.Reusable;

/**
 * Module for bindings shared between custom tabs and webapps.
 */
@Module
public class BaseCustomTabActivityModule {
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final @ActivityType int mActivityType;
    private final CustomTabNightModeStateController mNightModeController;
    private final IntentIgnoringCriterion mIntentIgnoringCriterion;

    public BaseCustomTabActivityModule(BrowserServicesIntentDataProvider intentDataProvider,
            CustomTabNightModeStateController nightModeController,
            IntentIgnoringCriterion intentIgnoringCriterion) {
        mIntentDataProvider = intentDataProvider;
        mActivityType = intentDataProvider.getActivityType();
        mNightModeController = nightModeController;
        mIntentIgnoringCriterion = intentIgnoringCriterion;
    }

    @Provides
    public BrowserServicesIntentDataProvider providesBrowserServicesIntentDataProvider() {
        return mIntentDataProvider;
    }

    @Provides
    public CustomTabIntentHandlingStrategy provideIntentHandler(
            Lazy<DefaultCustomTabIntentHandlingStrategy> defaultHandler,
            Lazy<TwaIntentHandlingStrategy> twaHandler) {
        return (mActivityType == ActivityType.TRUSTED_WEB_ACTIVITY
                       || mActivityType == ActivityType.WEB_APK)
                ? twaHandler.get()
                : defaultHandler.get();
    }

    @Provides
    public Verifier provideVerifier(Lazy<WebApkVerifier> webApkVerifier,
            Lazy<AddToHomescreenVerifier> addToHomescreenVerifier, Lazy<TwaVerifier> twaVerifier) {
        if (mActivityType == ActivityType.WEB_APK) {
            return webApkVerifier.get();
        }
        if (mActivityType == ActivityType.WEBAPP) {
            return addToHomescreenVerifier.get();
        }
        return twaVerifier.get();
    }

    @Provides
    public IntentIgnoringCriterion provideIntentIgnoringCriterion() {
        return mIntentIgnoringCriterion;
    }

    @Provides
    public CustomTabNightModeStateController provideNightModeController() {
        return mNightModeController;
    }

    @Provides
    @Reusable
    public WebApkPostShareTargetNavigator providePostShareTargetNavigator() {
        return new WebApkPostShareTargetNavigator();
    }
}
