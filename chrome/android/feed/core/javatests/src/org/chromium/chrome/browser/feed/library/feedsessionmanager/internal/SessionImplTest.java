// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedsessionmanager.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.internal.common.testing.InternalProtocolBuilder;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeTaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.feedsessionmanager.internal.testing.AbstractSessionImplTest;
import org.chromium.chrome.browser.feed.library.testing.modelprovider.FakeModelMutation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/** Tests of the {@link SessionImpl} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SessionImplTest extends AbstractSessionImplTest {
    private final FakeClock mFakeClock = new FakeClock();
    private final FakeThreadUtils mFakeThreadUtils = FakeThreadUtils.withThreadChecks();
    private final TimingUtils mTimingUtils = new TimingUtils();
    private SessionImpl mSession;

    @Before
    @Override
    public void setUp() {
        super.setUp();
        mFakeThreadUtils.enforceMainThread(false);
        mSession = getSessionImpl();
    }

    @Test
    public void testInvalidateOnResetHead() {
        assertThat(mSession.invalidateOnResetHead()).isTrue();
    }

    @Test
    public void testClearHead() {
        mSession.setSessionId(TEST_SESSION_ID);
        mSession.populateModelProvider(
                new ArrayList<>(), false, false, UiContext.getDefaultInstance());
        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder().addClearOperation();
        addFeatures(protocolBuilder, featureCnt, 1);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        // clear head will be ignored
        assertThat(streamStructures).hasSize(4);
        mSession.updateSession(true, streamStructures, SCHEMA_VERSION, null);
        assertThat(mFakeSessionMutation.streamStructures).isEmpty();
        FakeModelMutation fakeModelMutation = mFakeModelProvider.getLatestModelMutation();
        assertThat(fakeModelMutation.mAddedChildren).isEmpty();
        assertThat(mSession.getContentInSession()).isEmpty();
    }

    @Test
    public void testClearHead_paginationRequest() {
        ViewDepthProvider mockDepthProvider = mock(ViewDepthProvider.class);
        mSession.setSessionId(TEST_SESSION_ID);
        mSession.bindModelProvider(mFakeModelProvider, mockDepthProvider);
        mSession.populateModelProvider(
                new ArrayList<>(), false, false, UiContext.getDefaultInstance());
        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder().addClearOperation();
        addFeatures(protocolBuilder, featureCnt, 1);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();
        mSession.updateSession(true, streamStructures, SCHEMA_VERSION,
                new MutationContext.Builder()
                        .setContinuationToken(
                                StreamToken.newBuilder().setContentId("token").build())
                        .setRequestingSessionId(TEST_SESSION_ID)
                        .build());

        assertThat(mFakeModelProvider.isInvalidated()).isTrue();
    }

    @Test
    public void testModelProviderBinding_withDetach() {
        ViewDepthProvider mockDepthProvider = mock(ViewDepthProvider.class);
        mSession.bindModelProvider(mFakeModelProvider, mockDepthProvider);
        assertThat(mSession.mModelProvider).isEqualTo(mFakeModelProvider);
        assertThat(mSession.mViewDepthProvider).isEqualTo(mockDepthProvider);

        mSession.bindModelProvider(null, null);
        assertThat(mSession.mViewDepthProvider).isNull();
        assertThat(mSession.mModelProvider).isNull();
    }

    @Test
    public void testUpdateSession_requiredContent() {
        String contentId = mContentIdGenerators.createFeatureContentId(1);
        InternalProtocolBuilder protocolBuilder =
                new InternalProtocolBuilder().addRequiredContent(contentId);
        mSession.setSessionId(TEST_SESSION_ID);
        mSession.updateSession(
                /* clearHead= */ false, protocolBuilder.buildAsStreamStructure(), SCHEMA_VERSION,
                /* mutationContext= */ null);

        assertThat(mFakeSessionMutation.streamStructures).hasSize(1);
        assertThat(mFakeModelProvider.getLatestModelMutation().mAddedChildren).isEmpty();
        assertThat(mFakeModelProvider.getLatestModelMutation().mRemovedChildren).isEmpty();
        assertThat(mFakeModelProvider.getLatestModelMutation().mUpdateChildren).isEmpty();
        assertThat(mFakeModelProvider.getLatestModelMutation().isCommitted()).isTrue();
    }

    @Override
    protected SessionImpl getSessionImpl() {
        FakeTaskQueue fakeTaskQueue = new FakeTaskQueue(mFakeClock, mFakeThreadUtils);
        fakeTaskQueue.initialize(() -> {});
        SessionImpl session =
                new SessionImpl(mStore, false, fakeTaskQueue, mTimingUtils, mFakeThreadUtils);
        session.bindModelProvider(mFakeModelProvider, null);
        return session;
    }
}
