// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.piet;

import static com.google.common.truth.Truth.assertThat;

import com.google.search.now.ui.stream.StreamStructureProto.Content;
import com.google.search.now.ui.stream.StreamStructureProto.PietContent;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;
import com.google.search.now.wire.feed.DataOperationProto.DataOperation;
import com.google.search.now.wire.feed.DataOperationProto.DataOperation.Operation;
import com.google.search.now.wire.feed.FeatureProto.Feature;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link PietRequiredContentAdapter}. */
@RunWith(RobolectricTestRunner.class)
public class PietRequiredContentAdapterTest {
    private static final ContentId CONTENT_ID_1 = ContentId.newBuilder().setId(1).build();
    private static final ContentId CONTENT_ID_2 = ContentId.newBuilder().setId(2).build();
    private static final Feature FEATURE =
            Feature.newBuilder()
                    .setExtension(Content.contentExtension,
                            Content.newBuilder()
                                    .setExtension(PietContent.pietContentExtension,
                                            PietContent.newBuilder()
                                                    .addPietSharedStates(CONTENT_ID_1)
                                                    .addPietSharedStates(CONTENT_ID_2)
                                                    .build())
                                    .build())
                    .build();

    private final PietRequiredContentAdapter adapter = new PietRequiredContentAdapter();

    @Test
    public void testDetermineRequiredContentIds() {
        assertThat(adapter.determineRequiredContentIds(
                           DataOperation.newBuilder()
                                   .setOperation(Operation.UPDATE_OR_APPEND)
                                   .setFeature(FEATURE)
                                   .build()))
                .containsExactly(CONTENT_ID_1, CONTENT_ID_2);
    }

    @Test
    public void testDetermineRequiredContentIds_removeDoesNotRequireContent() {
        assertThat(adapter.determineRequiredContentIds(DataOperation.newBuilder()
                                                               .setOperation(Operation.REMOVE)
                                                               .setFeature(FEATURE)
                                                               .build()))
                .isEmpty();
    }

    @Test
    public void testDetermineRequiredContentIds_clearAllDoesNotRequireContent() {
        assertThat(adapter.determineRequiredContentIds(DataOperation.newBuilder()
                                                               .setOperation(Operation.CLEAR_ALL)
                                                               .setFeature(FEATURE)
                                                               .build()))
                .isEmpty();
    }

    @Test
    public void testDetermineRequiredContentIds_defaultInstanceDoesNotRequireContent() {
        assertThat(adapter.determineRequiredContentIds(
                           DataOperation.newBuilder()
                                   .setOperation(Operation.UPDATE_OR_APPEND)
                                   .build()))
                .isEmpty();
    }
}
