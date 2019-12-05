// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedsessionmanager.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.internal.common.testing.InternalProtocolBuilder;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeTaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.feedsessionmanager.internal.testing.AbstractSessionImplTest;
import org.chromium.chrome.browser.feed.library.testing.modelprovider.FakeModelChild;
import org.chromium.chrome.browser.feed.library.testing.modelprovider.FakeModelCursor;
import org.chromium.chrome.browser.feed.library.testing.modelprovider.FakeModelFeature;
import org.chromium.chrome.browser.feed.library.testing.modelprovider.FakeModelMutation;
import org.chromium.chrome.browser.feed.library.testing.modelprovider.FakeModelToken;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/** Tests of the {@link TimeoutSessionImpl} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TimeoutSessionImplTest extends AbstractSessionImplTest {
    private final FakeClock mFakeClock = new FakeClock();
    private final FakeThreadUtils mFakeThreadUtils = FakeThreadUtils.withThreadChecks();
    private final TimingUtils mTimingUtils = new TimingUtils();

    @Mock
    private ViewDepthProvider mViewDepthProvider;

    @Before
    @Override
    public void setUp() {
        super.setUp();
        mFakeThreadUtils.enforceMainThread(false);
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
        assertThat(mFakeSessionMutation.streamStructures).hasSize(featureCnt);
        FakeModelMutation fakeModelMutation = mFakeModelProvider.getLatestModelMutation();
        assertThat(fakeModelMutation.mAddedChildren).hasSize(featureCnt);
        assertThat(session.getContentInSession()).hasSize(featureCnt);
        assertThat(session.getContentInSession())
                .contains(mContentIdGenerators.createFeatureContentId(featureId));
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

        session.bindModelProvider(mFakeModelProvider, mViewDepthProvider);
        session.populateModelProvider(
                new ArrayList<>(), false, false, UiContext.getDefaultInstance());
        session.updateSession(true, streamStructures, SCHEMA_VERSION,
                new MutationContext.Builder()
                        .setRequestingSessionId(TEST_SESSION_ID)
                        .setContinuationToken(
                                StreamToken.newBuilder().setContentId("token").build())
                        .build());
        assertThat(mFakeModelProvider.isInvalidated()).isTrue();
    }

    @Test
    public void testPopulateModelProviderState() {
        TimeoutSessionImpl session = getSessionImpl();
        assertThat(session.mLegacyHeadContent).isFalse();
        assertThat(session.mViewDepthProvider).isNull();

        session.setSessionId(TEST_SESSION_ID);
        session.bindModelProvider(mFakeModelProvider, mViewDepthProvider);
        session.populateModelProvider(
                new ArrayList<>(), false, true, UiContext.getDefaultInstance());
        assertThat(session.mLegacyHeadContent).isTrue();
        assertThat(session.mViewDepthProvider).isEqualTo(mViewDepthProvider);
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
                session, mContentIdGenerators.createFeatureContentId(6), modelChildren);

        // Items below the lowest child should be removed.
        session.updateSession(
                /* clearHead= */ true, new ArrayList<>(), SCHEMA_VERSION,
                /* mutationContext= */ null);
        FakeModelMutation fakeModelMutation = mFakeModelProvider.getLatestModelMutation();
        assertThat(fakeModelMutation.mRemovedChildren).hasSize(3);
        assertThat(fakeModelMutation.mRemovedChildren.get(0).getContentId())
                .isEqualTo(mContentIdGenerators.createFeatureContentId(7));
        assertThat(fakeModelMutation.mRemovedChildren.get(1).getContentId())
                .isEqualTo(mContentIdGenerators.createFeatureContentId(8));
        assertThat(fakeModelMutation.mRemovedChildren.get(2).getContentId())
                .isEqualTo(mContentIdGenerators.createFeatureContentId(9));
    }

    @Test
    public void testRemoveItems_visibleTokenIsRemoved() {
        TimeoutSessionImpl session = getSessionImpl();
        List<ModelChild> modelChildren = new ArrayList<>();
        modelChildren.add(createModelChild(0, ModelChild.Type.TOKEN));
        String tokenContentId = mContentIdGenerators.createFeatureContentId(0);
        setupForViewDepthProvider(session, tokenContentId, modelChildren);

        // Because the lowest child is a token it should also be removed.
        session.updateSession(
                /* clearHead= */ true, new ArrayList<>(), SCHEMA_VERSION,
                /* mutationContext= */ null);
        FakeModelMutation fakeModelMutation = mFakeModelProvider.getLatestModelMutation();
        assertThat(fakeModelMutation.mRemovedChildren).hasSize(1);
        assertThat(fakeModelMutation.mRemovedChildren.get(0).getContentId())
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
        FakeModelMutation fakeModelMutation = mFakeModelProvider.getLatestModelMutation();
        assertThat(fakeModelMutation.mRemovedChildren).hasSize(1);
        assertThat(fakeModelMutation.mRemovedChildren.get(0).getContentId())
                .isEqualTo(mContentIdGenerators.createFeatureContentId(11));
    }

    private ModelChild createModelChild(long id, @ModelChild.Type int type) {
        String contentId = mContentIdGenerators.createFeatureContentId(id);
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
        mFakeModelProvider.triggerOnSessionStart(
                FakeModelFeature.newBuilder()
                        .setModelCursor(FakeModelCursor.newBuilder()
                                                .addChild(FakeModelChild.newBuilder().build())
                                                .build())
                        .build());
    }

    @Override
    protected TimeoutSessionImpl getSessionImpl() {
        FakeTaskQueue fakeTaskQueue = new FakeTaskQueue(mFakeClock, mFakeThreadUtils);
        fakeTaskQueue.initialize(() -> {});
        TimeoutSessionImpl session = new TimeoutSessionImpl(
                mStore, false, fakeTaskQueue, mTimingUtils, mFakeThreadUtils);
        session.bindModelProvider(mFakeModelProvider, /* viewDepthProvider= */ null);
        return session;
    }

    private void setupForViewDepthProvider(
            TimeoutSessionImpl session, String lowestChild, List<ModelChild> modelChildren) {
        when(mViewDepthProvider.getChildViewDepth()).thenReturn(lowestChild);
        mFakeModelProvider.triggerOnSessionStart(
                FakeModelFeature.newBuilder()
                        .setModelCursor(
                                FakeModelCursor.newBuilder().addChildren(modelChildren).build())
                        .build());
        session.setSessionId(TEST_SESSION_ID);
        session.bindModelProvider(mFakeModelProvider, mViewDepthProvider);
        session.populateModelProvider(new ArrayList<>(),
                /* cachedBindings= */ false,
                /* legacyHeadContent= */ true, UiContext.getDefaultInstance());
    }
}
