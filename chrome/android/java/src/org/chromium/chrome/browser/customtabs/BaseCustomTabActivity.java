// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityComponent;
import org.chromium.chrome.browser.ui.RootUiCoordinator;

/**
 * Contains functionality which is shared between {@link WebappActivity} and
 * {@link CustomTabActivity}. Purpose of the class is to simplify merging {@link WebappActivity}
 * and {@link CustomTabActivity}.
 * @param <C> - type of associated Dagger component.
 */
public abstract class BaseCustomTabActivity<C extends ChromeActivityComponent>
        extends ChromeActivity<C> {
    protected CustomTabToolbarCoordinator mToolbarCoordinator;
    protected CustomTabActivityNavigationController mNavigationController;

    // This is to give the right package name while using the client's resources during an
    // overridePendingTransition call.
    // TODO(ianwen, yusufo): Figure out a solution to extract external resources without having to
    // change the package name.
    protected boolean mShouldOverridePackage;

    @Override
    protected RootUiCoordinator createRootUiCoordinator() {
        // TODO(https://crbug.com/1020324): Move this logic into a CustomTabRootUICoordinator that
        // can synchronously orchestrate the creation of child coordinators.
        return new RootUiCoordinator(this, (toolbarManager) -> {
            mToolbarCoordinator.onToolbarInitialized(toolbarManager);
            mNavigationController.onToolbarInitialized(toolbarManager);
        }, null, getShareDelegateSupplier());
    }

    @Override
    protected int getControlContainerLayoutId() {
        return R.layout.custom_tabs_control_container;
    }

    @Override
    protected int getToolbarLayoutId() {
        return R.layout.custom_tabs_toolbar;
    }

    @Override
    public int getControlContainerHeightResource() {
        return R.dimen.custom_tabs_control_container_height;
    }

    @Override
    protected boolean handleBackPressed() {
        return mNavigationController.navigateOnBack();
    }

    @Override
    public boolean canShowAppMenu() {
        if (getActivityTab() == null || !mToolbarCoordinator.toolbarIsInitialized()) return false;

        return super.canShowAppMenu();
    }
}
