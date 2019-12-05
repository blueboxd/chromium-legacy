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

import java.util.List;

/** Tests which update (append) content to an existing model. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StructureUpdateTest {
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
    public void appendChildren() {
        // Create a simple stream with a root and two features
        ContentId[] startingCards = new ContentId[] {ResponseBuilder.createFeatureContentId(1),
                ResponseBuilder.createFeatureContentId(2)};
        // Define two features to be appended to the root
        ContentId[] appendedCards = new ContentId[] {ResponseBuilder.createFeatureContentId(3),
                ResponseBuilder.createFeatureContentId(4)};

        mFakeFeedRequestManager.queueResponse(
                ResponseBuilder.forClearAllWithCards(startingCards).build());
        mRequestManager.triggerScheduledRefresh();
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());

        ModelFeature root = modelProvider.getRootFeature();
        assertThat(root).isNotNull();
        FeatureChangeObserver rootObserver = mock(FeatureChangeObserver.class);
        root.registerObserver(rootObserver);
        mModelValidator.assertCursorSize(root.getCursor(), startingCards.length);

        // Append new children to root
        mFakeFeedRequestManager.queueResponse(
                ResponseBuilder.builder().addCardsToRoot(appendedCards).build());
        // TODO: sessions reject updates without a CLEAR_ALL or paging with a different token.
        mFakeThreadUtils.enforceMainThread(false);
        mFakeFeedRequestManager.loadMore(StreamToken.getDefaultInstance(),
                ConsistencyToken.getDefaultInstance(),
                mFeedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));

        // assert the new state of the stream
        mModelValidator.assertCursorSize(
                root.getCursor(), startingCards.length + appendedCards.length);
        ArgumentCaptor<FeatureChange> capture = ArgumentCaptor.forClass(FeatureChange.class);
        verify(rootObserver).onChange(capture.capture());
        List<FeatureChange> featureChanges = capture.getAllValues();
        assertThat(featureChanges).hasSize(1);
        FeatureChange change = featureChanges.get(0);
        assertThat(change.getChildChanges().getAppendedChildren()).hasSize(appendedCards.length);
        assertThat(change.isFeatureChanged()).isFalse();
        int i = 0;
        for (ModelChild appendedChild : change.getChildChanges().getAppendedChildren()) {
            mModelValidator.assertStreamContentId(appendedChild.getContentId(),
                    mProtocolAdapter.getStreamContentId(appendedCards[i++]));
        }
    }

    @Test
    public void appendChildren_concurrentModification() {
        // Test which verifies the root can be updated while we advance a cursor (without
        // a ConcurrentModificationException
        // Create a simple stream with a root and two features
        ContentId[] startingCards = new ContentId[] {ResponseBuilder.createFeatureContentId(1),
                ResponseBuilder.createFeatureContentId(2)};
        // Define two features to be appended to the root
        ContentId[] appendedCards = new ContentId[] {ResponseBuilder.createFeatureContentId(3),
                ResponseBuilder.createFeatureContentId(4)};

        mFakeFeedRequestManager.queueResponse(
                ResponseBuilder.forClearAllWithCards(startingCards).build());
        mRequestManager.triggerScheduledRefresh();
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());

        ModelFeature root = modelProvider.getRootFeature();
        assertThat(root).isNotNull();
        FeatureChangeObserver rootObserver = mock(FeatureChangeObserver.class);
        root.registerObserver(rootObserver);
        ModelCursor cursor = root.getCursor();
        cursor.getNextItem();

        // Now append additional children to the stream (and cursor)
        mFakeFeedRequestManager.queueResponse(
                ResponseBuilder.builder().addCardsToRoot(appendedCards).build());
        // TODO: sessions reject updates without a CLEAR_ALL or paging with a different token.
        mFakeThreadUtils.enforceMainThread(false);
        mFakeFeedRequestManager.loadMore(StreamToken.getDefaultInstance(),
                ConsistencyToken.getDefaultInstance(),
                mFeedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));
        mModelValidator.assertCursorSize(cursor, 3);
    }
}
