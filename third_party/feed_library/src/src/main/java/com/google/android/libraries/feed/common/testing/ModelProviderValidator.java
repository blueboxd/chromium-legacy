// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common.testing;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;

import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild.Type;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelCursor;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;

import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/** Helper class providing validation method for a ModelProvider. */
public class ModelProviderValidator {
    private final ProtocolAdapter mProtocolAdapter;

    public ModelProviderValidator(ProtocolAdapter protocolAdapter) {
        this.mProtocolAdapter = protocolAdapter;
    }

    public void assertRoot(ModelProvider modelProvider) {
        assertRoot(modelProvider, ResponseBuilder.ROOT_CONTENT_ID);
    }

    // Suppress nullness since it's just another failure type for tests
    @SuppressWarnings("nullness")
    public void assertRoot(ModelProvider modelProvider, ContentId contentId) {
        ModelFeature modelFeature = modelProvider.getRootFeature();
        assertThat(modelFeature).isNotNull();
        assertThat(modelFeature.getStreamFeature()).isNotNull();
        assertStreamContentId(modelFeature.getStreamFeature().getContentId(),
                mProtocolAdapter.getStreamContentId(contentId));
    }

    public void assertStreamContentId(String contentId, String expectedContentId) {
        assertThat(contentId).isEqualTo(expectedContentId);
    }

    public void assertStreamContentId(String contentId, ContentId expectedContentId) {
        assertThat(contentId).isEqualTo(mProtocolAdapter.getStreamContentId(expectedContentId));
    }

    public void verifyContent(ModelProvider modelProvider, List<ContentId> features) {
        for (ContentId contentId : features) {
            String streamContentId = mProtocolAdapter.getStreamContentId(contentId);
            if (modelProvider.getModelChild(streamContentId) == null) {
                fail("Feature was not found for " + streamContentId);
            }
        }
    }

    public void assertCardStructure(ModelChild modelChild) {
        AtomicInteger cursorCount = new AtomicInteger();
        assertThat(modelChild.getType()).isEqualTo(Type.FEATURE);
        ModelFeature feature = modelChild.getModelFeature();
        ModelCursor cursor = feature.getCursor();
        ModelChild child;
        while ((child = cursor.getNextItem()) != null) {
            cursorCount.incrementAndGet();
            assertThat(child.getType()).isEqualTo(Type.FEATURE);
        }
        assertThat(cursorCount.get()).isEqualTo(1);
    }

    public void assertCursorSize(ModelCursor cursor, int expectedSize) {
        int size = 0;
        while (cursor.getNextItem() != null) {
            size++;
        }
        assertThat(size).isEqualTo(expectedSize);
    }

    @SuppressWarnings("nullness")
    public void assertCursorContents(ModelProvider modelProvider, ContentId... cards) {
        ModelFeature rootFeature = modelProvider.getRootFeature();
        assertThat(rootFeature).isNotNull();
        ModelCursor cursor = rootFeature.getCursor();
        assertCursorContents(cursor, cards);
    }

    public void assertCursorContents(ModelCursor cursor, ContentId... args) {
        AtomicInteger size = new AtomicInteger(0);
        ModelChild child;
        while ((child = cursor.getNextItem()) != null) {
            int pos = size.getAndIncrement();
            assertStreamContentId(child.getContentId(), args[pos]);
        }
        assertThat(cursor.isAtEnd()).isTrue();
        assertThat(args.length).isEqualTo(size.get());
    }

    @SuppressWarnings("nullness")
    public ModelChild assertCursorContentsWithToken(
            ModelProvider modelProvider, ContentId... cards) {
        ModelFeature rootFeature = modelProvider.getRootFeature();
        assertThat(rootFeature).isNotNull();
        ModelCursor cursor = rootFeature.getCursor();
        ModelChild tokenFeature = assertCursorContentsWithToken(cursor, cards);
        assertThat(tokenFeature).isNotNull();
        return tokenFeature;
    }

    public ModelChild assertCursorContentsWithToken(ModelCursor cursor, ContentId... args) {
        AtomicInteger size = new AtomicInteger(0);
        AtomicReference<ModelChild> tokenFeature = new AtomicReference<>();
        ModelChild child;
        while ((child = cursor.getNextItem()) != null) {
            int pos = size.getAndIncrement();
            if (pos == args.length) {
                assertThat(child.getType()).isEqualTo(Type.TOKEN);
                tokenFeature.set(child);
            } else {
                assertStreamContentId(child.getContentId(), args[pos]);
            }
        }

        assertThat(cursor.isAtEnd()).isTrue();
        return tokenFeature.get();
    }
}
