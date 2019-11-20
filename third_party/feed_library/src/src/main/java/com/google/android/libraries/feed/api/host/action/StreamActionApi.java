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

package com.google.android.libraries.feed.api.host.action;

import android.view.View;
import com.google.android.libraries.feed.api.host.logging.ActionType;
import com.google.android.libraries.feed.api.host.stream.TooltipInfo;
import com.google.search.now.feed.client.StreamDataProto.StreamDataOperation;
import com.google.search.now.ui.action.FeedActionProto.OpenContextMenuData;
import com.google.search.now.ui.action.FeedActionProto.UndoAction;
import com.google.search.now.wire.feed.ActionPayloadProto.ActionPayload;
import java.util.List;

/**
 * Allows the {@link com.google.android.libraries.feed.internalapi.actionparser.ActionParser} to
 * communicate actions back to the Stream after parsing.
 */
public interface StreamActionApi extends ActionApi {

  /** Whether or not a context menu can be opened */
  boolean canOpenContextMenu();

  /** Opens context menu. */
  void openContextMenu(OpenContextMenuData openContextMenuData, View anchorView);

  /** Whether or not a card can be dismissed. */
  boolean canDismiss();

  /** Dismisses card. */
  void dismiss(
      String contentId,
      List<StreamDataOperation> dataOperations,
      UndoAction undoAction,
      ActionPayload payload);

  /** Whether or not the not interested in action can be handled. */
  boolean canHandleNotInterestedIn();

  /** Handles the Not interested in <source> action. */
  void handleNotInterestedIn(
      List<StreamDataOperation> dataOperations,
      UndoAction undoAction,
      ActionPayload payload,
      int interestType);

  /** Called when a client action has been performed. */
  void onClientAction(@ActionType int actionType);

  /** Whether or not the not view actions can be handled. */
  boolean canHandleElementView();

  /** Whether or not the not hide actions can be handled. */
  boolean canHandleElementHide();

  /** Whether or not the not click actions can be handled. */
  boolean canHandleElementClick();

  /**
   * Called when a view action has been performed on a server-defined {@link
   * com.google.search.now.ui.action.FeedActionProto.FeedActionMetadata.ElementType} value.
   */
  void onElementView(int elementType);

  /**
   * Called when a hide action has been performed on a server-defined {@link
   * com.google.search.now.ui.action.FeedActionProto.FeedActionMetadata.ElementType} value.
   */
  void onElementHide(int elementType);

  /**
   * Called when a click action has been performed on a server-defined {@link
   * com.google.search.now.ui.action.FeedActionProto.FeedActionMetadata.ElementType} value.
   */
  void onElementClick(int elementType);

  /** Whether or not show tooltip actions can be handled. */
  boolean canShowTooltip();

  /**
   * Called when the show tooltip action has been preformed. Tooltip might not be shown if it does
   * not meet triggering conditions.
   */
  void maybeShowTooltip(TooltipInfo tooltipInfo, View view);

  /* Called to open a url after uploading actions and attaching the consistency token to it. */
  void openUrl(String url, String consistencyTokenQueryParamName);

  /**
   * Opens the given URL in incognito mode after uploading actions and attaching the consistency
   * token to it.
   */
  void openUrlInIncognitoMode(String url, String consistencyTokenQueryParamName);

  /**
   * Opens the given URL in a new tab after uploading actions and attaching the consistency token to
   * it.
   */
  void openUrlInNewTab(String url, String consistencyTokenQueryParamName);

  /**
   * Opens the given URL in a new windowafter uploading actions and attaching the consistency token
   * to it.
   */
  void openUrlInNewWindow(String url, String consistencyTokenQueryParamName);
}
