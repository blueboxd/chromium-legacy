// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.library.hostimpl.logging;

import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.ContentLoggingData;
import org.chromium.chrome.browser.feed.library.api.host.logging.ElementLoggingData;
import org.chromium.chrome.browser.feed.library.api.host.logging.Task;

import java.util.List;

/** {@link BasicLoggingApi} implementation that no-ops for all methods. */
public final class NoOpBasicLoggingApi implements BasicLoggingApi {
    @Override
    public void onContentViewed(ContentLoggingData data) {}

    @Override
    public void onContentDismissed(ContentLoggingData data, boolean wasCommitted) {}

    @Override
    public void onContentSwiped(ContentLoggingData data) {}

    @Override
    public void onContentClicked(ContentLoggingData data) {}

    @Override
    public void onClientAction(ContentLoggingData data, int actionType) {}

    @Override
    public void onContentContextMenuOpened(ContentLoggingData data) {}

    @Override
    public void onMoreButtonViewed(int position) {}

    @Override
    public void onMoreButtonClicked(int position) {}

    @Override
    public void onNotInterestedIn(int interestType, ContentLoggingData data, boolean wasCommitted) {
    }

    @Override
    public void onOpenedWithContent(int timeToPopulateMs, int contentCount) {}

    @Override
    public void onOpenedWithNoImmediateContent() {}

    @Override
    public void onOpenedWithNoContent() {}

    @Override
    public void onSpinnerStarted(int spinnerType) {}

    @Override
    public void onSpinnerFinished(int timeShownMs, int spinnerType) {}

    @Override
    public void onSpinnerDestroyedWithoutCompleting(int timeShownMs, int spinnerType) {}

    @Override
    public void onPietFrameRenderingEvent(List<Integer> pietErrorCodes) {}

    @Override
    public void onVisualElementClicked(ElementLoggingData data, int elementType) {}

    @Override
    public void onVisualElementViewed(ElementLoggingData data, int elementType) {}

    @Override
    public void onInternalError(int internalError) {}

    @Override
    public void onTokenCompleted(boolean wasSynthetic, int contentCount, int tokenCount) {}

    @Override
    public void onTokenFailedToComplete(boolean wasSynthetic, int failureCount) {}

    @Override
    public void onServerRequest(int requestReason) {}

    @Override
    public void onZeroStateShown(int zeroStateShowReason) {}

    @Override
    public void onZeroStateRefreshCompleted(int newContentCount, int newTokenCount) {}

    @Override
    public void onInitialSessionEvent(
            int sessionEvent, int timeFromRegisteringMs, int sessionCount) {}

    @Override
    public void onScroll(int scrollType, int distanceScrolled) {}

    @Override
    public void onTaskFinished(@Task int task, int delayTime, int taskTime) {}
}
