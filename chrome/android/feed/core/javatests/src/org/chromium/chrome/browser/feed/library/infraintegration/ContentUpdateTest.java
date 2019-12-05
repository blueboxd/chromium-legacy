// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.client.requestmanager.RequestManager;
import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.FeatureChange;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.FeatureChangeObserver;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelCursor;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelFeature;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.testing.InfraIntegrationScope;
import org.chromium.chrome.browser.feed.library.common.testing.ModelProviderValidator;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder;
import org.chromium.chrome.browser.feed.library.testing.requestmanager.FakeFeedRequestManager;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.components.feed.core.proto.wire.ConsistencyTokenProto.ConsistencyToken;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/** Tests which update content within an existing model. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContentUpdateTest {
    private final InfraIntegrationScope mScope = new InfraIntegrationScope.Builder().build();
    private final FakeFeedRequestManager mFakeFeedRequestManager =
            mScope.getFakeFeedRequestManager();
    private final FakeThreadUtils mFakeThreadUtils = mScope.getFakeThreadUtils();
    private final ModelProviderFactory mModelProviderFactory = mScope.getModelProviderFactory();
    private final ModelProviderValidator mModelValidator =
            new ModelProviderValidator(mScope.getProtocolAdapter());
    private final ProtocolAdapter mProtocolAdapter = mScope.getProtocolAdapter();
    private final FeedSessionManager mFeedSessionManager = mScope.getFeedSessionManager();
    private final RequestManager mRequestManager = mScope.getRequestManager();

    @Test
    public void updateContent_observers() {
        // Create a simple stream with a root and two features
        ContentId[] cards = new ContentId[] {ResponseBuilder.createFeatureContentId(1),
                ResponseBuilder.createFeatureContentId(2)};
        mFakeFeedRequestManager.queueResponse(ResponseBuilder.forClearAllWithCards(cards).build());
        mRequestManager.triggerScheduledRefresh();
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());

        ModelFeature root = modelProvider.getRootFeature();
        assertThat(root).isNotNull();
        ModelCursor cursor = root.getCursor();
        List<FeatureChangeObserver> observers = new ArrayList<>();
        List<FeatureChangeObserver> contentObservers = new ArrayList<>();
        for (ContentId contentId : cards) {
            ModelChild child = cursor.getNextItem();
            assertThat(child).isNotNull();
            mModelValidator.assertStreamContentId(
                    child.getContentId(), mProtocolAdapter.getStreamContentId(contentId));
            mModelValidator.assertCardStructure(child);
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
        mFakeFeedRequestManager.queueResponse(
                ResponseBuilder.builder().addCardsToRoot(cards).build());
        // TODO: sessions reject updates without a CLEAR_ALL or paging with a different token.
        mFakeThreadUtils.enforceMainThread(false);
        mFakeFeedRequestManager.loadMore(StreamToken.getDefaultInstance(),
                ConsistencyToken.getDefaultInstance(),
                mFeedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));

        int id = 0;
        for (FeatureChangeObserver observer : observers) {
            ArgumentCaptor<FeatureChange> capture = ArgumentCaptor.forClass(FeatureChange.class);
            verify(observer).onChange(capture.capture());
            List<FeatureChange> featureChanges = capture.getAllValues();
            assertThat(featureChanges).hasSize(1);
            FeatureChange change = featureChanges.get(0);
            mModelValidator.assertStreamContentId(
                    change.getContentId(), mProtocolAdapter.getStreamContentId(cards[id]));
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
