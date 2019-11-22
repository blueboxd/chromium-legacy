// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedsessionmanager.internal;

import static com.google.common.truth.Truth.assertThat;

import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.internal.common.testing.ContentIdGenerators;
import com.google.android.libraries.feed.api.internal.common.testing.InternalProtocolBuilder;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.common.concurrent.testing.FakeTaskQueue;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.testing.store.FakeStore;
import com.google.common.collect.ImmutableList;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import java.util.List;
import java.util.Set;

/** Tests of the {@link HeadSessionImpl} class. */
@RunWith(RobolectricTestRunner.class)
public class HeadSessionImplTest {
    private static final int SCHEMA_VERSION = 1;

    private final ContentIdGenerators contentIdGenerators = new ContentIdGenerators();
    private final FakeClock fakeClock = new FakeClock();
    private final FakeThreadUtils fakeThreadUtils = FakeThreadUtils.withoutThreadChecks();
    private final TimingUtils timingUtils = new TimingUtils();
    private final FakeStore fakeStore = new FakeStore(Configuration.getDefaultInstance(),
            fakeThreadUtils, new FakeTaskQueue(fakeClock, fakeThreadUtils), fakeClock);
    private final HeadSessionImpl headSession =
            new HeadSessionImpl(fakeStore, timingUtils, /* limitPageUpdatesInHead= */ false);

    @Test
    public void testMinimalSessionManager() {
        assertThat(headSession.getSessionId()).isEqualTo(Store.HEAD_SESSION_ID);
    }

    @Test
    public void testInvalidateOnResetHead() {
        assertThat(headSession.invalidateOnResetHead()).isFalse();
    }

    @Test
    public void testUpdateSession_features() {
        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder().addClearOperation();
        addFeatures(protocolBuilder, featureCnt, 1);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        // 1 clear, 3 features
        assertThat(streamStructures).hasSize(featureCnt + 1);
        headSession.updateSession(false, streamStructures, SCHEMA_VERSION, null);

        // expect: 3 features
        assertThat(headSession.getContentInSession()).hasSize(featureCnt);
        assertThat(headSession.getContentInSession())
                .contains(contentIdGenerators.createFeatureContentId(1));
        assertThat(getContentInSession()).hasSize(featureCnt);
    }

    @Test
    public void testReset() {
        int featureCnt = 5;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder().addClearOperation();
        addFeatures(protocolBuilder, featureCnt, 1);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();
        assertThat(streamStructures).hasSize(featureCnt + 1);
        headSession.updateSession(false, streamStructures, SCHEMA_VERSION, null);
        assertThat(headSession.getContentInSession()).hasSize(featureCnt);

        headSession.reset();
        assertThat(headSession.getContentInSession()).isEmpty();
    }

    @Test
    public void testUpdateSession_token() {
        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder().addClearOperation();
        addFeatures(protocolBuilder, featureCnt, 1);
        protocolBuilder.addToken(contentIdGenerators.createTokenContentId(1));
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        // 1 clear, 3 features, token
        assertThat(streamStructures).hasSize(5);
        headSession.updateSession(false, streamStructures, SCHEMA_VERSION, null);

        // expect: 3 features, 1 token
        assertThat(headSession.getContentInSession()).hasSize(featureCnt + 1);
        assertThat(headSession.getContentInSession())
                .contains(contentIdGenerators.createFeatureContentId(1));
        assertThat(getContentInSession()).hasSize(featureCnt + 1);
    }

    @Test
    public void testUpdateFromToken() {
        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        addFeatures(protocolBuilder, featureCnt, 1);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        StreamToken token = StreamToken.newBuilder()
                                    .setContentId(contentIdGenerators.createTokenContentId(2))
                                    .build();

        // The token needs to be in the session so update its content IDs with the token.
        List<StreamStructure> tokenStructures = new InternalProtocolBuilder()
                                                        .addToken(token.getContentId())
                                                        .buildAsStreamStructure();
        headSession.updateSession(false, tokenStructures, SCHEMA_VERSION, null);

        MutationContext context = new MutationContext.Builder().setContinuationToken(token).build();
        headSession.updateSession(false, streamStructures, SCHEMA_VERSION, context);
        // features 3, plus the token added above
        assertThat(headSession.getContentInSession()).hasSize(featureCnt + 1);
    }

