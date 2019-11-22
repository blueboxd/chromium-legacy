// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.State;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderObserver;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.testing.InfraIntegrationScope;
import com.google.android.libraries.feed.common.testing.ResponseBuilder;
import com.google.android.libraries.feed.common.testing.ResponseBuilder.WireProtocolInfo;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.testing.proto.UiContextForTestProto.UiContextForTest;
import com.google.android.libraries.feed.testing.requestmanager.FakeFeedRequestManager;
import com.google.search.now.feed.client.StreamDataProto.UiContext;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Test which verifies the ModelProvider state for the empty stream cases. */
@RunWith(RobolectricTestRunner.class)
public class EmptyStreamTest {
    private FakeClock fakeClock;
    private FakeFeedRequestManager fakeFeedRequestManager;
    private ModelProviderFactory modelProviderFactory;
    private RequestManager requestManager;

    @Before
    public void setUp() {
        initMocks(this);
        InfraIntegrationScope scope = new InfraIntegrationScope.Builder().build();
        fakeClock = scope.getFakeClock();
        fakeFeedRequestManager = scope.getFakeFeedRequestManager();
        modelProviderFactory = scope.getModelProviderFactory();
        requestManager = scope.getRequestManager();
    }

    @Test
    public void emptyStream_observable() {
        // ModelProvider will be initialized from empty $HEAD
        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
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
        ModelProvider modelProvider = modelProviderFactory.createNew(null, uiContext);
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
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
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
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
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
        fakeFeedRequestManager.queueResponse(responseBuilder.build());
        requestManager.triggerScheduledRefresh();

        fakeClock.advance(TaskQueue.STARVATION_TIMEOUT_MS);
        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());

        assertThat(modelProvider).isNotNull();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        assertThat(modelProvider.getRootFeature()).isNull();

        WireProtocolInfo protocolInfo = responseBuilder.getWireProtocolInfo();
        // No features added
        assertThat(protocolInfo.featuresAdded).hasSize(0);
        assertThat(protocolInfo.hasClearOperation).isFalse();
    }
}
