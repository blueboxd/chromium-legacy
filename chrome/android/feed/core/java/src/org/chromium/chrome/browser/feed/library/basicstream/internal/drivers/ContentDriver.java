// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.drivers;

import static org.chromium.chrome.browser.feed.library.common.Validators.checkState;

import android.support.annotation.VisibleForTesting;
import android.support.v7.widget.RecyclerView;

import org.chromium.base.Consumer;
import org.chromium.base.Supplier;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.ContentMetadata;
import org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ContentChangedListener;
import org.chromium.chrome.browser.feed.library.api.host.action.ActionApi;
import org.chromium.chrome.browser.feed.library.api.host.action.StreamActionApi;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.ContentLoggingData;
import org.chromium.chrome.browser.feed.library.api.host.logging.InternalFeedError;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipApi;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionManager;
import org.chromium.chrome.browser.feed.library.api.internal.actionparser.ActionParser;
import org.chromium.chrome.browser.feed.library.api.internal.actionparser.ActionParserFactory;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelFeature;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.basicstream.internal.actions.StreamActionApiImpl;
import org.chromium.chrome.browser.feed.library.basicstream.internal.actions.ViewElementActionHandler;
import org.chromium.chrome.browser.feed.library.basicstream.internal.pendingdismiss.ClusterPendingDismissHelper;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.FeedViewHolder;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.PietViewHolder;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.ViewHolderType;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewloggingupdater.ResettableOneShotVisibilityLoggingListener;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewloggingupdater.ViewLoggingUpdater;
import org.chromium.chrome.browser.feed.library.common.concurrent.CancelableTask;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.sharedstream.constants.Constants;
import org.chromium.chrome.browser.feed.library.sharedstream.contextmenumanager.ContextMenuManager;
import org.chromium.chrome.browser.feed.library.sharedstream.logging.LoggingListener;
import org.chromium.chrome.browser.feed.library.sharedstream.logging.StreamContentLoggingData;
import org.chromium.chrome.browser.feed.library.sharedstream.offlinemonitor.StreamOfflineMonitor;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSharedState;
import org.chromium.components.feed.core.proto.ui.action.FeedActionPayloadProto.FeedActionPayload;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Frame;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.PietSharedState;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Content;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.PietContent;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.RepresentationData;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;

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
