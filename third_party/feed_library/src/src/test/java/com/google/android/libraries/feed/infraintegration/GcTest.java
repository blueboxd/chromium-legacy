// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.host.logging.RequestReason;
import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi.RequestBehavior;
import com.google.android.libraries.feed.api.internal.common.PayloadWithId;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.testing.InfraIntegrationScope;
import com.google.android.libraries.feed.common.testing.ResponseBuilder;
import com.google.android.libraries.feed.testing.host.scheduler.FakeSchedulerApi;
import com.google.search.now.feed.client.StreamDataProto.StreamSharedState;
import com.google.search.now.feed.client.StreamDataProto.UiContext;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;
import com.google.search.now.wire.feed.ResponseProto.Response;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import java.time.Duration;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Tests that assert the behavior of garbage collection. */
@RunWith(RobolectricTestRunner.class)
public final class GcTest {
    private static final ContentId PIET_SHARED_STATE_1 =
            ContentId.newBuilder()
                    .setContentDomain("piet-shared-state")
                    .setId(1)
                    .setTable("feature")
                    .build();
    private static final ContentId PIET_SHARED_STATE_2 =
            ContentId.newBuilder()
                    .setContentDomain("piet-shared-state")
                    .setId(2)
                    .setTable("feature")
                    .build();
    private static final ContentId[] REQUEST_1 = new ContentId[] {
            ResponseBuilder.createFeatureContentId(1), ResponseBuilder.createFeatureContentId(2)};
    private static final ContentId[] REQUEST_2 = new ContentId[] {
            ResponseBuilder.createFeatureContentId(3), ResponseBuilder.createFeatureContentId(4)};
    private static final long LIFETIME_MS = Duration.ofHours(1).toMillis();
    private static final long TIMEOUT_MS = Duration.ofSeconds(5).toMillis();

    private final Configuration configuration =
            new Configuration.Builder().put(ConfigKey.SESSION_LIFETIME_MS, LIFETIME_MS).build();
    private final InfraIntegrationScope scope = new InfraIntegrationScope.Builder()
                                                        .setConfiguration(configuration)
                                                        .withTimeoutSessionConfiguration(TIMEOUT_MS)
                                                        .build();

    @Test
    public void testGc_contentInLiveSessionRetained() {
        scope.getFakeFeedRequestManager()
                .queueResponse(createResponse(REQUEST_1, PIET_SHARED_STATE_1))
                .triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                        scope.getFeedSessionManager().getUpdateConsumer(
                                MutationContext.EMPTY_CONTEXT));

        // Create a new session based on this request.
        scope.getModelProviderFactory()
                .createNew(/* viewDepthProvider= */ null, UiContext.getDefaultInstance())
                .detachModelProvider();
        assertPayloads(REQUEST_1, scope, /* shouldExist= */ true);

