// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller;

import android.os.SystemClock;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.Verifier.VerificationState;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.Verifier.VerificationStatus;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;

import javax.inject.Inject;

/**
 * Records how long Trusted Web Activities are used for.
 */
@ActivityScope
public class TrustedWebActivityOpenTimeRecorder implements PauseResumeWithNativeObserver {
    private final Verifier mVerifier;
    private final TrustedWebActivityUmaRecorder mRecorder;
    private final ActivityTabProvider mTabProvider;

    private long mOnResumeTimestampMs;
    private long mLastStateChangeTimestampMs;

    private boolean mInVerifiedOrigin;
    private boolean mTwaOpenedRecorded;

    @Inject
    TrustedWebActivityOpenTimeRecorder(
            ActivityLifecycleDispatcher lifecycleDispatcher,
            Verifier verifier,
            TrustedWebActivityUmaRecorder recorder,
            ActivityTabProvider provider) {
        mVerifier = verifier;
        mRecorder = recorder;
        mTabProvider = provider;
        lifecycleDispatcher.register(this);
        verifier.addVerificationObserver(this::onVerificationStateChanged);
    }

    @Override
    public void onResumeWithNative() {
        mOnResumeTimestampMs = SystemClock.elapsedRealtime();
    }

    @Override
    public void onPauseWithNative() {
        assert mOnResumeTimestampMs != 0;
        mRecorder.recordTwaOpenTime(SystemClock.elapsedRealtime() - mOnResumeTimestampMs);
        recordTimeCurrentState();
        mOnResumeTimestampMs = 0;
    }

    private void onVerificationStateChanged() {
        VerificationState state = mVerifier.getState();
        if (state == null || state.status == VerificationStatus.PENDING) {
            return;
        }
        boolean inVerifiedOrigin = state.status == VerificationStatus.SUCCESS;
        if (inVerifiedOrigin == mInVerifiedOrigin) {
            return;
        }
        recordTimeCurrentState();
        mInVerifiedOrigin = inVerifiedOrigin;
        mLastStateChangeTimestampMs = SystemClock.elapsedRealtime();

        if (mInVerifiedOrigin && !mTwaOpenedRecorded) {
            mRecorder.recordTwaOpened(mTabProvider.get());
            mTwaOpenedRecorded = true;
        }
    }

    private void recordTimeCurrentState() {
        if (mLastStateChangeTimestampMs == 0) {
            return;
        }
        long timeInCurrentState = SystemClock.elapsedRealtime()
                - Math.max(mLastStateChangeTimestampMs, mOnResumeTimestampMs);
        if (mInVerifiedOrigin) {
            mRecorder.recordTimeInVerifiedOrigin(timeInCurrentState);
        } else {
            mRecorder.recordTimeOutOfVerifiedOrigin(timeInCurrentState);
        }
    }
}
