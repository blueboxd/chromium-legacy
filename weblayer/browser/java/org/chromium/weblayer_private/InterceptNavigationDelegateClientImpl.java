// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.Activity;
import android.os.SystemClock;

import org.chromium.base.ContextUtils;
import org.chromium.components.external_intents.AuthenticatorNavigationInterceptor;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.InterceptNavigationDelegateClient;
import org.chromium.components.external_intents.InterceptNavigationDelegateImpl;
import org.chromium.components.external_intents.RedirectHandlerImpl;
import org.chromium.components.navigation_interception.NavigationParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

/**
 * Class that provides embedder-level information to InterceptNavigationDelegateImpl based off a
 * Tab.
 */
public class InterceptNavigationDelegateClientImpl implements InterceptNavigationDelegateClient {
    private TabImpl mTab;
    private WebContentsObserver mWebContentsObserver;
    private RedirectHandlerImpl mRedirectHandler;
    private InterceptNavigationDelegateImpl mInterceptNavigationDelegate;
    private long mLastNavigationWithUserGestureTime = RedirectHandlerImpl.INVALID_TIME;

    InterceptNavigationDelegateClientImpl(TabImpl tab) {
        mTab = tab;
        mRedirectHandler = RedirectHandlerImpl.create();
        mWebContentsObserver = new WebContentsObserver() {
            @Override
            public void didFinishNavigation(NavigationHandle navigationHandle) {
                mInterceptNavigationDelegate.onNavigationFinished(navigationHandle);
            }
        };
    }

    public void initializeWithDelegate(InterceptNavigationDelegateImpl delegate) {
        mInterceptNavigationDelegate = delegate;
        getWebContents().addObserver(mWebContentsObserver);
    }

    public void onActivityAttachmentChanged(boolean attached) {
        if (attached) {
            mInterceptNavigationDelegate.setExternalNavigationHandler(
                    createExternalNavigationHandler());
        }
    }

    public void destroy() {
        getWebContents().removeObserver(mWebContentsObserver);
        mInterceptNavigationDelegate.associateWithWebContents(null);
    }

    @Override
    public WebContents getWebContents() {
        return mTab.getWebContents();
    }

    @Override
    public ExternalNavigationHandler createExternalNavigationHandler() {
        return new ExternalNavigationHandler(new ExternalNavigationDelegateImpl(mTab));
    }

    @Override
    public long getLastUserInteractionTime() {
        // NOTE: Chrome listens for user interaction with its Activity. However, this depends on
        // being able to subclass the Activity, which is not possible in WebLayer. As a proxy,
        // WebLayer uses the time of the last navigation with a user gesture to serve as the last
        // time of user interaction. Note that the user interacting with the webpage causes the
        // user gesture bit to be set on any navigation in that page for the next several seconds
        // (cf. comments on //third_party/blink/public/common/frame/user_activation_state.h). This
        // fact further increases the fidelity of this already-reasonable heuristic as a proxy. To
        // date we have not seen any concrete evidence of user-visible differences resulting from
        // the use of the different heuristic.
        return mLastNavigationWithUserGestureTime;
    }

    @Override
    public RedirectHandlerImpl getOrCreateRedirectHandler() {
        return mRedirectHandler;
    }

    @Override
    public AuthenticatorNavigationInterceptor createAuthenticatorNavigationInterceptor() {
        return null;
    }

    @Override
    public boolean isIncognito() {
        return mTab.getProfile().isIncognito();
    }

    @Override
    public boolean isHidden() {
        return !mTab.isVisible();
    }

    @Override
    public Activity getActivity() {
        return ContextUtils.activityFromContext(mTab.getBrowser().getContext());
    }

    @Override
    public boolean wasTabLaunchedFromExternalApp() {
        return false;
    }

    @Override
    public boolean wasTabLaunchedFromLongPressInBackground() {
        return false;
    }

    @Override
    public void closeTab() {
        // TODO(crbug.com/1031465): Bring up tab closing.
    }

    @Override
    public void onNavigationStarted(NavigationParams params) {
        if (params.hasUserGesture || params.hasUserGestureCarryover) {
            mLastNavigationWithUserGestureTime = SystemClock.elapsedRealtime();
        }
    }
}
