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

import org.chromium.chrome.browser.feed.library.api.client.lifecycle.AppLifecycleListener;
import org.chromium.chrome.browser.feed.library.api.client.requestmanager.RequestManager;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.State;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;
import org.chromium.chrome.browser.feed.library.common.testing.InfraIntegrationScope;
import org.chromium.chrome.browser.feed.library.common.testing.ModelProviderValidator;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder;
import org.chromium.chrome.browser.feed.library.testing.requestmanager.FakeFeedRequestManager;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** This will test the behavior of clear all. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ClearAllTest {
    private FakeFeedRequestManager mFakeFeedRequestManager;
    private ModelProviderFactory mModelProviderFactory;
    private ModelProviderValidator mModelValidator;
    private AppLifecycleListener mAppLifecycleListener;
    private RequestManager mRequestManager;

    @Before
    public void setUp() {
        initMocks(this);
        Configuration configuration =
                new Configuration.Builder().put(ConfigKey.LIMIT_PAGE_UPDATES, false).build();
        InfraIntegrationScope scope =
                new InfraIntegrationScope.Builder().setConfiguration(configuration).build();
        mFakeFeedRequestManager = scope.getFakeFeedRequestManager();
        mRequestManager = scope.getRequestManager();
        mModelProviderFactory = scope.getModelProviderFactory();
        mAppLifecycleListener = scope.getAppLifecycleListener();
        mModelValidator = new ModelProviderValidator(scope.getProtocolAdapter());
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
        mFakeFeedRequestManager.queueResponse(ResponseBuilder.forClearAllWithCards(cards).build());
        mRequestManager.triggerScheduledRefresh();
        ModelProvider modelProvider1 =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        mModelValidator.assertCursorContents(modelProvider1, cards);
        ModelProvider modelProvider2 =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        mModelValidator.assertCursorContents(modelProvider2, cards);

        mAppLifecycleListener.onClearAll();

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
        mFakeFeedRequestManager.queueResponse(ResponseBuilder.forClearAllWithCards(cards).build());
        mRequestManager.triggerScheduledRefresh();
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        mModelValidator.assertCursorContents(modelProvider, cards);
        String modelToken = modelProvider.getSessionId();
        mAppLifecycleListener.onClearAll();

        assertThat(modelProvider.getCurrentState()).isEqualTo(State.INVALIDATED);

        // create a new (Empty) ModelProvider
        modelProvider = mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        assertThat(modelProvider.getRootFeature()).isNull();

        // try to create the old existing ModelProvider
        modelProvider = mModelProviderFactory.create(modelToken, UiContext.getDefaultInstance());
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.INVALIDATED);
    }
}
