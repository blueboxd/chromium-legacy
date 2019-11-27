// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.logging;

import android.support.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.SpinnerType;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.time.Clock;

/** Logs events for displaying spinner in the Stream. */
public class SpinnerLogger {
    private static final String TAG = "SpinnerLogger";
    private static final int SPINNER_INACTIVE = -1;
    private final BasicLoggingApi mBasicLoggingApi;
    private final Clock mClock;

    private long mSpinnerStartTime = SPINNER_INACTIVE;
    @SpinnerType
    private int mSpinnerType;

    public SpinnerLogger(BasicLoggingApi basicLoggingApi, Clock clock) {
        this.mBasicLoggingApi = basicLoggingApi;
        this.mClock = clock;
    }

    public void spinnerStarted(@SpinnerType int spinnerType) {
        if (isSpinnerActive()) {
            Logger.wtf(TAG,
                    "spinnerStarted should not be called if another spinner is "
                            + "currently being tracked.");
            return;
        }

        mBasicLoggingApi.onSpinnerStarted(spinnerType);
        mSpinnerStartTime = mClock.currentTimeMillis();
        this.mSpinnerType = spinnerType;
    }

    public void spinnerFinished() {
        if (!isSpinnerActive()) {
            Logger.wtf(TAG, "spinnerFinished should only be called after spinnerStarted.");
            return;
        }

        long spinnerDuration = mClock.currentTimeMillis() - mSpinnerStartTime;
        mBasicLoggingApi.onSpinnerFinished((int) spinnerDuration, mSpinnerType);
        mSpinnerStartTime = SPINNER_INACTIVE;
    }

    public void spinnerDestroyedWithoutCompleting() {
        if (!isSpinnerActive()) {
            Logger.wtf(TAG,
                    "spinnerDestroyedWithoutCompleting should only be called after"
                            + " spinnerStarted.");
            return;
        }

        long spinnerDuration = mClock.currentTimeMillis() - mSpinnerStartTime;
        mBasicLoggingApi.onSpinnerDestroyedWithoutCompleting((int) spinnerDuration, mSpinnerType);
        mSpinnerStartTime = SPINNER_INACTIVE;
    }

    @VisibleForTesting
    @SpinnerType
    public int getSpinnerType() {
        return mSpinnerType;
    }

    public boolean isSpinnerActive() {
        return mSpinnerStartTime != SPINNER_INACTIVE;
    }
}
