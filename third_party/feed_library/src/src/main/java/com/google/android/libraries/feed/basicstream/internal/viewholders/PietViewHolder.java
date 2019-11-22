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
    private final CardConfiguration cardConfiguration;
    private final FrameLayout cardView;
    private final ScrollObservable scrollObservable;
    private final FrameAdapter frameAdapter;
    private final VisibilityMonitor visibilityMonitor;
    private final View viewport;
    private boolean bound;

    /*@Nullable*/ private ActionParser actionParser;
    /*@Nullable*/ private LoggingListener loggingListener;
    /*@Nullable*/ private StreamActionApi streamActionApi;
    /*@Nullable*/ private FeedActionPayload swipeAction;
    /*@Nullable*/ private PietViewActionScrollObserver scrollObserver;

    public PietViewHolder(CardConfiguration cardConfiguration, FrameLayout cardView,
            PietManager pietManager, ScrollObservable scrollObservable, View viewport,
            Context context, Configuration configuration, PietEventLogger eventLogger) {
        super(cardView);
        this.cardConfiguration = cardConfiguration;
        this.cardView = cardView;
        this.scrollObservable = scrollObservable;
        this.viewport = viewport;
        cardView.setId(R.id.feed_content_card);
        this.frameAdapter = pietManager.createPietFrameAdapter(
                () -> cardView, (action, actionType, frame, view, logData) -> {
                    if (actionParser == null) {
                        Logger.wtf(TAG, "Action being performed while unbound.");
                        return;
                    }

                    if (actionType == ActionType.CLICK) {
                        getLoggingListener().onContentClicked();
                    }
                    getActionParser().parseAction(action, getStreamActionApi(), view, logData,
                            ActionSourceConverter.convertPietAction(actionType));
                }, eventLogger::logEvents, context);
        visibilityMonitor = createVisibilityMonitor(cardView, configuration);
        cardView.addView(frameAdapter.getFrameContainer());
    }

    public void bind(Frame frame, List<PietSharedState> pietSharedStates,
            StreamActionApi streamActionApi, FeedActionPayload swipeAction,
            LoggingListener loggingListener, ActionParser actionParser) {
        if (bound) {
            return;
        }
        visibilityMonitor.setListener(loggingListener);
        this.loggingListener = loggingListener;
        this.streamActionApi = streamActionApi;
        this.swipeAction = swipeAction;
        this.actionParser = actionParser;
        scrollObserver = new PietViewActionScrollObserver(
                frameAdapter, viewport, scrollObservable, loggingListener);
        // Need to reset padding here.  Setting a background can affect padding so if we switch from
        // a background which has padding to one that does not, then the padding needs to be
        // removed.
        cardView.setPadding(0, 0, 0, 0);

        cardView.setBackground(cardConfiguration.getCardBackground());

        ViewGroup.LayoutParams layoutParams = cardView.getLayoutParams();
        if (layoutParams == null) {
            layoutParams = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
            cardView.setLayoutParams(layoutParams);
        } else if (!(layoutParams instanceof MarginLayoutParams)) {
            layoutParams = new LayoutParams(layoutParams);
            cardView.setLayoutParams(layoutParams);
        }
        LayoutUtils.setMarginsRelative((MarginLayoutParams) layoutParams,
                cardConfiguration.getCardStartMargin(), 0, cardConfiguration.getCardEndMargin(),
                cardConfiguration.getCardBottomMargin());

        frameAdapter.bindModel(frame,
                0, // TODO: set the frame width here
                null, pietSharedStates);
        if (scrollObserver != null) {
            scrollObservable.addScrollObserver(scrollObserver);
        }

        bound = true;
    }

    @Override
    public void unbind() {
        if (!bound) {
            return;
        }

        frameAdapter.unbindModel();
        actionParser = null;
        loggingListener = null;
        streamActionApi = null;
        swipeAction = null;
        visibilityMonitor.setListener(null);
        if (scrollObserver != null) {
            scrollObservable.removeScrollObserver(scrollObserver);
            scrollObserver.uninstallFirstDrawTrigger();
        }
        bound = false;
    }

    @Override
    public boolean canSwipe() {
        return swipeAction != null && !swipeAction.equals(FeedActionPayload.getDefaultInstance());
    }

    @Override
    public void onSwiped() {
        if (swipeAction == null || actionParser == null) {
            Logger.wtf(TAG, "Swipe performed on unbound ViewHolder.");
            return;
        }
        actionParser.parseFeedActionPayload(
                swipeAction, getStreamActionApi(), itemView, ActionSource.SWIPE);

        if (loggingListener == null) {
            Logger.wtf(TAG, "Logging listener is null. Swipe perfomred on unbound ViewHolder.");
            return;
        }
        loggingListener.onContentSwiped();
    }

    @VisibleForTesting
    VisibilityMonitor createVisibilityMonitor(
            /*@UnderInitialization*/ PietViewHolder this, View view, Configuration configuration) {
        return new VisibilityMonitor(view, configuration);
    }

    private LoggingListener getLoggingListener(/*@UnknownInitialization*/ PietViewHolder this) {
        return checkNotNull(loggingListener,
                "Logging listener can only be retrieved once view holder has been bound.");
    }

    private StreamActionApi getStreamActionApi(/*@UnknownInitialization*/ PietViewHolder this) {
        return checkNotNull(streamActionApi,
                "Stream action api can only be retrieved once view holder has been bound.");
    }

    private ActionParser getActionParser(/*@UnknownInitialization*/ PietViewHolder this) {
        return checkNotNull(actionParser,
                "Action parser can only be retrieved once view holder has been bound");
    }

    static class PietViewActionScrollObserver extends PietScrollObserver {
        private final LoggingListener loggingListener;

        PietViewActionScrollObserver(FrameAdapter frameAdapter, View viewport,
                ScrollObservable scrollObservable, LoggingListener loggingListener) {
            super(frameAdapter, viewport, scrollObservable);
            this.loggingListener = loggingListener;
        }

        @Override
        public void onScrollStateChanged(
                View view, String featureId, int newState, long timestamp) {
            super.onScrollStateChanged(view, featureId, newState, timestamp);
            loggingListener.onScrollStateChanged(
                    ScrollListenerNotifier.convertRecyclerViewScrollStateToListenerState(newState));
        }
    }
}
