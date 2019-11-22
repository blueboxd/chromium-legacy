// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.infraintegration;

import static com.google.android.libraries.feed.common.testing.ResponseBuilder.ROOT_CONTENT_ID;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.internal.modelprovider.FeatureChange;
import com.google.android.libraries.feed.api.internal.modelprovider.FeatureChangeObserver;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelCursor;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.testing.InfraIntegrationScope;
import com.google.android.libraries.feed.common.testing.ModelProviderValidator;
import com.google.android.libraries.feed.common.testing.ResponseBuilder;
import com.google.android.libraries.feed.testing.requestmanager.FakeFeedRequestManager;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;
import com.google.search.now.feed.client.StreamDataProto.UiContext;
import com.google.search.now.wire.feed.ConsistencyTokenProto.ConsistencyToken;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.RobolectricTestRunner;

import java.util.List;

/** Tests which remove content within an existing model. */
@RunWith(RobolectricTestRunner.class)
public class ContentRemoveTest {
    private final InfraIntegrationScope scope = new InfraIntegrationScope.Builder().build();
    private final FakeFeedRequestManager fakeFeedRequestManager = scope.getFakeFeedRequestManager();
    private final FakeThreadUtils fakeThreadUtils = scope.getFakeThreadUtils();
    private final FeedSessionManager feedSessionManager = scope.getFeedSessionManager();
    private final ModelProviderFactory modelProviderFactory = scope.getModelProviderFactory();
    private final ModelProviderValidator modelValidator =
            new ModelProviderValidator(scope.getProtocolAdapter());
    private final RequestManager requestManager = scope.getRequestManager();

    @Test
    public void removeContent() {
        // Create a simple stream with a root and four features
        ContentId[] cards = new ContentId[] {ResponseBuilder.createFeatureContentId(1),
                ResponseBuilder.createFeatureContentId(2),
                ResponseBuilder.createFeatureContentId(3),
                ResponseBuilder.createFeatureContentId(4)};
        fakeFeedRequestManager.queueResponse(ResponseBuilder.forClearAllWithCards(cards).build());
        requestManager.triggerScheduledRefresh();

        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertThat(modelProvider.getRootFeature()).isNotNull();
        ModelFeature rootFeature = modelProvider.getRootFeature();
        FeatureChangeObserver observer = mock(FeatureChangeObserver.class);
        rootFeature.registerObserver(observer);
        modelValidator.assertCursorSize(rootFeature.getCursor(), cards.length);

        // Create cursor advanced to each spot in the list of children
        ModelCursor advancedCursor0 = rootFeature.getCursor();
        ModelCursor advancedCursor1 = advanceCursor(rootFeature.getCursor(), 1);
        ModelCursor advancedCursor2 = advanceCursor(rootFeature.getCursor(), 2);
        ModelCursor advancedCursor3 = advanceCursor(rootFeature.getCursor(), 3);
        ModelCursor advancedCursor4 = advanceCursor(rootFeature.getCursor(), 4);

        fakeFeedRequestManager.queueResponse(
                ResponseBuilder.builder().removeFeature(cards[1], ROOT_CONTENT_ID).build());
        // TODO: sessions reject removes without a CLEAR_ALL or paging with a different token.
        fakeThreadUtils.enforceMainThread(false);
        fakeFeedRequestManager.loadMore(StreamToken.getDefaultInstance(),
                ConsistencyToken.getDefaultInstance(),
                feedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));

        ArgumentCaptor<FeatureChange> capture = ArgumentCaptor.forClass(FeatureChange.class);
        verify(observer).onChange(capture.capture());
        List<FeatureChange> featureChanges = capture.getAllValues();
        assertThat(featureChanges).hasSize(1);
        FeatureChange change = featureChanges.get(0);
        assertThat(change.getChildChanges().getRemovedChildren()).hasSize(1);

        modelValidator.assertCursorContents(advancedCursor0, cards[0], cards[2], cards[3]);
        modelValidator.assertCursorContents(advancedCursor1, cards[2], cards[3]);
        modelValidator.assertCursorContents(advancedCursor2, cards[2], cards[3]);
        modelValidator.assertCursorContents(advancedCursor3, cards[3]);
        modelValidator.assertCursorContents(advancedCursor4);

        // create a cursor after the remove to verify $HEAD was modified
        ModelCursor cursor = rootFeature.getCursor();
        modelValidator.assertCursorContents(cursor, cards[0], cards[2], cards[3]);
    }

    private ModelCursor advanceCursor(ModelCursor cursor, int count) {
        for (int i = 0; i < count; i++) {
            cursor.getNextItem();
        }
        return cursor;
    }
}
