// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedmodelprovider;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.functional.Predicate;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;

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
            @Nullable ViewDepthProvider viewDepthProvider, UiContext uiContext) {
        return createNew(viewDepthProvider, null, uiContext);
    }

    @Override
    public ModelProvider createNew(@Nullable ViewDepthProvider viewDepthProvider,
            @Nullable Predicate<StreamStructure> filterPredicate, UiContext uiContext) {
        FeedModelProvider modelProvider =
                new FeedModelProvider(mFeedSessionManager, mThreadUtils, mTimingUtils, mTaskQueue,
                        mMainThreadRunner, filterPredicate, mConfig, mBasicLoggingApi);
        mFeedSessionManager.getNewSession(
                modelProvider, modelProvider.getViewDepthProvider(viewDepthProvider), uiContext);
        return modelProvider;
    }
}
