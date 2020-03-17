// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;
import org.chromium.components.navigation_interception.NavigationParams;
import org.chromium.content_public.browser.WebContents;

/**
 * Class that controls navigations and allows to intercept them. It is used on Android to 'convert'
 * certain navigations to Intents to 3rd party applications.
 */
public class InterceptNavigationDelegateImpl implements InterceptNavigationDelegate {
    private TabImpl mTab;

    /**
     * Default constructor of {@link InterceptNavigationDelegateImpl}.
     */
    InterceptNavigationDelegateImpl(TabImpl tab) {
        mTab = tab;
        InterceptNavigationDelegateImplJni.get().associateWithWebContents(
                this, mTab.getWebContents());
    }

    @Override
    public boolean shouldIgnoreNavigation(NavigationParams navigationParams) {
        return ExternalNavigationHandler.shouldOverrideUrlLoading(mTab, navigationParams.url,
                navigationParams.hasUserGesture, navigationParams.isRedirect,
                navigationParams.isMainFrame, navigationParams.pageTransitionType);
    }

    @NativeMethods
    interface Natives {
        void associateWithWebContents(
                InterceptNavigationDelegateImpl nativeInterceptNavigationDelegateImpl,
                WebContents webContents);
    }
}