    @Test
    public void testUpdateFromToken_limitPageUpdatesInHead() {
        HeadSessionImpl headSession =
                new HeadSessionImpl(fakeStore, timingUtils, /* limitPageUpdatesInHead= */ true);

        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        addFeatures(protocolBuilder, featureCnt, 1);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        StreamToken token = StreamToken.newBuilder()
                                    .setContentId(contentIdGenerators.createTokenContentId(2))
                                    .build();

        // The token needs to be in the session so update its content IDs with the token.
        List<StreamStructure> tokenStructures = new InternalProtocolBuilder()
                                                        .addToken(token.getContentId())
                                                        .buildAsStreamStructure();
        headSession.updateSession(false, tokenStructures, SCHEMA_VERSION, null);

        MutationContext context = new MutationContext.Builder().setContinuationToken(token).build();
        headSession.updateSession(false, streamStructures, SCHEMA_VERSION, context);
        assertThat(headSession.getContentInSession()).hasSize(1);
    }

    @Test
    public void testUpdateFromToken_notInSession() {
        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        addFeatures(protocolBuilder, featureCnt, 1);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        StreamToken token = StreamToken.newBuilder()
                                    .setContentId(contentIdGenerators.createTokenContentId(2))
                                    .build();

        // The token needs to be in the session, if not we ignore the update
        MutationContext context = new MutationContext.Builder().setContinuationToken(token).build();
        headSession.updateSession(false, streamStructures, SCHEMA_VERSION, context);
        assertThat(headSession.getContentInSession()).isEmpty();
    }

    @Test
    public void testUpdateSession_remove() {
        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder().addClearOperation();
        addFeatures(protocolBuilder, featureCnt, 1);
        protocolBuilder.removeFeature(contentIdGenerators.createFeatureContentId(1),
                contentIdGenerators.createRootContentId(0));
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        // 1 clear, 3 features, 1 remove
        assertThat(streamStructures).hasSize(5);
        headSession.updateSession(false, streamStructures, SCHEMA_VERSION, null);

        // expect: 2 features (3 added, then 1 removed)
        assertThat(headSession.getContentInSession()).hasSize(featureCnt - 1);
        assertThat(headSession.getContentInSession())
                .contains(contentIdGenerators.createFeatureContentId(2));
        assertThat(headSession.getContentInSession())
                .contains(contentIdGenerators.createFeatureContentId(3));
        assertThat(getContentInSession()).hasSize(featureCnt - 1);
    }

    @Test
    public void testUpdateSession_updates() {
        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder().addClearOperation();
        addFeatures(protocolBuilder, featureCnt, 1);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();
        assertThat(streamStructures).hasSize(4);

        // 1 clear, 3 features
        headSession.updateSession(false, streamStructures, SCHEMA_VERSION, null);
        assertThat(headSession.getContentInSession()).hasSize(featureCnt);
        assertThat(getContentInSession()).hasSize(featureCnt);

        // Now we will update feature 2
        protocolBuilder = new InternalProtocolBuilder();
        addFeatures(protocolBuilder, 1, 2);
        streamStructures = protocolBuilder.buildAsStreamStructure();
        assertThat(streamStructures).hasSize(1);

        // 0 features
        headSession.updateSession(false, streamStructures, SCHEMA_VERSION, null);
        assertThat(headSession.getContentInSession()).hasSize(featureCnt);
        assertThat(getContentInSession()).hasSize(featureCnt);
    }

    @Test
    public void testUpdateSession_paging() {
        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder().addClearOperation();
        addFeatures(protocolBuilder, featureCnt, 1);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();
        assertThat(streamStructures).hasSize(4);

        // 1 clear, 3 features
        headSession.updateSession(false, streamStructures, SCHEMA_VERSION, null);
        assertThat(headSession.getContentInSession()).hasSize(featureCnt);
        assertThat(getContentInSession()).hasSize(featureCnt);

        // Now we add two new features
        int additionalFeatureCnt = 2;
        protocolBuilder = new InternalProtocolBuilder();
        addFeatures(protocolBuilder, additionalFeatureCnt, featureCnt + 1);
        streamStructures = protocolBuilder.buildAsStreamStructure();
        assertThat(streamStructures).hasSize(additionalFeatureCnt);

        // 0 features
        headSession.updateSession(false, streamStructures, SCHEMA_VERSION, null);
        assertThat(headSession.getContentInSession()).hasSize(featureCnt + additionalFeatureCnt);
        assertThat(getContentInSession()).hasSize(featureCnt + additionalFeatureCnt);
    }

