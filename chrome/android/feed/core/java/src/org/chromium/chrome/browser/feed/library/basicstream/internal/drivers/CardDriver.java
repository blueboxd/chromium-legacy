// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.drivers;

import static org.chromium.chrome.browser.feed.library.common.Validators.checkState;

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
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewloggingupdater.ViewLoggingUpdater;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.sharedstream.contextmenumanager.ContextMenuManager;
import org.chromium.chrome.browser.feed.library.sharedstream.offlinemonitor.StreamOfflineMonitor;
import org.chromium.components.feed.core.proto.ui.action.FeedActionPayloadProto.FeedActionPayload;
import org.chromium.components.feed.core.proto.ui.stream.StreamSwipeExtensionProto.SwipeActionExtension;

/** {@link FeatureDriver} for cards. */
public class CardDriver implements FeatureDriver {
    private static final String TAG = "CardDriver";

    private final ActionApi mActionApi;
    private final ActionManager mActionManager;
    private final ActionParserFactory mActionParserFactory;
    private final BasicLoggingApi mBasicLoggingApi;
    private final ModelFeature mCardModel;
    private final ModelProvider mModelProvider;
    private final int mPosition;
    private final ClusterPendingDismissHelper mClusterPendingDismissHelper;
    private final StreamOfflineMonitor mStreamOfflineMonitor;
    private final ContentChangedListener mContentChangedListener;
    private final ContextMenuManager mContextMenuManager;
    private final MainThreadRunner mMainThreadRunner;
    private final Configuration mConfiguration;
    private final ViewLoggingUpdater mViewLoggingUpdater;
    private final TooltipApi mTooltipApi;

    /*@Nullable*/ private ContentDriver mContentDriver;

    CardDriver(ActionApi actionApi, ActionManager actionManager,
            ActionParserFactory actionParserFactory, BasicLoggingApi basicLoggingApi,
            ModelFeature cardModel, ModelProvider modelProvider, int position,
            ClusterPendingDismissHelper clusterPendingDismissHelper,
            StreamOfflineMonitor streamOfflineMonitor,
            ContentChangedListener contentChangedListener, ContextMenuManager contextMenuManager,
            MainThreadRunner mainThreadRunner, Configuration configuration,
            ViewLoggingUpdater viewLoggingUpdater, TooltipApi tooltipApi) {
        this.mActionApi = actionApi;
        this.mActionManager = actionManager;
        this.mActionParserFactory = actionParserFactory;
        this.mBasicLoggingApi = basicLoggingApi;
        this.mCardModel = cardModel;
        this.mModelProvider = modelProvider;
        this.mPosition = position;
        this.mClusterPendingDismissHelper = clusterPendingDismissHelper;
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
        if (mContentDriver != null) {
            mContentDriver.onDestroy();
        }
    }

    @Override
    /*@Nullable*/
    public LeafFeatureDriver getLeafFeatureDriver() {
        if (mContentDriver == null) {
            mContentDriver = createContentChild(mCardModel);
        }

        if (mContentDriver != null) {
            return mContentDriver.getLeafFeatureDriver();
        }

        return null;
    }

    /*@Nullable*/
    private ContentDriver createContentChild(ModelFeature modelFeature) {
        ModelCursor cursor = modelFeature.getCursor();
        // TODO: add change listener to ModelFeature.
        ModelChild child;
        if ((child = cursor.getNextItem()) != null) {
            if (child.getType() != Type.FEATURE) {
                mBasicLoggingApi.onInternalError(InternalFeedError.CARD_CHILD_MISSING_FEATURE);
                Logger.e(TAG, "ContentChild was not bound to a Feature, found type: %s",
                        child.getType());
                return null;
            }
            ModelFeature contentModel = child.getModelFeature();
            checkState(
                    contentModel.getStreamFeature().hasContent(), "Expected content for feature");
            return createContentDriver(contentModel,
                    mCardModel.getStreamFeature()
                            .getCard()
                            .getExtension(SwipeActionExtension.swipeActionExtension)
                            .getSwipeAction());
        }

        return null;
    }

    @VisibleForTesting
    ContentDriver createContentDriver(ModelFeature contentModel, FeedActionPayload swipeAction) {
        return new ContentDriver(mActionApi, mActionManager, mActionParserFactory, mBasicLoggingApi,
                contentModel, mModelProvider, mPosition, swipeAction, mClusterPendingDismissHelper,
                mStreamOfflineMonitor, mContentChangedListener, mContextMenuManager,
                mMainThreadRunner, mConfiguration, mViewLoggingUpdater, mTooltipApi);
    }
}
