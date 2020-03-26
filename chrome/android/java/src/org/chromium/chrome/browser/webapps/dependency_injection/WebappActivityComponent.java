// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.dependency_injection;

import org.chromium.chrome.browser.customtabs.dependency_injection.BaseCustomTabActivityComponent;
import org.chromium.chrome.browser.customtabs.dependency_injection.BaseCustomTabActivityModule;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityCommonsModule;
import org.chromium.chrome.browser.webapps.WebApkUpdateManager;
import org.chromium.chrome.browser.webapps.WebappActivityTabController;

import dagger.Subcomponent;

/**
 * Activity-scoped component associated with
 * {@link org.chromium.chrome.browser.webapps.WebappActivity}.
 */
@Subcomponent(modules = {ChromeActivityCommonsModule.class, BaseCustomTabActivityModule.class,
                      WebappActivityModule.class})
@ActivityScope
public interface WebappActivityComponent extends BaseCustomTabActivityComponent {
    WebappActivityTabController resolveTabController();
    WebApkUpdateManager resolveWebApkUpdateManager();
}
