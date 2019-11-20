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

package com.google.android.libraries.feed.basicstream.internal.actions;

import android.view.View;
import com.google.android.libraries.feed.api.client.knowncontent.ContentMetadata;
import com.google.android.libraries.feed.api.host.action.ActionApi;
import com.google.android.libraries.feed.api.host.action.StreamActionApi;
import com.google.android.libraries.feed.api.host.logging.ActionType;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.ContentLoggingData;
import com.google.android.libraries.feed.api.host.stream.TooltipApi;
import com.google.android.libraries.feed.api.host.stream.TooltipCallbackApi;
import com.google.android.libraries.feed.api.host.stream.TooltipInfo;
import com.google.android.libraries.feed.api.internal.actionmanager.ActionManager;
import com.google.android.libraries.feed.api.internal.actionparser.ActionParser;
import com.google.android.libraries.feed.api.internal.actionparser.ActionSource;
import com.google.android.libraries.feed.basicstream.internal.pendingdismiss.ClusterPendingDismissHelper;
import com.google.android.libraries.feed.common.functional.Supplier;
import com.google.android.libraries.feed.sharedstream.contextmenumanager.ContextMenuManager;
import com.google.android.libraries.feed.sharedstream.pendingdismiss.PendingDismissCallback;
import com.google.search.now.feed.client.StreamDataProto.StreamDataOperation;
import com.google.search.now.ui.action.FeedActionProto.FeedActionMetadata.ElementType;
import com.google.search.now.ui.action.FeedActionProto.LabelledFeedActionData;
import com.google.search.now.ui.action.FeedActionProto.OpenContextMenuData;
import com.google.search.now.ui.action.FeedActionProto.UndoAction;
import com.google.search.now.wire.feed.ActionPayloadProto.ActionPayload;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Action handler for Stream. */
public class StreamActionApiImpl implements StreamActionApi {

  private static final String TAG = "StreamActionApiImpl";

  private final ActionApi actionApi;
  private final ActionParser actionParser;
  private final ActionManager actionManager;
  private final BasicLoggingApi basicLoggingApi;
  private final Supplier<ContentLoggingData> contentLoggingData;
  private final ContextMenuManager contextMenuManager;
  private final ClusterPendingDismissHelper clusterPendingDismissHelper;
  private final ViewElementActionHandler viewElementActionHandler;
  private final String contentId;
  private final TooltipApi tooltipApi;

  /*@Nullable*/ private final String sessionId;

  public StreamActionApiImpl(
      ActionApi actionApi,
      ActionParser actionParser,
      ActionManager actionManager,
      BasicLoggingApi basicLoggingApi,
      Supplier<ContentLoggingData> contentLoggingData,
      ContextMenuManager contextMenuManager,
      /*@Nullable*/ String sessionId,
      ClusterPendingDismissHelper clusterPendingDismissHelper,
      ViewElementActionHandler viewElementActionHandler,
      String contentId,
      TooltipApi tooltipApi) {
    this.actionApi = actionApi;
    this.actionParser = actionParser;
    this.actionManager = actionManager;
    this.basicLoggingApi = basicLoggingApi;
    this.contentLoggingData = contentLoggingData;
    this.contextMenuManager = contextMenuManager;
    this.sessionId = sessionId;
    this.clusterPendingDismissHelper = clusterPendingDismissHelper;
    this.viewElementActionHandler = viewElementActionHandler;
    this.contentId = contentId;
    this.tooltipApi = tooltipApi;
  }

  @Override
  public void openContextMenu(OpenContextMenuData openContextMenuData, View anchorView) {
    List<String> actionLabels = new ArrayList<>();
    List<LabelledFeedActionData> enabledActions = new ArrayList<>();
    for (LabelledFeedActionData labelledFeedActionData :
        openContextMenuData.getContextMenuDataList()) {
      if (actionParser.canPerformAction(labelledFeedActionData.getFeedActionPayload(), this)) {
        actionLabels.add(labelledFeedActionData.getLabel());
        enabledActions.add(labelledFeedActionData);
      }
    }

    boolean menuOpened =
        contextMenuManager.openContextMenu(
            anchorView,
            actionLabels,
            (int position) ->
                actionParser.parseFeedActionPayload(
                    enabledActions.get(position).getFeedActionPayload(),
                    StreamActionApiImpl.this,
                    anchorView,
                    ActionSource.CONTEXT_MENU));

    if (menuOpened) {
      basicLoggingApi.onContentContextMenuOpened(contentLoggingData.get());
    }
  }

  @Override
  public boolean canOpenContextMenu() {
    return true;
  }

  @Override
  public boolean canDismiss() {
    return true;
  }

  @Override
  public boolean canHandleNotInterestedIn() {
    return true;
  }

  @Override
  public void handleNotInterestedIn(
      List<StreamDataOperation> dataOperations,
      UndoAction undoAction,
      ActionPayload payload,
      int interestType) {
    if (!undoAction.hasConfirmationLabel()) {
      dismiss(dataOperations);
      basicLoggingApi.onNotInterestedIn(
          interestType, contentLoggingData.get(), /* wasCommitted = */ true);
    } else {
      dismissWithSnackbar(
          undoAction,
          new PendingDismissCallback() {
            @Override
            public void onDismissReverted() {
              basicLoggingApi.onNotInterestedIn(
                  interestType, contentLoggingData.get(), /* wasCommitted = */ false);
            }

            @Override
            public void onDismissCommitted() {
              dismiss(dataOperations);
              actionManager.createAndUploadAction(contentId, payload);
              basicLoggingApi.onNotInterestedIn(
                  interestType, contentLoggingData.get(), /* wasCommitted = */ true);
            }
          });
    }
  }

