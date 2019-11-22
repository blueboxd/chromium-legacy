// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.infraintegration;

import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.common.testing.InfraIntegrationScope;
import com.google.android.libraries.feed.common.testing.ModelProviderValidator;
import com.google.android.libraries.feed.common.testing.ResponseBuilder;
import com.google.search.now.feed.client.StreamDataProto.UiContext;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/**
 * Integration test for {@link
 * com.google.android.libraries.feed.feedrequestmanager.RequestManagerImpl}
 */
@RunWith(RobolectricTestRunner.class)
public class ClientRequestManagerTest {
    private final InfraIntegrationScope scope = new InfraIntegrationScope.Builder().build();
    private RequestManager requestManager;
    private ModelProviderValidator modelValidator;

    // Create a simple stream with a root and three features
    private static final ContentId[] CARDS = new ContentId[] {
            ResponseBuilder.createFeatureContentId(1), ResponseBuilder.createFeatureContentId(2),
            ResponseBuilder.createFeatureContentId(3)};

    @Before
    public void setup() {
        requestManager = scope.getRequestManager();
        modelValidator = new ModelProviderValidator(scope.getProtocolAdapter());
    }

    @Test
    public void testRequestManager() {
        // Set up new response with 3 cards
        scope.getFakeFeedRequestManager().queueResponse(
                ResponseBuilder.forClearAllWithCards(CARDS).build());

        // Trigger refresh
        requestManager.triggerScheduledRefresh();

        // Create new session
        ModelProvider modelProvider =
                scope.getModelProviderFactory().createNew(null, UiContext.getDefaultInstance());

        // Model provider should now hold the cards
        modelValidator.assertCursorContents(modelProvider, CARDS);
    }
}
