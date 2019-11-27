// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.action;

import android.view.View;

import org.chromium.chrome.browser.feed.library.api.host.logging.ActionType;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipInfo;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.OpenContextMenuData;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.UndoAction;
import org.chromium.components.feed.core.proto.wire.ActionPayloadProto.ActionPayload;

import java.util.List;

/**
 * Allows the {@link org.chromium.chrome.browser.feed.library.internalapi.actionparser.ActionParser}
 * to communicate actions back to the Stream after parsing.
 */
public interface StreamActionApi extends ActionApi {
    /** Whether or not a context menu can be opened */
    boolean canOpenContextMenu();

    /** Opens context menu. */
    void openContextMenu(OpenContextMenuData openContextMenuData, View anchorView);

    /** Whether or not a card can be dismissed. */
    boolean canDismiss();

    /** Dismisses card. */
    void dismiss(String contentId, List<StreamDataOperation> dataOperations, UndoAction undoAction,
            ActionPayload payload);

    /** Whether or not the not interested in action can be handled. */
    boolean canHandleNotInterestedIn();

    /** Handles the Not interested in <source> action. */
    void handleNotInterestedIn(List<StreamDataOperation> dataOperations, UndoAction undoAction,
            ActionPayload payload, int interestType);

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
     * org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata.ElementType}
     * value.
     */
    void onElementView(int elementType);

    /**
     * Called when a hide action has been performed on a server-defined {@link
     * org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata.ElementType}
     * value.
     */
    void onElementHide(int elementType);

    /**
     * Called when a click action has been performed on a server-defined {@link
     * org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata.ElementType}
     * value.
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
     * Opens the given URL in a new tab after uploading actions and attaching the consistency token
     * to it.
     */
    void openUrlInNewTab(String url, String consistencyTokenQueryParamName);

    /**
     * Opens the given URL in a new windowafter uploading actions and attaching the consistency
     * token to it.
     */
    void openUrlInNewWindow(String url, String consistencyTokenQueryParamName);
}
