// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui;

import org.chromium.chrome.browser.browserservices.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityBrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityDisclosureController;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityOpenTimeRecorder;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.Verifier;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.Verifier.VerificationStatus;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.splashscreen.TwaSplashController;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.view.TrustedWebActivityDisclosureView;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabStatusBarColorProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.features.ImmersiveModeController;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * Coordinator for the Trusted Web Activity component.
 * Add methods here if other components need to communicate with Trusted Web Activity component.
 */
@ActivityScope
public class TrustedWebActivityCoordinator implements InflationObserver {

    private final Verifier mVerifier;
    private TrustedWebActivityBrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    private final CustomTabStatusBarColorProvider mStatusBarColorProvider;
    private final Lazy<ImmersiveModeController> mImmersiveModeController;

    private boolean mInTwaMode = true;

    @Inject
    public TrustedWebActivityCoordinator(
            TrustedWebActivityDisclosureController disclosureController,
            TrustedWebActivityDisclosureView disclosureView,
            TrustedWebActivityOpenTimeRecorder openTimeRecorder,
            Verifier verifier,
            CustomTabActivityNavigationController navigationController,
            Lazy<TwaSplashController> splashController,
            CustomTabIntentDataProvider intentDataProvider,
            TrustedWebActivityUmaRecorder umaRecorder,
            CustomTabStatusBarColorProvider statusBarColorProvider,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            TrustedWebActivityBrowserControlsVisibilityManager browserControlsVisibilityManager,
            Lazy<ImmersiveModeController> immersiveModeController) {
        // We don't need to do anything with most of the classes above, we just need to resolve them
        // so they start working.
        mVerifier = verifier;
        mBrowserControlsVisibilityManager = browserControlsVisibilityManager;
        mStatusBarColorProvider = statusBarColorProvider;
        mImmersiveModeController = immersiveModeController;

        navigationController.setLandingPageOnCloseCriterion(verifier::isPageOnVerifiedOrigin);
        initSplashScreen(splashController, intentDataProvider, umaRecorder);

        verifier.addVerificationObserver(this::onVerificationUpdate);
        lifecycleDispatcher.register(this);
    }

    @Override
    public void onPreInflationStartup() {
        if (mVerifier.getState() == null) {
            updateImmersiveMode(true); // Set immersive mode ASAP, before layout inflation.
        }
    }

    @Override
    public void onPostInflationStartup() {
        // Before the verification completes, we optimistically expect it to be successful and apply
        // the trusted web activity mode to UI.
        if (mVerifier.getState() == null) {
            updateUi(true);
        }
    }

    private void initSplashScreen(Lazy<TwaSplashController> splashController,
            CustomTabIntentDataProvider intentDataProvider,
            TrustedWebActivityUmaRecorder umaRecorder) {
        boolean showSplashScreen =
                TwaSplashController.intentIsForTwaWithSplashScreen(intentDataProvider.getIntent());

        if (showSplashScreen) {
            splashController.get();
        }

        umaRecorder.recordSplashScreenUsage(showSplashScreen);
    }

    private void onVerificationUpdate() {
        Verifier.VerificationState state = mVerifier.getState();
        boolean inTwaMode = state == null || state.status != VerificationStatus.FAILURE;
        if (inTwaMode == mInTwaMode) return;
        mInTwaMode = inTwaMode;
        updateUi(mInTwaMode);
    }

    private void updateUi(boolean inTwaMode) {
        updateImmersiveMode(inTwaMode);
        mStatusBarColorProvider.setUseTabThemeColor(inTwaMode);
        mBrowserControlsVisibilityManager.updateIsInTwaMode(inTwaMode);
    }

    private void updateImmersiveMode(boolean inTwaMode) {
        // TODO(pshmakov): implement this once we can depend on tip-of-tree of androidx-browser.
    }
}
