// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.host.logging.RequestReason;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.State;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderObserver;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.testing.InfraIntegrationScope;
import com.google.android.libraries.feed.common.testing.ModelProviderValidator;
import com.google.android.libraries.feed.common.testing.ResponseBuilder;
import com.google.android.libraries.feed.common.testing.ResponseBuilder.WireProtocolInfo;
import com.google.android.libraries.feed.testing.requestmanager.FakeFeedRequestManager;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;
import com.google.search.now.feed.client.StreamDataProto.UiContext;
import com.google.search.now.wire.feed.ConsistencyTokenProto.ConsistencyToken;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests of a Stream with only a root. */
@RunWith(RobolectricTestRunner.class)
public class RootOnlyTest {
    private final InfraIntegrationScope scope = new InfraIntegrationScope.Builder().build();
    private final FakeFeedRequestManager fakeFeedRequestManager = scope.getFakeFeedRequestManager();
    private final FakeThreadUtils fakeThreadUtils = scope.getFakeThreadUtils();
    private final ModelProviderFactory modelProviderFactory = scope.getModelProviderFactory();
    private final ModelProviderValidator modelValidator =
            new ModelProviderValidator(scope.getProtocolAdapter());
    private final FeedSessionManager feedSessionManager = scope.getFeedSessionManager();
    private final RequestManager requestManager = scope.getRequestManager();

    @Test
    public void rootOnlyResponse_beforeSessionWithLifecycle() {
        // ModelProvider is created from $HEAD containing content
        ResponseBuilder responseBuilder =
                new ResponseBuilder().addClearOperation().addRootFeature();
        fakeFeedRequestManager.queueResponse(responseBuilder.build());
        requestManager.triggerScheduledRefresh();
        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());

        ModelProviderObserver changeObserver = mock(ModelProviderObserver.class);
        modelProvider.registerObserver(changeObserver);
        verify(changeObserver).onSessionStart(UiContext.getDefaultInstance());
        verify(changeObserver, never()).onSessionFinished(any(UiContext.class));

        assertThat(modelProvider).isNotNull();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        modelValidator.assertRoot(modelProvider);

        WireProtocolInfo protocolInfo = responseBuilder.getWireProtocolInfo();
        // 1 root
        assertThat(protocolInfo.featuresAdded).hasSize(1);
        assertThat(protocolInfo.hasClearOperation).isTrue();

        modelProvider.invalidate();
        verify(changeObserver).onSessionFinished(UiContext.getDefaultInstance());
    }

    @Test
    public void rootOnlyResponse_afterSessionWithLifecycle() {
        // ModelProvider created from empty $HEAD, followed by a response adding head
        // Verify the observer lifecycle is correctly called
        ModelProviderObserver changeObserver = mock(ModelProviderObserver.class);
        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        modelProvider.registerObserver(changeObserver);
        verify(changeObserver).onSessionStart(UiContext.getDefaultInstance());
        verify(changeObserver, never()).onSessionFinished(any(UiContext.class));

        ResponseBuilder responseBuilder = new ResponseBuilder().addRootFeature();
        fakeFeedRequestManager.queueResponse(responseBuilder.build());
        // TODO: sessions reject updates without a CLEAR_ALL or paging with a different token.
        fakeThreadUtils.enforceMainThread(false);
        fakeFeedRequestManager.loadMore(StreamToken.getDefaultInstance(),
                ConsistencyToken.getDefaultInstance(),
                feedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));
        verify(changeObserver, never()).onSessionFinished(any(UiContext.class));

        assertThat(modelProvider).isNotNull();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        modelValidator.assertRoot(modelProvider);

        WireProtocolInfo protocolInfo = responseBuilder.getWireProtocolInfo();
        // 1 root
        assertThat(protocolInfo.featuresAdded).hasSize(1);
        assertThat(protocolInfo.hasClearOperation).isFalse();

        modelProvider.invalidate();
        verify(changeObserver).onSessionFinished(UiContext.getDefaultInstance());
    }

    @Test
    public void rootOnlyResponse_setSecondRoot() {
        // Set the root in two different responses, verify the lifecycle is called correctly
        // and the root is replaced
        ModelProviderObserver changeObserver = mock(ModelProviderObserver.class);
        ResponseBuilder responseBuilder =
                new ResponseBuilder().addClearOperation().addRootFeature();
        fakeFeedRequestManager.queueResponse(responseBuilder.build());
        fakeFeedRequestManager.triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                feedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));
        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        modelProvider.registerObserver(changeObserver);
        modelValidator.assertRoot(modelProvider);

        ContentId anotherRoot = ContentId.newBuilder()
                                        .setContentDomain("root-feature")
                                        .setId(2)
                                        .setTable("feature")
                                        .build();
        responseBuilder = new ResponseBuilder().addRootFeature(anotherRoot);
        fakeFeedRequestManager.queueResponse(responseBuilder.build());
        // TODO: sessions reject updates without a CLEAR_ALL or paging with a different token.
        fakeThreadUtils.enforceMainThread(false);
        fakeFeedRequestManager.loadMore(StreamToken.getDefaultInstance(),
                ConsistencyToken.getDefaultInstance(),
                feedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));
        verify(changeObserver).onSessionFinished(any(UiContext.class));
    }
}
