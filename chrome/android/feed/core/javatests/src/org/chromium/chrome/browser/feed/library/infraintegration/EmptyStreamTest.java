// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.client.requestmanager.RequestManager;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.State;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderObserver;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.testing.InfraIntegrationScope;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder.WireProtocolInfo;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.testing.requestmanager.FakeFeedRequestManager;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.components.feed.core.proto.libraries.testing.UiContextForTestProto.UiContextForTest;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Test which verifies the ModelProvider state for the empty stream cases. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EmptyStreamTest {
    private FakeClock mFakeClock;
    private FakeFeedRequestManager mFakeFeedRequestManager;
    private ModelProviderFactory mModelProviderFactory;
    private RequestManager mRequestManager;

    @Before
    public void setUp() {
        initMocks(this);
        InfraIntegrationScope scope = new InfraIntegrationScope.Builder().build();
        mFakeClock = scope.getFakeClock();
        mFakeFeedRequestManager = scope.getFakeFeedRequestManager();
        mModelProviderFactory = scope.getModelProviderFactory();
        mRequestManager = scope.getRequestManager();
    }

    @Test
    public void emptyStream_observable() {
        // ModelProvider will be initialized from empty $HEAD
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        assertThat(modelProvider.getRootFeature()).isNull();
        ModelProviderObserver observer = mock(ModelProviderObserver.class);
        modelProvider.registerObserver(observer);
        verify(observer).onSessionStart(UiContext.getDefaultInstance());
    }

    @Test
    public void emptyStream_usesGivenUiContext() {
        // ModelProvider will be initialized with the given UiContext.
        UiContext uiContext = UiContext.newBuilder()
                                      .setExtension(UiContextForTest.uiContextForTest,
                                              UiContextForTest.newBuilder().setValue(2).build())
                                      .build();
        ModelProvider modelProvider = mModelProviderFactory.createNew(null, uiContext);
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        assertThat(modelProvider.getRootFeature()).isNull();
        ModelProviderObserver observer = mock(ModelProviderObserver.class);
        modelProvider.registerObserver(observer);
        verify(observer).onSessionStart(uiContext);
    }

    @Test
    public void emptyStream_observableInvalidate() {
        // Verify both ModelProviderObserver events are called
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        ModelProviderObserver observer = mock(ModelProviderObserver.class);
        modelProvider.registerObserver(observer);
        verify(observer).onSessionStart(UiContext.getDefaultInstance());
        modelProvider.invalidate();
        verify(observer).onSessionFinished(UiContext.getDefaultInstance());
    }

    @Test
    public void emptyStream_unregisterObservable() {
        // Verify unregister works, so the ModelProviderObserver is not called for invalidate
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        ModelProviderObserver observer = mock(ModelProviderObserver.class);
        modelProvider.registerObserver(observer);
        verify(observer).onSessionStart(UiContext.getDefaultInstance());
        modelProvider.unregisterObserver(observer);
        modelProvider.invalidate();
        verify(observer, never()).onSessionFinished(UiContext.getDefaultInstance());
    }

    @Test
    public void emptyStream_emptyResponse() {
        // Create an empty stream through a response
        ResponseBuilder responseBuilder = new ResponseBuilder();
        mFakeFeedRequestManager.queueResponse(responseBuilder.build());
        mRequestManager.triggerScheduledRefresh();

        mFakeClock.advance(TaskQueue.STARVATION_TIMEOUT_MS);
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());

        assertThat(modelProvider).isNotNull();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        assertThat(modelProvider.getRootFeature()).isNull();

        WireProtocolInfo protocolInfo = responseBuilder.getWireProtocolInfo();
        // No features added
        assertThat(protocolInfo.featuresAdded).hasSize(0);
        assertThat(protocolInfo.hasClearOperation).isFalse();
    }
}
