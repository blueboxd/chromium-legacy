// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.host.scheduler;

import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;

/** Fake implementation of {@link SchedulerApi}. */
public final class FakeSchedulerApi implements SchedulerApi {
    private final ThreadUtils mThreadUtils;
    @RequestBehavior
    private int mRequestBehavior = RequestBehavior.NO_REQUEST_WITH_CONTENT;

    public FakeSchedulerApi(ThreadUtils threadUtils) {
        this.mThreadUtils = threadUtils;
    }

    @Override
    public void onReceiveNewContent(long contentCreationDateTimeMs) {}

    @Override
    public void onRequestError(int networkResponseCode) {}

    @Override
    @RequestBehavior
    public int shouldSessionRequestData(SessionState sessionState) {
        mThreadUtils.checkMainThread();
        return mRequestBehavior;
    }

    /** Sets the result returned from {@link shouldSessionRequestData( SessionState )}. */
    public FakeSchedulerApi setRequestBehavior(@RequestBehavior int requestBehavior) {
        this.mRequestBehavior = requestBehavior;
        return this;
    }
}
