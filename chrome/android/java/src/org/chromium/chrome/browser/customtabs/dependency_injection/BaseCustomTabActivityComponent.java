// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dependency_injection;

import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TwaFinishHandler;
import org.chromium.chrome.browser.customtabs.CustomTabCompositorContentInitializer;
import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory;
import org.chromium.chrome.browser.customtabs.CustomTabStatusBarColorProvider;
import org.chromium.chrome.browser.customtabs.CustomTabTaskDescriptionHelper;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabFactory;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarColorController;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityComponent;
import org.chromium.chrome.browser.webapps.SplashController;

/**
 * Contains accessors which are shared between {@link CustomTabActivityComponent} and
 * {@link WebappActivityComponent}.
 */
public interface BaseCustomTabActivityComponent extends ChromeActivityComponent {
    CustomTabActivityNavigationController resolveNavigationController();
    CustomTabActivityTabFactory resolveTabFactory();
    CustomTabActivityTabProvider resolveTabProvider();
    CustomTabCompositorContentInitializer resolveCompositorContentInitializer();
    CustomTabDelegateFactory resolveTabDelegateFactory();
    CustomTabStatusBarColorProvider resolveCustomTabStatusBarColorProvider();
    CustomTabToolbarColorController resolveToolbarColorController();
    CustomTabTaskDescriptionHelper resolveTaskDescriptionHelper();
    CustomTabToolbarCoordinator resolveToolbarCoordinator();
    SplashController resolveSplashController();
    TabObserverRegistrar resolveTabObserverRegistrar();
    TwaFinishHandler resolveTwaFinishHandler();
}
