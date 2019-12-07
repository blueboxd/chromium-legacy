// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.requestmanager;

import androidx.annotation.Nullable;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.host.logging.RequestReason;
import org.chromium.chrome.browser.feed.library.api.host.logging.Task;
import org.chromium.chrome.browser.feed.library.api.internal.common.Model;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.api.internal.requestmanager.FeedRequestManager;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue.TaskType;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.components.feed.core.proto.wire.ConsistencyTokenProto.ConsistencyToken;
import org.chromium.components.feed.core.proto.wire.ResponseProto.Response;

import java.util.ArrayDeque;
import java.util.Queue;

/**
 * Fake implementation of a {@link FeedRequestManager}. This acts a Queue of responses which will be
 * sent through the {@link ProtocolAdapter} to create the {@code List<StreamDataOperation>} which is
 * then returned as a {@link Result} to the {@link Consumer}.
 */
public class FakeFeedRequestManager implements FeedRequestManager {
    private final FakeThreadUtils mFakeThreadUtils;
    private final MainThreadRunner mMainThreadRunner;
    private final ProtocolAdapter mProtocolAdapter;
    private final Queue<ResponseWithDelay> mResponses = new ArrayDeque<>();
    private final TaskQueue mTaskQueue;
    @Nullable
    private StreamToken mLatestStreamToken;
    @RequestReason
    private int mLatestRequestReason = RequestReason.UNKNOWN;

    public FakeFeedRequestManager(FakeThreadUtils fakeThreadUtils,
            MainThreadRunner mainThreadRunner, ProtocolAdapter protocolAdapter,
            TaskQueue taskQueue) {
        this.mFakeThreadUtils = fakeThreadUtils;
        this.mMainThreadRunner = mainThreadRunner;
        this.mProtocolAdapter = protocolAdapter;
        this.mTaskQueue = taskQueue;
    }

    // TODO: queue responses for action uploads
    /** Adds a Response to the queue. */
    public FakeFeedRequestManager queueResponse(Response response) {
        return queueResponse(response, /* delayMs= */ 0);
    }

    /** Adds a Response to the queue with a delay. */
    public FakeFeedRequestManager queueResponse(Response response, long delayMs) {
        mResponses.add(new ResponseWithDelay(response, delayMs));
        return this;
    }

    /** Adds an error to the queue. */
    public FakeFeedRequestManager queueError() {
        return queueError(/* delayMs= */ 0);
    }

    /** Adds an error to the queue with a delay. */
    public FakeFeedRequestManager queueError(long delayMs) {
        mResponses.add(new ResponseWithDelay(delayMs));
        return this;
    }

    @Override
    public void loadMore(
            StreamToken streamToken, ConsistencyToken token, Consumer<Result<Model>> consumer) {
        mFakeThreadUtils.checkNotMainThread();
        mLatestStreamToken = streamToken;
        handleResponseWithDelay(mResponses.remove(), consumer);
    }

    @Override
    public void triggerRefresh(@RequestReason int reason, Consumer<Result<Model>> consumer) {
        triggerRefresh(reason, ConsistencyToken.getDefaultInstance(), consumer);
    }

    @Override
    public void triggerRefresh(
            @RequestReason int reason, ConsistencyToken token, Consumer<Result<Model>> consumer) {
        mLatestRequestReason = reason;
        ResponseWithDelay responseWithDelay = mResponses.remove();
        mTaskQueue.execute(Task.UNKNOWN, TaskType.HEAD_INVALIDATE,
                () -> { handleResponseWithDelay(responseWithDelay, consumer); });
    }

    private void handleResponseWithDelay(
            ResponseWithDelay responseWithDelay, Consumer<Result<Model>> consumer) {
        if (responseWithDelay.mDelayMs > 0) {
            mMainThreadRunner.executeWithDelay("FakeFeedRequestManager#consumer", () -> {
                invokeConsumer(responseWithDelay, consumer);
            }, responseWithDelay.mDelayMs);
        } else {
            invokeConsumer(responseWithDelay, consumer);
        }
    }

    private void invokeConsumer(
            ResponseWithDelay responseWithDelay, Consumer<Result<Model>> consumer) {
        boolean policy = mFakeThreadUtils.enforceMainThread(true);
        if (responseWithDelay.mIsError) {
            consumer.accept(Result.failure());
        } else {
            consumer.accept(mProtocolAdapter.createModel(responseWithDelay.mResponse));
        }
        mFakeThreadUtils.enforceMainThread(policy);
    }

    /** Returns the latest {@link StreamToken} passed in to the {@link FeedRequestManager}. */
    @Nullable
    public StreamToken getLatestStreamToken() {
        return mLatestStreamToken;
    }

    /**
     * Returns the latest {@link RequestReason} passed in to the {@link FeedRequestManager}. Returns
     * {@literal RequestReason.UNKNOWN} if the request manager has not been invoked.
     */
    @RequestReason
    public int getLatestRequestReason() {
        return mLatestRequestReason;
    }

    private static final class ResponseWithDelay {
        private final Response mResponse;
        private final boolean mIsError;
        private final long mDelayMs;

        private ResponseWithDelay(Response response, long delayMs) {
            this.mResponse = response;
            this.mDelayMs = delayMs;
            mIsError = false;
        }

        private ResponseWithDelay(long delayMs) {
            this.mResponse = Response.getDefaultInstance();
            this.mDelayMs = delayMs;
            mIsError = true;
        }
    }
}
