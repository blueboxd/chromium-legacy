// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedsessionmanager.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;

import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.internal.common.testing.InternalProtocolBuilder;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import com.google.android.libraries.feed.common.concurrent.testing.FakeTaskQueue;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.feedsessionmanager.internal.testing.AbstractSessionImplTest;
import com.google.android.libraries.feed.testing.modelprovider.FakeModelMutation;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;
import com.google.search.now.feed.client.StreamDataProto.UiContext;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/** Tests of the {@link SessionImpl} class. */
@RunWith(RobolectricTestRunner.class)
public class SessionImplTest extends AbstractSessionImplTest {
    private final FakeClock fakeClock = new FakeClock();
    private final FakeThreadUtils fakeThreadUtils = FakeThreadUtils.withThreadChecks();
    private final TimingUtils timingUtils = new TimingUtils();
    private SessionImpl session;

    @Before
    @Override
    public void setUp() {
        super.setUp();
        fakeThreadUtils.enforceMainThread(false);
        session = getSessionImpl();
    }

    @Test
    public void testInvalidateOnResetHead() {
        assertThat(session.invalidateOnResetHead()).isTrue();
    }

    @Test
    public void testClearHead() {
        session.setSessionId(TEST_SESSION_ID);
        session.populateModelProvider(
                new ArrayList<>(), false, false, UiContext.getDefaultInstance());
        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder().addClearOperation();
        addFeatures(protocolBuilder, featureCnt, 1);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        // clear head will be ignored
        assertThat(streamStructures).hasSize(4);
        session.updateSession(true, streamStructures, SCHEMA_VERSION, null);
        assertThat(fakeSessionMutation.streamStructures).isEmpty();
        FakeModelMutation fakeModelMutation = fakeModelProvider.getLatestModelMutation();
        assertThat(fakeModelMutation.addedChildren).isEmpty();
        assertThat(session.getContentInSession()).isEmpty();
    }

    @Test
    public void testClearHead_paginationRequest() {
        ViewDepthProvider mockDepthProvider = mock(ViewDepthProvider.class);
        session.setSessionId(TEST_SESSION_ID);
        session.bindModelProvider(fakeModelProvider, mockDepthProvider);
        session.populateModelProvider(
                new ArrayList<>(), false, false, UiContext.getDefaultInstance());
        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder().addClearOperation();
        addFeatures(protocolBuilder, featureCnt, 1);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();
        session.updateSession(true, streamStructures, SCHEMA_VERSION,
                new MutationContext.Builder()
                        .setContinuationToken(
                                StreamToken.newBuilder().setContentId("token").build())
                        .setRequestingSessionId(TEST_SESSION_ID)
                        .build());

        assertThat(fakeModelProvider.isInvalidated()).isTrue();
    }

    @Test
    public void testModelProviderBinding_withDetach() {
        ViewDepthProvider mockDepthProvider = mock(ViewDepthProvider.class);
        session.bindModelProvider(fakeModelProvider, mockDepthProvider);
        assertThat(session.modelProvider).isEqualTo(fakeModelProvider);
        assertThat(session.viewDepthProvider).isEqualTo(mockDepthProvider);

        session.bindModelProvider(null, null);
        assertThat(session.viewDepthProvider).isNull();
        assertThat(session.modelProvider).isNull();
    }

    @Test
    public void testUpdateSession_requiredContent() {
        String contentId = contentIdGenerators.createFeatureContentId(1);
        InternalProtocolBuilder protocolBuilder =
                new InternalProtocolBuilder().addRequiredContent(contentId);
        session.setSessionId(TEST_SESSION_ID);
        session.updateSession(
                /* clearHead= */ false, protocolBuilder.buildAsStreamStructure(), SCHEMA_VERSION,
                /* mutationContext= */ null);

        assertThat(fakeSessionMutation.streamStructures).hasSize(1);
        assertThat(fakeModelProvider.getLatestModelMutation().addedChildren).isEmpty();
        assertThat(fakeModelProvider.getLatestModelMutation().removedChildren).isEmpty();
        assertThat(fakeModelProvider.getLatestModelMutation().updateChildren).isEmpty();
        assertThat(fakeModelProvider.getLatestModelMutation().isCommitted()).isTrue();
    }

    @Override
    protected SessionImpl getSessionImpl() {
        FakeTaskQueue fakeTaskQueue = new FakeTaskQueue(fakeClock, fakeThreadUtils);
        fakeTaskQueue.initialize(() -> {});
        SessionImpl session =
                new SessionImpl(store, false, fakeTaskQueue, timingUtils, fakeThreadUtils);
        session.bindModelProvider(fakeModelProvider, null);
        return session;
    }
}
