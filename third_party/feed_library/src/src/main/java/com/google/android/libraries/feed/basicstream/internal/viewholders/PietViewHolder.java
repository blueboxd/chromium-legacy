// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal.viewholders;

import static com.google.android.libraries.feed.common.Validators.checkNotNull;

import android.content.Context;
import android.support.annotation.VisibleForTesting;
import android.support.v7.widget.RecyclerView.LayoutParams;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import com.google.android.libraries.feed.api.host.action.StreamActionApi;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.stream.CardConfiguration;
import com.google.android.libraries.feed.api.internal.actionparser.ActionParser;
import com.google.android.libraries.feed.api.internal.actionparser.ActionSource;
import com.google.android.libraries.feed.api.internal.actionparser.ActionSourceConverter;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.common.ui.LayoutUtils;
import com.google.android.libraries.feed.piet.FrameAdapter;
import com.google.android.libraries.feed.piet.PietManager;
import com.google.android.libraries.feed.piet.host.ActionHandler.ActionType;
import com.google.android.libraries.feed.sharedstream.logging.LoggingListener;
import com.google.android.libraries.feed.sharedstream.logging.VisibilityMonitor;
import com.google.android.libraries.feed.sharedstream.piet.PietEventLogger;
import com.google.android.libraries.feed.sharedstream.publicapi.scroll.ScrollObservable;
import com.google.android.libraries.feed.sharedstream.scroll.PietScrollObserver;
import com.google.android.libraries.feed.sharedstream.scroll.ScrollListenerNotifier;
import com.google.search.now.ui.action.FeedActionPayloadProto.FeedActionPayload;
import com.google.search.now.ui.piet.PietProto.Frame;
import com.google.search.now.ui.piet.PietProto.PietSharedState;

import java.util.List;

/**
 * {@link android.support.v7.widget.RecyclerView.ViewHolder} for {@link
 * com.google.search.now.ui.stream.StreamStructureProto.PietContent}.
 */
public class PietViewHolder extends FeedViewHolder implements SwipeableViewHolder {
    private static final String TAG = "PietViewHolder";
    private final CardConfiguration mCardConfiguration;
    private final FrameLayout mCardView;
    private final ScrollObservable mScrollObservable;
    private final FrameAdapter mFrameAdapter;
    private final VisibilityMonitor mVisibilityMonitor;
    private final View mViewport;
    private boolean mBound;

    /*@Nullable*/ private ActionParser mActionParser;
    /*@Nullable*/ private LoggingListener mLoggingListener;
    /*@Nullable*/ private StreamActionApi mStreamActionApi;
    /*@Nullable*/ private FeedActionPayload mSwipeAction;
    /*@Nullable*/ private PietViewActionScrollObserver mScrollobserver;

    public PietViewHolder(CardConfiguration cardConfiguration, FrameLayout cardView,
            PietManager pietManager, ScrollObservable scrollObservable, View viewport,
            Context context, Configuration configuration, PietEventLogger eventLogger) {
        super(cardView);
        this.mCardConfiguration = cardConfiguration;
        this.mCardView = cardView;
        this.mScrollObservable = scrollObservable;
        this.mViewport = viewport;
        cardView.setId(R.id.feed_content_card);
        this.mFrameAdapter = pietManager.createPietFrameAdapter(
                () -> cardView, (action, actionType, frame, view, logData) -> {
                    if (mActionParser == null) {
                        Logger.wtf(TAG, "Action being performed while unbound.");
                        return;
                    }

                    if (actionType == ActionType.CLICK) {
                        getLoggingListener().onContentClicked();
                    }
                    getActionParser().parseAction(action, getStreamActionApi(), view, logData,
                            ActionSourceConverter.convertPietAction(actionType));
                }, eventLogger::logEvents, context);
        mVisibilityMonitor = createVisibilityMonitor(cardView, configuration);
        cardView.addView(mFrameAdapter.getFrameContainer());
    }