        // Populate HEAD with new data.
        scope.getFakeFeedRequestManager()
                .queueResponse(createResponse(REQUEST_2, PIET_SHARED_STATE_2))
                .triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                        scope.getFeedSessionManager().getUpdateConsumer(
                                MutationContext.EMPTY_CONTEXT));

        // Advance the clock without expiring the first session.
        scope.getFakeClock().advance(LIFETIME_MS / 2);
        InfraIntegrationScope secondScope = scope.clone();
        assertPayloads(REQUEST_1, secondScope, /* shouldExist= */ true);
        assertSharedStates(
                new ContentId[] {PIET_SHARED_STATE_1}, secondScope, /* shouldExist= */ true);
        assertPayloads(REQUEST_2, secondScope, /* shouldExist= */ true);
        assertSharedStates(
                new ContentId[] {PIET_SHARED_STATE_2}, secondScope, /* shouldExist= */ true);
    }

    @Test
    public void testGc_contentInExpiredSessionDeleted() {
        scope.getFakeFeedRequestManager()
                .queueResponse(createResponse(REQUEST_1, PIET_SHARED_STATE_1))
                .triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                        scope.getFeedSessionManager().getUpdateConsumer(
                                MutationContext.EMPTY_CONTEXT));

        // Create a new session based on this request.
        scope.getModelProviderFactory()
                .createNew(/* viewDepthProvider= */ null, UiContext.getDefaultInstance())
                .detachModelProvider();
        assertPayloads(REQUEST_1, scope, /* shouldExist= */ true);

        // Populate HEAD with new data.
        scope.getFakeFeedRequestManager()
                .queueResponse(createResponse(REQUEST_2, PIET_SHARED_STATE_2))
                .triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                        scope.getFeedSessionManager().getUpdateConsumer(
                                MutationContext.EMPTY_CONTEXT));

        // Advance the clock to expire the first session, create a new scope that will run
        // initialization and delete content from the expired session.
        scope.getFakeClock().advance(LIFETIME_MS + 1L);
        InfraIntegrationScope secondScope = scope.clone();
        assertPayloads(REQUEST_1, secondScope, /* shouldExist= */ false);
        assertSharedStates(
                new ContentId[] {PIET_SHARED_STATE_1}, secondScope, /* shouldExist= */ false);
        assertPayloads(REQUEST_2, secondScope, /* shouldExist= */ true);
        assertSharedStates(
                new ContentId[] {PIET_SHARED_STATE_2}, secondScope, /* shouldExist= */ true);
    }

    @Test
    public void testGc_contentBranchedMidInitializationRetained() {
        InfraIntegrationScope scope =
                new InfraIntegrationScope.Builder()
                        .setConfiguration(configuration)
                        .setSchedulerApi(
                                new FakeSchedulerApi(FakeThreadUtils.withoutThreadChecks())
                                        .setRequestBehavior(RequestBehavior.REQUEST_WITH_CONTENT))
                        .withQueuingTasks()
                        .withTimeoutSessionConfiguration(TIMEOUT_MS)
                        .build();

        // Populate HEAD with REQUEST_1.
        scope.getFakeFeedRequestManager()
                .queueResponse(createResponse(REQUEST_1, PIET_SHARED_STATE_1))
                .triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                        scope.getFeedSessionManager().getUpdateConsumer(
                                MutationContext.EMPTY_CONTEXT));
        scope.getFakeDirectExecutor().runAllTasks();

        // Make a new scope and enqueue a request to be sent on the next new session. GC should run
        // after the new session is created and branched off a HEAD containing REQUEST_1.
        InfraIntegrationScope secondScope = scope.clone();
        secondScope.getFakeFeedRequestManager().queueResponse(
                createResponse(REQUEST_2, PIET_SHARED_STATE_2));
        secondScope.getModelProviderFactory().createNew(
                /* viewDepthProvider= */ null, UiContext.getDefaultInstance());
        secondScope.getFakeDirectExecutor().runAllTasks();
        assertThat(secondScope.getFakeDirectExecutor().hasTasks()).isFalse();
        assertPayloads(REQUEST_1, secondScope, /* shouldExist= */ true);
        assertPayloads(REQUEST_2, secondScope, /* shouldExist= */ true);
    }

    private static void assertPayloads(
            ContentId[] contentIds, InfraIntegrationScope scope, boolean shouldExist) {
        scope.getFakeThreadUtils().enforceMainThread(false);
        for (ContentId contentId : contentIds) {
            List<PayloadWithId> payloads =
                    scope.getStore()
                            .getPayloads(Arrays.asList(new String[] {
                                    scope.getProtocolAdapter().getStreamContentId(contentId)}))
                            .getValue();
            if (shouldExist) {
                assertThat(payloads).hasSize(1);
            } else {
                assertThat(payloads).isEmpty();
            }
        }
    }

    private static void assertSharedStates(
            ContentId[] contentIds, InfraIntegrationScope scope, boolean shouldExist) {
        scope.getFakeThreadUtils().enforceMainThread(false);
        List<String> sharedStateContentIds = new ArrayList<>();
        for (StreamSharedState streamSharedState : scope.getStore().getSharedStates().getValue()) {
            sharedStateContentIds.add(streamSharedState.getContentId());
        }
        List<String> expectedContentIds = new ArrayList<>(contentIds.length);
        for (ContentId contentId : contentIds) {
            expectedContentIds.add(scope.getProtocolAdapter().getStreamContentId(contentId));
        }

        if (shouldExist) {
            assertThat(sharedStateContentIds).containsAtLeastElementsIn(expectedContentIds);
        } else {
            assertThat(sharedStateContentIds).containsNoneIn(expectedContentIds);
        }
    }

    private static Response createResponse(ContentId[] cards, ContentId pietSharedStateContentId) {
        return ResponseBuilder.builder()
                .addClearOperation()
                .addPietSharedState(pietSharedStateContentId)
                .addRootFeature()
                .addCardsToRoot(cards)
                .build();
    }
}
