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

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.client.requestmanager.RequestManager;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.common.functional.Function;
import org.chromium.chrome.browser.feed.library.common.testing.InfraIntegrationScope;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder;
import org.chromium.chrome.browser.feed.library.testing.requestmanager.FakeFeedRequestManager;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link FeedSessionManager#getStreamFeaturesFromHead(Function, Consumer)} method. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FilterHeadTest {
    private FakeFeedRequestManager mFakeFeedRequestManager;

    private FeedSessionManager mFeedSessionManager;
    private RequestManager mRequestManager;

    @Before
    public void setUp() {
        initMocks(this);
        InfraIntegrationScope scope = new InfraIntegrationScope.Builder().build();
        mFakeFeedRequestManager = scope.getFakeFeedRequestManager();
        mFeedSessionManager = scope.getFeedSessionManager();
        mRequestManager = scope.getRequestManager();
    }

    @Test
    public void testFiltering() {
        ContentId[] cards = new ContentId[] {
                ResponseBuilder.createFeatureContentId(1),
                ResponseBuilder.createFeatureContentId(2),
                ResponseBuilder.createFeatureContentId(3),
                ResponseBuilder.createFeatureContentId(4),
                ResponseBuilder.createFeatureContentId(5),
        };

        mFakeFeedRequestManager.queueResponse(ResponseBuilder.forClearAllWithCards(cards).build());
        mRequestManager.triggerScheduledRefresh();

        mFeedSessionManager.getStreamFeaturesFromHead(mTransformer, result -> {
            assertThat(result.isSuccessful()).isTrue();
            assertThat(result.getValue()).hasSize(11);
        });

        // only return the contentId of the Content cards
        mFeedSessionManager.getStreamFeaturesFromHead(mToContent, result -> {
            assertThat(result.isSuccessful()).isTrue();
            assertThat(result.getValue()).hasSize(5);
        });
    }

    // Return only StreamFeatures
    private Function<StreamPayload, /*@Nullable*/ StreamFeature> mTransformer = streamPayload
            -> streamPayload.hasStreamFeature() ? streamPayload.getStreamFeature() : null;

    // Return the contentId from a feature with a RenderableUnit type of CONTENT
    private Function<StreamPayload, /*@Nullable*/ String> mToContent = streamPayload -> {
        if (!streamPayload.hasStreamFeature()) {
            return null;
        }
        StreamFeature streamFeature = streamPayload.getStreamFeature();
        return streamFeature.hasContent() ? streamFeature.getContentId() : null;
    };
}
