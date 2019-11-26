// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedmodelprovider;

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.functional.Predicate;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.UiContext;

/**
 * Factory for creating instances of {@link ModelProviderFactory} using the {@link
 * FeedModelProvider}.
 */
public final class FeedModelProviderFactory implements ModelProviderFactory {
    private final FeedSessionManager mFeedSessionManager;
    private final ThreadUtils mThreadUtils;
    private final TimingUtils mTimingUtils;
    private final TaskQueue mTaskQueue;
    private final MainThreadRunner mMainThreadRunner;
    private final Configuration mConfig;
    private final BasicLoggingApi mBasicLoggingApi;

    public FeedModelProviderFactory(FeedSessionManager feedSessionManager, ThreadUtils threadUtils,
            TimingUtils timingUtils, TaskQueue taskQueue, MainThreadRunner mainThreadRunner,
            Configuration config, BasicLoggingApi basicLoggingApi) {
        this.mFeedSessionManager = feedSessionManager;
        this.mThreadUtils = threadUtils;
        this.mTimingUtils = timingUtils;
        this.mTaskQueue = taskQueue;
        this.mMainThreadRunner = mainThreadRunner;
        this.mConfig = config;
        this.mBasicLoggingApi = basicLoggingApi;
    }

    @Override
    public ModelProvider create(String sessionId, UiContext uiContext) {
        FeedModelProvider modelProvider = new FeedModelProvider(mFeedSessionManager, mThreadUtils,
                mTimingUtils, mTaskQueue, mMainThreadRunner, null, mConfig, mBasicLoggingApi);
        mFeedSessionManager.getExistingSession(sessionId, modelProvider, uiContext);
        return modelProvider;
    }

    @Override
    public ModelProvider createNew(
            /*@Nullable*/ ViewDepthProvider viewDepthProvider, UiContext uiContext) {
        return createNew(viewDepthProvider, null, uiContext);
    }

    @Override
    public ModelProvider createNew(
            /*@Nullable*/ ViewDepthProvider viewDepthProvider,
            /*@Nullable*/ Predicate<StreamStructure> filterPredicate, UiContext uiContext) {
        FeedModelProvider modelProvider =
                new FeedModelProvider(mFeedSessionManager, mThreadUtils, mTimingUtils, mTaskQueue,
                        mMainThreadRunner, filterPredicate, mConfig, mBasicLoggingApi);
        mFeedSessionManager.getNewSession(
                modelProvider, modelProvider.getViewDepthProvider(viewDepthProvider), uiContext);
        return modelProvider;
    }
}
