// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.hostimpl.scheduler;

import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.SimpleSettableFuture;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;

import java.util.concurrent.ExecutionException;

/** Wrapper class which will call the {@link SchedulerApi} on the main thread. */
public final class SchedulerApiWrapper implements SchedulerApi {
    private static final String TAG = "SchedulerApiWrapper";
    private final SchedulerApi mDirectSchedulerApi;
    private final ThreadUtils mThreadUtils;
    private final MainThreadRunner mMainThreadRunner;

    public SchedulerApiWrapper(SchedulerApi directSchedulerApi, ThreadUtils threadUtils,
            MainThreadRunner mainThreadRunner) {
        Logger.i(TAG, "Create SchedulerApiMainThreadWrapper");
        this.mDirectSchedulerApi = directSchedulerApi;
        this.mThreadUtils = threadUtils;
        this.mMainThreadRunner = mainThreadRunner;
    }

    @RequestBehavior
    @Override
    public int shouldSessionRequestData(SessionState sessionState) {
        if (mThreadUtils.isMainThread()) {
            return mDirectSchedulerApi.shouldSessionRequestData(sessionState);
        }
        SimpleSettableFuture<Integer> future = new SimpleSettableFuture<>();
        mMainThreadRunner.execute(TAG + " shouldSessionRequestData",
                () -> future.put(mDirectSchedulerApi.shouldSessionRequestData(sessionState)));
        try {
            return future.get();
        } catch (InterruptedException | ExecutionException e) {
            Logger.e(TAG, e, null);
        }
        return RequestBehavior.NO_REQUEST_WITH_WAIT;
    }

    @Override
    public void onReceiveNewContent(long contentCreationDateTimeMs) {
        if (mThreadUtils.isMainThread()) {
            mDirectSchedulerApi.onReceiveNewContent(contentCreationDateTimeMs);
            return;
        }
        mMainThreadRunner.execute(TAG + " onReceiveNewContent",
                () -> { mDirectSchedulerApi.onReceiveNewContent(contentCreationDateTimeMs); });
    }

    @Override
    public void onRequestError(int networkResponseCode) {
        if (mThreadUtils.isMainThread()) {
            mDirectSchedulerApi.onRequestError(networkResponseCode);
            return;
        }
        mMainThreadRunner.execute(TAG + " onRequestError",
                () -> { mDirectSchedulerApi.onRequestError(networkResponseCode); });
    }
}
