// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui;

import org.chromium.chrome.browser.browserservices.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityDisclosureController;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityOpenTimeRecorder;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityVerifier;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityVerifier.VerificationStatus;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.splashscreen.TwaSplashController;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.view.TrustedWebActivityDisclosureView;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabStatusBarColorProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
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

    private final TrustedWebActivityVerifier mVerifier;
    private final CustomTabToolbarCoordinator mToolbarCoordinator;
    private final CustomTabStatusBarColorProvider mStatusBarColorProvider;

    private boolean mInTwaMode = true;

    @Inject
    public TrustedWebActivityCoordinator(
            TrustedWebActivityDisclosureController disclosureController,
            TrustedWebActivityDisclosureView disclosureView,
            TrustedWebActivityOpenTimeRecorder openTimeRecorder,
            TrustedWebActivityVerifier verifier,
            CustomTabActivityNavigationController navigationController,
            Lazy<TwaSplashController> splashController,
            CustomTabIntentDataProvider intentDataProvider,
            TrustedWebActivityUmaRecorder umaRecorder,
            CustomTabStatusBarColorProvider statusBarColorProvider,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            CustomTabToolbarCoordinator toolbarCoordinator) {
        // We don't need to do anything with most of the classes above, we just need to resolve them
        // so they start working.
        mVerifier = verifier;
        mToolbarCoordinator = toolbarCoordinator;
        mStatusBarColorProvider = statusBarColorProvider;

        navigationController.setLandingPageOnCloseCriterion(verifier::isPageOnVerifiedOrigin);

        initSplashScreen(splashController, intentDataProvider, umaRecorder);

        verifier.addVerificationObserver(this::onVerificationUpdate);
        lifecycleDispatcher.register(this);
    }

    @Override
    public void onPreInflationStartup() {}

    @Override
    public void onPostInflationStartup() {
        // Before the verification completes, we optimistically expect it to be successful and apply
        // the trusted web activity mode to UI. So hide the toolbar as soon as possible.
        if (mVerifier.getState() == null) {
            mToolbarCoordinator.setToolbarHidden(true);
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
        TrustedWebActivityVerifier.VerificationState state = mVerifier.getState();
        boolean inTwaMode = state == null || state.status != VerificationStatus.FAILURE;
        if (inTwaMode == mInTwaMode) return;
        mInTwaMode = inTwaMode;
        updateToolbar();
        updateStatusBar();
    }

    private void updateToolbar() {
        mToolbarCoordinator.setToolbarHidden(mInTwaMode);

        if (!mInTwaMode) {
            // Force showing the controls for a bit when leaving Trusted Web Activity mode.
            mToolbarCoordinator.showToolbarTemporarily();
        }
    }

    private void updateStatusBar() {
        mStatusBarColorProvider.setUseTabThemeColor(mInTwaMode);
    }
}
