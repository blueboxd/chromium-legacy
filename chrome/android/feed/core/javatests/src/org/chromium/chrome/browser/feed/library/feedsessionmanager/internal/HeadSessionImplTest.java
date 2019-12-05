// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedsessionmanager.internal;

import static com.google.common.truth.Truth.assertThat;

import com.google.common.collect.ImmutableList;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.internal.common.testing.ContentIdGenerators;
import org.chromium.chrome.browser.feed.library.api.internal.common.testing.InternalProtocolBuilder;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeTaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.testing.store.FakeStore;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.List;
import java.util.Set;

/** Tests of the {@link HeadSessionImpl} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HeadSessionImplTest {
    private static final int SCHEMA_VERSION = 1;

    private final ContentIdGenerators mContentIdGenerators = new ContentIdGenerators();
    private final FakeClock mFakeClock = new FakeClock();
    private final FakeThreadUtils mFakeThreadUtils = FakeThreadUtils.withoutThreadChecks();
    private final TimingUtils mTimingUtils = new TimingUtils();
    private final FakeStore mFakeStore = new FakeStore(Configuration.getDefaultInstance(),
            mFakeThreadUtils, new FakeTaskQueue(mFakeClock, mFakeThreadUtils), mFakeClock);
    private final HeadSessionImpl mHeadSession =
            new HeadSessionImpl(mFakeStore, mTimingUtils, /* limitPageUpdatesInHead= */ false);

    @Test
    public void testMinimalSessionManager() {
        assertThat(mHeadSession.getSessionId()).isEqualTo(Store.HEAD_SESSION_ID);
    }

    @Test
    public void testInvalidateOnResetHead() {
        assertThat(mHeadSession.invalidateOnResetHead()).isFalse();
    }

    @Test
    public void testUpdateSession_features() {
        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder().addClearOperation();
        addFeatures(protocolBuilder, featureCnt, 1);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        // 1 clear, 3 features
        assertThat(streamStructures).hasSize(featureCnt + 1);
        mHeadSession.updateSession(false, streamStructures, SCHEMA_VERSION, null);

        // expect: 3 features
        assertThat(mHeadSession.getContentInSession()).hasSize(featureCnt);
        assertThat(mHeadSession.getContentInSession())
                .contains(mContentIdGenerators.createFeatureContentId(1));
        assertThat(getContentInSession()).hasSize(featureCnt);
    }

    @Test
    public void testReset() {
        int featureCnt = 5;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder().addClearOperation();
        addFeatures(protocolBuilder, featureCnt, 1);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();
        assertThat(streamStructures).hasSize(featureCnt + 1);
        mHeadSession.updateSession(false, streamStructures, SCHEMA_VERSION, null);
        assertThat(mHeadSession.getContentInSession()).hasSize(featureCnt);

        mHeadSession.reset();
        assertThat(mHeadSession.getContentInSession()).isEmpty();
    }

    @Test
    public void testUpdateSession_token() {
        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder().addClearOperation();
        addFeatures(protocolBuilder, featureCnt, 1);
        protocolBuilder.addToken(mContentIdGenerators.createTokenContentId(1));
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        // 1 clear, 3 features, token
        assertThat(streamStructures).hasSize(5);
        mHeadSession.updateSession(false, streamStructures, SCHEMA_VERSION, null);

        // expect: 3 features, 1 token
        assertThat(mHeadSession.getContentInSession()).hasSize(featureCnt + 1);
        assertThat(mHeadSession.getContentInSession())
                .contains(mContentIdGenerators.createFeatureContentId(1));
        assertThat(getContentInSession()).hasSize(featureCnt + 1);
    }

    @Test
    public void testUpdateFromToken() {
        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        addFeatures(protocolBuilder, featureCnt, 1);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        StreamToken token = StreamToken.newBuilder()
                                    .setContentId(mContentIdGenerators.createTokenContentId(2))
                                    .build();

        // The token needs to be in the session so update its content IDs with the token.
        List<StreamStructure> tokenStructures = new InternalProtocolBuilder()
                                                        .addToken(token.getContentId())
                                                        .buildAsStreamStructure();
        mHeadSession.updateSession(false, tokenStructures, SCHEMA_VERSION, null);

        MutationContext context = new MutationContext.Builder().setContinuationToken(token).build();
        mHeadSession.updateSession(false, streamStructures, SCHEMA_VERSION, context);
        // features 3, plus the token added above
        assertThat(mHeadSession.getContentInSession()).hasSize(featureCnt + 1);
    }

    @Test
    public void testUpdateFromToken_limitPageUpdatesInHead() {
        HeadSessionImpl headSession =
                new HeadSessionImpl(mFakeStore, mTimingUtils, /* limitPageUpdatesInHead= */ true);

        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        addFeatures(protocolBuilder, featureCnt, 1);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        StreamToken token = StreamToken.newBuilder()
                                    .setContentId(mContentIdGenerators.createTokenContentId(2))
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
                                    .setContentId(mContentIdGenerators.createTokenContentId(2))
                                    .build();

        // The token needs to be in the session, if not we ignore the update
        MutationContext context = new MutationContext.Builder().setContinuationToken(token).build();
        mHeadSession.updateSession(false, streamStructures, SCHEMA_VERSION, context);
        assertThat(mHeadSession.getContentInSession()).isEmpty();
    }

    @Test
    public void testUpdateSession_remove() {
        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder().addClearOperation();
        addFeatures(protocolBuilder, featureCnt, 1);
        protocolBuilder.removeFeature(mContentIdGenerators.createFeatureContentId(1),
                mContentIdGenerators.createRootContentId(0));
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        // 1 clear, 3 features, 1 remove
        assertThat(streamStructures).hasSize(5);
        mHeadSession.updateSession(false, streamStructures, SCHEMA_VERSION, null);

        // expect: 2 features (3 added, then 1 removed)
        assertThat(mHeadSession.getContentInSession()).hasSize(featureCnt - 1);
        assertThat(mHeadSession.getContentInSession())
                .contains(mContentIdGenerators.createFeatureContentId(2));
        assertThat(mHeadSession.getContentInSession())
                .contains(mContentIdGenerators.createFeatureContentId(3));
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
        mHeadSession.updateSession(false, streamStructures, SCHEMA_VERSION, null);
        assertThat(mHeadSession.getContentInSession()).hasSize(featureCnt);
        assertThat(getContentInSession()).hasSize(featureCnt);

        // Now we will update feature 2
        protocolBuilder = new InternalProtocolBuilder();
        addFeatures(protocolBuilder, 1, 2);
        streamStructures = protocolBuilder.buildAsStreamStructure();
        assertThat(streamStructures).hasSize(1);

        // 0 features
        mHeadSession.updateSession(false, streamStructures, SCHEMA_VERSION, null);
        assertThat(mHeadSession.getContentInSession()).hasSize(featureCnt);
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
        mHeadSession.updateSession(false, streamStructures, SCHEMA_VERSION, null);
        assertThat(mHeadSession.getContentInSession()).hasSize(featureCnt);
        assertThat(getContentInSession()).hasSize(featureCnt);

        // Now we add two new features
        int additionalFeatureCnt = 2;
        protocolBuilder = new InternalProtocolBuilder();
        addFeatures(protocolBuilder, additionalFeatureCnt, featureCnt + 1);
        streamStructures = protocolBuilder.buildAsStreamStructure();
        assertThat(streamStructures).hasSize(additionalFeatureCnt);

        // 0 features
        mHeadSession.updateSession(false, streamStructures, SCHEMA_VERSION, null);
        assertThat(mHeadSession.getContentInSession()).hasSize(featureCnt + additionalFeatureCnt);
        assertThat(getContentInSession()).hasSize(featureCnt + additionalFeatureCnt);
    }

    @Test
    public void testUpdateSession_storeClearHead() {
        mHeadSession.initializeSession(ImmutableList.of(), SCHEMA_VERSION);

        int featureCnt = 3;
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder().addClearOperation();
        addFeatures(protocolBuilder, featureCnt, 1);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();
        assertThat(streamStructures).hasSize(4);

        // 1 clear, 3 features
        mHeadSession.updateSession(false, streamStructures, SCHEMA_VERSION + 1, null);
        assertThat(mHeadSession.getContentInSession()).hasSize(featureCnt);
        assertThat(getContentInSession()).hasSize(featureCnt);
        assertThat(mHeadSession.getSchemaVersion()).isEqualTo(SCHEMA_VERSION);

        // Clear head and add 2 features, make sure we have new content ids
        int newFeatureCnt = 2;
        protocolBuilder = new InternalProtocolBuilder().addClearOperation();
        addFeatures(protocolBuilder, newFeatureCnt, featureCnt + 1);
        streamStructures = protocolBuilder.buildAsStreamStructure();

        // 2 features, 1 clear
        assertThat(streamStructures).hasSize(3);

        // 0 features
        mFakeStore.clearHead();
        mHeadSession.updateSession(false, streamStructures, SCHEMA_VERSION + 1, null);
        assertThat(mHeadSession.getContentInSession()).hasSize(newFeatureCnt);
        assertThat(getContentInSession()).hasSize(newFeatureCnt);
        assertThat(mHeadSession.getSchemaVersion()).isEqualTo(SCHEMA_VERSION);
    }

    @Test
    public void testUpdateSession_clearHeadUpdatesSchemaVersion() {
        mHeadSession.initializeSession(ImmutableList.of(), SCHEMA_VERSION);
        mHeadSession.updateSession(
                /* clearHead= */ true, ImmutableList.of(), SCHEMA_VERSION + 1,
                /* mutationContext= */ null);
        assertThat(mHeadSession.getSchemaVersion()).isEqualTo(SCHEMA_VERSION + 1);
    }

    @Test
    public void testUpdateSession_schemaVersionUnchanged() {
        mHeadSession.initializeSession(ImmutableList.of(), SCHEMA_VERSION);
        mHeadSession.updateSession(
                /* clearHead= */ false, ImmutableList.of(), SCHEMA_VERSION + 1,
                /* mutationContext= */ null);
        assertThat(mHeadSession.getSchemaVersion()).isEqualTo(SCHEMA_VERSION);
    }

    @Test
    public void testUpdateSession_requiredContent() {
        String contentId = mContentIdGenerators.createFeatureContentId(1);
        InternalProtocolBuilder protocolBuilder =
                new InternalProtocolBuilder().addRequiredContent(contentId);

        mHeadSession.updateSession(
                /* clearHead= */ false, protocolBuilder.buildAsStreamStructure(), SCHEMA_VERSION,
                /* mutationContext= */ null);
        assertThat(mHeadSession.getContentInSession()).hasSize(1);
        assertThat(getContentInSession()).hasSize(1);
    }

    @Test
    public void testInitializeSession_schemaVersion() {
        int schemaVersion = 3;
        mHeadSession.initializeSession(ImmutableList.of(), schemaVersion);
        assertThat(mHeadSession.getSchemaVersion()).isEqualTo(schemaVersion);
    }

    private void addFeatures(InternalProtocolBuilder protocolBuilder, int featureCnt, int startId) {
        for (int i = 0; i < featureCnt; i++) {
            protocolBuilder.addFeature(mContentIdGenerators.createFeatureContentId(startId++),
                    mContentIdGenerators.createRootContentId(0));
        }
    }

    /** Re-read the session from disk and return the set of content. */
    private Set<String> getContentInSession() {
        HeadSessionImpl headSession =
                new HeadSessionImpl(mFakeStore, mTimingUtils, /* limitPageUpdatesInHead= */ false);
        headSession.initializeSession(
                mFakeStore.getStreamStructures(Store.HEAD_SESSION_ID).getValue(),
                /* schemaVersion= */ 0);
        return headSession.getContentInSession();
    }
}
