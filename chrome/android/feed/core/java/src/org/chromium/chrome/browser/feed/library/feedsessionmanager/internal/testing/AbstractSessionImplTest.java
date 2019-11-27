// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedsessionmanager.internal.testing;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;

import org.junit.Test;
import org.mockito.Mock;

import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.internal.common.testing.ContentIdGenerators;
import org.chromium.chrome.browser.feed.library.api.internal.common.testing.InternalProtocolBuilder;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.store.SessionMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.feedsessionmanager.internal.SessionImpl;
import org.chromium.chrome.browser.feed.library.testing.modelprovider.FakeModelMutation;
import org.chromium.chrome.browser.feed.library.testing.modelprovider.FakeModelProvider;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;

import java.util.ArrayList;
import java.util.List;

/** Abstract class implementing many of the core SessionImpl tests. */
public abstract class AbstractSessionImplTest {
    protected static final String TEST_SESSION_ID = "TEST$1";
    protected static final int SCHEMA_VERSION = 1;

    protected final ContentIdGenerators mContentIdGenerators = new ContentIdGenerators();

    @Mock
    protected Store mStore;
    protected FakeSessionMutation mFakeSessionMutation;
    protected FakeModelProvider mFakeModelProvider;

    private Boolean mSessionMutationResults = true;

    protected void setUp() {
        initMocks(this);
        mFakeSessionMutation = new FakeSessionMutation();
        mFakeModelProvider = new FakeModelProvider();
        when(mStore.editSession(TEST_SESSION_ID)).thenReturn(mFakeSessionMutation);
    }

    protected abstract SessionImpl getSessionImpl();

    @Test
    public void testPopulateModelProvider_features() {
        SessionImpl session = getSessionImpl();
        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        addFeatures(protocolBuilder, featureCnt, 1);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        // 3 features
        assertThat(streamStructures).hasSize(featureCnt);
        session.setSessionId(TEST_SESSION_ID);
        session.populateModelProvider(
                streamStructures, false, false, UiContext.getDefaultInstance());
        FakeModelMutation fakeModelMutation = mFakeModelProvider.getLatestModelMutation();
        assertThat(fakeModelMutation.mAddedChildren).hasSize(featureCnt);
        assertThat(fakeModelMutation.isCommitted()).isTrue();
        assertThat(session.getContentInSession()).hasSize(featureCnt);
        assertThat(session.getContentInSession())
                .contains(mContentIdGenerators.createFeatureContentId(1));
    }

    @Test
    public void testUpdateSession_notFullyInitialized() {
        SessionImpl session = getSessionImpl();
        assertThatRunnable(
                () -> session.updateSession(false, new ArrayList<>(), SCHEMA_VERSION, null))
                .throwsAnExceptionOfType(NullPointerException.class);
    }

    @Test
    public void testUpdateSession_features() {
        SessionImpl session = getSessionImpl();
        session.setSessionId(TEST_SESSION_ID);
        session.populateModelProvider(
                new ArrayList<>(), false, false, UiContext.getDefaultInstance());
        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder().addClearOperation();
        addFeatures(protocolBuilder, featureCnt, 1);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        // 1 clear, 3 features
        assertThat(streamStructures).hasSize(4);
        session.updateSession(false, streamStructures, SCHEMA_VERSION, null);
        assertThat(mFakeSessionMutation.streamStructures).hasSize(featureCnt);
        FakeModelMutation fakeModelMutation = mFakeModelProvider.getLatestModelMutation();
        assertThat(fakeModelMutation.mAddedChildren).hasSize(featureCnt);
        assertThat(session.getContentInSession()).hasSize(featureCnt);
        assertThat(session.getContentInSession())
                .contains(mContentIdGenerators.createFeatureContentId(1));
    }

