// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedsessionmanager.internal;

import static com.google.android.libraries.feed.api.internal.common.testing.ContentIdGenerators.ROOT_PREFIX;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.internal.common.testing.ContentIdGenerators;
import com.google.android.libraries.feed.api.internal.common.testing.InternalProtocolBuilder;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.testing.FakeTaskQueue;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.feedsessionmanager.internal.HeadAsStructure.TreeNode;
import com.google.android.libraries.feed.testing.store.FakeStore;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;
import com.google.search.now.feed.client.StreamDataProto.StreamPayload;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import java.util.List;

/** Tests of the {@link HeadAsStructure}. */
@RunWith(RobolectricTestRunner.class)
public class HeadAsStructureTest {
    private final ContentIdGenerators idGenerators = new ContentIdGenerators();
    private final FakeThreadUtils fakeThreadUtils = FakeThreadUtils.withThreadChecks();
    private final String rootContentId = idGenerators.createRootContentId(0);
    private final TimingUtils timingUtils = new TimingUtils();
    private final FakeClock fakeClock = new FakeClock();

    private FakeStore fakeStore;

    @Before
    public void setUp() {
        initMocks(this);
        fakeStore = new FakeStore(Configuration.getDefaultInstance(), fakeThreadUtils,
                new FakeTaskQueue(fakeClock, fakeThreadUtils), fakeClock);
        fakeThreadUtils.enforceMainThread(false);
    }

    @Test
    public void testUninitialized() {
        HeadAsStructure headAsStructure =
                new HeadAsStructure(fakeStore, timingUtils, fakeThreadUtils);
        Result<List<Void>> result = headAsStructure.filter(payload -> null);
        assertThat(result.isSuccessful()).isFalse();
    }

    @Test
    public void testInitialization() {
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        protocolBuilder.addClearOperation().addRootFeature();
        for (int i = 0; i < 3; i++) {
            String contentId = idGenerators.createFeatureContentId(i);
            protocolBuilder.addFeature(contentId, rootContentId);
        }
        fakeStore
                .setStreamStructures(
                        Store.HEAD_SESSION_ID, protocolBuilder.buildAsStreamStructure())
                .setContent(protocolBuilder.buildAsPayloadWithId());

        HeadAsStructure headAsStructure =
                new HeadAsStructure(fakeStore, timingUtils, fakeThreadUtils);
        headAsStructure.initialize(result -> assertThat(result.isSuccessful()).isTrue());
        assertThat(headAsStructure.root).isNotNull();
        assertThat(headAsStructure.content).hasSize(4);
        assertThat(headAsStructure.tree).hasSize(4);
        TreeNode root = headAsStructure.content.get(rootContentId);
        assertThat(root).isNotNull();
        assertThat(headAsStructure.root).isEqualTo(root);
        List<TreeNode> rootChildren = headAsStructure.tree.get(rootContentId);
        assertThat(rootChildren).isNotNull();
        assertThat(rootChildren).hasSize(3);
    }

    @Test
    public void testInitialization_withRemove() {
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        protocolBuilder.addClearOperation().addRootFeature();
        for (int i = 0; i < 3; i++) {
            String contentId = idGenerators.createFeatureContentId(i);
            protocolBuilder.addFeature(contentId, rootContentId);
        }
        String contentId = idGenerators.createFeatureContentId(2);
        protocolBuilder.removeFeature(contentId, rootContentId);
        fakeStore
                .setStreamStructures(
                        Store.HEAD_SESSION_ID, protocolBuilder.buildAsStreamStructure())
                .setContent(protocolBuilder.buildAsPayloadWithId());

        HeadAsStructure headAsStructure =
                new HeadAsStructure(fakeStore, timingUtils, fakeThreadUtils);
        headAsStructure.initialize(result -> assertThat(result.isSuccessful()).isTrue());
        assertThat(headAsStructure.root).isNotNull();
        assertThat(headAsStructure.content).hasSize(3);
        assertThat(headAsStructure.tree).hasSize(3);
        List<TreeNode> rootChildren = headAsStructure.tree.get(rootContentId);
        assertThat(rootChildren).isNotNull();
        assertThat(rootChildren).hasSize(2);
    }

