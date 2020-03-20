// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.dependency_injection;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.browserservices.BrowserServicesActivityTabController;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.Verifier;
import org.chromium.chrome.browser.customtabs.CustomTabUmaRecorder;
import org.chromium.chrome.browser.init.StartupTabPreloader;
import org.chromium.chrome.browser.webapps.AddToHomescreenVerifier;
import org.chromium.chrome.browser.webapps.WebApkVerifier;
import org.chromium.chrome.browser.webapps.WebappActivityTabController;

import dagger.Lazy;
import dagger.Module;
import dagger.Provides;

/**
 * Module for webapp specific bindings.
 */
@Module
public final class WebappActivityModule {
    private final BrowserServicesIntentDataProvider mIntentDataProvider;

    public WebappActivityModule(BrowserServicesIntentDataProvider intentDataProvider) {
        mIntentDataProvider = intentDataProvider;
    }

    @Provides
    public BrowserServicesIntentDataProvider provideIntentDataProvider() {
        return mIntentDataProvider;
    }

    @Provides
    public BrowserServicesActivityTabController provideTabController(
            WebappActivityTabController webappTabController) {
        return webappTabController;
    }

    @Provides
    public Verifier provideVerifier(Lazy<AddToHomescreenVerifier> addToHomescreenVerifier,
            Lazy<WebApkVerifier> webApkVerifier) {
        return mIntentDataProvider.isWebApkActivity() ? webApkVerifier.get()
                                                      : addToHomescreenVerifier.get();
    }

    @Nullable
    @Provides
    // TODO(crbug.com/1021201): Add bit to all metrics to record whether they are recorded from a
    // WebappActivity.
    public CustomTabUmaRecorder provideCustomTabUmaRecorder() {
        return null;
    }

    @Nullable
    @Provides
    public StartupTabPreloader provideStartupTabPreloader() {
        return null;
    }
}