    @Test
    public void testUpdateFromToken() {
        SessionImpl session = getSessionImpl();
        session.setSessionId(TEST_SESSION_ID);
        session.populateModelProvider(
                new ArrayList<>(), false, false, UiContext.getDefaultInstance());
        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        addFeatures(protocolBuilder, featureCnt, 1);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        StreamToken token = StreamToken.newBuilder()
                                    .setContentId(mContentIdGenerators.createTokenContentId(2))
                                    .build();
        MutationContext context = new MutationContext.Builder().setContinuationToken(token).build();

        // The token needs to be in the session so temporarily detach the session to update its
        // content IDs with the token.
        ModelProvider modelProvider = session.getModelProvider();
        session.bindModelProvider(null, null);

        List<StreamStructure> tokenStructures = new InternalProtocolBuilder()
                                                        .addToken(token.getContentId())
                                                        .buildAsStreamStructure();
        session.updateSession(false, tokenStructures, SCHEMA_VERSION, null);

        session.bindModelProvider(modelProvider, null);

        session.updateSession(false, streamStructures, SCHEMA_VERSION, context);
        FakeModelMutation fakeModelMutation = mFakeModelProvider.getLatestModelMutation();
        assertThat(fakeModelMutation.mMutationContext).isEqualTo(context);
    }

    @Test
    public void testUpdateFromToken_notInSession() {
        SessionImpl session = getSessionImpl();
        session.setSessionId(TEST_SESSION_ID);
        session.populateModelProvider(
                new ArrayList<>(), false, false, UiContext.getDefaultInstance());
        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        addFeatures(protocolBuilder, featureCnt, 1);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        // The token is not in the session, so we ignore the update
        assertThat(session.getContentInSession()).isEmpty();
        StreamToken token = StreamToken.newBuilder()
                                    .setContentId(mContentIdGenerators.createTokenContentId(2))
                                    .build();
        MutationContext context = new MutationContext.Builder().setContinuationToken(token).build();
        session.updateSession(false, streamStructures, SCHEMA_VERSION, context);
        assertThat(session.getContentInSession()).isEmpty();
    }

    @Test
    public void testStorageFailure() {
        SessionImpl session = getSessionImpl();
        session.setSessionId(TEST_SESSION_ID);
        session.populateModelProvider(
                new ArrayList<>(), false, false, UiContext.getDefaultInstance());
        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        addFeatures(protocolBuilder, featureCnt, 1);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        mSessionMutationResults = false;
        session.updateSession(false, streamStructures, SCHEMA_VERSION, null);
        FakeModelMutation fakeModelMutation = mFakeModelProvider.getLatestModelMutation();
        assertThat(fakeModelMutation.isCommitted())
                .isTrue(); // Optimistic write will still call this
    }

    @Test
    public void testRemove() {
        SessionImpl session = getSessionImpl();
        session.setSessionId(TEST_SESSION_ID);
        session.populateModelProvider(
                new ArrayList<>(), false, false, UiContext.getDefaultInstance());
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        int featureCnt = 2;
        addFeatures(protocolBuilder, featureCnt, 1);
        protocolBuilder.removeFeature(mContentIdGenerators.createFeatureContentId(1),
                mContentIdGenerators.createRootContentId(0));
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();
        session.updateSession(false, streamStructures, SCHEMA_VERSION, null);
        FakeModelMutation fakeModelMutation = mFakeModelProvider.getLatestModelMutation();
        assertThat(fakeModelMutation.mRemovedChildren).hasSize(1);
        assertThat(session.getContentInSession()).hasSize(1);
    }

    protected void addFeatures(
            InternalProtocolBuilder protocolBuilder, int featureCnt, int startId) {
        for (int i = 0; i < featureCnt; i++) {
            protocolBuilder.addFeature(mContentIdGenerators.createFeatureContentId(startId++),
                    mContentIdGenerators.createRootContentId(0));
        }
    }

    /** Fake SessionMutation for tests. */
    protected final class FakeSessionMutation implements SessionMutation {
        public final List<StreamStructure> streamStructures = new ArrayList<>();

        @Override
        public SessionMutation add(StreamStructure dataOperation) {
            streamStructures.add(dataOperation);
            return this;
        }

        @Override
        public Boolean commit() {
            return mSessionMutationResults;
        }
    }
}
