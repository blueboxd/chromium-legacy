// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder.ROOT_CONTENT_ID;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.client.requestmanager.RequestManager;
import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.internal.common.Model;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.RemoveTrackingFactory;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.RemoveTracking;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.functional.Function;
import org.chromium.chrome.browser.feed.library.common.testing.InfraIntegrationScope;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder;
import org.chromium.chrome.browser.feed.library.testing.requestmanager.FakeFeedRequestManager;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.components.feed.core.proto.wire.ResponseProto.Response;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/** Tests of the ModelProvider RemoveTracking behavior. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RemoveTrackingBehaviorTest {
    private static final ContentId[] CARDS = new ContentId[] {
            ResponseBuilder.createFeatureContentId(1),
            ResponseBuilder.createFeatureContentId(2),
            ResponseBuilder.createFeatureContentId(3),
            ResponseBuilder.createFeatureContentId(4),
            ResponseBuilder.createFeatureContentId(5),
    };

    private FakeFeedRequestManager mFakeFeedRequestManager;
    private FeedSessionManager mFeedSessionManager;
    private ModelProviderFactory mModelProviderFactory;
    private ProtocolAdapter mProtocolAdapter;
    private RequestManager mRequestManager;

    @Before
    public void setUp() {
        initMocks(this);
        InfraIntegrationScope scope = new InfraIntegrationScope.Builder().build();
        mFakeFeedRequestManager = scope.getFakeFeedRequestManager();
        mFeedSessionManager = scope.getFeedSessionManager();
        mModelProviderFactory = scope.getModelProviderFactory();
        mProtocolAdapter = scope.getProtocolAdapter();
        mRequestManager = scope.getRequestManager();
    }

    @Test
    public void testBaseRemoveTracking() {
        loadInitialData();

        AtomicBoolean called = new AtomicBoolean(false);
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        modelProvider.enableRemoveTracking(getRemoveTrackingFactory((contentIds) -> {
            assertThat(contentIds).hasSize(1);
            called.set(true);
        }));
        ResponseBuilder responseBuilder =
                ResponseBuilder.builder().removeFeature(CARDS[1], ROOT_CONTENT_ID);
        List<StreamDataOperation> dataOperations = getDataOperations(responseBuilder);
        MutationContext mutationContext =
                new MutationContext.Builder().setUserInitiated(true).build();
        Consumer<Result<Model>> updateConsumer =
                mFeedSessionManager.getUpdateConsumer(mutationContext);
        updateConsumer.accept(Result.success(Model.of(dataOperations)));
        assertThat(called.get()).isTrue();
    }

    @Test
    public void testBaseRemoveTracking_multipleItems() {
        loadInitialData();

        AtomicBoolean called = new AtomicBoolean(false);
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        modelProvider.enableRemoveTracking(getRemoveTrackingFactory((contentIds) -> {
            assertThat(contentIds).hasSize(2);
            called.set(true);
        }));
        ResponseBuilder responseBuilder = ResponseBuilder.builder()
                                                  .removeFeature(CARDS[1], ROOT_CONTENT_ID)
                                                  .removeFeature(CARDS[3], ROOT_CONTENT_ID);
        List<StreamDataOperation> dataOperations = getDataOperations(responseBuilder);
        MutationContext mutationContext =
                new MutationContext.Builder().setUserInitiated(true).build();
        Consumer<Result<Model>> updateConsumer =
                mFeedSessionManager.getUpdateConsumer(mutationContext);
        updateConsumer.accept(Result.success(Model.of(dataOperations)));
        assertThat(called.get()).isTrue();
    }

    /**
     * For non-user initiated mutations, the test is setup to return null from the factory. The
     * result is the Consumer should not be called.
     */
    @Test
    public void testNonUserInitiated() {
        loadInitialData();

        AtomicBoolean called = new AtomicBoolean(false);
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        modelProvider.enableRemoveTracking(getRemoveTrackingFactory((contentIds) -> {
            assertThat(contentIds).isEmpty();
            called.set(true);
        }));

        ResponseBuilder responseBuilder =
                ResponseBuilder.builder().removeFeature(CARDS[1], ROOT_CONTENT_ID);
        List<StreamDataOperation> dataOperations = getDataOperations(responseBuilder);
        MutationContext mutationContext =
                new MutationContext.Builder().setUserInitiated(false).build();
        Consumer<Result<Model>> updateConsumer =
                mFeedSessionManager.getUpdateConsumer(mutationContext);
        updateConsumer.accept(Result.success(Model.of(dataOperations)));
        assertThat(called.get()).isFalse();
    }

    private RemoveTrackingFactory<String> getRemoveTrackingFactory(
            Consumer<List<String>> consumer) {
        return new RemoveTrackingFactory<String>() {
            @Override
            public /*@Nullable*/ RemoveTracking<String> create(MutationContext mutationContext) {
                // Only support RemoveTracking on user initiated removes
                return mutationContext.isUserInitiated() ? getRemoveTracking(
                               (streamFeature) -> simpleTransform(streamFeature), consumer)
                                                         : null;
            }
        };
    }

    private RemoveTracking<String> getRemoveTracking(
            Function<StreamFeature, String> transformer, Consumer<List<String>> consumer) {
        return new RemoveTracking<>(transformer, consumer);
    }

    @SuppressWarnings("unused")
    private boolean alwaysTrue(String value) {
        return true;
    }

    private String simpleTransform(StreamFeature streamFeature) {
        // only return the Content StreamFeatures.
        return streamFeature.hasContent() ? streamFeature.getContentId() : null;
    }

    private List<StreamDataOperation> getDataOperations(ResponseBuilder builder) {
        Response response = builder.build();
        Result<Model> result = mProtocolAdapter.createModel(response);
        assertThat(result.isSuccessful()).isTrue();
        return result.getValue().streamDataOperations;
    }

    private void loadInitialData() {
        mFakeFeedRequestManager.queueResponse(ResponseBuilder.forClearAllWithCards(CARDS).build());
        mRequestManager.triggerScheduledRefresh();
    }
}
