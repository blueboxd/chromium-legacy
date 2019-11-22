// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.testing.host.logging;

import com.google.android.libraries.feed.api.host.logging.ActionType;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.ContentLoggingData;
import com.google.android.libraries.feed.api.host.logging.ElementLoggingData;
import com.google.android.libraries.feed.api.host.logging.InternalFeedError;
import com.google.android.libraries.feed.api.host.logging.RequestReason;
import com.google.android.libraries.feed.api.host.logging.ScrollType;
import com.google.android.libraries.feed.api.host.logging.SessionEvent;
import com.google.android.libraries.feed.api.host.logging.SpinnerType;
import com.google.android.libraries.feed.api.host.logging.Task;
import com.google.android.libraries.feed.api.host.logging.ZeroStateShowReason;

import java.util.List;

/** Fake implementation of {@link BasicLoggingApi}. */
public class FakeBasicLoggingApi implements BasicLoggingApi {
    @InternalFeedError
    public int lastInternalError = InternalFeedError.NEXT_VALUE;
    @RequestReason
    public int serverRequestReason = RequestReason.UNKNOWN;
    @Task
    public int lastTaskLogged;
    public int lastTaskDelay;
    public int lastTaskTime;

    public FakeBasicLoggingApi() {}

    @Override
    public void onContentViewed(ContentLoggingData data) {}

    @Override
    public void onContentDismissed(ContentLoggingData data, boolean wasCommitted) {}

    @Override
    public void onContentSwiped(ContentLoggingData data) {}

    @Override
    public void onContentClicked(ContentLoggingData data) {}

    @Override
    public void onClientAction(ContentLoggingData data, @ActionType int actionType) {}

    @Override
    public void onContentContextMenuOpened(ContentLoggingData data) {}

    @Override
    public void onInitialSessionEvent(
            @SessionEvent int sessionEvent, int timeFromRegisteringMs, int sessionCount) {}

    @Override
    public void onMoreButtonViewed(int position) {}

    @Override
    public void onMoreButtonClicked(int position) {}

    @Override
    public void onNotInterestedIn(int type, ContentLoggingData data, boolean wasCommitted) {}

    @Override
    public void onOpenedWithContent(int timeToPopulateMs, int contentCount) {}

    @Override
    public void onOpenedWithNoImmediateContent() {}

    @Override
    public void onOpenedWithNoContent() {}

    @Override
    public void onServerRequest(@RequestReason int requestReason) {
        serverRequestReason = requestReason;
    }

    @Override
    public void onSpinnerDestroyedWithoutCompleting(int timeShownMs, @SpinnerType int spinnerType) {
    }

    @Override
    public void onSpinnerFinished(int timeShownMs, @SpinnerType int spinnerType) {}

    @Override
    public void onSpinnerStarted(@SpinnerType int spinnerType) {}

    @Override
    public void onTokenCompleted(boolean wasSynthetic, int contentCount, int tokenCount) {}

    @Override
    public void onTokenFailedToComplete(boolean wasSynthetic, int failureCount) {}

    @Override
    public void onPietFrameRenderingEvent(List<Integer> pietErrorCodes) {}

    @Override
    public void onVisualElementClicked(ElementLoggingData data, int elementType) {}

    @Override
    public void onVisualElementViewed(ElementLoggingData data, int elementType) {}

    @Override
    public void onInternalError(@InternalFeedError int internalError) {
        lastInternalError = internalError;
    }

    @Override
    public void onZeroStateRefreshCompleted(int newContentCount, int newTokenCount) {}

    @Override
    public void onZeroStateShown(@ZeroStateShowReason int zeroStateShowReason) {}

    @Override
    public void onScroll(@ScrollType int scrollType, int distanceScrolled) {}

    @Override
    public void onTaskFinished(@Task int task, int delayTime, int taskTime) {
        lastTaskDelay = delayTime;
        lastTaskTime = taskTime;
        lastTaskLogged = task;
    }
}
