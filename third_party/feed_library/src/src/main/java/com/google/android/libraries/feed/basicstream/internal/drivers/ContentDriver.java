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
public class ContentDriver extends LeafFeatureDriver
    implements LoggingListener, ViewElementActionHandler {

  private static final String TAG = "ContentDriver";

  private final BasicLoggingApi basicLoggingApi;
  private final List<PietSharedState> pietSharedStates;
  private final Frame frame;
  private final StreamActionApi streamActionApi;
  private final FeedActionPayload swipeAction;
  private final String contentId;
  private final StreamOfflineMonitor streamOfflineMonitor;
  private final OfflineStatusConsumer offlineStatusConsumer;
  private final String contentUrl;
  private final ContentChangedListener contentChangedListener;
  private final ActionParser actionParser;
  private final MainThreadRunner mainThreadRunner;
  private final long viewDelayMs;
  private final HashMap<Integer, CancelableTask> viewActionTaskMap = new HashMap<>();
  private final ViewLoggingUpdater viewLoggingUpdater;
  private final ResettableOneShotVisibilityLoggingListener loggingListener;

  private StreamContentLoggingData contentLoggingData;
  private boolean availableOffline;
  /*@Nullable*/ private PietViewHolder viewHolder;

  // TODO: Remove these suppressions when drivers have a proper lifecycle.
  @SuppressWarnings({
    "nullness:argument.type.incompatible",
    "nullness:assignment.type.incompatible"
  })
  ContentDriver(
      ActionApi actionApi,
      ActionManager actionManager,
      ActionParserFactory actionParserFactory,
      BasicLoggingApi basicLoggingApi,
      ModelFeature contentFeatureModel,
      ModelProvider modelProvider,
      int position,
      FeedActionPayload swipeAction,
      ClusterPendingDismissHelper clusterPendingDismissHelper,
      StreamOfflineMonitor streamOfflineMonitor,
      ContentChangedListener contentChangedListener,
      ContextMenuManager contextMenuManager,
      MainThreadRunner mainThreadRunner,
      Configuration configuration,
      ViewLoggingUpdater viewLoggingUpdater,
      TooltipApi tooltipApi) {
    this.mainThreadRunner = mainThreadRunner;
    viewDelayMs =
        configuration.getValueOrDefault(
            ConfigKey.VIEW_MIN_TIME_MS, Constants.VIEW_MIN_TIME_MS_DEFAULT);
    Content content = contentFeatureModel.getStreamFeature().getContent();

    PietContent pietContent = getPietContent(content);
    this.basicLoggingApi = basicLoggingApi;
    frame = pietContent.getFrame();
    pietSharedStates = getPietSharedStates(pietContent, modelProvider, basicLoggingApi);
    contentId = contentFeatureModel.getStreamFeature().getContentId();
    RepresentationData representationData = content.getRepresentationData();
    contentUrl = representationData.getUri();
    availableOffline = streamOfflineMonitor.isAvailableOffline(contentUrl);
    offlineStatusConsumer = new OfflineStatusConsumer();
    streamOfflineMonitor.addOfflineStatusConsumer(contentUrl, offlineStatusConsumer);
    contentLoggingData =
        new StreamContentLoggingData(
            position, content.getBasicLoggingMetadata(), representationData, availableOffline);
    actionParser =
        actionParserFactory.build(
            () ->
                ContentMetadata.maybeCreateContentMetadata(
                    content.getOfflineMetadata(), representationData));
    streamActionApi =
        createStreamActionApi(
            actionApi,
            actionParser,
            actionManager,
            basicLoggingApi,
            () -> contentLoggingData,
            modelProvider.getSessionId(),
            contextMenuManager,
            clusterPendingDismissHelper,
            this,
            contentId,
            tooltipApi);
    this.swipeAction = swipeAction;
    this.streamOfflineMonitor = streamOfflineMonitor;
    this.contentChangedListener = contentChangedListener;
    this.viewLoggingUpdater = viewLoggingUpdater;
    loggingListener = new ResettableOneShotVisibilityLoggingListener(this);
    viewLoggingUpdater.registerObserver(loggingListener);
  }

  @Override
  public void onDestroy() {
    streamOfflineMonitor.removeOfflineStatusConsumer(contentUrl, offlineStatusConsumer);
    removeAllPendingTasks();
    viewLoggingUpdater.unregisterObserver(loggingListener);
  }

  @Override
  public LeafFeatureDriver getLeafFeatureDriver() {
    return this;
  }

  private PietContent getPietContent(/*@UnderInitialization*/ ContentDriver this, Content content) {
    checkState(
        content.getType() == StreamStructureProto.Content.Type.PIET,
        "Expected Piet type for feature");

    checkState(
        content.hasExtension(PietContent.pietContentExtension),
        "Expected Piet content for feature");

    return content.getExtension(PietContent.pietContentExtension);
  }

  private List<PietSharedState> getPietSharedStates(
      /*@UnderInitialization*/ ContentDriver this,
      PietContent pietContent,
      ModelProvider modelProvider,
      BasicLoggingApi basicLoggingApi) {
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
      /*@UnderInitialization*/ ContentDriver this,
      ContentId pietSharedStateId,
      ModelProvider modelProvider,
      BasicLoggingApi basicLoggingApi) {
    StreamSharedState sharedState = modelProvider.getSharedState(pietSharedStateId);
    if (sharedState != null) {
      return sharedState.getPietSharedStateItem().getPietSharedState();
    }

    basicLoggingApi.onInternalError(InternalFeedError.NULL_SHARED_STATES);
    Logger.e(
        TAG,
        "Shared state was null. Stylesheets and templates on PietSharedState "
            + "will not be loaded.");
    return null;
  }

  @Override
  public void bind(FeedViewHolder viewHolder) {
    if (!(viewHolder instanceof PietViewHolder)) {
      throw new AssertionError();
    }

    this.viewHolder = (PietViewHolder) viewHolder;

    ((PietViewHolder) viewHolder)
        .bind(frame, pietSharedStates, streamActionApi, swipeAction, loggingListener, actionParser);
  }

  @Override
  public void unbind() {
    if (viewHolder == null) {
      return;
    }

    viewHolder.unbind();
    viewHolder = null;
  }

  @Override
  public void maybeRebind() {
    if (viewHolder == null) {
      return;
    }

    // Unbinding clears the viewHolder, so storing to rebind.
    PietViewHolder localViewHolder = viewHolder;
    unbind();
    bind(localViewHolder);
    contentChangedListener.onContentChanged();
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
    return viewHolder != null;
  }

  @Override
  public String getContentId() {
    return contentId;
  }

  @Override
  public void onViewVisible() {
    basicLoggingApi.onContentViewed(contentLoggingData);
  }

  @Override
  public void onContentClicked() {
    basicLoggingApi.onContentClicked(contentLoggingData);
  }

  @Override
  public void onContentSwiped() {
    basicLoggingApi.onContentSwiped(contentLoggingData);
  }

  @Override
  public void onScrollStateChanged(int newScrollState) {
    if (newScrollState != RecyclerView.SCROLL_STATE_IDLE) {
      removeAllPendingTasks();
    }
  }

  private void removeAllPendingTasks() {
    for (CancelableTask cancellable : viewActionTaskMap.values()) {
      cancellable.cancel();
    }

    viewActionTaskMap.clear();
  }

  private void removePendingViewActionTaskForElement(int elementType) {
    CancelableTask cancelable = viewActionTaskMap.remove(elementType);
    if (cancelable != null) {
      cancelable.cancel();
    }
  }

  @VisibleForTesting
  StreamActionApi createStreamActionApi(
      /*@UnknownInitialization*/ ContentDriver this,
      ActionApi actionApi,
      ActionParser actionParser,
      ActionManager actionManager,
      BasicLoggingApi basicLoggingApi,
      Supplier<ContentLoggingData> contentLoggingData,
      /*@Nullable*/ String sessionId,
      ContextMenuManager contextMenuManager,
      ClusterPendingDismissHelper clusterPendingDismissHelper,
      ViewElementActionHandler handler,
      String contentId,
      TooltipApi tooltipApi) {
    return new StreamActionApiImpl(
        actionApi,
        actionParser,
        actionManager,
        basicLoggingApi,
        contentLoggingData,
        contextMenuManager,
        sessionId,
        clusterPendingDismissHelper,
        handler,
        contentId,
        tooltipApi);
  }

  @Override
  public void onElementView(int elementType) {
    removePendingViewActionTaskForElement(elementType);
    CancelableTask cancelableTask =
        mainThreadRunner.executeWithDelay(
            TAG + elementType,
            () -> basicLoggingApi.onVisualElementViewed(contentLoggingData, elementType),
            viewDelayMs);
    viewActionTaskMap.put(elementType, cancelableTask);
  }

  @Override
  public void onElementHide(int elementType) {
    removePendingViewActionTaskForElement(elementType);
  }

  private class OfflineStatusConsumer implements Consumer<Boolean> {

    @Override
    public void accept(Boolean offlineStatus) {
      if (offlineStatus.equals(availableOffline)) {
        return;
      }

      availableOffline = offlineStatus;
      contentLoggingData = contentLoggingData.createWithOfflineStatus(offlineStatus);
      maybeRebind();
    }
  }
}
