// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.testing.host.scheduler;

import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;

/** Fake implementation of {@link SchedulerApi}. */
public final class FakeSchedulerApi implements SchedulerApi {
    private final ThreadUtils threadUtils;
    @RequestBehavior
    private int requestBehavior = RequestBehavior.NO_REQUEST_WITH_CONTENT;

    public FakeSchedulerApi(ThreadUtils threadUtils) {
        this.threadUtils = threadUtils;
    }

    @Override
    public void onReceiveNewContent(long contentCreationDateTimeMs) {}

    @Override
    public void onRequestError(int networkResponseCode) {}

    @Override
    @RequestBehavior
    public int shouldSessionRequestData(SessionState sessionState) {
        threadUtils.checkMainThread();
        return requestBehavior;
    }

    /** Sets the result returned from {@link shouldSessionRequestData( SessionState )}. */
    public FakeSchedulerApi setRequestBehavior(@RequestBehavior int requestBehavior) {
        this.requestBehavior = requestBehavior;
        return this;
    }
}
