// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dependency_injection;

import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityCoordinator;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TwaFinishHandler;
import org.chromium.chrome.browser.customtabs.CustomTabActivityClientConnectionKeeper;
import org.chromium.chrome.browser.customtabs.CustomTabActivityLifecycleUmaTracker;
import org.chromium.chrome.browser.customtabs.CustomTabBottomBarDelegate;
import org.chromium.chrome.browser.customtabs.CustomTabIncognitoManager;
import org.chromium.chrome.browser.customtabs.CustomTabSessionHandler;
import org.chromium.chrome.browser.customtabs.CustomTabTabPersistencePolicy;
import org.chromium.chrome.browser.customtabs.CustomTabUmaRecorder;
import org.chromium.chrome.browser.customtabs.ReparentingTaskProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabFactory;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler;
import org.chromium.chrome.browser.customtabs.dynamicmodule.DynamicModuleCoordinator;
import org.chromium.chrome.browser.customtabs.dynamicmodule.DynamicModuleToolbarController;
import org.chromium.chrome.browser.customtabs.features.ImmersiveModeController;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarColorController;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityCommonsModule;

import dagger.Subcomponent;

/**
 * Activity-scoped component associated with
 * {@link org.chromium.chrome.browser.customtabs.CustomTabActivity}.
 */
@Subcomponent(modules = {ChromeActivityCommonsModule.class, CustomTabActivityModule.class})
@ActivityScope
public interface CustomTabActivityComponent extends BaseCustomTabActivityComponent {
    TrustedWebActivityCoordinator resolveTrustedWebActivityCoordinator();
    DynamicModuleToolbarController resolveDynamicModuleToolbarController();
    DynamicModuleCoordinator resolveDynamicModuleCoordinator();

    CustomTabBottomBarDelegate resolveBottomBarDelegate();
    CustomTabActivityTabController resolveTabController();
    CustomTabActivityTabFactory resolveTabFactory();
    CustomTabActivityLifecycleUmaTracker resolveUmaTracker();
    CustomTabIntentHandler resolveIntentHandler();
    CustomTabIncognitoManager resolveCustomTabIncognitoManager();
    CustomTabToolbarColorController resolveToolbarColorController();
    CustomTabUmaRecorder resolveCustomTabUmaRecorder();
    CustomTabSessionHandler resolveSessionHandler();
    CustomTabActivityClientConnectionKeeper resolveConnectionKeeper();
    TwaFinishHandler resolveTwaFinishHandler();
    ImmersiveModeController resolveImmersiveModeController();

    // For testing
    CustomTabTabPersistencePolicy resolveTabPersistencePolicy();
    ReparentingTaskProvider resolveReparentingTaskProvider();
}