  @Override
  public void dismiss(
      String contentId,
      List<StreamDataOperation> dataOperations,
      UndoAction undoAction,
      ActionPayload payload) {
    if (!undoAction.hasConfirmationLabel()) {
      dismissLocal(contentId, dataOperations);
      basicLoggingApi.onContentDismissed(contentLoggingData.get(), /* wasCommitted = */ true);
    } else {
      dismissWithSnackbar(
          undoAction,
          new PendingDismissCallback() {
            @Override
            public void onDismissReverted() {
              basicLoggingApi.onContentDismissed(
                  contentLoggingData.get(), /* wasCommitted = */ false);
            }

            @Override
            public void onDismissCommitted() {
              dismissLocal(contentId, dataOperations);
              dismiss(dataOperations);
              actionManager.createAndUploadAction(contentId, payload);
              basicLoggingApi.onContentDismissed(
                  contentLoggingData.get(), /* wasCommitted = */ true);
            }
          });
    }
  }

  private void dismiss(List<StreamDataOperation> dataOperations) {
    actionManager.dismiss(dataOperations, sessionId);
  }

  private void dismissLocal(String contentId, List<StreamDataOperation> dataOperations) {
    actionManager.dismissLocal(Collections.singletonList(contentId), dataOperations, sessionId);
  }

  private void dismissWithSnackbar(
      UndoAction undoAction, PendingDismissCallback pendingDismissCallback) {
    clusterPendingDismissHelper.triggerPendingDismissForCluster(undoAction, pendingDismissCallback);
  }

  @Override
  public void onClientAction(@ActionType int actionType) {
    basicLoggingApi.onClientAction(contentLoggingData.get(), actionType);
  }

  @Override
  public void openUrl(String url) {
    actionApi.openUrl(url);
  }

  @Override
  public void openUrl(String url, String consistencyTokenQueryParamName) {
    actionManager.uploadAllActionsAndUpdateUrl(
        url,
        consistencyTokenQueryParamName,
        result -> {
          actionApi.openUrl(result);
        });
  }

  @Override
  public boolean canOpenUrl() {
    return actionApi.canOpenUrl();
  }

  @Override
  public void openUrlInIncognitoMode(String url) {
    actionApi.openUrlInIncognitoMode(url);
  }

  @Override
  public void openUrlInIncognitoMode(String url, String consistencyTokenQueryParamName) {
    actionManager.uploadAllActionsAndUpdateUrl(
        url,
        consistencyTokenQueryParamName,
        result -> {
          actionApi.openUrlInIncognitoMode(result);
        });
  }

  @Override
  public boolean canOpenUrlInIncognitoMode() {
    return actionApi.canOpenUrlInIncognitoMode();
  }

  @Override
  public void openUrlInNewTab(String url) {
    actionApi.openUrlInNewTab(url);
  }

  @Override
  public void openUrlInNewTab(String url, String consistencyTokenQueryParamName) {
    actionManager.uploadAllActionsAndUpdateUrl(
        url,
        consistencyTokenQueryParamName,
        result -> {
          actionApi.openUrlInNewTab(result);
        });
  }

  @Override
  public boolean canOpenUrlInNewTab() {
    return actionApi.canOpenUrlInNewTab();
  }

  @Override
  public void openUrlInNewWindow(String url) {
    actionApi.openUrlInNewWindow(url);
  }

  @Override
  public void openUrlInNewWindow(String url, String consistencyTokenQueryParamName) {
    actionManager.uploadAllActionsAndUpdateUrl(
        url,
        consistencyTokenQueryParamName,
        result -> {
          actionApi.openUrlInNewWindow(result);
        });
  }

  @Override
  public boolean canOpenUrlInNewWindow() {
    return actionApi.canOpenUrlInNewWindow();
  }

  @Override
  public void downloadUrl(ContentMetadata contentMetadata) {
    actionApi.downloadUrl(contentMetadata);
  }

  @Override
  public boolean canDownloadUrl() {
    return actionApi.canDownloadUrl();
  }

  @Override
  public void learnMore() {
    actionApi.learnMore();
  }

  @Override
  public boolean canLearnMore() {
    return actionApi.canLearnMore();
  }

  @Override
  public boolean canHandleElementView() {
    return true;
  }

  @Override
  public boolean canHandleElementHide() {
    return true;
  }

  @Override
  public boolean canHandleElementClick() {
    return true;
  }

  @Override
  public void onElementView(int elementType) {
    viewElementActionHandler.onElementView(elementType);
  }

  @Override
  public void onElementHide(int elementType) {
    viewElementActionHandler.onElementHide(elementType);
  }

  @Override
  public void onElementClick(int elementType) {
    basicLoggingApi.onVisualElementClicked(contentLoggingData.get(), elementType);
  }

  @Override
  public boolean canShowTooltip() {
    return true;
  }

  @Override
  public void maybeShowTooltip(TooltipInfo tooltipInfo, View view) {
    tooltipApi.maybeShowHelpUi(
        tooltipInfo,
        view,
        new TooltipCallbackApi() {
          @Override
          public void onShow() {
            onElementView(ElementType.TOOLTIP.getNumber());
          }

          @Override
          public void onHide(int dismissType) {
            onElementHide(ElementType.TOOLTIP.getNumber());
          }
        });
  }
}
