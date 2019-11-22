// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.internal.modelprovider.FeatureChange;
import com.google.android.libraries.feed.api.internal.modelprovider.FeatureChangeObserver;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelCursor;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
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

import java.util.ArrayList;
import java.util.List;

/** Tests which update content within an existing model. */
@RunWith(RobolectricTestRunner.class)
public class ContentUpdateTest {
    private final InfraIntegrationScope scope = new InfraIntegrationScope.Builder().build();
    private final FakeFeedRequestManager fakeFeedRequestManager = scope.getFakeFeedRequestManager();
    private final FakeThreadUtils fakeThreadUtils = scope.getFakeThreadUtils();
    private final ModelProviderFactory modelProviderFactory = scope.getModelProviderFactory();
    private final ModelProviderValidator modelValidator =
            new ModelProviderValidator(scope.getProtocolAdapter());
    private final ProtocolAdapter protocolAdapter = scope.getProtocolAdapter();
    private final FeedSessionManager feedSessionManager = scope.getFeedSessionManager();
    private final RequestManager requestManager = scope.getRequestManager();

    @Test
    public void updateContent_observers() {
        // Create a simple stream with a root and two features
        ContentId[] cards = new ContentId[] {ResponseBuilder.createFeatureContentId(1),
                ResponseBuilder.createFeatureContentId(2)};
        fakeFeedRequestManager.queueResponse(ResponseBuilder.forClearAllWithCards(cards).build());
        requestManager.triggerScheduledRefresh();
        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());

        ModelFeature root = modelProvider.getRootFeature();
        assertThat(root).isNotNull();
        ModelCursor cursor = root.getCursor();
        List<FeatureChangeObserver> observers = new ArrayList<>();
        List<FeatureChangeObserver> contentObservers = new ArrayList<>();
        for (ContentId contentId : cards) {
            ModelChild child = cursor.getNextItem();
            assertThat(child).isNotNull();
            modelValidator.assertStreamContentId(
                    child.getContentId(), protocolAdapter.getStreamContentId(contentId));
            modelValidator.assertCardStructure(child);
            // register observer on the card
            ModelFeature feature = child.getModelFeature();
            FeatureChangeObserver observer = mock(FeatureChangeObserver.class);
            observers.add(observer);
            feature.registerObserver(observer);

            // register observer on the content of the card
            FeatureChangeObserver contentObserver = mock(FeatureChangeObserver.class);
            contentObservers.add(contentObserver);
            ModelCursor cardCursor = feature.getCursor();
            ModelChild cardCursorNextItem = cardCursor.getNextItem();
            assertThat(cardCursorNextItem).isNotNull();
            cardCursorNextItem.getModelFeature().registerObserver(contentObserver);
        }
        assertThat(cursor.isAtEnd()).isTrue();

        // Create an update response for the two content items
        fakeFeedRequestManager.queueResponse(
                ResponseBuilder.builder().addCardsToRoot(cards).build());
        // TODO: sessions reject updates without a CLEAR_ALL or paging with a different token.
        fakeThreadUtils.enforceMainThread(false);
        fakeFeedRequestManager.loadMore(StreamToken.getDefaultInstance(),
                ConsistencyToken.getDefaultInstance(),
                feedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));

        int id = 0;
        for (FeatureChangeObserver observer : observers) {
            ArgumentCaptor<FeatureChange> capture = ArgumentCaptor.forClass(FeatureChange.class);
            verify(observer).onChange(capture.capture());
            List<FeatureChange> featureChanges = capture.getAllValues();
            assertThat(featureChanges).hasSize(1);
            FeatureChange change = featureChanges.get(0);
            modelValidator.assertStreamContentId(
                    change.getContentId(), protocolAdapter.getStreamContentId(cards[id]));
            assertThat(change.isFeatureChanged()).isTrue();
            assertThat(change.getChildChanges().getAppendedChildren()).isEmpty();
            id++;
        }
        for (FeatureChangeObserver observer : contentObservers) {
            ArgumentCaptor<FeatureChange> capture = ArgumentCaptor.forClass(FeatureChange.class);
            verify(observer).onChange(capture.capture());
            List<FeatureChange> featureChanges = capture.getAllValues();
            assertThat(featureChanges).hasSize(1);
            FeatureChange change = featureChanges.get(0);
            assertThat(change.isFeatureChanged()).isTrue();
        }
    }
}
