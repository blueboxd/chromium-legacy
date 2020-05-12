// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.dependency_injection;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.browserservices.BrowserServicesActivityTabController;
import org.chromium.chrome.browser.init.StartupTabPreloader;
import org.chromium.chrome.browser.webapps.WebappActivityTabController;

import dagger.Module;
import dagger.Provides;

/**
 * Module for webapp specific bindings.
 */
@Module
public final class WebappActivityModule {
    @Provides
    public BrowserServicesActivityTabController provideTabController(
            WebappActivityTabController webappTabController) {
        return webappTabController;
    }

    @Nullable
    @Provides
    public StartupTabPreloader provideStartupTabPreloader() {
        return null;
    }
}
