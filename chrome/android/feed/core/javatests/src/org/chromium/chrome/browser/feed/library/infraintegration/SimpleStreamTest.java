// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.client.requestmanager.RequestManager;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelCursor;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelFeature;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.common.testing.InfraIntegrationScope;
import org.chromium.chrome.browser.feed.library.common.testing.ModelProviderValidator;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder.WireProtocolInfo;
import org.chromium.chrome.browser.feed.library.testing.requestmanager.FakeFeedRequestManager;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Simple tests of a stream with multiple cards. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SimpleStreamTest {
    private FakeFeedRequestManager mFakeFeedRequestManager;
    private ModelProviderFactory mModelProviderFactory;
    private ModelProviderValidator mModelValidator;
    private ProtocolAdapter mProtocolAdapter;
    private RequestManager mRequestManager;

    @Before
    public void setUp() {
        initMocks(this);
        InfraIntegrationScope scope = new InfraIntegrationScope.Builder().build();
        mFakeFeedRequestManager = scope.getFakeFeedRequestManager();
        mModelProviderFactory = scope.getModelProviderFactory();
        mProtocolAdapter = scope.getProtocolAdapter();
        mModelValidator = new ModelProviderValidator(mProtocolAdapter);
        mRequestManager = scope.getRequestManager();
    }

    @Test
    public void simpleStream_oneCard() {
        // ModelProvider created after $HEAD has content, one root, and one Card
        // A Card is two features, the Card and Content.
        ResponseBuilder responseBuilder = ResponseBuilder.forClearAllWithCards(
                new ContentId[] {ResponseBuilder.createFeatureContentId(1)});
        mFakeFeedRequestManager.queueResponse(responseBuilder.build());
        mRequestManager.triggerScheduledRefresh();
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        mModelValidator.assertRoot(modelProvider);

        WireProtocolInfo protocolInfo = responseBuilder.getWireProtocolInfo();
        int expectedFeatureCount = 3; // 1 root, 1 Card (2 features)
        assertThat(protocolInfo.featuresAdded).hasSize(expectedFeatureCount);
        assertThat(protocolInfo.hasClearOperation).isTrue();
        mModelValidator.verifyContent(modelProvider, protocolInfo.featuresAdded);

        // Validate the cursors
        ModelFeature root = modelProvider.getRootFeature();
        assertThat(root).isNotNull();
        ModelCursor cursor = root.getCursor();
        int cursorCount = 0;
        while (cursor.getNextItem() != null) {
            cursorCount++;
        }
        assertThat(cursorCount).isEqualTo(1);

        // Validate that the structure of the card
        cursor = root.getCursor();
        ModelChild child = cursor.getNextItem();
        mModelValidator.assertCardStructure(child);
    }

    @Test
    public void simpleStream_twoCard() {
        // ModelProvider created after $HEAD has content, one root, and two Cards
        ContentId[] cards = new ContentId[] {ResponseBuilder.createFeatureContentId(1),
                ResponseBuilder.createFeatureContentId(2)};
        ResponseBuilder responseBuilder = ResponseBuilder.forClearAllWithCards(cards);
        mFakeFeedRequestManager.queueResponse(responseBuilder.build());
        mRequestManager.triggerScheduledRefresh();
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        mModelValidator.assertRoot(modelProvider);

        WireProtocolInfo protocolInfo = responseBuilder.getWireProtocolInfo();
        int expectedFeatureCount = 5; // 1 root, 2 Card (* 2 features)
        assertThat(protocolInfo.featuresAdded).hasSize(expectedFeatureCount);
        assertThat(protocolInfo.hasClearOperation).isTrue();
        mModelValidator.verifyContent(modelProvider, protocolInfo.featuresAdded);

        // Validate the cursor, we should have one card for each added
        ModelFeature root = modelProvider.getRootFeature();
        assertThat(root).isNotNull();
        ModelCursor cursor = root.getCursor();
        for (ContentId contentId : cards) {
            ModelChild child = cursor.getNextItem();
            assertThat(child).isNotNull();
            mModelValidator.assertStreamContentId(
                    child.getContentId(), mProtocolAdapter.getStreamContentId(contentId));
            mModelValidator.assertCardStructure(child);
        }
        assertThat(cursor.isAtEnd()).isTrue();
    }
}
