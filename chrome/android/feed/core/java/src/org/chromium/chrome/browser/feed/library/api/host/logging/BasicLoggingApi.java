// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.logging;

import java.util.List;

/** The BasicLoggingApi is used by the Feed to log actions performed on the Feed. */
public interface BasicLoggingApi {
    /**
     * Called when a section of content (generally a card) comes into view. Will not get called by
     * Stream if content has already been logged. If the stream is recreated this will get logged
     * again.
     *
     * <p>Content viewed will get logged as soon as content is 2/3 (will be configurable in {@link
     * org.chromium.chrome.browser.feed.library.api.host.config.Configuration.
     *    ConfigKey#VIEW_LOG_THRESHOLD})
     * visible in view-port. This means logging will not be tied to scroll state events (scroll
     * start/stop) but rather raw scrolling events.
     */
    void onContentViewed(ContentLoggingData data);

    /**
     * Called when content has been dismissed by a user. This could be done via a swipe or through
     * the context menu.
     *
     * @param data data for content on which swipe was performed.
     * @param wasCommitted true if the action was committed and thus important to the user model
     *         false
     *     if the action was undone.
     */
    void onContentDismissed(ContentLoggingData data, boolean wasCommitted);

    /**
     * Called when content has been swiped by a user.
     *
     * @param data data for content on which swipe was performed.
     */
    void onContentSwiped(ContentLoggingData data);

    /** Called when content is clicked/tapped. */
    void onContentClicked(ContentLoggingData data);

    /**
     * Called when a client action has been performed on a piece of content.
     *
     * @param data data for content on which action was performed.
     * @param actionType describes the type of action which is being performed by user.
     */
    void onClientAction(ContentLoggingData data, @ActionType int actionType);

    /** Called when the context menu for content has been opened. */
    void onContentContextMenuOpened(ContentLoggingData data);

    /**
     * Called when the more button appears at the bottom of the screen. This is a view on the more
     * button which is created from continuation features (paging). A view from the more button due
     * to having zero cards will not trigger this event.
     *
     * @param position index of the more button in the Stream.
     */
    void onMoreButtonViewed(int position);

    /**
     * Called when the more button appears at the bottom of the screen and is clicked. This is a
     * click on the more button which is created from continuation features (paging). A click from
     * the more button due to having zero cards will not trigger this event.
     *
     * @param position index of the more button in the Stream.
     */
    void onMoreButtonClicked(int position);

    /**
     * Called when the user has indicated they aren't interested in the story's topic.
     *
     * @param interestType type of content a user is not interested in
     * @param data data for content on which swipe was performed.
     * @param wasCommitted true if the action was committed and thus important to the user model
     *         false
     *     if the action was undone.
     */
    void onNotInterestedIn(int interestType, ContentLoggingData data, boolean wasCommitted);

    /**
     * Called when Stream is shown and content was shown to the user. Content could have been cached
     * or a network fetch may have been needed.
     *
     * @param timeToPopulateMs time in milliseconds, since {@link
     *     org.chromium.chrome.browser.feed.library.api.client.stream.Stream#onShow()}, it took to
     * show content to the user. This does not include time to render but time to populate data in
     * the UI.
     * @param contentCount Count of content shown to user. This will generally be the number of
     *         cards.
     */
    void onOpenedWithContent(int timeToPopulateMs, int contentCount);

    /**
     * Called when Stream was shown and no content was immediately available to be shown. Content
     * may have been shown after a network fetch.
     */
    void onOpenedWithNoImmediateContent();

    /**
     * Called when Stream was shown and no content could be shown to the user at all. This means
     * that there was no cached content and a network request to fetch new content was not allowed,
     * could not complete, or failed.
     */
    void onOpenedWithNoContent();

    /**
     * Called when a loading spinner starts showing.
     *
     * @param spinnerType type of spinner that is shown.
     */
    void onSpinnerStarted(@SpinnerType int spinnerType);

    /**
     * Called when a loading spinner finishes showing.
     *
     * @param timeShownMs time in milliseconds that the spinner was shown before completing.
     * @param spinnerType type of spinner that was shown.
     */
    void onSpinnerFinished(int timeShownMs, @SpinnerType int spinnerType);

