// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import androidx.annotation.NonNull;

import org.chromium.base.StrictModeContext;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityBrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.Verifier;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabOrientationController;
import org.chromium.chrome.browser.customtabs.CustomTabStatusBarColorProvider;
import org.chromium.chrome.browser.customtabs.ExternalIntentsPolicyProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarColorController;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.metrics.LaunchMetrics;
import org.chromium.chrome.browser.util.AndroidTaskUtils;

import javax.inject.Inject;

/**
 * Coordinator shared between webapp activity and WebAPK activity components.
 * Add methods here if other components need to communicate with either of these components.
 */
@ActivityScope
public class WebappActivityCoordinator
        implements InflationObserver, PauseResumeWithNativeObserver, StartStopWithNativeObserver {
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final WebappInfo mWebappInfo;
    private final ChromeActivity<?> mActivity;
    private final CurrentPageVerifier mCurrentPageVerifier;
    private TrustedWebActivityBrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    private final CustomTabToolbarColorController mToolbarColorController;
    private final CustomTabStatusBarColorProvider mStatusBarColorProvider;
    private final WebappDeferredStartupWithStorageHandler mDeferredStartupWithStorageHandler;

    // Whether the current page is within the webapp's scope.
    private boolean mInScope = true;

    @Inject
    public WebappActivityCoordinator(ChromeActivity<?> activity,
            BrowserServicesIntentDataProvider intentDataProvider,
            ActivityTabProvider activityTabProvider, CurrentPageVerifier currentPageVerifier,
            Verifier verifier, CustomTabActivityNavigationController navigationController,
            ExternalIntentsPolicyProvider externalIntentsPolicyProvider,
            CustomTabOrientationController orientationController, SplashController splashController,
            WebappDeferredStartupWithStorageHandler deferredStartupWithStorageHandler,
            WebappActionsNotificationManager actionsNotificationManager,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            TrustedWebActivityBrowserControlsVisibilityManager browserControlsVisibilityManager,
            CustomTabToolbarColorController toolbarColorController,
            CustomTabStatusBarColorProvider statusBarColorProvider) {
        // We don't need to do anything with |actionsNotificationManager|. We just need to resolve
        // it so that it starts working.

        mIntentDataProvider = intentDataProvider;
        mWebappInfo = WebappInfo.create(mIntentDataProvider);
        mActivity = activity;
        mCurrentPageVerifier = currentPageVerifier;
        mDeferredStartupWithStorageHandler = deferredStartupWithStorageHandler;
        mBrowserControlsVisibilityManager = browserControlsVisibilityManager;
        mToolbarColorController = toolbarColorController;
        mStatusBarColorProvider = statusBarColorProvider;

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

        orientationController.delayOrientationRequestsIfNeeded(
                splashController, BaseCustomTabActivity.isWindowInitiallyTranslucent(activity));

        currentPageVerifier.addVerificationObserver(this::onVerificationUpdate);
        lifecycleDispatcher.register(this);

        // Initialize the WebappRegistry and warm up the shared preferences for this web app. No-ops
        // if the registry and this web app are already initialized. Must override Strict Mode to
        // avoid a violation.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            WebappRegistry.getInstance();
            WebappRegistry.warmUpSharedPrefsForId(mWebappInfo.id());
        }
    }

    /**
     * Invoked to add deferred startup tasks to queue.
     */
    public void initDeferredStartupForActivity() {
        mDeferredStartupWithStorageHandler.initDeferredStartupForActivity();
    }

    @Override
    public void onPreInflationStartup() {
        LaunchMetrics.recordHomeScreenLaunchIntoStandaloneActivity(mWebappInfo);
    }

    @Override
    public void onPostInflationStartup() {
        // Before verification completes, optimistically expect it to be successful.
        if (mCurrentPageVerifier.getState() == null) {
            updateUi(true);
        }
    }

    @Override
    public void onStartWithNative() {
        WebappDirectoryManager.cleanUpDirectories();
    }

    @Override
    public void onStopWithNative() {}

    @Override
    public void onPauseWithNative() {}

    @Override
    public void onResumeWithNative() {
        if (!mActivity.isFinishing() && mActivity.getIntent() != null) {
            // Avoid situations where Android starts two Activities with the same data.
            // TODO: Determine whether this is still needed now that we use
            // Activity#finishAndRemoveTask() when handling 'back'.
            AndroidTaskUtils.finishOtherTasksWithData(
                    mActivity.getIntent().getData(), mActivity.getTaskId());
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
        mToolbarColorController.setUseTabThemeColor(inScope);
        mStatusBarColorProvider.setUseTabThemeColor(inScope);
    }

    private void updateStorage(@NonNull WebappDataStorage storage) {
        // The information in the WebappDataStorage may have been purged by the
        // user clearing their history or not launching the web app recently.
        // Restore the data if necessary.
        storage.updateFromWebappIntentDataProvider(mIntentDataProvider);

        // A recent last used time is the indicator that the web app is still
        // present on the home screen, and enables sources such as notifications to
        // launch web apps. Thus, we do not update the last used time when the web
        // app is not directly launched from the home screen, as this interferes
        // with the heuristic.
        if (mWebappInfo.isLaunchedFromHomescreen()) {
            // TODO(yusufo): WebappRegistry#unregisterOldWebapps uses this information to delete
            // WebappDataStorage objects for legacy webapps which haven't been used in a while.
            storage.updateLastUsedTime();
        }
    }
}
