// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.host.logging.RequestReason;
import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi.RequestBehavior;
import org.chromium.chrome.browser.feed.library.api.internal.common.PayloadWithId;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.testing.InfraIntegrationScope;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder;
import org.chromium.chrome.browser.feed.library.testing.host.scheduler.FakeSchedulerApi;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSharedState;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.components.feed.core.proto.wire.ResponseProto.Response;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.time.Duration;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Tests that assert the behavior of garbage collection. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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

    private final Configuration mConfiguration =
            new Configuration.Builder().put(ConfigKey.SESSION_LIFETIME_MS, LIFETIME_MS).build();
    private final InfraIntegrationScope mScope =
            new InfraIntegrationScope.Builder()
                    .setConfiguration(mConfiguration)
                    .withTimeoutSessionConfiguration(TIMEOUT_MS)
                    .build();

    @Test
    public void testGc_contentInLiveSessionRetained() {
        mScope.getFakeFeedRequestManager()
                .queueResponse(createResponse(REQUEST_1, PIET_SHARED_STATE_1))
                .triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                        mScope.getFeedSessionManager().getUpdateConsumer(
                                MutationContext.EMPTY_CONTEXT));

        // Create a new session based on this request.
        mScope.getModelProviderFactory()
                .createNew(/* viewDepthProvider= */ null, UiContext.getDefaultInstance())
                .detachModelProvider();
        assertPayloads(REQUEST_1, mScope, /* shouldExist= */ true);

        // Populate HEAD with new data.
        mScope.getFakeFeedRequestManager()
                .queueResponse(createResponse(REQUEST_2, PIET_SHARED_STATE_2))
                .triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                        mScope.getFeedSessionManager().getUpdateConsumer(
                                MutationContext.EMPTY_CONTEXT));

        // Advance the clock without expiring the first session.
        mScope.getFakeClock().advance(LIFETIME_MS / 2);
        InfraIntegrationScope secondScope = mScope.clone();
        assertPayloads(REQUEST_1, secondScope, /* shouldExist= */ true);
        assertSharedStates(
                new ContentId[] {PIET_SHARED_STATE_1}, secondScope, /* shouldExist= */ true);
        assertPayloads(REQUEST_2, secondScope, /* shouldExist= */ true);
        assertSharedStates(
                new ContentId[] {PIET_SHARED_STATE_2}, secondScope, /* shouldExist= */ true);
    }

    @Test
    public void testGc_contentInExpiredSessionDeleted() {
        mScope.getFakeFeedRequestManager()
                .queueResponse(createResponse(REQUEST_1, PIET_SHARED_STATE_1))
                .triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                        mScope.getFeedSessionManager().getUpdateConsumer(
                                MutationContext.EMPTY_CONTEXT));

        // Create a new session based on this request.
        mScope.getModelProviderFactory()
                .createNew(/* viewDepthProvider= */ null, UiContext.getDefaultInstance())
                .detachModelProvider();
        assertPayloads(REQUEST_1, mScope, /* shouldExist= */ true);

        // Populate HEAD with new data.
        mScope.getFakeFeedRequestManager()
                .queueResponse(createResponse(REQUEST_2, PIET_SHARED_STATE_2))
                .triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                        mScope.getFeedSessionManager().getUpdateConsumer(
                                MutationContext.EMPTY_CONTEXT));

        // Advance the clock to expire the first session, create a new scope that will run
        // initialization and delete content from the expired session.
        mScope.getFakeClock().advance(LIFETIME_MS + 1L);
        InfraIntegrationScope secondScope = mScope.clone();
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
                        .setConfiguration(mConfiguration)
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