    /**
     * Called when a spinner is destroyed without completing.
     *
     * @param timeShownMs time in milliseconds that the spinner was shown before being destroyed.
     * @param spinnerType type of spinner that was shown.
     */
    void onSpinnerDestroyedWithoutCompleting(int timeShownMs, @SpinnerType int spinnerType);

    /**
     * Called when Piet wants to report events that occurred during Frame rendering.
     *
     * @param pietErrorCodes int versions of values from Piet {@link
     *     org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode} enum
     */
    void onPietFrameRenderingEvent(List<Integer> pietErrorCodes);

    /**
     * Called when a user has clicked on an element. This is similar to onContentClicked, but
     * records the specific elementType that was clicked.
     *
     * @param data data for content on which action was performed.
     * @param elementType describes the server-defined type of element which was clicked.
     *         Corresponds
     *     to the int numbers of the server-defined {@link
     *     org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata.
     *       ElementType}
     * value.
     */
    void onVisualElementClicked(ElementLoggingData data, int elementType);

    /**
     * Called when a user has viewed an element. This is similar to onContentViewed, but records the
     * view after it has been on the screen for a given amount of time instead of on predraw.
     *
     * @param data data for content on which action was performed.
     * @param elementType describes the type of element which was clicked.
     */
    void onVisualElementViewed(ElementLoggingData data, int elementType);

    /**
     * Reports an internal error. This error should be logged by hosts, but not handled. In
     * particular, the host should not crash if this method is called.
     */
    void onInternalError(@InternalFeedError int internalError);

    /**
     * Called when a token successfully completes.
     *
     * @param wasSynthetic whether the token was synthetic.
     * @param contentCount how many top level features were in the response, typically clusters.
     * @param tokenCount how many tokens were in the response.
     */
    void onTokenCompleted(boolean wasSynthetic, int contentCount, int tokenCount);

    /**
     * Called when a token fails to complete.
     *
     * @param wasSynthetic whether the token was synthetic.
     * @param failureCount How many times the token has failed to complete, including this failure.
     */
    void onTokenFailedToComplete(boolean wasSynthetic, int failureCount);

    /**
     * Called when a request is made.
     *
     * @param requestReason the reason the request was made.
     */
    void onServerRequest(@RequestReason int requestReason);

    /**
     * Logged when the zero state is shown. Shown in this context specifically means created, but
     * doesn't necessarily indicate that the user saw the zero state.
     *
     * @param zeroStateShowReason the reason the zero state was shown.
     */
    void onZeroStateShown(@ZeroStateShowReason int zeroStateShowReason);

    /**
     * Called when a refresh from a zero state is completed.
     *
     * @param newContentCount Number of content created. Note: content here refers to content from
     *         the
     *     server. If the zero state refresh completes to show another zero state, this value would
     * be zero.
     * @param newTokenCount Number of tokens created.
     */
    void onZeroStateRefreshCompleted(int newContentCount, int newTokenCount);

    /**
     * Called with the first event that occurs after the UI registers for a new session. If, for
     * example, multiple errors occurred repeatedly due to a user pressing a refresh button on the
     * zero-state UI, those would not be reported by this, as the UI would have only registered for
     * a new session before the first error.
     *
     * @param sessionEvent the event that first occurred after the UI registered for a session.
     * @param timeFromRegisteringMs how long it took, in milliseconds, for the event to occur after
     *     the UI registered for a session.
     * @param sessionCount the number of sessions that have been requested by the UI. This would
     *         never
     *     be reset until the UI is destroyed, and would start at {@literal 1}.
     */
    void onInitialSessionEvent(
            @SessionEvent int sessionEvent, int timeFromRegisteringMs, int sessionCount);

    /**
     * Called when a client scrolls in the feed
     *
     * @param scrollType describes the type of scroll which is being performed by user.
     * @param distanceScrolled distance in px the user scrolled for this particular scroll.
     */
    void onScroll(@ScrollType int scrollType, int distanceScrolled);

    /**
     * Called when a task finishes executing.
     *
     * @param task the task that is being run.
     * @param delayTime how long it took from enqueuing the task to when it was run.
     * @param taskTime how long it took from the task starting to when it finished executing.
     */
    void onTaskFinished(@Task int task, int delayTime, int taskTime);
}
