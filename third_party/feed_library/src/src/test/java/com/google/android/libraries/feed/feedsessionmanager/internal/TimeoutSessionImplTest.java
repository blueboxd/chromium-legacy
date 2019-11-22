// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedsessionmanager.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;

import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.internal.common.testing.InternalProtocolBuilder;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import com.google.android.libraries.feed.common.concurrent.testing.FakeTaskQueue;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.feedsessionmanager.internal.testing.AbstractSessionImplTest;
import com.google.android.libraries.feed.testing.modelprovider.FakeModelChild;
import com.google.android.libraries.feed.testing.modelprovider.FakeModelCursor;
import com.google.android.libraries.feed.testing.modelprovider.FakeModelFeature;
import com.google.android.libraries.feed.testing.modelprovider.FakeModelMutation;
import com.google.android.libraries.feed.testing.modelprovider.FakeModelToken;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;
import com.google.search.now.feed.client.StreamDataProto.UiContext;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/** Tests of the {@link TimeoutSessionImpl} class. */
@RunWith(RobolectricTestRunner.class)
public class TimeoutSessionImplTest extends AbstractSessionImplTest {
    private final FakeClock fakeClock = new FakeClock();
    private final FakeThreadUtils fakeThreadUtils = FakeThreadUtils.withThreadChecks();
    private final TimingUtils timingUtils = new TimingUtils();

    @Mock
    private ViewDepthProvider viewDepthProvider;

    @Before
    @Override
    public void setUp() {
        super.setUp();
        fakeThreadUtils.enforceMainThread(false);
    }

    @Test
    public void testInvalidateOnResetHead() {
        TimeoutSessionImpl session = getSessionImpl();
        assertThat(session.invalidateOnResetHead()).isFalse();
    }

    @Test
    public void testClearHead() {
        SessionImpl session = getSessionImpl();
        session.setSessionId(TEST_SESSION_ID);
        session.populateModelProvider(
                new ArrayList<>(), false, true, UiContext.getDefaultInstance());
        int featureCnt = 3;
        int featureId = 5;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder().addClearOperation();
        addFeatures(protocolBuilder, featureCnt, featureId);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        // 1 clear, 3 features
        assertThat(streamStructures).hasSize(4);
        session.updateSession(true, streamStructures, SCHEMA_VERSION, null);
        assertThat(fakeSessionMutation.streamStructures).hasSize(featureCnt);
        FakeModelMutation fakeModelMutation = fakeModelProvider.getLatestModelMutation();
        assertThat(fakeModelMutation.addedChildren).hasSize(featureCnt);
        assertThat(session.getContentInSession()).hasSize(featureCnt);
        assertThat(session.getContentInSession())
                .contains(contentIdGenerators.createFeatureContentId(featureId));
    }

    @Test
    public void testClearHead_onPaginationRequest() {
        SessionImpl session = getSessionImpl();
        session.setSessionId(TEST_SESSION_ID);
        session.populateModelProvider(
                new ArrayList<>(), false, true, UiContext.getDefaultInstance());
        int featureCnt = 3;
        int featureId = 5;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder().addClearOperation();
        addFeatures(protocolBuilder, featureCnt, featureId);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        session.bindModelProvider(fakeModelProvider, viewDepthProvider);
        session.populateModelProvider(
                new ArrayList<>(), false, false, UiContext.getDefaultInstance());
        session.updateSession(true, streamStructures, SCHEMA_VERSION,
                new MutationContext.Builder()
                        .setRequestingSessionId(TEST_SESSION_ID)
                        .setContinuationToken(
                                StreamToken.newBuilder().setContentId("token").build())
                        .build());
        assertThat(fakeModelProvider.isInvalidated()).isTrue();
    }

    @Test
    public void testPopulateModelProviderState() {
        TimeoutSessionImpl session = getSessionImpl();
        assertThat(session.legacyHeadContent).isFalse();
        assertThat(session.viewDepthProvider).isNull();

        session.setSessionId(TEST_SESSION_ID);
        session.bindModelProvider(fakeModelProvider, viewDepthProvider);
        session.populateModelProvider(
                new ArrayList<>(), false, true, UiContext.getDefaultInstance());
        assertThat(session.legacyHeadContent).isTrue();
        assertThat(session.viewDepthProvider).isEqualTo(viewDepthProvider);
    }

    @Test
    public void testCreateRemoveFeature() {
        TimeoutSessionImpl session = getSessionImpl();
        String contentId = "contentId";
        String parentContentId = "parentId";

        StreamStructure streamStructure = session.createRemoveFeature(contentId, parentContentId);
        assertThat(streamStructure.getContentId()).isEqualTo(contentId);
        assertThat(streamStructure.getParentContentId()).isEqualTo(parentContentId);
    }

    @Test
    public void testCreateRemoveFeature_nullParent() {
        TimeoutSessionImpl session = getSessionImpl();
        String contentId = "contentId";

        StreamStructure streamStructure = session.createRemoveFeature(contentId, null);
        assertThat(streamStructure.getContentId()).isEqualTo(contentId);
        assertThat(streamStructure.getParentContentId()).isEmpty();
    }

    @Test
    public void testCaptureRootContent() {
        TimeoutSessionImpl session = getSessionImpl();
        setupModelProviderRoot();

        List<ModelChild> children = session.captureRootContent();
        assertThat(children).isNotNull();
        assertThat(children).hasSize(1);
    }

    @Test
    public void testCaptureRootContent_nullRoot() {
        TimeoutSessionImpl session = getSessionImpl();

        List<ModelChild> children = session.captureRootContent();
        assertThat(children).isEmpty();
    }

