// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityBrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.Verifier;
import org.chromium.chrome.browser.customtabs.ExternalIntentsPolicyProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;

import javax.inject.Inject;

/**
 * Coordinator shared between webapp activity and WebAPK activity components.
 * Add methods here if other components need to communicate with either of these components.
 */
@ActivityScope
public class WebappActivityCoordinator implements InflationObserver {
    private final WebappActivity mActivity;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final CurrentPageVerifier mCurrentPageVerifier;
    private TrustedWebActivityBrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    private final WebappDeferredStartupWithStorageHandler mDeferredStartupWithStorageHandler;

    // Whether the current page is within the webapp's scope.
    private boolean mInScope = true;

    @Inject
    public WebappActivityCoordinator(ChromeActivity<?> activity,
            BrowserServicesIntentDataProvider intentDataProvider,
            ActivityTabProvider activityTabProvider, CurrentPageVerifier currentPageVerifier,
            Verifier verifier, CustomTabActivityNavigationController navigationController,
            ExternalIntentsPolicyProvider externalIntentsPolicyProvider,
            WebappDeferredStartupWithStorageHandler deferredStartupWithStorageHandler,
            WebappActionsNotificationManager actionsNotificationManager,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            TrustedWebActivityBrowserControlsVisibilityManager browserControlsVisibilityManager) {
        // We don't need to do anything with |actionsNotificationManager|. We just need to resolve
        // it so that it starts working.

        mActivity = (WebappActivity) activity;
        mIntentDataProvider = intentDataProvider;
        mCurrentPageVerifier = currentPageVerifier;
        mDeferredStartupWithStorageHandler = deferredStartupWithStorageHandler;
        mBrowserControlsVisibilityManager = browserControlsVisibilityManager;

        // WebappActiveTabUmaTracker sets itself as an observer of |activityTabProvider|.
        new WebappActiveTabUmaTracker(
                activityTabProvider, intentDataProvider, mCurrentPageVerifier);

        navigationController.setLandingPageOnCloseCriterion(verifier::wasPreviouslyVerified);
        externalIntentsPolicyProvider.setPolicyCriteria(
                verifier::shouldIgnoreExternalIntentHandlers);

        mDeferredStartupWithStorageHandler.addTask((storage, didCreateStorage) -> {
            if (activity.isActivityFinishingOrDestroyed()) return;

            if (storage != null) {
                updateStorage(storage);
            }
        });

        currentPageVerifier.addVerificationObserver(this::onVerificationUpdate);
        lifecycleDispatcher.register(this);
    }

    /**
     * Invoked to add deferred startup tasks to queue.
     */
    public void initDeferredStartupForActivity() {
        mDeferredStartupWithStorageHandler.initDeferredStartupForActivity();
    }

    @Override
    public void onPreInflationStartup() {}

    @Override
    public void onPostInflationStartup() {
        // Before verification completes, optimistically expect it to be successful.
        if (mCurrentPageVerifier.getState() == null) {
            updateUi(true);
        }
    }

    private void onVerificationUpdate() {
        CurrentPageVerifier.VerificationState state = mCurrentPageVerifier.getState();
        // Consider null |state| to be 'in scope' to mimic TWA logic. |state| should never be null
        // for webapps.
        boolean inScope =
                state == null || state.status != CurrentPageVerifier.VerificationStatus.FAILURE;
        if (inScope == mInScope) return;
        mInScope = inScope;
        updateUi(inScope);
    }

    private void updateUi(boolean inScope) {
        mBrowserControlsVisibilityManager.updateIsInTwaMode(inScope);
    }

    private void updateStorage(@NonNull WebappDataStorage storage) {
        // The information in the WebappDataStorage may have been purged by the
        // user clearing their history or not launching the web app recently.
        // Restore the data if necessary.
        WebappInfo webappInfo = mActivity.getWebappInfo();
        storage.updateFromWebappInfo(webappInfo);

        // A recent last used time is the indicator that the web app is still
        // present on the home screen, and enables sources such as notifications to
        // launch web apps. Thus, we do not update the last used time when the web
        // app is not directly launched from the home screen, as this interferes
        // with the heuristic.
        if (webappInfo.isLaunchedFromHomescreen()) {
            // TODO(yusufo): WebappRegistry#unregisterOldWebapps uses this information to delete
            // WebappDataStorage objects for legacy webapps which haven't been used in a while.
            storage.updateLastUsedTime();
        }
    }
}
