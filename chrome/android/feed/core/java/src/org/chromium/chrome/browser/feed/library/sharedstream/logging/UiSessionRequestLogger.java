// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.logging;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.SessionEvent;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelError;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderObserver;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;

/** Logs the initial event after sessions have been requested by the UI. */
public class UiSessionRequestLogger implements ModelProviderObserver {
    private static final String TAG = "UiSessionRequestLogger";
    private final Clock mClock;
    private final BasicLoggingApi mBasicLoggingApi;
    private int mSessionCount;
    @Nullable
    private SessionRequestState mSessionRequestState;

    public UiSessionRequestLogger(Clock clock, BasicLoggingApi basicLoggingApi) {
        this.mClock = clock;
        this.mBasicLoggingApi = basicLoggingApi;
    }

    /** Should be called whenever the UI requests a new session from the {@link ModelProvider}. */
    public void onSessionRequested(ModelProvider modelProvider) {
        if (mSessionRequestState != null) {
            mSessionRequestState.mModelProvider.unregisterObserver(this);
        }

        this.mSessionRequestState = new SessionRequestState(modelProvider, mClock);
        modelProvider.registerObserver(this);
    }

    @Override
    public void onSessionStart(UiContext uiContext) {
        if (mSessionRequestState == null) {
            Logger.wtf(TAG, "onSessionStart() called without SessionRequestState.");
            return;
        }

        SessionRequestState localSessionRequestState = mSessionRequestState;

        mBasicLoggingApi.onInitialSessionEvent(
                SessionEvent.STARTED, localSessionRequestState.getElapsedTime(), ++mSessionCount);

        localSessionRequestState.getModelProvider().unregisterObserver(this);
        mSessionRequestState = null;
    }

    @Override
    public void onSessionFinished(UiContext uiContext) {
        if (mSessionRequestState == null) {
            Logger.wtf(TAG, "onSessionFinished() called without SessionRequestState.");
            return;
        }

        SessionRequestState localSessionRequestState = mSessionRequestState;

        mBasicLoggingApi.onInitialSessionEvent(SessionEvent.FINISHED_IMMEDIATELY,
                localSessionRequestState.getElapsedTime(), ++mSessionCount);

        localSessionRequestState.getModelProvider().unregisterObserver(this);
        mSessionRequestState = null;
    }

    @Override
    public void onError(ModelError modelError) {
        if (mSessionRequestState == null) {
            Logger.wtf(TAG, "onError() called without SessionRequestState.");
            return;
        }

        SessionRequestState localSessionRequestState = mSessionRequestState;

        mBasicLoggingApi.onInitialSessionEvent(
                SessionEvent.ERROR, localSessionRequestState.getElapsedTime(), ++mSessionCount);

        localSessionRequestState.getModelProvider().unregisterObserver(this);
        mSessionRequestState = null;
    }

    /** Should be called whenever the UI is destroyed, and will log the event if necessary. */
    public void onDestroy() {
        if (mSessionRequestState == null) {
            // We don't wtf here as to allow onDestroy() to be called regardless of the internal
            // state.
            return;
        }

        SessionRequestState localSessionRequestState = mSessionRequestState;

        mBasicLoggingApi.onInitialSessionEvent(SessionEvent.USER_ABANDONED,
                localSessionRequestState.getElapsedTime(), ++mSessionCount);

        localSessionRequestState.getModelProvider().unregisterObserver(this);
        mSessionRequestState = null;
    }

    /** Encapsulates the state of whether a session has been requested and when it was requested. */
    private static class SessionRequestState {
        private final long mSessionRequestTime;
        private final ModelProvider mModelProvider;
        private final Clock mClock;

        private SessionRequestState(ModelProvider modelProvider, Clock clock) {
            this.mSessionRequestTime = clock.elapsedRealtime();
            this.mModelProvider = modelProvider;
            this.mClock = clock;
        }

        int getElapsedTime() {
            return (int) (mClock.elapsedRealtime() - mSessionRequestTime);
        }

        ModelProvider getModelProvider() {
            return mModelProvider;
        }
    }
}