    @Test
    public void testRemoveItems() {
        TimeoutSessionImpl session = getSessionImpl();
        List<ModelChild> modelChildren = new ArrayList<>();
        for (int i = 0; i < 10; i++) {
            modelChildren.add(createModelChild(i, ModelChild.Type.FEATURE));
        }
        setupForViewDepthProvider(
                session, contentIdGenerators.createFeatureContentId(6), modelChildren);

        // Items below the lowest child should be removed.
        session.updateSession(
                /* clearHead= */ true, new ArrayList<>(), SCHEMA_VERSION,
                /* mutationContext= */ null);
        FakeModelMutation fakeModelMutation = fakeModelProvider.getLatestModelMutation();
        assertThat(fakeModelMutation.removedChildren).hasSize(3);
        assertThat(fakeModelMutation.removedChildren.get(0).getContentId())
                .isEqualTo(contentIdGenerators.createFeatureContentId(7));
        assertThat(fakeModelMutation.removedChildren.get(1).getContentId())
                .isEqualTo(contentIdGenerators.createFeatureContentId(8));
        assertThat(fakeModelMutation.removedChildren.get(2).getContentId())
                .isEqualTo(contentIdGenerators.createFeatureContentId(9));
    }

    @Test
    public void testRemoveItems_visibleTokenIsRemoved() {
        TimeoutSessionImpl session = getSessionImpl();
        List<ModelChild> modelChildren = new ArrayList<>();
        modelChildren.add(createModelChild(0, ModelChild.Type.TOKEN));
        String tokenContentId = contentIdGenerators.createFeatureContentId(0);
        setupForViewDepthProvider(session, tokenContentId, modelChildren);

        // Because the lowest child is a token it should also be removed.
        session.updateSession(
                /* clearHead= */ true, new ArrayList<>(), SCHEMA_VERSION,
                /* mutationContext= */ null);
        FakeModelMutation fakeModelMutation = fakeModelProvider.getLatestModelMutation();
        assertThat(fakeModelMutation.removedChildren).hasSize(1);
        assertThat(fakeModelMutation.removedChildren.get(0).getContentId())
                .isEqualTo(tokenContentId);
    }

    @Test
    public void testRemoveItems_nullLowestChildShouldRemoveToken() {
        TimeoutSessionImpl session = getSessionImpl();
        List<ModelChild> modelChildren = new ArrayList<>();
        for (int i = 0; i < 10; i++) {
            modelChildren.add(createModelChild(i, ModelChild.Type.FEATURE));
        }
        modelChildren.add(createModelChild(11, ModelChild.Type.TOKEN));
        setupForViewDepthProvider(session, /* lowestChild= */ null, modelChildren);

        // The token should be removed even when there is a null lowest child.
        session.updateSession(
                /* clearHead= */ true, new ArrayList<>(), SCHEMA_VERSION,
                /* mutationContext= */ null);
        FakeModelMutation fakeModelMutation = fakeModelProvider.getLatestModelMutation();
        assertThat(fakeModelMutation.removedChildren).hasSize(1);
        assertThat(fakeModelMutation.removedChildren.get(0).getContentId())
                .isEqualTo(contentIdGenerators.createFeatureContentId(11));
    }

    private ModelChild createModelChild(long id, @ModelChild.Type int type) {
        String contentId = contentIdGenerators.createFeatureContentId(id);
        switch (type) {
            case ModelChild.Type.FEATURE:
                return FakeModelChild.newBuilder()
                        .setContentId(contentId)
                        .setModelFeature(FakeModelFeature.newBuilder().build())
                        .build();
            case ModelChild.Type.TOKEN:
                return FakeModelChild.newBuilder()
                        .setContentId(contentId)
                        .setModelToken(FakeModelToken.newBuilder().build())
                        .build();
            default:
                return FakeModelChild.newBuilder().setContentId(contentId).build();
        }
    }

    private void setupModelProviderRoot() {
        fakeModelProvider.triggerOnSessionStart(
                FakeModelFeature.newBuilder()
                        .setModelCursor(FakeModelCursor.newBuilder()
                                                .addChild(FakeModelChild.newBuilder().build())
                                                .build())
                        .build());
    }

    @Override
    protected TimeoutSessionImpl getSessionImpl() {
        FakeTaskQueue fakeTaskQueue = new FakeTaskQueue(fakeClock, fakeThreadUtils);
        fakeTaskQueue.initialize(() -> {});
        TimeoutSessionImpl session =
                new TimeoutSessionImpl(store, false, fakeTaskQueue, timingUtils, fakeThreadUtils);
        session.bindModelProvider(fakeModelProvider, /* viewDepthProvider= */ null);
        return session;
    }

    private void setupForViewDepthProvider(
            TimeoutSessionImpl session, String lowestChild, List<ModelChild> modelChildren) {
        when(viewDepthProvider.getChildViewDepth()).thenReturn(lowestChild);
        fakeModelProvider.triggerOnSessionStart(
                FakeModelFeature.newBuilder()
                        .setModelCursor(
                                FakeModelCursor.newBuilder().addChildren(modelChildren).build())
                        .build());
        session.setSessionId(TEST_SESSION_ID);
        session.bindModelProvider(fakeModelProvider, viewDepthProvider);
        session.populateModelProvider(new ArrayList<>(),
                /* cachedBindings= */ false,
                /* legacyHeadContent= */ true, UiContext.getDefaultInstance());
    }
}
