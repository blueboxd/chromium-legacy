// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller;

import static org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel.DISCLOSURE_EVENTS_CALLBACK;
import static org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel.DISCLOSURE_STATE;
import static org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel.DISCLOSURE_STATE_DISMISSED_BY_USER;
import static org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel.DISCLOSURE_STATE_NOT_SHOWN;
import static org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel.DISCLOSURE_STATE_SHOWN;

import org.chromium.chrome.browser.browserservices.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.Verifier.VerificationState;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.Verifier.VerificationStatus;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;

import javax.inject.Inject;

/**
 * Controls when Trusted Web Activity disclosure should be shown and hidden, reacts to interaction
 * with it.
 */
public class TrustedWebActivityDisclosureController implements NativeInitObserver,
        TrustedWebActivityModel.DisclosureEventsCallback {
    private final ChromePreferenceManager mPreferenceManager;
    private final TrustedWebActivityModel mModel;
    private final Verifier mVerifier;
    private final TrustedWebActivityUmaRecorder mRecorder;

    @Inject
    TrustedWebActivityDisclosureController(
            ChromePreferenceManager preferenceManager,
            TrustedWebActivityModel model,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            Verifier verifier,
            TrustedWebActivityUmaRecorder recorder) {
        mVerifier = verifier;
        mPreferenceManager = preferenceManager;
        mModel = model;
        mRecorder = recorder;
        model.set(DISCLOSURE_EVENTS_CALLBACK, this);
        verifier.addVerificationObserver(this::onVerificationStatusChanged);
        lifecycleDispatcher.register(this);
    }

    private void onVerificationStatusChanged() {
        if (shouldShowInCurrentState()) {
            showIfNeeded();
        } else {
            dismiss();
        }
    }

    @Override
    public void onDisclosureAccepted() {
        mRecorder.recordDisclosureAccepted();
        mPreferenceManager.setUserAcceptedTwaDisclosureForPackage(mVerifier.getClientPackageName());
        mModel.set(DISCLOSURE_STATE, DISCLOSURE_STATE_DISMISSED_BY_USER);
    }

    /** Shows the disclosure if it is not already showing and hasn't been accepted. */
    private void showIfNeeded() {
        if (!isShowing() && !wasDismissed()) {
            mRecorder.recordDisclosureShown();
            mModel.set(DISCLOSURE_STATE, DISCLOSURE_STATE_SHOWN);
        }
    }

    /** Dismisses the disclosure if it is showing. */
    private void dismiss() {
        if (isShowing()) {
            mModel.set(DISCLOSURE_STATE, DISCLOSURE_STATE_NOT_SHOWN);
        }
    }

    /** Has a disclosure been dismissed for this client package before? */
    private boolean wasDismissed() {
        return mPreferenceManager.hasUserAcceptedTwaDisclosureForPackage(
                mVerifier.getClientPackageName());
    }

    @Override
    public void onFinishNativeInitialization() {
        // We want to show disclosure ASAP, which is limited by SnackbarManager requiring native.
        if (shouldShowInCurrentState()) {
            showIfNeeded();
        }
    }

    private boolean shouldShowInCurrentState() {
        VerificationState state = mVerifier.getState();
        return state != null && state.status != VerificationStatus.FAILURE;
    }

    public boolean isShowing() {
        return mModel.get(DISCLOSURE_STATE) == DISCLOSURE_STATE_SHOWN;
    }
}