    @Test
    public void testInitialization_doubleInitialization() {
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        protocolBuilder.addClearOperation().addRootFeature();
        for (int i = 0; i < 3; i++) {
            String contentId = idGenerators.createFeatureContentId(i);
            protocolBuilder.addFeature(contentId, rootContentId);
        }
        fakeStore
                .setStreamStructures(
                        Store.HEAD_SESSION_ID, protocolBuilder.buildAsStreamStructure())
                .setContent(protocolBuilder.buildAsPayloadWithId());

        HeadAsStructure headAsStructure =
                new HeadAsStructure(fakeStore, timingUtils, fakeThreadUtils);
        headAsStructure.initialize(result -> assertThat(result.isSuccessful()).isTrue());
        headAsStructure.initialize(result -> assertThat(result.isSuccessful()).isFalse());
    }

    @Test
    public void testInitialization_buildTreeFailure() {
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        protocolBuilder.addClearOperation().addRootFeature();
        for (int i = 0; i < 3; i++) {
            String contentId = idGenerators.createFeatureContentId(i);
            protocolBuilder.addFeature(contentId, rootContentId);
        }
        fakeStore.setAllowGetStreamStructures(false).setContent(
                protocolBuilder.buildAsPayloadWithId());

        HeadAsStructure headAsStructure =
                new HeadAsStructure(fakeStore, timingUtils, fakeThreadUtils);
        headAsStructure.initialize(result -> assertThat(result.isSuccessful()).isFalse());
    }

    @Test
    public void testInitialization_bindTreeFailure() {
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        protocolBuilder.addClearOperation().addRootFeature();
        for (int i = 0; i < 3; i++) {
            String contentId = idGenerators.createFeatureContentId(i);
            protocolBuilder.addFeature(contentId, rootContentId);
        }
        fakeStore
                .setStreamStructures(
                        Store.HEAD_SESSION_ID, protocolBuilder.buildAsStreamStructure())
                .setAllowGetPayloads(false);

        HeadAsStructure headAsStructure =
                new HeadAsStructure(fakeStore, timingUtils, fakeThreadUtils);
        headAsStructure.initialize(result -> assertThat(result.isSuccessful()).isFalse());
    }

    @Test
    public void testFiltering_payload() {
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        protocolBuilder.addClearOperation().addRootFeature();
        for (int i = 0; i < 3; i++) {
            String contentId = idGenerators.createFeatureContentId(i);
            protocolBuilder.addFeature(contentId, rootContentId);
        }
        fakeStore
                .setStreamStructures(
                        Store.HEAD_SESSION_ID, protocolBuilder.buildAsStreamStructure())
                .setContent(protocolBuilder.buildAsPayloadWithId());

        HeadAsStructure headAsStructure =
                new HeadAsStructure(fakeStore, timingUtils, fakeThreadUtils);
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
            String contentId = idGenerators.createFeatureContentId(i);
            protocolBuilder.addFeature(contentId, rootContentId);
        }
        fakeStore
                .setStreamStructures(
                        Store.HEAD_SESSION_ID, protocolBuilder.buildAsStreamStructure())
                .setContent(protocolBuilder.buildAsPayloadWithId());

        HeadAsStructure headAsStructure =
                new HeadAsStructure(fakeStore, timingUtils, fakeThreadUtils);
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
            String contentId = idGenerators.createFeatureContentId(i);
            protocolBuilder.addFeature(contentId, rootContentId);
        }
        String contentId = idGenerators.createFeatureContentId(2);
        protocolBuilder.removeFeature(contentId, rootContentId);
        fakeStore
                .setStreamStructures(
                        Store.HEAD_SESSION_ID, protocolBuilder.buildAsStreamStructure())
                .setContent(protocolBuilder.buildAsPayloadWithId());

        HeadAsStructure headAsStructure =
                new HeadAsStructure(fakeStore, timingUtils, fakeThreadUtils);
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