    @Test
    public void testUpdateSession_storeClearHead() {
        headSession.initializeSession(ImmutableList.of(), SCHEMA_VERSION);

        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder().addClearOperation();
        addFeatures(protocolBuilder, featureCnt, 1);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();
        assertThat(streamStructures).hasSize(4);

        // 1 clear, 3 features
        headSession.updateSession(false, streamStructures, SCHEMA_VERSION + 1, null);
        assertThat(headSession.getContentInSession()).hasSize(featureCnt);
        assertThat(getContentInSession()).hasSize(featureCnt);
        assertThat(headSession.getSchemaVersion()).isEqualTo(SCHEMA_VERSION);

        // Clear head and add 2 features, make sure we have new content ids
        int newFeatureCnt = 2;
        protocolBuilder = new InternalProtocolBuilder().addClearOperation();
        addFeatures(protocolBuilder, newFeatureCnt, featureCnt + 1);
        streamStructures = protocolBuilder.buildAsStreamStructure();

        // 2 features, 1 clear
        assertThat(streamStructures).hasSize(3);

        // 0 features
        fakeStore.clearHead();
        headSession.updateSession(false, streamStructures, SCHEMA_VERSION + 1, null);
        assertThat(headSession.getContentInSession()).hasSize(newFeatureCnt);
        assertThat(getContentInSession()).hasSize(newFeatureCnt);
        assertThat(headSession.getSchemaVersion()).isEqualTo(SCHEMA_VERSION);
    }

    @Test
    public void testUpdateSession_clearHeadUpdatesSchemaVersion() {
        headSession.initializeSession(ImmutableList.of(), SCHEMA_VERSION);
        headSession.updateSession(
                /* clearHead= */ true, ImmutableList.of(), SCHEMA_VERSION + 1,
                /* mutationContext= */ null);
        assertThat(headSession.getSchemaVersion()).isEqualTo(SCHEMA_VERSION + 1);
    }

    @Test
    public void testUpdateSession_schemaVersionUnchanged() {
        headSession.initializeSession(ImmutableList.of(), SCHEMA_VERSION);
        headSession.updateSession(
                /* clearHead= */ false, ImmutableList.of(), SCHEMA_VERSION + 1,
                /* mutationContext= */ null);
        assertThat(headSession.getSchemaVersion()).isEqualTo(SCHEMA_VERSION);
    }

    @Test
    public void testUpdateSession_requiredContent() {
        String contentId = contentIdGenerators.createFeatureContentId(1);
        InternalProtocolBuilder protocolBuilder =
                new InternalProtocolBuilder().addRequiredContent(contentId);

        headSession.updateSession(
                /* clearHead= */ false, protocolBuilder.buildAsStreamStructure(), SCHEMA_VERSION,
                /* mutationContext= */ null);
        assertThat(headSession.getContentInSession()).hasSize(1);
        assertThat(getContentInSession()).hasSize(1);
    }

    @Test
    public void testInitializeSession_schemaVersion() {
        int schemaVersion = 3;
        headSession.initializeSession(ImmutableList.of(), schemaVersion);
        assertThat(headSession.getSchemaVersion()).isEqualTo(schemaVersion);
    }

    private void addFeatures(InternalProtocolBuilder protocolBuilder, int featureCnt, int startId) {
        for (int i = 0; i < featureCnt; i++) {
            protocolBuilder.addFeature(contentIdGenerators.createFeatureContentId(startId++),
                    contentIdGenerators.createRootContentId(0));
        }
    }

    /** Re-read the session from disk and return the set of content. */
    private Set<String> getContentInSession() {
        HeadSessionImpl headSession =
                new HeadSessionImpl(fakeStore, timingUtils, /* limitPageUpdatesInHead= */ false);
        headSession.initializeSession(
                fakeStore.getStreamStructures(Store.HEAD_SESSION_ID).getValue(),
                /* schemaVersion= */ 0);
        return headSession.getContentInSession();
    }
}
