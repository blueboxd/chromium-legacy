// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.infraintegration;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.client.requestmanager.RequestManager;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.common.testing.InfraIntegrationScope;
import org.chromium.chrome.browser.feed.library.common.testing.ModelProviderValidator;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Integration test for {@link
 * org.chromium.chrome.browser.feed.library.feedrequestmanager.RequestManagerImpl}
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ClientRequestManagerTest {
    private final InfraIntegrationScope mScope = new InfraIntegrationScope.Builder().build();
    private RequestManager mRequestManager;
    private ModelProviderValidator mModelValidator;

    // Create a simple stream with a root and three features
    private static final ContentId[] CARDS = new ContentId[] {
            ResponseBuilder.createFeatureContentId(1), ResponseBuilder.createFeatureContentId(2),
            ResponseBuilder.createFeatureContentId(3)};

    @Before
    public void setup() {
        mRequestManager = mScope.getRequestManager();
        mModelValidator = new ModelProviderValidator(mScope.getProtocolAdapter());
    }

    @Test
    public void testRequestManager() {
        // Set up new response with 3 cards
        mScope.getFakeFeedRequestManager().queueResponse(
                ResponseBuilder.forClearAllWithCards(CARDS).build());

        // Trigger refresh
        mRequestManager.triggerScheduledRefresh();

        // Create new session
        ModelProvider modelProvider =
                mScope.getModelProviderFactory().createNew(null, UiContext.getDefaultInstance());

        // Model provider should now hold the cards
        mModelValidator.assertCursorContents(modelProvider, CARDS);
    }
}
