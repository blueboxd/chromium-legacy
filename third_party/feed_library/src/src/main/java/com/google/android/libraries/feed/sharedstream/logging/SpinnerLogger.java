// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.logging;

import android.support.annotation.VisibleForTesting;

import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.SpinnerType;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.common.time.Clock;

/** Logs events for displaying spinner in the Stream. */
public class SpinnerLogger {
    private static final String TAG = "SpinnerLogger";
    private static final int SPINNER_INACTIVE = -1;
    private final BasicLoggingApi basicLoggingApi;
    private final Clock clock;

    private long spinnerStartTime = SPINNER_INACTIVE;
    @SpinnerType
    private int spinnerType;

    public SpinnerLogger(BasicLoggingApi basicLoggingApi, Clock clock) {
        this.basicLoggingApi = basicLoggingApi;
        this.clock = clock;
    }

    public void spinnerStarted(@SpinnerType int spinnerType) {
        if (isSpinnerActive()) {
            Logger.wtf(TAG,
                    "spinnerStarted should not be called if another spinner is "
                            + "currently being tracked.");
            return;
        }

        basicLoggingApi.onSpinnerStarted(spinnerType);
        spinnerStartTime = clock.currentTimeMillis();
        this.spinnerType = spinnerType;
    }

    public void spinnerFinished() {
        if (!isSpinnerActive()) {
            Logger.wtf(TAG, "spinnerFinished should only be called after spinnerStarted.");
            return;
        }

        long spinnerDuration = clock.currentTimeMillis() - spinnerStartTime;
        basicLoggingApi.onSpinnerFinished((int) spinnerDuration, spinnerType);
        spinnerStartTime = SPINNER_INACTIVE;
    }

    public void spinnerDestroyedWithoutCompleting() {
        if (!isSpinnerActive()) {
            Logger.wtf(TAG,
                    "spinnerDestroyedWithoutCompleting should only be called after"
                            + " spinnerStarted.");
            return;
        }

        long spinnerDuration = clock.currentTimeMillis() - spinnerStartTime;
        basicLoggingApi.onSpinnerDestroyedWithoutCompleting((int) spinnerDuration, spinnerType);
        spinnerStartTime = SPINNER_INACTIVE;
    }

    @VisibleForTesting
    @SpinnerType
    public int getSpinnerType() {
        return spinnerType;
    }

    public boolean isSpinnerActive() {
        return spinnerStartTime != SPINNER_INACTIVE;
    }
}
