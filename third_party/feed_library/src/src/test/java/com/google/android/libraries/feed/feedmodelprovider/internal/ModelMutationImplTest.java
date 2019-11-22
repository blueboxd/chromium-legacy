// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedmodelprovider.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.common.functional.Committer;
import com.google.android.libraries.feed.feedmodelprovider.internal.ModelMutationImpl.Change;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure.Operation;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests of {@link ModelMutationImpl}. */
@RunWith(RobolectricTestRunner.class)
public class ModelMutationImplTest {
    @Mock
    private Committer<Void, Change> committer;

    @Before
    public void setup() {
        initMocks(this);
    }

    @Test
    public void testFeature() {
        ModelMutationImpl modelMutator = new ModelMutationImpl(committer);
        StreamFeature streamFeature = StreamFeature.newBuilder().build();
        modelMutator.addChild(createStreamStructureFromFeature(streamFeature));
        assertThat(modelMutator.change.structureChanges).hasSize(1);
        modelMutator.commit();
        verify(committer).commit(modelMutator.change);
    }

    @Test
    public void testToken() {
        ModelMutationImpl modelMutator = new ModelMutationImpl(committer);
        StreamToken streamToken = StreamToken.newBuilder().build();
        modelMutator.addChild(createStreamStructureFromToken(streamToken));
        assertThat(modelMutator.change.structureChanges).hasSize(1);
        modelMutator.commit();
        verify(committer).commit(modelMutator.change);
    }

    @Test
    public void testMutationContext() {
        ModelMutationImpl modelMutator = new ModelMutationImpl(committer);
        MutationContext mutationContext = MutationContext.EMPTY_CONTEXT;
        modelMutator.setMutationContext(mutationContext);
        assertThat(modelMutator.change.structureChanges).isEmpty();
        modelMutator.commit();
        verify(committer).commit(modelMutator.change);
        assertThat(modelMutator.change.mutationContext).isEqualTo(mutationContext);
    }

    @Test
    public void testRemove() {
        ModelMutationImpl modelMutator = new ModelMutationImpl(committer);
        assertThat(modelMutator.change.structureChanges).isEmpty();
        modelMutator.removeChild(StreamStructure.getDefaultInstance());
        assertThat(modelMutator.change.structureChanges).hasSize(1);
    }

    private StreamStructure createStreamStructureFromFeature(StreamFeature feature) {
        StreamStructure.Builder builder = StreamStructure.newBuilder()
                                                  .setContentId(feature.getContentId())
                                                  .setOperation(Operation.UPDATE_OR_APPEND);
        if (feature.hasParentId()) {
            builder.setParentContentId(feature.getParentId());
        }
        return builder.build();
    }

    private StreamStructure createStreamStructureFromToken(StreamToken token) {
        StreamStructure.Builder builder = StreamStructure.newBuilder()
                                                  .setContentId(token.getContentId())
                                                  .setOperation(Operation.UPDATE_OR_APPEND);
        if (token.hasParentId()) {
            builder.setParentContentId(token.getParentId());
        }
        return builder.build();
    }
}
