// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal.drivers;

import static com.google.android.libraries.feed.common.Validators.checkState;

import android.support.annotation.VisibleForTesting;
import android.support.v7.widget.RecyclerView;

import com.google.android.libraries.feed.api.client.knowncontent.ContentMetadata;
import com.google.android.libraries.feed.api.client.stream.Stream.ContentChangedListener;
import com.google.android.libraries.feed.api.host.action.ActionApi;
import com.google.android.libraries.feed.api.host.action.StreamActionApi;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.ContentLoggingData;
import com.google.android.libraries.feed.api.host.logging.InternalFeedError;
import com.google.android.libraries.feed.api.host.stream.TooltipApi;
import com.google.android.libraries.feed.api.internal.actionmanager.ActionManager;
import com.google.android.libraries.feed.api.internal.actionparser.ActionParser;
import com.google.android.libraries.feed.api.internal.actionparser.ActionParserFactory;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.basicstream.internal.actions.StreamActionApiImpl;
import com.google.android.libraries.feed.basicstream.internal.actions.ViewElementActionHandler;
import com.google.android.libraries.feed.basicstream.internal.pendingdismiss.ClusterPendingDismissHelper;
import com.google.android.libraries.feed.basicstream.internal.viewholders.FeedViewHolder;
import com.google.android.libraries.feed.basicstream.internal.viewholders.PietViewHolder;
import com.google.android.libraries.feed.basicstream.internal.viewholders.ViewHolderType;
import com.google.android.libraries.feed.basicstream.internal.viewloggingupdater.ResettableOneShotVisibilityLoggingListener;
import com.google.android.libraries.feed.basicstream.internal.viewloggingupdater.ViewLoggingUpdater;
import com.google.android.libraries.feed.common.concurrent.CancelableTask;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.common.functional.Supplier;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.sharedstream.constants.Constants;
import com.google.android.libraries.feed.sharedstream.contextmenumanager.ContextMenuManager;
import com.google.android.libraries.feed.sharedstream.logging.LoggingListener;
import com.google.android.libraries.feed.sharedstream.logging.StreamContentLoggingData;
import com.google.android.libraries.feed.sharedstream.offlinemonitor.StreamOfflineMonitor;
import com.google.search.now.feed.client.StreamDataProto.StreamSharedState;
import com.google.search.now.ui.action.FeedActionPayloadProto.FeedActionPayload;
import com.google.search.now.ui.piet.PietProto.Frame;
import com.google.search.now.ui.piet.PietProto.PietSharedState;
import com.google.search.now.ui.stream.StreamStructureProto;
import com.google.search.now.ui.stream.StreamStructureProto.Content;
import com.google.search.now.ui.stream.StreamStructureProto.PietContent;
import com.google.search.now.ui.stream.StreamStructureProto.RepresentationData;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/** {@link FeatureDriver} for content. */
public class ContentDriver
        extends LeafFeatureDriver implements LoggingListener, ViewElementActionHandler {
    private static final String TAG = "ContentDriver";

    private final BasicLoggingApi mBasicLoggingApi;
    private final List<PietSharedState> mPietSharedStates;
    private final Frame mFrame;
    private final StreamActionApi mStreamActionApi;
    private final FeedActionPayload mSwipeAction;
    private final String mContentId;
    private final StreamOfflineMonitor mStreamOfflineMonitor;
    private final OfflineStatusConsumer mOfflineStatusConsumer;
    private final String mContentUrl;
    private final ContentChangedListener mContentChangedListener;
    private final ActionParser mActionParser;
    private final MainThreadRunner mMainThreadRunner;
    private final long mViewDelayMs;
    private final HashMap<Integer, CancelableTask> mViewActionTaskMap = new HashMap<>();
    private final ViewLoggingUpdater mViewLoggingUpdater;
    private final ResettableOneShotVisibilityLoggingListener mLoggingListener;

    private StreamContentLoggingData mContentLoggingData;
    private boolean mAvailableOffline;
    /*@Nullable*/ private PietViewHolder mViewHolder;

    // TODO: Remove these suppressions when drivers have a proper lifecycle.
    @SuppressWarnings(
            {"nullness:argument.type.incompatible", "nullness:assignment.type.incompatible"})
    ContentDriver(ActionApi actionApi, ActionManager actionManager,
            ActionParserFactory actionParserFactory, BasicLoggingApi basicLoggingApi,
            ModelFeature contentFeatureModel, ModelProvider modelProvider, int position,
            FeedActionPayload swipeAction, ClusterPendingDismissHelper clusterPendingDismissHelper,
            StreamOfflineMonitor streamOfflineMonitor,
            ContentChangedListener contentChangedListener, ContextMenuManager contextMenuManager,
            MainThreadRunner mainThreadRunner, Configuration configuration,
            ViewLoggingUpdater viewLoggingUpdater, TooltipApi tooltipApi) {
        this.mMainThreadRunner = mainThreadRunner;
        mViewDelayMs = configuration.getValueOrDefault(
                ConfigKey.VIEW_MIN_TIME_MS, Constants.VIEW_MIN_TIME_MS_DEFAULT);
        Content content = contentFeatureModel.getStreamFeature().getContent();

        PietContent pietContent = getPietContent(content);
        this.mBasicLoggingApi = basicLoggingApi;
        mFrame = pietContent.getFrame();
        mPietSharedStates = getPietSharedStates(pietContent, modelProvider, basicLoggingApi);
        mContentId = contentFeatureModel.getStreamFeature().getContentId();
        RepresentationData representationData = content.getRepresentationData();
        mContentUrl = representationData.getUri();
        mAvailableOffline = streamOfflineMonitor.isAvailableOffline(mContentUrl);
        mOfflineStatusConsumer = new OfflineStatusConsumer();
        streamOfflineMonitor.addOfflineStatusConsumer(mContentUrl, mOfflineStatusConsumer);
        mContentLoggingData = new StreamContentLoggingData(
                position, content.getBasicLoggingMetadata(), representationData, mAvailableOffline);
        mActionParser = actionParserFactory.build(
                ()
                        -> ContentMetadata.maybeCreateContentMetadata(
                                content.getOfflineMetadata(), representationData));
        mStreamActionApi =
                createStreamActionApi(actionApi, mActionParser, actionManager, basicLoggingApi,
                        ()
                                -> mContentLoggingData,
                        modelProvider.getSessionId(), contextMenuManager,
                        clusterPendingDismissHelper, this, mContentId, tooltipApi);
        this.mSwipeAction = swipeAction;
        this.mStreamOfflineMonitor = streamOfflineMonitor;
        this.mContentChangedListener = contentChangedListener;
        this.mViewLoggingUpdater = viewLoggingUpdater;
        mLoggingListener = new ResettableOneShotVisibilityLoggingListener(this);
        viewLoggingUpdater.registerObserver(mLoggingListener);
    }

    @Override
    public void onDestroy() {
        mStreamOfflineMonitor.removeOfflineStatusConsumer(mContentUrl, mOfflineStatusConsumer);
        removeAllPendingTasks();
        mViewLoggingUpdater.unregisterObserver(mLoggingListener);
    }

    @Override
    public LeafFeatureDriver getLeafFeatureDriver() {
        return this;
    }

    private PietContent getPietContent(
            /*@UnderInitialization*/ ContentDriver this, Content content) {
        checkState(content.getType() == StreamStructureProto.Content.Type.PIET,
                "Expected Piet type for feature");

        checkState(content.hasExtension(PietContent.pietContentExtension),
                "Expected Piet content for feature");

        return content.getExtension(PietContent.pietContentExtension);
    }

    private List<PietSharedState> getPietSharedStates(
            /*@UnderInitialization*/ ContentDriver this, PietContent pietContent,
            ModelProvider modelProvider, BasicLoggingApi basicLoggingApi) {
        List<PietSharedState> sharedStates = new ArrayList<>();
        for (ContentId contentId : pietContent.getPietSharedStatesList()) {
            PietSharedState pietSharedState =
                    extractPietSharedState(contentId, modelProvider, basicLoggingApi);
            if (pietSharedState == null) {
                return new ArrayList<>();
            }

            sharedStates.add(pietSharedState);
        }
        return sharedStates;
    }

    /*@Nullable*/
    private PietSharedState extractPietSharedState(
            /*@UnderInitialization*/ ContentDriver this, ContentId pietSharedStateId,
            ModelProvider modelProvider, BasicLoggingApi basicLoggingApi) {
        StreamSharedState sharedState = modelProvider.getSharedState(pietSharedStateId);
        if (sharedState != null) {
            return sharedState.getPietSharedStateItem().getPietSharedState();
        }

        basicLoggingApi.onInternalError(InternalFeedError.NULL_SHARED_STATES);
        Logger.e(TAG,
                "Shared state was null. Stylesheets and templates on PietSharedState "
                        + "will not be loaded.");
        return null;
    }

    @Override
    public void bind(FeedViewHolder viewHolder) {
        if (!(viewHolder instanceof PietViewHolder)) {
            throw new AssertionError();
        }

        this.mViewHolder = (PietViewHolder) viewHolder;

        ((PietViewHolder) viewHolder)
                .bind(mFrame, mPietSharedStates, mStreamActionApi, mSwipeAction, mLoggingListener,
                        mActionParser);
    }

    @Override
    public void unbind() {
        if (mViewHolder == null) {
            return;
        }

        mViewHolder.unbind();
        mViewHolder = null;
    }

    @Override
    public void maybeRebind() {
        if (mViewHolder == null) {
            return;
        }

        // Unbinding clears the viewHolder, so storing to rebind.
        PietViewHolder localViewHolder = mViewHolder;
        unbind();
        bind(localViewHolder);
        mContentChangedListener.onContentChanged();
    }

    @Override
    public int getItemViewType() {
        return ViewHolderType.TYPE_CARD;
    }

    @Override
    public long itemId() {
        return hashCode();
    }

    @VisibleForTesting
    boolean isBound() {
        return mViewHolder != null;
    }

    @Override
    public String getContentId() {
        return mContentId;
    }

    @Override
    public void onViewVisible() {
        mBasicLoggingApi.onContentViewed(mContentLoggingData);
    }

    @Override
    public void onContentClicked() {
        mBasicLoggingApi.onContentClicked(mContentLoggingData);
    }

    @Override
    public void onContentSwiped() {
        mBasicLoggingApi.onContentSwiped(mContentLoggingData);
    }

    @Override
    public void onScrollStateChanged(int newScrollState) {
        if (newScrollState != RecyclerView.SCROLL_STATE_IDLE) {
            removeAllPendingTasks();
        }
    }

    private void removeAllPendingTasks() {
        for (CancelableTask cancellable : mViewActionTaskMap.values()) {
            cancellable.cancel();
        }

        mViewActionTaskMap.clear();
    }

    private void removePendingViewActionTaskForElement(int elementType) {
        CancelableTask cancelable = mViewActionTaskMap.remove(elementType);
        if (cancelable != null) {
            cancelable.cancel();
        }
    }

    @VisibleForTesting
    StreamActionApi createStreamActionApi(
            /*@UnknownInitialization*/ ContentDriver this, ActionApi actionApi,
            ActionParser actionParser, ActionManager actionManager, BasicLoggingApi basicLoggingApi,
            Supplier<ContentLoggingData> contentLoggingData,
            /*@Nullable*/ String sessionId, ContextMenuManager contextMenuManager,
            ClusterPendingDismissHelper clusterPendingDismissHelper,
            ViewElementActionHandler handler, String contentId, TooltipApi tooltipApi) {
        return new StreamActionApiImpl(actionApi, actionParser, actionManager, basicLoggingApi,
                contentLoggingData, contextMenuManager, sessionId, clusterPendingDismissHelper,
                handler, contentId, tooltipApi);
    }

    @Override
    public void onElementView(int elementType) {
        removePendingViewActionTaskForElement(elementType);
        CancelableTask cancelableTask = mMainThreadRunner.executeWithDelay(TAG + elementType,
                ()
                        -> mBasicLoggingApi.onVisualElementViewed(mContentLoggingData, elementType),
                mViewDelayMs);
        mViewActionTaskMap.put(elementType, cancelableTask);
    }

    @Override
    public void onElementHide(int elementType) {
        removePendingViewActionTaskForElement(elementType);
    }

    private class OfflineStatusConsumer implements Consumer<Boolean> {
        @Override
        public void accept(Boolean offlineStatus) {
            if (offlineStatus.equals(mAvailableOffline)) {
                return;
            }

            mAvailableOffline = offlineStatus;
            mContentLoggingData = mContentLoggingData.createWithOfflineStatus(offlineStatus);
            maybeRebind();
        }
    }
}
