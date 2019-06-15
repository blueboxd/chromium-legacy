// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.support.annotation.NonNull;

import com.google.android.libraries.feed.api.client.stream.Stream.ScrollListener;
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
import com.google.search.now.ui.action.FeedActionProto;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Implementation of {@link BasicLoggingApi} that log actions performed on the Feed,
 * and provides access to native implementation of feed logging.
 */
@JNINamespace("feed")
public class FeedLoggingBridge implements BasicLoggingApi {
    private long mNativeFeedLoggingBridge;

    /**
     * Creates a {@link FeedLoggingBridge} for accessing native feed logging
     * implementation for the current user, and initial native side bridge.
     *
     * @param profile {@link Profile} of the user we are rendering the Feed for.
     */
    public FeedLoggingBridge(Profile profile) {
        mNativeFeedLoggingBridge = nativeInit(profile);
    }

    /** Cleans up native half of this bridge. */
    public void destroy() {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        nativeDestroy(mNativeFeedLoggingBridge);
        mNativeFeedLoggingBridge = 0;
    }

    @Override
    public void onContentViewed(ContentLoggingData data) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        nativeOnContentViewed(mNativeFeedLoggingBridge, data.getPositionInStream(),
                TimeUnit.SECONDS.toMillis(data.getPublishedTimeSeconds()),
                TimeUnit.SECONDS.toMillis(data.getTimeContentBecameAvailable()), data.getScore(),
                data.isAvailableOffline());
    }

    @Override
    public void onContentDismissed(ContentLoggingData data, boolean wasCommitted) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        nativeOnContentDismissed(mNativeFeedLoggingBridge, data.getPositionInStream(),
                data.getRepresentationUri(), wasCommitted);
    }

    @Override
    public void onContentSwiped(ContentLoggingData data) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        nativeOnContentSwiped(mNativeFeedLoggingBridge);
    }

    @Override
    public void onContentClicked(ContentLoggingData data) {
        // Records content's clicks in onClientAction. When a user clicks on content, Feed libraries
        // will call both onClientAction and onContentClicked, and onClientAction will receive
        // ActionType.OPEN_URL in this case. so to avoid double counts, we records content's clicks
        // in onClientAction.
    }

    @Override
    public void onClientAction(ContentLoggingData data, @ActionType int actionType) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        recordUserAction(actionType);
        nativeOnClientAction(mNativeFeedLoggingBridge,
                feedActionToWindowOpenDisposition(actionType), data.getPositionInStream(),
                TimeUnit.SECONDS.toMillis(data.getPublishedTimeSeconds()), data.getScore(),
                data.isAvailableOffline());
    }

    @Override
    public void onContentContextMenuOpened(ContentLoggingData data) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        nativeOnContentContextMenuOpened(mNativeFeedLoggingBridge, data.getPositionInStream(),
                TimeUnit.SECONDS.toMillis(data.getPublishedTimeSeconds()), data.getScore());
    }

    @Override
    public void onMoreButtonViewed(int position) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        nativeOnMoreButtonViewed(mNativeFeedLoggingBridge, position);
    }

    @Override
    public void onMoreButtonClicked(int position) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        nativeOnMoreButtonClicked(mNativeFeedLoggingBridge, position);
    }

    @Override
    public void onNotInterestedIn(int interestType, ContentLoggingData data, boolean wasCommitted) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        // TODO(crbug.com/935602): Fail to compile when new values are added to NotInterestedInData.
        if (interestType == FeedActionProto.NotInterestedInData.RecordedInterestType.TOPIC_VALUE) {
            nativeOnNotInterestedInTopic(
                    mNativeFeedLoggingBridge, data.getPositionInStream(), wasCommitted);
        } else if (interestType
                == FeedActionProto.NotInterestedInData.RecordedInterestType.SOURCE_VALUE) {
            nativeOnNotInterestedInSource(
                    mNativeFeedLoggingBridge, data.getPositionInStream(), wasCommitted);
        }
    }

    @Override
    public void onOpenedWithContent(int timeToPopulateMs, int contentCount) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        nativeOnOpenedWithContent(mNativeFeedLoggingBridge, timeToPopulateMs, contentCount);
    }

    @Override
    public void onOpenedWithNoImmediateContent() {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        nativeOnOpenedWithNoImmediateContent(mNativeFeedLoggingBridge);
    }

    @Override
    public void onOpenedWithNoContent() {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        nativeOnOpenedWithNoContent(mNativeFeedLoggingBridge);
    }

    @Override
    public void onSpinnerStarted(@SpinnerType int spinnerType) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        nativeOnSpinnerStarted(mNativeFeedLoggingBridge, spinnerType);
    }

    @Override
    public void onSpinnerFinished(int timeShownMs, @SpinnerType int spinnerType) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        nativeOnSpinnerFinished(mNativeFeedLoggingBridge, timeShownMs, spinnerType);
    }

    @Override
    public void onSpinnerDestroyedWithoutCompleting(int timeShownMs, @SpinnerType int spinnerType) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        nativeOnSpinnerDestroyedWithoutCompleting(
                mNativeFeedLoggingBridge, timeShownMs, spinnerType);
    }

    @Override
    public void onPietFrameRenderingEvent(List<Integer> pietErrorCodes) {
        if (mNativeFeedLoggingBridge == 0) return;
        int[] pietErrorCodesArray = new int[pietErrorCodes.size()];
        for (int i = 0; i < pietErrorCodes.size(); ++i) {
            pietErrorCodesArray[i] = pietErrorCodes.get(i);
        }
        nativeOnPietFrameRenderingEvent(mNativeFeedLoggingBridge, pietErrorCodesArray);
    }

    @Override
    public void onVisualElementClicked(ElementLoggingData data, int elementType) {
        if (mNativeFeedLoggingBridge == 0) return;
        nativeOnVisualElementClicked(mNativeFeedLoggingBridge, elementType,
                data.getPositionInStream(),
                TimeUnit.SECONDS.toMillis(data.getTimeContentBecameAvailable()));
    }

    @Override
    public void onVisualElementViewed(ElementLoggingData data, int elementType) {
        if (mNativeFeedLoggingBridge == 0) return;
        nativeOnVisualElementViewed(mNativeFeedLoggingBridge, elementType,
                data.getPositionInStream(),
                TimeUnit.SECONDS.toMillis(data.getTimeContentBecameAvailable()));
    }

    @Override
    public void onInternalError(@InternalFeedError int internalError) {
        if (mNativeFeedLoggingBridge == 0) return;
        nativeOnInternalError(mNativeFeedLoggingBridge, internalError);
    }

    @Override
    public void onTokenCompleted(boolean wasSynthetic, int contentCount, int tokenCount) {
        if (mNativeFeedLoggingBridge == 0) return;
        nativeOnTokenCompleted(mNativeFeedLoggingBridge, wasSynthetic, contentCount, tokenCount);
    }

    @Override
    public void onTokenFailedToComplete(boolean wasSynthetic, int failureCount) {
        if (mNativeFeedLoggingBridge == 0) return;
        nativeOnTokenFailedToComplete(mNativeFeedLoggingBridge, wasSynthetic, failureCount);
    }

    @Override
    public void onServerRequest(@RequestReason int requestReason) {
        if (mNativeFeedLoggingBridge == 0) return;
        nativeOnServerRequest(mNativeFeedLoggingBridge, requestReason);
    }

    @Override
    public void onZeroStateShown(@ZeroStateShowReason int zeroStateShowReason) {
        if (mNativeFeedLoggingBridge == 0) return;
        nativeOnZeroStateShown(mNativeFeedLoggingBridge, zeroStateShowReason);
    }

    @Override
    public void onZeroStateRefreshCompleted(int newContentCount, int newTokenCount) {
        if (mNativeFeedLoggingBridge == 0) return;
        nativeOnZeroStateRefreshCompleted(mNativeFeedLoggingBridge, newContentCount, newTokenCount);
    }

    @Override
    public void onInitialSessionEvent(
            @SessionEvent int sessionEvent, int timeFromRegisteringMs, int sessionCount) {
        // TODO(https://crbug.com/924739): Implementation.
    }

    @Override
    public void onScroll(@ScrollType int scrollType, int distanceScrolled) {
        // TODO(https://crbug.com/924739): Implementation.
    }

    @Override
    public void onTaskFinished(@Task int task, int delayTime, int taskTime) {
        if (mNativeFeedLoggingBridge == 0) return;
        nativeOnTaskFinished(mNativeFeedLoggingBridge, task, delayTime, taskTime);
    }

    /**
     * Reports how long a user spends on the page.
     *
     * @param visitTimeMs Time spent reading the page.
     * @param isOffline If the page is viewed in offline mode or not.
     * @param returnToNtp User backed to NTP after visit the page.
     */
    public void onContentTargetVisited(long visitTimeMs, boolean isOffline, boolean returnToNtp) {
        // We cannot assume that the|mNativeFeedLoggingBridge| is always available like other
        // methods. This method is called by objects not controlled by Feed lifetimes, and destroy()
        // may have already been called if Feed is disabled by policy.
        if (mNativeFeedLoggingBridge != 0) {
            nativeOnContentTargetVisited(
                    mNativeFeedLoggingBridge, visitTimeMs, isOffline, returnToNtp);
        }
    }

    private int feedActionToWindowOpenDisposition(@ActionType int actionType) {
        switch (actionType) {
            case ActionType.OPEN_URL:
                return WindowOpenDisposition.CURRENT_TAB;
            case ActionType.OPEN_URL_INCOGNITO:
                return WindowOpenDisposition.OFF_THE_RECORD;
            case ActionType.OPEN_URL_NEW_TAB:
                return WindowOpenDisposition.NEW_BACKGROUND_TAB;
            case ActionType.OPEN_URL_NEW_WINDOW:
                return WindowOpenDisposition.NEW_WINDOW;
            case ActionType.DOWNLOAD:
                return WindowOpenDisposition.SAVE_TO_DISK;
            case ActionType.LEARN_MORE:
            case ActionType.UNKNOWN:
            default:
                return WindowOpenDisposition.UNKNOWN;
        }
    }

    private void recordUserAction(@ActionType int actionType) {
        switch (actionType) {
            case ActionType.OPEN_URL:
            case ActionType.OPEN_URL_INCOGNITO:
            case ActionType.OPEN_URL_NEW_TAB:
            case ActionType.OPEN_URL_NEW_WINDOW:
                NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_SNIPPET);
                break;
            case ActionType.LEARN_MORE:
                NewTabPageUma.recordAction(NewTabPageUma.ACTION_CLICKED_LEARN_MORE);
                break;
            case ActionType.DOWNLOAD:
            case ActionType.UNKNOWN:
            default:
                break;
        }
    }

    private void reportScrolledAfterOpen() {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        nativeReportScrolledAfterOpen(mNativeFeedLoggingBridge);
    }

    /**
     * One-shot reporter that records the first time the user scrolls in the {@link Stream}.
     */
    public static class ScrollEventReporter implements ScrollListener {
        private final FeedLoggingBridge mLoggingBridge;
        private boolean mFired;

        public ScrollEventReporter(@NonNull FeedLoggingBridge loggingBridge) {
            super();
            mLoggingBridge = loggingBridge;
        }

        @Override
        public void onScrollStateChanged(@ScrollState int state) {
            if (mFired) return;
            if (state != ScrollState.DRAGGING) return;

            mLoggingBridge.reportScrolledAfterOpen();
            mFired = true;
        }

        @Override
        public void onScrolled(int dx, int dy) {}
    }

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeFeedLoggingBridge);
    private native void nativeOnContentViewed(long nativeFeedLoggingBridge, int position,
            long publishedTimeMs, long timeContentBecameAvailableMs, float score,
            boolean isAvailableOffline);
    private native void nativeOnContentDismissed(
            long nativeFeedLoggingBridge, int position, String uri, boolean wasCommitted);
    private native void nativeOnContentSwiped(long nativeFeedLoggingBridge);
    private native void nativeOnClientAction(long nativeFeedLoggingBridge,
            int windowOpenDisposition, int position, long publishedTimeMs, float score,
            boolean isAvailableOffline);
    private native void nativeOnContentContextMenuOpened(
            long nativeFeedLoggingBridge, int position, long publishedTimeMs, float score);
    private native void nativeOnMoreButtonViewed(long nativeFeedLoggingBridge, int position);
    private native void nativeOnMoreButtonClicked(long nativeFeedLoggingBridge, int position);
    private native void nativeOnNotInterestedInSource(
            long nativeFeedLoggingBridge, int position, boolean wasCommitted);
    private native void nativeOnNotInterestedInTopic(
            long nativeFeedLoggingBridge, int position, boolean wasCommitted);
    private native void nativeOnOpenedWithContent(
            long nativeFeedLoggingBridge, int timeToPopulateMs, int contentCount);
    private native void nativeOnOpenedWithNoImmediateContent(long nativeFeedLoggingBridge);
    private native void nativeOnOpenedWithNoContent(long nativeFeedLoggingBridge);
    private native void nativeOnSpinnerStarted(long nativeFeedLoggingBridge, int spinnerType);
    private native void nativeOnSpinnerFinished(
            long nativeFeedLoggingBridge, long spinnerShownTimeMs, int spinnerType);
    private native void nativeOnSpinnerDestroyedWithoutCompleting(
            long nativeFeedLoggingBridge, long spinnerShownTimeMs, int spinnerType);
    private native void nativeOnPietFrameRenderingEvent(
            long nativeFeedLoggingBridge, int[] pietErrorCodes);
    private native void nativeOnVisualElementClicked(long nativeFeedLoggingBridge, int elementType,
            int position, long timeContentBecameAvailableMs);
    private native void nativeOnVisualElementViewed(long nativeFeedLoggingBridge, int elementType,
            int position, long timeContentBecameAvailableMs);
    private native void nativeOnInternalError(long nativeFeedLoggingBridge, int internalError);
    private native void nativeOnTokenCompleted(
            long nativeFeedLoggingBridge, boolean wasSynthetic, int contentCount, int tokenCount);
    private native void nativeOnTokenFailedToComplete(
            long nativeFeedLoggingBridge, boolean wasSynthetic, int failureCount);
    private native void nativeOnServerRequest(long nativeFeedLoggingBridge, int requestReason);
    private native void nativeOnZeroStateShown(
            long nativeFeedLoggingBridge, int zeroStateShowReason);
    private native void nativeOnZeroStateRefreshCompleted(
            long nativeFeedLoggingBridge, int newContentCount, int newTokenCount);
    private native void nativeOnTaskFinished(
            long nativeFeedLoggingBridge, int task, int delayTimeMs, int taskTimeMs);
    private native void nativeOnContentTargetVisited(
            long nativeFeedLoggingBridge, long visitTimeMs, boolean isOffline, boolean returnToNtp);
    private native void nativeReportScrolledAfterOpen(long nativeFeedLoggingBridge);
}
