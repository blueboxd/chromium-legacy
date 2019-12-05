// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.client.requestmanager.RequestManager;
import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.host.logging.RequestReason;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.State;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderObserver;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.testing.InfraIntegrationScope;
import org.chromium.chrome.browser.feed.library.common.testing.ModelProviderValidator;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder.WireProtocolInfo;
import org.chromium.chrome.browser.feed.library.testing.requestmanager.FakeFeedRequestManager;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.components.feed.core.proto.wire.ConsistencyTokenProto.ConsistencyToken;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of a Stream with only a root. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RootOnlyTest {
    private final InfraIntegrationScope mScope = new InfraIntegrationScope.Builder().build();
    private final FakeFeedRequestManager mFakeFeedRequestManager =
            mScope.getFakeFeedRequestManager();
    private final FakeThreadUtils mFakeThreadUtils = mScope.getFakeThreadUtils();
    private final ModelProviderFactory mModelProviderFactory = mScope.getModelProviderFactory();
    private final ModelProviderValidator mModelValidator =
            new ModelProviderValidator(mScope.getProtocolAdapter());
    private final FeedSessionManager mFeedSessionManager = mScope.getFeedSessionManager();
    private final RequestManager mRequestManager = mScope.getRequestManager();

    @Test
    public void rootOnlyResponse_beforeSessionWithLifecycle() {
        // ModelProvider is created from $HEAD containing content
        ResponseBuilder responseBuilder =
                new ResponseBuilder().addClearOperation().addRootFeature();
        mFakeFeedRequestManager.queueResponse(responseBuilder.build());
        mRequestManager.triggerScheduledRefresh();
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());

        ModelProviderObserver changeObserver = mock(ModelProviderObserver.class);
        modelProvider.registerObserver(changeObserver);
        verify(changeObserver).onSessionStart(UiContext.getDefaultInstance());
        verify(changeObserver, never()).onSessionFinished(any(UiContext.class));

        assertThat(modelProvider).isNotNull();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        mModelValidator.assertRoot(modelProvider);

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
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        modelProvider.registerObserver(changeObserver);
        verify(changeObserver).onSessionStart(UiContext.getDefaultInstance());
        verify(changeObserver, never()).onSessionFinished(any(UiContext.class));

        ResponseBuilder responseBuilder = new ResponseBuilder().addRootFeature();
        mFakeFeedRequestManager.queueResponse(responseBuilder.build());
        // TODO: sessions reject updates without a CLEAR_ALL or paging with a different token.
        mFakeThreadUtils.enforceMainThread(false);
        mFakeFeedRequestManager.loadMore(StreamToken.getDefaultInstance(),
                ConsistencyToken.getDefaultInstance(),
                mFeedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));
        verify(changeObserver, never()).onSessionFinished(any(UiContext.class));

        assertThat(modelProvider).isNotNull();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        mModelValidator.assertRoot(modelProvider);

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
        mFakeFeedRequestManager.queueResponse(responseBuilder.build());
        mFakeFeedRequestManager.triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                mFeedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        modelProvider.registerObserver(changeObserver);
        mModelValidator.assertRoot(modelProvider);

        ContentId anotherRoot = ContentId.newBuilder()
                                        .setContentDomain("root-feature")
                                        .setId(2)
                                        .setTable("feature")
                                        .build();
        responseBuilder = new ResponseBuilder().addRootFeature(anotherRoot);
        mFakeFeedRequestManager.queueResponse(responseBuilder.build());
        // TODO: sessions reject updates without a CLEAR_ALL or paging with a different token.
        mFakeThreadUtils.enforceMainThread(false);
        mFakeFeedRequestManager.loadMore(StreamToken.getDefaultInstance(),
                ConsistencyToken.getDefaultInstance(),
                mFeedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));
        verify(changeObserver).onSessionFinished(any(UiContext.class));
    }
}
