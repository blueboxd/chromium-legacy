// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedknowncontent;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.ContentMetadata;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.ContentRemoval;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.KnownContent;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.knowncontent.FeedKnownContent;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Content;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Default implementation of the {@link KnownContent}. */
public final class FeedKnownContentImpl implements FeedKnownContent {
    private static final String TAG = "FeedKnownContentImpl";
    private final FeedSessionManager mFeedSessionManager;
    private final Set<KnownContent.Listener> mListeners = new HashSet<>();
    private final MainThreadRunner mMainThreadRunner;
    private final ThreadUtils mThreadUtils;
    private final KnownContent.Listener mListener;

    @SuppressWarnings("nullness:method.invocation.invalid")
    public FeedKnownContentImpl(FeedSessionManager feedSessionManager,
            MainThreadRunner mainThreadRunner, ThreadUtils threadUtils) {
        this.mFeedSessionManager = feedSessionManager;
        this.mMainThreadRunner = mainThreadRunner;
        this.mThreadUtils = threadUtils;

        this.mListener = new KnownContent.Listener() {
            @Override
            public void onContentRemoved(List<ContentRemoval> contentRemoved) {
                runOnMainThread(TAG + " onContentRemoved", () -> {
                    for (KnownContent.Listener knownContentListener : mListeners) {
                        knownContentListener.onContentRemoved(contentRemoved);
                    }
                });
            }

            @Override
            public void onNewContentReceived(boolean isNewRefresh, long contentCreationDateTimeMs) {
                runOnMainThread(TAG + " onNewContentReceived", () -> {
                    for (KnownContent.Listener knownContentListener : mListeners) {
                        knownContentListener.onNewContentReceived(
                                isNewRefresh, contentCreationDateTimeMs);
                    }
                });
            }
        };

        feedSessionManager.setKnownContentListener(this.mListener);
    }

    @Override
    public void getKnownContent(Consumer<List<ContentMetadata>> knownContentConsumer) {
        mFeedSessionManager.getStreamFeaturesFromHead(
                streamPayload
                -> {
                    if (!streamPayload.getStreamFeature().hasContent()) {
                        return null;
                    }

                    Content content = streamPayload.getStreamFeature().getContent();

                    return ContentMetadata.maybeCreateContentMetadata(
                            content.getOfflineMetadata(), content.getRepresentationData());
                },
                (Result<List<ContentMetadata>> result) -> {
                    runOnMainThread(TAG + " getKnownContentAccept", () -> {
                        if (!result.isSuccessful()) {
                            Logger.e(TAG,
                                    "Can't inform on known content due to internal feed error.");
                            return;
                        }

                        knownContentConsumer.accept(result.getValue());
                    });
                });
    }

    @Override
    public void addListener(KnownContent.Listener listener) {
        mListeners.add(listener);
    }

    @Override
    public void removeListener(KnownContent.Listener listener) {
        mListeners.remove(listener);
    }

    @Override
    public KnownContent.Listener getKnownContentHostNotifier() {
        return mListener;
    }

    private void runOnMainThread(String name, Runnable runnable) {
        if (mThreadUtils.isMainThread()) {
            runnable.run();
            return;
        }

        mMainThreadRunner.execute(name, runnable);
    }
}
