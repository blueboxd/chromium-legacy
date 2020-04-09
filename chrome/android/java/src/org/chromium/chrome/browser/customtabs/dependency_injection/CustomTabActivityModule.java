// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dependency_injection;

import org.chromium.chrome.browser.browserservices.BrowserServicesActivityTabController;
import org.chromium.chrome.browser.browserservices.ClientAppDataRegister;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.init.StartupTabPreloader;

import dagger.Module;
import dagger.Provides;

/**
 * Module for custom tab specific bindings.
 */
@Module
public class CustomTabActivityModule {
    private final StartupTabPreloader mStartupTabPreloader;

    public CustomTabActivityModule(StartupTabPreloader startupTabPreloader) {
        mStartupTabPreloader = startupTabPreloader;
    }

    @Provides
    public BrowserServicesActivityTabController provideTabController(
            CustomTabActivityTabController customTabActivityTabController) {
        return customTabActivityTabController;
    }

    @Provides
    public ClientAppDataRegister provideClientAppDataRegister() {
        return new ClientAppDataRegister();
    }

    @Provides
    public StartupTabPreloader provideStartupTabPreloader() {
        return mStartupTabPreloader;
    }
}
