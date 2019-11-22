// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common.testing;

import static com.google.common.truth.Truth.assertThat;

import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.host.logging.RequestReason;
import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi.RequestBehavior;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.testing.host.scheduler.FakeSchedulerApi;
import com.google.android.libraries.feed.testing.modelprovider.FakeViewDepthProvider;
import com.google.search.now.feed.client.StreamDataProto.UiContext;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;

import java.util.List;

/** Utilities for tests that are creating new sessions. */
public final class SessionTestUtils {
    public static final long TIMEOUT_MS = 50L;

    private static final ContentId[] REQUEST_1 = new ContentId[] {
            ResponseBuilder.createFeatureContentId(1), ResponseBuilder.createFeatureContentId(2)};
    private static final ContentId[] REQUEST_2 = new ContentId[] {
            ResponseBuilder.createFeatureContentId(3), ResponseBuilder.createFeatureContentId(4)};
    private static final long DELAY_MS = 100L;
    private static final long SHORT_DELAY_MS = 25L;

    private final InfraIntegrationScope scope;
    private long currentDelay;

    public SessionTestUtils(@RequestBehavior int requestBehavior) {
        scope = new InfraIntegrationScope.Builder()
                        .setSchedulerApi(new FakeSchedulerApi(FakeThreadUtils.withoutThreadChecks())
                                                 .setRequestBehavior(requestBehavior))
                        .withTimeoutSessionConfiguration(TIMEOUT_MS)
                        .build();
        currentDelay = DELAY_MS;
    }

    /** Shortens the delay on requests so that they complete before timeout. */
    public SessionTestUtils requestBeforeTimeout() {
        currentDelay = SHORT_DELAY_MS;
        return this;
    }

    /** Returns the {@link InfraIntegrationScope}. */
    public InfraIntegrationScope getScope() {
        return scope;
    }

    private ViewDepthProvider createViewDepthProvider() {
        return new FakeViewDepthProvider().setChildViewDepth(
                scope.getProtocolAdapter().getStreamContentId(REQUEST_1[1]));
    }

    /** Creates a new session and returns the {@link ModelProvider}. */
    public ModelProvider createNewSession() {
        return scope.getModelProviderFactory().createNew(
                createViewDepthProvider(), UiContext.getDefaultInstance());
    }

    /** Populates HEAD with data. */
    public void populateHead() {
        scope.getFakeFeedRequestManager()
                .queueResponse(ResponseBuilder.forClearAllWithCards(REQUEST_1).build())
                .triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                        scope.getFeedSessionManager().getUpdateConsumer(
                                MutationContext.EMPTY_CONTEXT));
    }

    /** Enqueues an error response and returns the delay in milliseconds. */
    public long queueError() {
        scope.getFakeFeedRequestManager().queueError(currentDelay);
        return currentDelay;
    }

    /** Enqueues a response and returns the delay in milliseconds. */
    public long queueRequest() {
        scope.getFakeFeedRequestManager().queueResponse(
                ResponseBuilder.forClearAllWithCards(REQUEST_2).build(), currentDelay);
        return currentDelay;
    }

    /** Enqueues an error response, triggers the refresh, and returns the delay in milliseconds. */
    public long startOutstandingRequestWithError() {
        long delayMs = queueError();
        scope.getFakeFeedRequestManager().triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                scope.getFeedSessionManager().getUpdateConsumer(MutationContext.EMPTY_CONTEXT));
        return delayMs;
    }

    /** Enqueues a response, triggers the refresh, and returns the delay in milliseconds. */
    public long startOutstandingRequest() {
        long delayMs = queueRequest();
        scope.getFakeFeedRequestManager().triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                scope.getFeedSessionManager().getUpdateConsumer(MutationContext.EMPTY_CONTEXT));
        return delayMs;
    }

    /** Asserts that the children are from HEAD. */
    public void assertHeadContent(List<ModelChild> rootChildren) {
        assertThat(rootChildren).hasSize(REQUEST_1.length);
        for (int i = 0; i < rootChildren.size(); i++) {
            assertThat(rootChildren.get(i).getContentId())
                    .isEqualTo(scope.getProtocolAdapter().getStreamContentId(REQUEST_1[i]));
        }
    }

    /** Asserts that the children are from the outstanding request. */
    public void assertNewContent(List<ModelChild> rootChildren) {
        assertThat(rootChildren).hasSize(REQUEST_2.length);
        for (int i = 0; i < rootChildren.size(); i++) {
            assertThat(rootChildren.get(i).getContentId())
                    .isEqualTo(scope.getProtocolAdapter().getStreamContentId(REQUEST_2[i]));
        }
    }

    /** Asserts that the children are the union of HEAD and the outstanding request. */
    public void assertAppendedContent(List<ModelChild> rootChildren) {
        assertThat(rootChildren).hasSize(REQUEST_1.length + REQUEST_2.length);
        assertHeadContent(rootChildren.subList(0, REQUEST_1.length));
        assertNewContent(
                rootChildren.subList(REQUEST_1.length, REQUEST_1.length + REQUEST_2.length));
    }

    /**
     * Asserts that no runnables are pending in the {@link TaskQueue} or {@link MainThreadRunner}.
     */
    public void assertWorkComplete() {
        assertThat(scope.getTaskQueue().hasBacklog()).isFalse();
        assertThat(scope.getFakeMainThreadRunner().hasPendingTasks()).isFalse();
    }
}
