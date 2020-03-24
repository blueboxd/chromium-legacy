// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.components.external_intents.ExternalNavigationParams;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;
import org.chromium.components.navigation_interception.NavigationParams;
import org.chromium.content_public.browser.WebContents;

/**
 * Class that controls navigations and allows to intercept them. It is used on Android to 'convert'
 * certain navigations to Intents to 3rd party applications.
 */
public class InterceptNavigationDelegateImpl implements InterceptNavigationDelegate {
    private TabImpl mTab;
    private ExternalNavigationHandler mExternalNavHandler;
    private ExternalNavigationDelegateImpl mExternalNavigationDelegate;

    /**
     * Default constructor of {@link InterceptNavigationDelegateImpl}.
     */
    InterceptNavigationDelegateImpl(TabImpl tab) {
        mTab = tab;
        mExternalNavigationDelegate = new ExternalNavigationDelegateImpl(mTab);
        mExternalNavHandler = new ExternalNavigationHandler(mExternalNavigationDelegate);
        InterceptNavigationDelegateImplJni.get().associateWithWebContents(
                this, mTab.getWebContents());
    }

    public void onTabDestroyed() {
        mExternalNavigationDelegate.onTabDestroyed();
    }

    /**
     * Returns ExternalNavigationParams.Builder to generate ExternalNavigationParams for
     * ExternalNavigationHandler#shouldOverrideUrlLoading().
     */
    private ExternalNavigationParams.Builder buildExternalNavigationParams(
            NavigationParams navigationParams, boolean shouldCloseTab) {
        return new ExternalNavigationParams
                .Builder(navigationParams.url, mTab.getProfile().isIncognito(),
                        navigationParams.referrer, navigationParams.pageTransitionType,
                        navigationParams.isRedirect)
                .setApplicationMustBeInForeground(true)
                // NOTE: ExternalNavigationHandler.java supports the redirect handler being
                // null, although WebLayer will likely eventually want one.
                .setRedirectHandler(null)
                .setOpenInNewTab(shouldCloseTab)
                // TODO(crbug.com/1031465): See whether this needs to become more refined
                // (cf. //chrome's setting of this field in its version of this method).
                .setIsBackgroundTabNavigation(!mTab.isVisible())
                .setIsMainFrame(navigationParams.isMainFrame)
                .setHasUserGesture(navigationParams.hasUserGesture)
                .setShouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent(
                        shouldCloseTab && navigationParams.isMainFrame);
    }

    private boolean shouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent() {
        // Closing of tabs as part of intent launching is not yet implemented in WebLayer; specify
        // parameters such that this flow is never invoked.
        // TODO(crbug.com/1031465): Adapt //chrome's logic for closing of tabs.
        return false;
    }

    @Override
    public boolean shouldIgnoreNavigation(NavigationParams navigationParams) {
        boolean shouldCloseTab = shouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent();
        ExternalNavigationParams params =
                buildExternalNavigationParams(navigationParams, shouldCloseTab).build();
        return (mExternalNavHandler.shouldOverrideUrlLoading(params)
                != OverrideUrlLoadingResult.NO_OVERRIDE);
    }

    @NativeMethods
    interface Natives {
        void associateWithWebContents(
                InterceptNavigationDelegateImpl nativeInterceptNavigationDelegateImpl,
                WebContents webContents);
    }
}
