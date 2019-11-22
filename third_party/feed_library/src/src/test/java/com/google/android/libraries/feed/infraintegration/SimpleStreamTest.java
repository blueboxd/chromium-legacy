// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelCursor;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.common.testing.InfraIntegrationScope;
import com.google.android.libraries.feed.common.testing.ModelProviderValidator;
import com.google.android.libraries.feed.common.testing.ResponseBuilder;
import com.google.android.libraries.feed.common.testing.ResponseBuilder.WireProtocolInfo;
import com.google.android.libraries.feed.testing.requestmanager.FakeFeedRequestManager;
import com.google.search.now.feed.client.StreamDataProto.UiContext;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Simple tests of a stream with multiple cards. */
@RunWith(RobolectricTestRunner.class)
public class SimpleStreamTest {
    private FakeFeedRequestManager fakeFeedRequestManager;
    private ModelProviderFactory modelProviderFactory;
    private ModelProviderValidator modelValidator;
    private ProtocolAdapter protocolAdapter;
    private RequestManager requestManager;

    @Before
    public void setUp() {
        initMocks(this);
        InfraIntegrationScope scope = new InfraIntegrationScope.Builder().build();
        fakeFeedRequestManager = scope.getFakeFeedRequestManager();
        modelProviderFactory = scope.getModelProviderFactory();
        protocolAdapter = scope.getProtocolAdapter();
        modelValidator = new ModelProviderValidator(protocolAdapter);
        requestManager = scope.getRequestManager();
    }

    @Test
    public void simpleStream_oneCard() {
        // ModelProvider created after $HEAD has content, one root, and one Card
        // A Card is two features, the Card and Content.
        ResponseBuilder responseBuilder = ResponseBuilder.forClearAllWithCards(
                new ContentId[] {ResponseBuilder.createFeatureContentId(1)});
        fakeFeedRequestManager.queueResponse(responseBuilder.build());
        requestManager.triggerScheduledRefresh();
        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        modelValidator.assertRoot(modelProvider);

        WireProtocolInfo protocolInfo = responseBuilder.getWireProtocolInfo();
        int expectedFeatureCount = 3; // 1 root, 1 Card (2 features)
        assertThat(protocolInfo.featuresAdded).hasSize(expectedFeatureCount);
        assertThat(protocolInfo.hasClearOperation).isTrue();
        modelValidator.verifyContent(modelProvider, protocolInfo.featuresAdded);

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
        modelValidator.assertCardStructure(child);
    }

    @Test
    public void simpleStream_twoCard() {
        // ModelProvider created after $HEAD has content, one root, and two Cards
        ContentId[] cards = new ContentId[] {ResponseBuilder.createFeatureContentId(1),
                ResponseBuilder.createFeatureContentId(2)};
        ResponseBuilder responseBuilder = ResponseBuilder.forClearAllWithCards(cards);
        fakeFeedRequestManager.queueResponse(responseBuilder.build());
        requestManager.triggerScheduledRefresh();
        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        modelValidator.assertRoot(modelProvider);

        WireProtocolInfo protocolInfo = responseBuilder.getWireProtocolInfo();
        int expectedFeatureCount = 5; // 1 root, 2 Card (* 2 features)
        assertThat(protocolInfo.featuresAdded).hasSize(expectedFeatureCount);
        assertThat(protocolInfo.hasClearOperation).isTrue();
        modelValidator.verifyContent(modelProvider, protocolInfo.featuresAdded);

        // Validate the cursor, we should have one card for each added
        ModelFeature root = modelProvider.getRootFeature();
        assertThat(root).isNotNull();
        ModelCursor cursor = root.getCursor();
        for (ContentId contentId : cards) {
            ModelChild child = cursor.getNextItem();
            assertThat(child).isNotNull();
            modelValidator.assertStreamContentId(
                    child.getContentId(), protocolAdapter.getStreamContentId(contentId));
            modelValidator.assertCardStructure(child);
        }
        assertThat(cursor.isAtEnd()).isTrue();
    }
}
