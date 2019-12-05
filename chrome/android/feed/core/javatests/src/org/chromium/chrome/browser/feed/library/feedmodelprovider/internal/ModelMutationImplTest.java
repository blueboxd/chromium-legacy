// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedmodelprovider.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.common.functional.Committer;
import org.chromium.chrome.browser.feed.library.feedmodelprovider.internal.ModelMutationImpl.Change;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure.Operation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of {@link ModelMutationImpl}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ModelMutationImplTest {
    @Mock
    private Committer<Void, Change> mCommitter;

    @Before
    public void setup() {
        initMocks(this);
    }

    @Test
    public void testFeature() {
        ModelMutationImpl modelMutator = new ModelMutationImpl(mCommitter);
        StreamFeature streamFeature = StreamFeature.newBuilder().build();
        modelMutator.addChild(mCreateStreamStructureFromFeature(streamFeature));
        assertThat(modelMutator.mChange.mStructureChanges).hasSize(1);
        modelMutator.commit();
        verify(mCommitter).commit(modelMutator.mChange);
    }

    @Test
    public void testToken() {
        ModelMutationImpl modelMutator = new ModelMutationImpl(mCommitter);
        StreamToken streamToken = StreamToken.newBuilder().build();
        modelMutator.addChild(mCreateStreamStructureFromToken(streamToken));
        assertThat(modelMutator.mChange.mStructureChanges).hasSize(1);
        modelMutator.commit();
        verify(mCommitter).commit(modelMutator.mChange);
    }

    @Test
    public void testMutationContext() {
        ModelMutationImpl modelMutator = new ModelMutationImpl(mCommitter);
        MutationContext mutationContext = MutationContext.EMPTY_CONTEXT;
        modelMutator.setMutationContext(mutationContext);
        assertThat(modelMutator.mChange.mStructureChanges).isEmpty();
        modelMutator.commit();
        verify(mCommitter).commit(modelMutator.mChange);
        assertThat(modelMutator.mChange.mMutationContext).isEqualTo(mutationContext);
    }

    @Test
    public void testRemove() {
        ModelMutationImpl modelMutator = new ModelMutationImpl(mCommitter);
        assertThat(modelMutator.mChange.mStructureChanges).isEmpty();
        modelMutator.removeChild(StreamStructure.getDefaultInstance());
        assertThat(modelMutator.mChange.mStructureChanges).hasSize(1);
    }

    private StreamStructure mCreateStreamStructureFromFeature(StreamFeature feature) {
        StreamStructure.Builder builder = StreamStructure.newBuilder()
                                                  .setContentId(feature.getContentId())
                                                  .setOperation(Operation.UPDATE_OR_APPEND);
        if (feature.hasParentId()) {
            builder.setParentContentId(feature.getParentId());
        }
        return builder.build();
    }

    private StreamStructure mCreateStreamStructureFromToken(StreamToken token) {
        StreamStructure.Builder builder = StreamStructure.newBuilder()
                                                  .setContentId(token.getContentId())
                                                  .setOperation(Operation.UPDATE_OR_APPEND);
        if (token.hasParentId()) {
            builder.setParentContentId(token.getParentId());
        }
        return builder.build();
    }
}
