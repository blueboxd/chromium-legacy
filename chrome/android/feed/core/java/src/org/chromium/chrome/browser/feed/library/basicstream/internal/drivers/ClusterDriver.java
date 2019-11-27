// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.drivers;

import android.support.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ContentChangedListener;
import org.chromium.chrome.browser.feed.library.api.host.action.ActionApi;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.InternalFeedError;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipApi;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionManager;
import org.chromium.chrome.browser.feed.library.api.internal.actionparser.ActionParserFactory;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild.Type;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelCursor;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelFeature;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.basicstream.internal.pendingdismiss.ClusterPendingDismissHelper;
import org.chromium.chrome.browser.feed.library.basicstream.internal.pendingdismiss.PendingDismissHandler;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewloggingupdater.ViewLoggingUpdater;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.sharedstream.contextmenumanager.ContextMenuManager;
import org.chromium.chrome.browser.feed.library.sharedstream.offlinemonitor.StreamOfflineMonitor;
import org.chromium.chrome.browser.feed.library.sharedstream.pendingdismiss.PendingDismissCallback;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.UndoAction;

/** {@link FeatureDriver} for Clusters. */
public class ClusterDriver implements FeatureDriver, ClusterPendingDismissHelper {
    private static final String TAG = "ClusterDriver";
    private final ActionApi mActionApi;
    private final ActionManager mActionManager;
    private final ActionParserFactory mActionParserFactory;
    private final BasicLoggingApi mBasicLoggingApi;
    private final ModelFeature mClusterModel;
    private final ModelProvider mModelProvider;
    private final int mPosition;
    private final PendingDismissHandler mPendingDismissHandler;
    private final StreamOfflineMonitor mStreamOfflineMonitor;
    private final ContentChangedListener mContentChangedListener;
    private final ContextMenuManager mContextMenuManager;
    private final MainThreadRunner mMainThreadRunner;
    private final Configuration mConfiguration;
    private final ViewLoggingUpdater mViewLoggingUpdater;
    private final TooltipApi mTooltipApi;

    /*@Nullable*/ private CardDriver mCardDriver;

    ClusterDriver(ActionApi actionApi, ActionManager actionManager,
            ActionParserFactory actionParserFactory, BasicLoggingApi basicLoggingApi,
            ModelFeature clusterModel, ModelProvider modelProvider, int position,
            PendingDismissHandler pendingDismissHandler, StreamOfflineMonitor streamOfflineMonitor,
            ContentChangedListener contentChangedListener, ContextMenuManager contextMenuManager,
            MainThreadRunner mainThreadRunner, Configuration configuration,
            ViewLoggingUpdater viewLoggingUpdater, TooltipApi tooltipApi) {
        this.mActionApi = actionApi;
        this.mActionManager = actionManager;
        this.mActionParserFactory = actionParserFactory;
        this.mBasicLoggingApi = basicLoggingApi;
        this.mClusterModel = clusterModel;
        this.mModelProvider = modelProvider;
        this.mPosition = position;
        this.mPendingDismissHandler = pendingDismissHandler;
        this.mStreamOfflineMonitor = streamOfflineMonitor;
        this.mContentChangedListener = contentChangedListener;
        this.mContextMenuManager = contextMenuManager;
        this.mMainThreadRunner = mainThreadRunner;
        this.mConfiguration = configuration;
        this.mViewLoggingUpdater = viewLoggingUpdater;
        this.mTooltipApi = tooltipApi;
    }

    @Override
    public void onDestroy() {
        if (mCardDriver != null) {
            mCardDriver.onDestroy();
        }
    }

    @Override
    /*@Nullable*/
    public LeafFeatureDriver getLeafFeatureDriver() {
        if (mCardDriver == null) {
            mCardDriver = createCardChild(mClusterModel);
        }

        if (mCardDriver != null) {
            return mCardDriver.getLeafFeatureDriver();
        }

        return null;
    }

    /*@Nullable*/
    private CardDriver createCardChild(ModelFeature clusterFeature) {
        ModelCursor cursor = clusterFeature.getCursor();
        // TODO: add change listener to clusterCursor.
        ModelChild child;
        while ((child = cursor.getNextItem()) != null) {
            if (child.getType() != Type.FEATURE) {
                mBasicLoggingApi.onInternalError(InternalFeedError.CLUSTER_CHILD_MISSING_FEATURE);
                Logger.e(TAG, "Child of cursor is not a feature");
                continue;
            }

            ModelFeature childModelFeature = child.getModelFeature();

            if (!childModelFeature.getStreamFeature().hasCard()) {
                mBasicLoggingApi.onInternalError(InternalFeedError.CLUSTER_CHILD_NOT_CARD);
                Logger.e(TAG, "Content not card.");
                continue;
            }

            return createCardDriver(childModelFeature);
        }

        return null;
    }

    @VisibleForTesting
    CardDriver createCardDriver(ModelFeature content) {
        return new CardDriver(mActionApi, mActionManager, mActionParserFactory, mBasicLoggingApi,
                content, mModelProvider, mPosition, this, mStreamOfflineMonitor,
                mContentChangedListener, mContextMenuManager, mMainThreadRunner, mConfiguration,
                mViewLoggingUpdater, mTooltipApi);
    }

    @Override
    public void triggerPendingDismissForCluster(
            UndoAction undoAction, PendingDismissCallback pendingDismissCallback) {
        // Get the content id assoc with this ClusterDriver and pass the dismiss to the
        // StreamDriver.
        mPendingDismissHandler.triggerPendingDismiss(
                mClusterModel.getStreamFeature().getContentId(), undoAction,
                pendingDismissCallback);
    }
}