    public void bind(Frame frame, List<PietSharedState> pietSharedStates,
            StreamActionApi streamActionApi, FeedActionPayload swipeAction,
            LoggingListener loggingListener, ActionParser actionParser) {
        if (mBound) {
            return;
        }
        mVisibilityMonitor.setListener(loggingListener);
        this.mLoggingListener = loggingListener;
        this.mStreamActionApi = streamActionApi;
        this.mSwipeAction = swipeAction;
        this.mActionParser = actionParser;
        mScrollobserver = new PietViewActionScrollObserver(
                mFrameAdapter, mViewport, mScrollObservable, loggingListener);
        // Need to reset padding here.  Setting a background can affect padding so if we switch from
        // a background which has padding to one that does not, then the padding needs to be
        // removed.
        mCardView.setPadding(0, 0, 0, 0);

        mCardView.setBackground(mCardConfiguration.getCardBackground());

        ViewGroup.LayoutParams layoutParams = mCardView.getLayoutParams();
        if (layoutParams == null) {
            layoutParams = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
            mCardView.setLayoutParams(layoutParams);
        } else if (!(layoutParams instanceof MarginLayoutParams)) {
            layoutParams = new LayoutParams(layoutParams);
            mCardView.setLayoutParams(layoutParams);
        }
        LayoutUtils.setMarginsRelative((MarginLayoutParams) layoutParams,
                mCardConfiguration.getCardStartMargin(), 0, mCardConfiguration.getCardEndMargin(),
                mCardConfiguration.getCardBottomMargin());

        mFrameAdapter.bindModel(frame,
                0, // TODO: set the frame width here
                null, pietSharedStates);
        if (mScrollobserver != null) {
            mScrollObservable.addScrollObserver(mScrollobserver);
        }

        mBound = true;
    }

    @Override
    public void unbind() {
        if (!mBound) {
            return;
        }

        mFrameAdapter.unbindModel();
        mActionParser = null;
        mLoggingListener = null;
        mStreamActionApi = null;
        mSwipeAction = null;
        mVisibilityMonitor.setListener(null);
        if (mScrollobserver != null) {
            mScrollObservable.removeScrollObserver(mScrollobserver);
            mScrollobserver.uninstallFirstDrawTrigger();
        }
        mBound = false;
    }

    @Override
    public boolean canSwipe() {
        return mSwipeAction != null && !mSwipeAction.equals(FeedActionPayload.getDefaultInstance());
    }

    @Override
    public void onSwiped() {
        if (mSwipeAction == null || mActionParser == null) {
            Logger.wtf(TAG, "Swipe performed on unbound ViewHolder.");
            return;
        }
        mActionParser.parseFeedActionPayload(
                mSwipeAction, getStreamActionApi(), itemView, ActionSource.SWIPE);

        if (mLoggingListener == null) {
            Logger.wtf(TAG, "Logging listener is null. Swipe perfomred on unbound ViewHolder.");
            return;
        }
        mLoggingListener.onContentSwiped();
    }

    @VisibleForTesting
    VisibilityMonitor createVisibilityMonitor(
            /*@UnderInitialization*/ PietViewHolder this, View view, Configuration configuration) {
        return new VisibilityMonitor(view, configuration);
    }

    private LoggingListener getLoggingListener(/*@UnknownInitialization*/ PietViewHolder this) {
        return checkNotNull(mLoggingListener,
                "Logging listener can only be retrieved once view holder has been bound.");
    }

    private StreamActionApi getStreamActionApi(/*@UnknownInitialization*/ PietViewHolder this) {
        return checkNotNull(mStreamActionApi,
                "Stream action api can only be retrieved once view holder has been bound.");
    }

    private ActionParser getActionParser(/*@UnknownInitialization*/ PietViewHolder this) {
        return checkNotNull(mActionParser,
                "Action parser can only be retrieved once view holder has been bound");
    }

    static class PietViewActionScrollObserver extends PietScrollObserver {
        private final LoggingListener mLoggingListener;

        PietViewActionScrollObserver(FrameAdapter frameAdapter, View viewport,
                ScrollObservable scrollObservable, LoggingListener loggingListener) {
            super(frameAdapter, viewport, scrollObservable);
            this.mLoggingListener = loggingListener;
        }

        @Override
        public void onScrollStateChanged(
                View view, String featureId, int newState, long timestamp) {
            super.onScrollStateChanged(view, featureId, newState, timestamp);
            mLoggingListener.onScrollStateChanged(
                    ScrollListenerNotifier.convertRecyclerViewScrollStateToListenerState(newState));
        }
    }
}
