// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder.ROOT_CONTENT_ID;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.client.requestmanager.RequestManager;
import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.FeatureChange;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.FeatureChangeObserver;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelCursor;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelFeature;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;
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

/** Tests which remove content within an existing model. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContentRemoveTest {
    private final InfraIntegrationScope mScope = new InfraIntegrationScope.Builder().build();
    private final FakeFeedRequestManager mFakeFeedRequestManager =
            mScope.getFakeFeedRequestManager();
    private final FakeThreadUtils mFakeThreadUtils = mScope.getFakeThreadUtils();
    private final FeedSessionManager mFeedSessionManager = mScope.getFeedSessionManager();
    private final ModelProviderFactory mModelProviderFactory = mScope.getModelProviderFactory();
    private final ModelProviderValidator mModelValidator =
            new ModelProviderValidator(mScope.getProtocolAdapter());
    private final RequestManager mRequestManager = mScope.getRequestManager();

    @Test
    public void removeContent() {
        // Create a simple stream with a root and four features
        ContentId[] cards = new ContentId[] {ResponseBuilder.createFeatureContentId(1),
                ResponseBuilder.createFeatureContentId(2),
                ResponseBuilder.createFeatureContentId(3),
                ResponseBuilder.createFeatureContentId(4)};
        mFakeFeedRequestManager.queueResponse(ResponseBuilder.forClearAllWithCards(cards).build());
        mRequestManager.triggerScheduledRefresh();

        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertThat(modelProvider.getRootFeature()).isNotNull();
        ModelFeature rootFeature = modelProvider.getRootFeature();
        FeatureChangeObserver observer = mock(FeatureChangeObserver.class);
        rootFeature.registerObserver(observer);
        mModelValidator.assertCursorSize(rootFeature.getCursor(), cards.length);

        // Create cursor advanced to each spot in the list of children
        ModelCursor advancedCursor0 = rootFeature.getCursor();
        ModelCursor advancedCursor1 = advanceCursor(rootFeature.getCursor(), 1);
        ModelCursor advancedCursor2 = advanceCursor(rootFeature.getCursor(), 2);
        ModelCursor advancedCursor3 = advanceCursor(rootFeature.getCursor(), 3);
        ModelCursor advancedCursor4 = advanceCursor(rootFeature.getCursor(), 4);

        mFakeFeedRequestManager.queueResponse(
                ResponseBuilder.builder().removeFeature(cards[1], ROOT_CONTENT_ID).build());
        // TODO: sessions reject removes without a CLEAR_ALL or paging with a different token.
        mFakeThreadUtils.enforceMainThread(false);
        mFakeFeedRequestManager.loadMore(StreamToken.getDefaultInstance(),
                ConsistencyToken.getDefaultInstance(),
                mFeedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));

        ArgumentCaptor<FeatureChange> capture = ArgumentCaptor.forClass(FeatureChange.class);
        verify(observer).onChange(capture.capture());
        List<FeatureChange> featureChanges = capture.getAllValues();
        assertThat(featureChanges).hasSize(1);
        FeatureChange change = featureChanges.get(0);
        assertThat(change.getChildChanges().getRemovedChildren()).hasSize(1);

        mModelValidator.assertCursorContents(advancedCursor0, cards[0], cards[2], cards[3]);
        mModelValidator.assertCursorContents(advancedCursor1, cards[2], cards[3]);
        mModelValidator.assertCursorContents(advancedCursor2, cards[2], cards[3]);
        mModelValidator.assertCursorContents(advancedCursor3, cards[3]);
        mModelValidator.assertCursorContents(advancedCursor4);

        // create a cursor after the remove to verify $HEAD was modified
        ModelCursor cursor = rootFeature.getCursor();
        mModelValidator.assertCursorContents(cursor, cards[0], cards[2], cards[3]);
    }

    private ModelCursor advanceCursor(ModelCursor cursor, int count) {
        for (int i = 0; i < count; i++) {
            cursor.getNextItem();
        }
        return cursor;
    }
}
