// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedsessionmanager.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.api.internal.common.testing.ContentIdGenerators.ROOT_PREFIX;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.internal.common.testing.ContentIdGenerators;
import org.chromium.chrome.browser.feed.library.api.internal.common.testing.InternalProtocolBuilder;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeTaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.feedsessionmanager.internal.HeadAsStructure.TreeNode;
import org.chromium.chrome.browser.feed.library.testing.store.FakeStore;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.List;

/** Tests of the {@link HeadAsStructure}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HeadAsStructureTest {
    private final ContentIdGenerators mIdGenerators = new ContentIdGenerators();
    private final FakeThreadUtils mFakeThreadUtils = FakeThreadUtils.withThreadChecks();
    private final String mRootContentId = mIdGenerators.createRootContentId(0);
    private final TimingUtils mTimingUtils = new TimingUtils();
    private final FakeClock mFakeClock = new FakeClock();

    private FakeStore mFakeStore;

    @Before
    public void setUp() {
        initMocks(this);
        mFakeStore = new FakeStore(Configuration.getDefaultInstance(), mFakeThreadUtils,
                new FakeTaskQueue(mFakeClock, mFakeThreadUtils), mFakeClock);
        mFakeThreadUtils.enforceMainThread(false);
    }

    @Test
    public void testUninitialized() {
        HeadAsStructure headAsStructure =
                new HeadAsStructure(mFakeStore, mTimingUtils, mFakeThreadUtils);
        Result<List<Void>> result = headAsStructure.filter(payload -> null);
        assertThat(result.isSuccessful()).isFalse();
    }

    @Test
    public void testInitialization() {
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        protocolBuilder.addClearOperation().addRootFeature();
        for (int i = 0; i < 3; i++) {
            String contentId = mIdGenerators.createFeatureContentId(i);
            protocolBuilder.addFeature(contentId, mRootContentId);
        }
        mFakeStore
                .setStreamStructures(
                        Store.HEAD_SESSION_ID, protocolBuilder.buildAsStreamStructure())
                .setContent(protocolBuilder.buildAsPayloadWithId());

        HeadAsStructure headAsStructure =
                new HeadAsStructure(mFakeStore, mTimingUtils, mFakeThreadUtils);
        headAsStructure.initialize(result -> assertThat(result.isSuccessful()).isTrue());
        assertThat(headAsStructure.mRoot).isNotNull();
        assertThat(headAsStructure.mContent).hasSize(4);
        assertThat(headAsStructure.mTree).hasSize(4);
        TreeNode root = headAsStructure.mContent.get(mRootContentId);
        assertThat(root).isNotNull();
        assertThat(headAsStructure.mRoot).isEqualTo(root);
        List<TreeNode> rootChildren = headAsStructure.mTree.get(mRootContentId);
        assertThat(rootChildren).isNotNull();
        assertThat(rootChildren).hasSize(3);
    }

    @Test
    public void testInitialization_withRemove() {
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        protocolBuilder.addClearOperation().addRootFeature();
        for (int i = 0; i < 3; i++) {
            String contentId = mIdGenerators.createFeatureContentId(i);
            protocolBuilder.addFeature(contentId, mRootContentId);
        }
        String contentId = mIdGenerators.createFeatureContentId(2);
        protocolBuilder.removeFeature(contentId, mRootContentId);
        mFakeStore
                .setStreamStructures(
                        Store.HEAD_SESSION_ID, protocolBuilder.buildAsStreamStructure())
                .setContent(protocolBuilder.buildAsPayloadWithId());

        HeadAsStructure headAsStructure =
                new HeadAsStructure(mFakeStore, mTimingUtils, mFakeThreadUtils);
        headAsStructure.initialize(result -> assertThat(result.isSuccessful()).isTrue());
        assertThat(headAsStructure.mRoot).isNotNull();
        assertThat(headAsStructure.mContent).hasSize(3);
        assertThat(headAsStructure.mTree).hasSize(3);
        List<TreeNode> rootChildren = headAsStructure.mTree.get(mRootContentId);
        assertThat(rootChildren).isNotNull();
        assertThat(rootChildren).hasSize(2);
    }

    @Test
    public void testInitialization_doubleInitialization() {
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        protocolBuilder.addClearOperation().addRootFeature();
        for (int i = 0; i < 3; i++) {
            String contentId = mIdGenerators.createFeatureContentId(i);
            protocolBuilder.addFeature(contentId, mRootContentId);
        }
        mFakeStore
                .setStreamStructures(
                        Store.HEAD_SESSION_ID, protocolBuilder.buildAsStreamStructure())
                .setContent(protocolBuilder.buildAsPayloadWithId());

        HeadAsStructure headAsStructure =
                new HeadAsStructure(mFakeStore, mTimingUtils, mFakeThreadUtils);
        headAsStructure.initialize(result -> assertThat(result.isSuccessful()).isTrue());
        headAsStructure.initialize(result -> assertThat(result.isSuccessful()).isFalse());
    }

    @Test
    public void testInitialization_buildTreeFailure() {
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        protocolBuilder.addClearOperation().addRootFeature();
        for (int i = 0; i < 3; i++) {
            String contentId = mIdGenerators.createFeatureContentId(i);
            protocolBuilder.addFeature(contentId, mRootContentId);
        }
        mFakeStore.setAllowGetStreamStructures(false).setContent(
                protocolBuilder.buildAsPayloadWithId());

        HeadAsStructure headAsStructure =
                new HeadAsStructure(mFakeStore, mTimingUtils, mFakeThreadUtils);
        headAsStructure.initialize(result -> assertThat(result.isSuccessful()).isFalse());
    }

    @Test
    public void testInitialization_bindTreeFailure() {
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        protocolBuilder.addClearOperation().addRootFeature();
        for (int i = 0; i < 3; i++) {
            String contentId = mIdGenerators.createFeatureContentId(i);
            protocolBuilder.addFeature(contentId, mRootContentId);
        }
        mFakeStore
                .setStreamStructures(
                        Store.HEAD_SESSION_ID, protocolBuilder.buildAsStreamStructure())
                .setAllowGetPayloads(false);

        HeadAsStructure headAsStructure =
                new HeadAsStructure(mFakeStore, mTimingUtils, mFakeThreadUtils);
        headAsStructure.initialize(result -> assertThat(result.isSuccessful()).isFalse());
    }

    @Test
    public void testFiltering_payload() {
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        protocolBuilder.addClearOperation().addRootFeature();
        for (int i = 0; i < 3; i++) {
            String contentId = mIdGenerators.createFeatureContentId(i);
            protocolBuilder.addFeature(contentId, mRootContentId);
        }
        mFakeStore
                .setStreamStructures(
                        Store.HEAD_SESSION_ID, protocolBuilder.buildAsStreamStructure())
                .setContent(protocolBuilder.buildAsPayloadWithId());

        HeadAsStructure headAsStructure =
                new HeadAsStructure(mFakeStore, mTimingUtils, mFakeThreadUtils);
        headAsStructure.initialize(result -> assertThat(result.isSuccessful()).isTrue());
        Result<List<StreamFeature>> results = headAsStructure.filter(node -> {
            StreamPayload payload = node.getStreamPayload();
            assertThat(payload).isNotNull();
            if (payload.hasStreamFeature()) {
                return payload.getStreamFeature();
            } else {
                return null;
            }
        });
        assertThat(results.isSuccessful()).isTrue();
        assertThat(results.getValue()).hasSize(4);
    }

    @Test
    public void testFiltering_childrenOfRoot() {
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        protocolBuilder.addClearOperation().addRootFeature();
        for (int i = 0; i < 3; i++) {
            String contentId = mIdGenerators.createFeatureContentId(i);
            protocolBuilder.addFeature(contentId, mRootContentId);
        }
        mFakeStore
                .setStreamStructures(
                        Store.HEAD_SESSION_ID, protocolBuilder.buildAsStreamStructure())
                .setContent(protocolBuilder.buildAsPayloadWithId());

        HeadAsStructure headAsStructure =
                new HeadAsStructure(mFakeStore, mTimingUtils, mFakeThreadUtils);
        headAsStructure.initialize(result -> assertThat(result.isSuccessful()).isTrue());
        Result<List<String>> results = headAsStructure.filter(node
                -> !node.getStreamStructure().getContentId().startsWith(ROOT_PREFIX)
                        ? node.getStreamStructure().getContentId()
                        : null);
        assertThat(results.isSuccessful()).isTrue();
        assertThat(results.getValue()).hasSize(3);
    }

    @Test
    public void testFiltering_withRemove() {
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        protocolBuilder.addClearOperation().addRootFeature();
        for (int i = 0; i < 3; i++) {
            String contentId = mIdGenerators.createFeatureContentId(i);
            protocolBuilder.addFeature(contentId, mRootContentId);
        }
        String contentId = mIdGenerators.createFeatureContentId(2);
        protocolBuilder.removeFeature(contentId, mRootContentId);
        mFakeStore
                .setStreamStructures(
                        Store.HEAD_SESSION_ID, protocolBuilder.buildAsStreamStructure())
                .setContent(protocolBuilder.buildAsPayloadWithId());

        HeadAsStructure headAsStructure =
                new HeadAsStructure(mFakeStore, mTimingUtils, mFakeThreadUtils);
        headAsStructure.initialize(result -> assertThat(result.isSuccessful()).isTrue());
        Result<List<StreamFeature>> results = headAsStructure.filter(node -> {
            StreamPayload payload = node.getStreamPayload();
            assertThat(payload).isNotNull();
            if (payload.hasStreamFeature()) {
                return payload.getStreamFeature();
            } else {
                return null;
            }
        });
        assertThat(results.isSuccessful()).isTrue();
        assertThat(results.getValue()).hasSize(3);
    }
}
