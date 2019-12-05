// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.piet;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Content;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.PietContent;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.components.feed.core.proto.wire.DataOperationProto.DataOperation;
import org.chromium.components.feed.core.proto.wire.DataOperationProto.DataOperation.Operation;
import org.chromium.components.feed.core.proto.wire.FeatureProto.Feature;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link PietRequiredContentAdapter}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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

    private final PietRequiredContentAdapter mAdapter = new PietRequiredContentAdapter();

    @Test
    public void testDetermineRequiredContentIds() {
        assertThat(mAdapter.determineRequiredContentIds(
                           DataOperation.newBuilder()
                                   .setOperation(Operation.UPDATE_OR_APPEND)
                                   .setFeature(FEATURE)
                                   .build()))
                .containsExactly(CONTENT_ID_1, CONTENT_ID_2);
    }

    @Test
    public void testDetermineRequiredContentIds_removeDoesNotRequireContent() {
        assertThat(mAdapter.determineRequiredContentIds(DataOperation.newBuilder()
                                                                .setOperation(Operation.REMOVE)
                                                                .setFeature(FEATURE)
                                                                .build()))
                .isEmpty();
    }

    @Test
    public void testDetermineRequiredContentIds_clearAllDoesNotRequireContent() {
        assertThat(mAdapter.determineRequiredContentIds(DataOperation.newBuilder()
                                                                .setOperation(Operation.CLEAR_ALL)
                                                                .setFeature(FEATURE)
                                                                .build()))
                .isEmpty();
    }

    @Test
    public void testDetermineRequiredContentIds_defaultInstanceDoesNotRequireContent() {
        assertThat(mAdapter.determineRequiredContentIds(
                           DataOperation.newBuilder()
                                   .setOperation(Operation.UPDATE_OR_APPEND)
                                   .build()))
                .isEmpty();
    }
}
