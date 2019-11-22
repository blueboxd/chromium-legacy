// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.client.lifecycle.AppLifecycleListener;
import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.State;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.common.testing.InfraIntegrationScope;
import com.google.android.libraries.feed.common.testing.ModelProviderValidator;
import com.google.android.libraries.feed.common.testing.ResponseBuilder;
import com.google.android.libraries.feed.testing.requestmanager.FakeFeedRequestManager;
import com.google.search.now.feed.client.StreamDataProto.UiContext;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** This will test the behavior of clear all. */
@RunWith(RobolectricTestRunner.class)
public class ClearAllTest {
    private FakeFeedRequestManager fakeFeedRequestManager;
    private ModelProviderFactory modelProviderFactory;
    private ModelProviderValidator modelValidator;
    private AppLifecycleListener appLifecycleListener;
    private RequestManager requestManager;

    @Before
    public void setUp() {
        initMocks(this);
        Configuration configuration =
                new Configuration.Builder().put(ConfigKey.LIMIT_PAGE_UPDATES, false).build();
        InfraIntegrationScope scope =
                new InfraIntegrationScope.Builder().setConfiguration(configuration).build();
        fakeFeedRequestManager = scope.getFakeFeedRequestManager();
        requestManager = scope.getRequestManager();
        modelProviderFactory = scope.getModelProviderFactory();
        appLifecycleListener = scope.getAppLifecycleListener();
        modelValidator = new ModelProviderValidator(scope.getProtocolAdapter());
    }

    /**
     * This test creates two sessions/ModelProviders then will trigger the clear all. We then verify
     * the expected behavior that the previously created ModelProviders are invalid.
     */
    @Test
    public void testClearAll() {
        ContentId[] cards = new ContentId[] {ResponseBuilder.createFeatureContentId(1),
                ResponseBuilder.createFeatureContentId(2),
                ResponseBuilder.createFeatureContentId(3)};
        fakeFeedRequestManager.queueResponse(ResponseBuilder.forClearAllWithCards(cards).build());
        requestManager.triggerScheduledRefresh();
        ModelProvider modelProvider1 =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        modelValidator.assertCursorContents(modelProvider1, cards);
        ModelProvider modelProvider2 =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        modelValidator.assertCursorContents(modelProvider2, cards);

        appLifecycleListener.onClearAll();

        assertThat(modelProvider1.getCurrentState()).isEqualTo(State.INVALIDATED);
        assertThat(modelProvider2.getCurrentState()).isEqualTo(State.INVALIDATED);
    }

    /**
     * This test create a session/ModelProvider then calls clear all. It this validates validates
     * creating a new empty ModelProvider and attempts to create the previously created
     * ModelProvider.
     */
    @Test
    public void testPostClearAll() {
        ContentId[] cards = new ContentId[] {ResponseBuilder.createFeatureContentId(1),
                ResponseBuilder.createFeatureContentId(2),
                ResponseBuilder.createFeatureContentId(3)};
        fakeFeedRequestManager.queueResponse(ResponseBuilder.forClearAllWithCards(cards).build());
        requestManager.triggerScheduledRefresh();
        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        modelValidator.assertCursorContents(modelProvider, cards);
        String modelToken = modelProvider.getSessionId();
        appLifecycleListener.onClearAll();

        assertThat(modelProvider.getCurrentState()).isEqualTo(State.INVALIDATED);

        // create a new (Empty) ModelProvider
        modelProvider = modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        assertThat(modelProvider.getRootFeature()).isNull();

        // try to create the old existing ModelProvider
        modelProvider = modelProviderFactory.create(modelToken, UiContext.getDefaultInstance());
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.INVALIDATED);
    }
}
