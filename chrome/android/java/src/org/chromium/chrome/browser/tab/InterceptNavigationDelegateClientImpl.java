// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.app.Activity;

import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.external_intents.AuthenticatorNavigationInterceptor;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.InterceptNavigationDelegateClient;
import org.chromium.components.external_intents.RedirectHandlerImpl;
import org.chromium.content_public.browser.WebContents;

/**
 * Class that provides embedder-level information to InterceptNavigationDelegateImpl based off a
 * Tab.
 */
public class InterceptNavigationDelegateClientImpl implements InterceptNavigationDelegateClient {
    private TabImpl mTab;

    InterceptNavigationDelegateClientImpl(Tab tab) {
        mTab = (TabImpl) tab;
    }

    @Override
    public WebContents getWebContents() {
        return mTab.getWebContents();
    }

    @Override
    public ExternalNavigationHandler createExternalNavigationHandler() {
        return mTab.getDelegateFactory().createExternalNavigationHandler(mTab);
    }

    @Override
    public long getLastUserInteractionTime() {
        ChromeActivity associatedActivity = mTab.getActivity();
        return (associatedActivity == null) ? -1 : associatedActivity.getLastUserInteractionTime();
    }

    @Override
    public RedirectHandlerImpl getOrCreateRedirectHandler() {
        return RedirectHandlerTabHelper.getOrCreateHandlerFor(mTab);
    }

    @Override
    public AuthenticatorNavigationInterceptor createAuthenticatorNavigationInterceptor() {
        // NOTE: This will be transitioned back to calling
        // AppHooks#createAuthenticatorNavigationInterceptor() once the latter has been transitioned
        // to talk in terms of the //components-level interface.
        return AppHooks.get().createAuthenticatorNavigationInterceptorV2(mTab);
    }

    @Override
    public boolean isIncognito() {
        return mTab.isIncognito();
    }

    @Override
    public boolean isHidden() {
        return mTab.isHidden();
    }

    @Override
    public Activity getActivity() {
        return mTab.getActivity();
    }

    @Override
    public boolean wasTabLaunchedFromExternalApp() {
        return mTab.getLaunchType() == TabLaunchType.FROM_EXTERNAL_APP;
    }

    @Override
    public boolean wasTabLaunchedFromLongPressInBackground() {
        return mTab.getLaunchType() == TabLaunchType.FROM_LONGPRESS_BACKGROUND;
    }

    @Override
    public void closeTab() {
        TabModelSelector.from(mTab).closeTab(mTab);
    }
}
