// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dependency_injection;

import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TwaIntentHandlingStrategy;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TwaVerifier;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.Verifier;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler.IntentIgnoringCriterion;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandlingStrategy;
import org.chromium.chrome.browser.customtabs.content.DefaultCustomTabIntentHandlingStrategy;
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
    private final IntentIgnoringCriterion mIntentIgnoringCriterion;

    public BaseCustomTabActivityModule(BrowserServicesIntentDataProvider intentDataProvider,
            IntentIgnoringCriterion intentIgnoringCriterion) {
        mIntentDataProvider = intentDataProvider;
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
        return (mIntentDataProvider.isTrustedWebActivity()
                       || mIntentDataProvider.isWebApkActivity())
                ? twaHandler.get()
                : defaultHandler.get();
    }

    @Provides
    public Verifier provideVerifier(Lazy<WebApkVerifier> webApkVerifier,
            Lazy<AddToHomescreenVerifier> addToHomescreenVerifier, Lazy<TwaVerifier> twaVerifier) {
        if (mIntentDataProvider.isWebApkActivity()) {
            return webApkVerifier.get();
        }
        if (mIntentDataProvider.isWebappOrWebApkActivity()) {
            return addToHomescreenVerifier.get();
        }
        return twaVerifier.get();
    }

    @Provides
    public IntentIgnoringCriterion provideIntentIgnoringCriterion() {
        return mIntentIgnoringCriterion;
    }

    @Provides
    @Reusable
    public WebApkPostShareTargetNavigator providePostShareTargetNavigator() {
        return new WebApkPostShareTargetNavigator();
    }
}
