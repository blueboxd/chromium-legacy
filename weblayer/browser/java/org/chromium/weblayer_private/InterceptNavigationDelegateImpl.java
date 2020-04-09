// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.SystemClock;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.components.external_intents.ExternalNavigationParams;
import org.chromium.components.external_intents.RedirectHandlerImpl;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;
import org.chromium.components.navigation_interception.NavigationParams;
import org.chromium.content_public.browser.WebContents;

/**
 * Class that controls navigations and allows to intercept them. It is used on Android to 'convert'
 * certain navigations to Intents to 3rd party applications.
 */
public class InterceptNavigationDelegateImpl implements InterceptNavigationDelegate {
    private TabImpl mTab;
    private RedirectHandlerImpl mRedirectHandler;
    private ExternalNavigationHandler mExternalNavHandler;
    private ExternalNavigationDelegateImpl mExternalNavigationDelegate;
    private long mLastNavigationWithUserGestureTime = RedirectHandlerImpl.INVALID_TIME;

    /**
     * Default constructor of {@link InterceptNavigationDelegateImpl}.
     */
    InterceptNavigationDelegateImpl(TabImpl tab) {
        mTab = tab;
        mRedirectHandler = RedirectHandlerImpl.create();
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
            NavigationParams navigationParams, RedirectHandlerImpl redirectHandler,
            boolean shouldCloseTab) {
        return new ExternalNavigationParams
                .Builder(navigationParams.url, mTab.getProfile().isIncognito(),
                        navigationParams.referrer, navigationParams.pageTransitionType,
                        navigationParams.isRedirect)
                .setApplicationMustBeInForeground(true)
                .setRedirectHandler(redirectHandler)
                .setOpenInNewTab(shouldCloseTab)
                // TODO(crbug.com/1031465): See whether this needs to become more refined
                // (cf. //chrome's setting of this field in its version of this method).
                .setIsBackgroundTabNavigation(!mTab.isVisible())
                .setIsMainFrame(navigationParams.isMainFrame)
                .setHasUserGesture(navigationParams.hasUserGesture)
                .setShouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent(
                        shouldCloseTab && navigationParams.isMainFrame);
    }

    private int getLastCommittedEntryIndex() {
        if (mTab.getWebContents() == null) return -1;
        return mTab.getWebContents().getNavigationController().getLastCommittedEntryIndex();
    }

    private boolean shouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent() {
        // Closing of tabs as part of intent launching is not yet implemented in WebLayer; specify
        // parameters such that this flow is never invoked.
        // TODO(crbug.com/1031465): Adapt //chrome's logic for closing of tabs.
        return false;
    }

    @Override
    public boolean shouldIgnoreNavigation(NavigationParams navigationParams) {
        if (navigationParams.hasUserGesture || navigationParams.hasUserGestureCarryover) {
            mLastNavigationWithUserGestureTime = SystemClock.elapsedRealtime();
        }

        RedirectHandlerImpl redirectHandler = null;
        if (navigationParams.isMainFrame) {
            redirectHandler = mRedirectHandler;
        } else if (navigationParams.isExternalProtocol) {
            // Only external protocol navigations are intercepted for iframe navigations.  Since
            // we do not see all previous navigations for the iframe, we can not build a complete
            // redirect handler for each iframe.  Nor can we use the top level redirect handler as
            // that has the potential to incorrectly give access to the navigation due to previous
            // main frame gestures.
            //
            // By creating a new redirect handler for each external navigation, we are specifically
            // not covering the case where a gesture is carried over via a redirect.  This is
            // currently not feasible because we do not see all navigations for iframes and it is
            // better to error on the side of caution and require direct user gestures for iframes.
            redirectHandler = RedirectHandlerImpl.create();
        } else {
            assert false;
            return false;
        }

        // NOTE: Chrome listens for user interaction with its Activity. However, this depends on
        // being able to subclass the Activity, which is not possible in WebLayer. As a proxy,
        // WebLayer uses the time of the last navigation with a user gesture to serve as the last
        // time of user interaction. Note that the user interacting with the webpage causes the
        // user gesture bit to be set on any navigation in that page for the next several seconds
        // (cf. comments on //third_party/blink/public/common/frame/user_activation_state.h). This
        // fact further increases the fidelity of this already-reasonable heuristic as a proxy. To
        // date we have not seen any concrete evidence of user-visible differences resulting from
        // the use of the different heuristic.
        redirectHandler.updateNewUrlLoading(navigationParams.pageTransitionType,
                navigationParams.isRedirect,
                navigationParams.hasUserGesture || navigationParams.hasUserGestureCarryover,
                mLastNavigationWithUserGestureTime, getLastCommittedEntryIndex());

        boolean shouldCloseTab = shouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent();
        ExternalNavigationParams params =
                buildExternalNavigationParams(navigationParams, redirectHandler, shouldCloseTab)
                        .build();
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
