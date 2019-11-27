// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedrequestmanager;

import org.chromium.chrome.browser.feed.library.api.client.requestmanager.RequestManager;
import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.host.logging.RequestReason;
import org.chromium.chrome.browser.feed.library.api.internal.requestmanager.FeedRequestManager;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;

/** Implementation of Client-visible RequestManagerApi. */
public final class RequestManagerImpl implements RequestManager {
    private static final String TAG = "RequestManagerImpl";

    private final FeedRequestManager mFeedRequestManager;
    private final FeedSessionManager mFeedSessionManager;

    public RequestManagerImpl(
            FeedRequestManager feedRequestManager, FeedSessionManager feedSessionManager) {
        this.mFeedRequestManager = feedRequestManager;
        this.mFeedSessionManager = feedSessionManager;
    }

    @Override
    public void triggerScheduledRefresh() {
        mFeedRequestManager.triggerRefresh(RequestReason.HOST_REQUESTED,
                mFeedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));
    }
}
