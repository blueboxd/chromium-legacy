// Copyright 2018 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.android.libraries.feed.basicstream.internal.drivers;

import android.support.annotation.VisibleForTesting;
import com.google.android.libraries.feed.api.client.stream.Stream.ContentChangedListener;
import com.google.android.libraries.feed.api.host.action.ActionApi;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.InternalFeedError;
import com.google.android.libraries.feed.api.host.stream.TooltipApi;
import com.google.android.libraries.feed.api.internal.actionmanager.ActionManager;
import com.google.android.libraries.feed.api.internal.actionparser.ActionParserFactory;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild.Type;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelCursor;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.basicstream.internal.pendingdismiss.ClusterPendingDismissHelper;
import com.google.android.libraries.feed.basicstream.internal.pendingdismiss.PendingDismissHandler;
import com.google.android.libraries.feed.basicstream.internal.viewloggingupdater.ViewLoggingUpdater;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.sharedstream.contextmenumanager.ContextMenuManager;
import com.google.android.libraries.feed.sharedstream.offlinemonitor.StreamOfflineMonitor;
import com.google.android.libraries.feed.sharedstream.pendingdismiss.PendingDismissCallback;
import com.google.search.now.ui.action.FeedActionProto.UndoAction;

/** {@link FeatureDriver} for Clusters. */
public class ClusterDriver implements FeatureDriver, ClusterPendingDismissHelper {

  private static final String TAG = "ClusterDriver";
  private final ActionApi actionApi;
  private final ActionManager actionManager;
  private final ActionParserFactory actionParserFactory;
  private final BasicLoggingApi basicLoggingApi;
  private final ModelFeature clusterModel;
  private final ModelProvider modelProvider;
  private final int position;
  private final PendingDismissHandler pendingDismissHandler;
  private final StreamOfflineMonitor streamOfflineMonitor;
  private final ContentChangedListener contentChangedListener;
  private final ContextMenuManager contextMenuManager;
  private final MainThreadRunner mainThreadRunner;
  private final Configuration configuration;
  private final ViewLoggingUpdater viewLoggingUpdater;
  private final TooltipApi tooltipApi;

  /*@Nullable*/ private CardDriver cardDriver;

  ClusterDriver(
      ActionApi actionApi,
      ActionManager actionManager,
      ActionParserFactory actionParserFactory,
      BasicLoggingApi basicLoggingApi,
      ModelFeature clusterModel,
      ModelProvider modelProvider,
      int position,
      PendingDismissHandler pendingDismissHandler,
      StreamOfflineMonitor streamOfflineMonitor,
      ContentChangedListener contentChangedListener,
      ContextMenuManager contextMenuManager,
      MainThreadRunner mainThreadRunner,
      Configuration configuration,
      ViewLoggingUpdater viewLoggingUpdater,
      TooltipApi tooltipApi) {
    this.actionApi = actionApi;
    this.actionManager = actionManager;
    this.actionParserFactory = actionParserFactory;
    this.basicLoggingApi = basicLoggingApi;
    this.clusterModel = clusterModel;
    this.modelProvider = modelProvider;
    this.position = position;
    this.pendingDismissHandler = pendingDismissHandler;
    this.streamOfflineMonitor = streamOfflineMonitor;
    this.contentChangedListener = contentChangedListener;
    this.contextMenuManager = contextMenuManager;
    this.mainThreadRunner = mainThreadRunner;
    this.configuration = configuration;
    this.viewLoggingUpdater = viewLoggingUpdater;
    this.tooltipApi = tooltipApi;
  }

  @Override
  public void onDestroy() {
    if (cardDriver != null) {
      cardDriver.onDestroy();
    }
  }

  @Override
  /*@Nullable*/
  public LeafFeatureDriver getLeafFeatureDriver() {
    if (cardDriver == null) {
      cardDriver = createCardChild(clusterModel);
    }

    if (cardDriver != null) {
      return cardDriver.getLeafFeatureDriver();
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
        basicLoggingApi.onInternalError(InternalFeedError.CLUSTER_CHILD_MISSING_FEATURE);
        Logger.e(TAG, "Child of cursor is not a feature");
        continue;
      }

      ModelFeature childModelFeature = child.getModelFeature();

      if (!childModelFeature.getStreamFeature().hasCard()) {
        basicLoggingApi.onInternalError(InternalFeedError.CLUSTER_CHILD_NOT_CARD);
        Logger.e(TAG, "Content not card.");
        continue;
      }

      return createCardDriver(childModelFeature);
    }

    return null;
  }

  @VisibleForTesting
  CardDriver createCardDriver(ModelFeature content) {
    return new CardDriver(
        actionApi,
        actionManager,
        actionParserFactory,
        basicLoggingApi,
        content,
        modelProvider,
        position,
        this,
        streamOfflineMonitor,
        contentChangedListener,
        contextMenuManager,
        mainThreadRunner,
        configuration,
        viewLoggingUpdater,
        tooltipApi);
  }

  @Override
  public void triggerPendingDismissForCluster(
      UndoAction undoAction, PendingDismissCallback pendingDismissCallback) {
    // Get the content id assoc with this ClusterDriver and pass the dismiss to the StreamDriver.
    pendingDismissHandler.triggerPendingDismiss(
        clusterModel.getStreamFeature().getContentId(), undoAction, pendingDismissCallback);
  }
}
