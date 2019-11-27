// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.conformance.scheduler;

import org.junit.Test;

import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi;
import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi.SessionState;

public abstract class SchedulerConformanceTest {
    private static final int NOT_FOUND = 404;
    private static final int SERVER_ERROR = 500;

    protected SchedulerApi mScheduler;

    @Test
    public void shouldSessionRequestData() {
        // Should not throw error
        mScheduler.shouldSessionRequestData(new SessionState(false, 0, false));
    }

    @Test
    public void onReceiveNewContent() {
        // Should not throw error
        mScheduler.onReceiveNewContent(System.currentTimeMillis());
    }

    @Test
    public void onRequestError_notFound() {
        // Should not throw error
        mScheduler.onRequestError(NOT_FOUND);
    }

    @Test
    public void onRequestError_serverError() {
        // Should not throw error
        mScheduler.onRequestError(SERVER_ERROR);
    }
}
