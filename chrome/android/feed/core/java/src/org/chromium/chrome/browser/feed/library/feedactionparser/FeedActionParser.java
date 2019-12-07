// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedactionparser;

import static org.chromium.chrome.browser.feed.library.common.Validators.checkState;
import static org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata.Type.DOWNLOAD;
import static org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata.Type.LEARN_MORE;
import static org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata.Type.OPEN_URL;
import static org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata.Type.OPEN_URL_INCOGNITO;
import static org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata.Type.OPEN_URL_NEW_TAB;
import static org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata.Type.OPEN_URL_NEW_WINDOW;

import android.view.View;

import org.chromium.base.Supplier;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.ContentMetadata;
import org.chromium.chrome.browser.feed.library.api.host.action.StreamActionApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.InternalFeedError;
import org.chromium.chrome.browser.feed.library.api.internal.actionparser.ActionParser;
import org.chromium.chrome.browser.feed.library.api.internal.actionparser.ActionSource;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.feedactionparser.internal.ActionTypesConverter;
import org.chromium.chrome.browser.feed.library.feedactionparser.internal.PietFeedActionPayloadRetriever;
import org.chromium.chrome.browser.feed.library.feedactionparser.internal.TooltipInfoImpl;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.ui.action.FeedActionPayloadProto.FeedActionPayload;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedAction;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata.Type;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.OpenUrlData;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.Action;
import org.chromium.components.feed.core.proto.ui.piet.LogDataProto.LogData;

import java.util.List;

/**
 * Action parser which is able to parse Feed actions and notify clients about which action needs to
 * be performed.
 */
public final class FeedActionParser implements ActionParser {
    private static final String TAG = "FeedActionParser";

    private final PietFeedActionPayloadRetriever mPietFeedActionPayloadRetriever;
    private final ProtocolAdapter mProtocolAdapter;
    private final Supplier</*@Nullable*/ ContentMetadata> mContentMetadata;
    private final BasicLoggingApi mBasicLoggingApi;

    FeedActionParser(ProtocolAdapter protocolAdapter,
            PietFeedActionPayloadRetriever pietFeedActionPayloadRetriever,
            Supplier</*@Nullable*/ ContentMetadata> contentMetadata,
            BasicLoggingApi basicLoggingApi) {
        this.mProtocolAdapter = protocolAdapter;
        this.mPietFeedActionPayloadRetriever = pietFeedActionPayloadRetriever;
        this.mContentMetadata = contentMetadata;
        this.mBasicLoggingApi = basicLoggingApi;
    }

    @Override
    public void parseAction(Action action, StreamActionApi streamActionApi, View view,
            LogData logData, @ActionSource int actionSource) {
        FeedActionPayload feedActionPayload =
                mPietFeedActionPayloadRetriever.getFeedActionPayload(action);
        if (feedActionPayload == null) {
            Logger.w(TAG, "Unable to get FeedActionPayload from PietFeedActionPayloadRetriever");
            return;
        }
        parseFeedActionPayload(feedActionPayload, streamActionApi, view, actionSource);
    }

    @Override
    public void parseFeedActionPayload(FeedActionPayload feedActionPayload,
            StreamActionApi streamActionApi, View view, @ActionSource int actionSource) {
        FeedActionMetadata feedActionMetadata =
                feedActionPayload.getExtension(FeedAction.feedActionExtension).getMetadata();
        switch (feedActionMetadata.getType()) {
            case OPEN_URL:
            case OPEN_URL_NEW_WINDOW:
            case OPEN_URL_INCOGNITO:
            case OPEN_URL_NEW_TAB:
                handleOpenUrl(feedActionMetadata.getType(), feedActionMetadata.getOpenUrlData(),
                        streamActionApi);
                break;
            case OPEN_CONTEXT_MENU:
                if (!streamActionApi.canOpenContextMenu()) {
                    Logger.e(TAG, "Cannot open context menu: StreamActionApi does not support it.");
                    break;
                }

                if (!feedActionMetadata.hasOpenContextMenuData()) {
                    Logger.e(TAG, "Cannot open context menu: does not have context menu data.");
                    break;
                }

                streamActionApi.openContextMenu(feedActionMetadata.getOpenContextMenuData(), view);
                break;
            case DISMISS:
            case DISMISS_LOCAL:
                if (!streamActionApi.canDismiss()) {
                    Logger.e(TAG, "Cannot dismiss: StreamActionApi does not support it.");
                    return;
                }

                Result<List<StreamDataOperation>> streamDataOperationsResult =
                        mProtocolAdapter.createOperations(
                                feedActionMetadata.getDismissData().getDataOperationsList());

                if (!streamDataOperationsResult.isSuccessful()) {
                    Logger.e(TAG, "Cannot dismiss: conversion to StreamDataOperation failed.");
                    return;
                }
                if (!feedActionMetadata.getDismissData().hasContentId()) {
                    Logger.e(TAG, "Cannot dismiss: no Content Id");
                    return;
                }
                // TODO: Once we start logging DISMISS via the feed action end point, DISMISS
                // and DISMISS_LOCAL should not be handled in the exact same way.
                streamActionApi.dismiss(mProtocolAdapter.getStreamContentId(
                                                feedActionMetadata.getDismissData().getContentId()),
                        streamDataOperationsResult.getValue(),
                        feedActionMetadata.getDismissData().getUndoAction(),
                        feedActionMetadata.getDismissData().getPayload());

                break;
            case NOT_INTERESTED_IN:
                if (!streamActionApi.canHandleNotInterestedIn()) {
                    Logger.e(TAG,
                            "Cannot preform action not interested in action: StreamActionApi does"
                                    + " not support it.");
                    return;
                }

                Result<List<StreamDataOperation>> streamDataOperationResult =
                        mProtocolAdapter.createOperations(
                                feedActionMetadata.getNotInterestedInData()
                                        .getDataOperationsList());

                if (!streamDataOperationResult.isSuccessful()) {
                    Logger.e(TAG,
                            "Cannot preform action not interested in action: conversion to"
                                    + " StreamDataOperation failed.");
                    return;
                }

                streamActionApi.handleNotInterestedIn(streamDataOperationResult.getValue(),
                        feedActionMetadata.getNotInterestedInData().getUndoAction(),
                        feedActionMetadata.getNotInterestedInData().getPayload(),
                        feedActionMetadata.getNotInterestedInData().getInterestTypeValue());
                break;

            case DOWNLOAD:
                if (!streamActionApi.canDownloadUrl()) {
                    Logger.e(TAG, "Cannot download: StreamActionApi does not support it");
                    break;
                }
                ContentMetadata contentMetadata = this.mContentMetadata.get();
                if (contentMetadata == null) {
                    Logger.e(TAG, " Cannot download: no ContentMetadata");
                    break;
                }

                streamActionApi.downloadUrl(contentMetadata);
                streamActionApi.onClientAction(ActionTypesConverter.convert(DOWNLOAD));
                break;
            case LEARN_MORE:
                if (!streamActionApi.canLearnMore()) {
                    Logger.e(TAG, "Cannot learn more: StreamActionApi does not support it");
                    break;
                }

                streamActionApi.learnMore();
                streamActionApi.onClientAction(ActionTypesConverter.convert(LEARN_MORE));
                break;
            case VIEW_ELEMENT:
                if (!streamActionApi.canHandleElementView()) {
                    Logger.e(TAG, "Cannot log Element View: StreamActionApi does not support it");
                    break;
                } else if (!feedActionMetadata.hasElementTypeValue()) {
                    Logger.e(TAG, "Cannot log ElementView : no Element Type");
                    break;
                }
                streamActionApi.onElementView(feedActionMetadata.getElementTypeValue());
                break;
            case HIDE_ELEMENT:
                if (!streamActionApi.canHandleElementHide()) {
                    Logger.e(TAG, "Cannot log Element Hide: StreamActionApi does not support it");
                    break;
                } else if (!feedActionMetadata.hasElementTypeValue()) {
                    Logger.e(TAG, "Cannot log Element Hide : no Element Type");
                    break;
                }
                streamActionApi.onElementHide(feedActionMetadata.getElementTypeValue());
                break;
            case SHOW_TOOLTIP:
                if (!streamActionApi.canShowTooltip()) {
                    Logger.e(
                            TAG, "Cannot try to show tooltip: StreamActionApi does not support it");
                    break;
                }
                streamActionApi.maybeShowTooltip(
                        new TooltipInfoImpl(feedActionMetadata.getTooltipData()), view);
                break;
            default:
                Logger.wtf(TAG, "Haven't implemented host handling of %s",
                        feedActionMetadata.getType());
        }
        if (actionSource == ActionSource.CLICK) {
            if (!streamActionApi.canHandleElementClick()) {
                Logger.e(TAG, "Cannot log Element Click: StreamActionApi does not support it");
            } else if (!feedActionMetadata.hasElementTypeValue()) {
                Logger.e(TAG, "Cannot log Element Click: no Element Type");
            } else {
                streamActionApi.onElementClick(feedActionMetadata.getElementTypeValue());
            }
        }
    }

    private void handleOpenUrl(
            Type urlType, OpenUrlData openUrlData, StreamActionApi streamActionApi) {
        checkState(urlType.equals(OPEN_URL) || urlType.equals(OPEN_URL_NEW_WINDOW)
                        || urlType.equals(OPEN_URL_INCOGNITO) || urlType.equals(OPEN_URL_NEW_TAB),
                "Attempting to handle URL that is not a URL type: %s", urlType);
        if (!canPerformAction(urlType, streamActionApi)) {
            Logger.e(TAG, "Cannot open URL action: %s, not supported.", urlType);
            return;
        }

        if (!openUrlData.hasUrl()) {
            mBasicLoggingApi.onInternalError(InternalFeedError.NO_URL_FOR_OPEN);
            Logger.e(TAG, "Cannot open URL action: %s, no URL available.", urlType);
            return;
        }

        String url = openUrlData.getUrl();
        switch (urlType) {
            case OPEN_URL:
                if (openUrlData.hasConsistencyTokenQueryParamName()) {
                    streamActionApi.openUrl(url, openUrlData.getConsistencyTokenQueryParamName());
                } else {
                    streamActionApi.openUrl(url);
                }
                break;
            case OPEN_URL_NEW_WINDOW:
                if (openUrlData.hasConsistencyTokenQueryParamName()) {
                    streamActionApi.openUrlInNewWindow(
                            url, openUrlData.getConsistencyTokenQueryParamName());
                } else {
                    streamActionApi.openUrlInNewWindow(url);
                }
                break;
            case OPEN_URL_INCOGNITO:
                if (openUrlData.hasConsistencyTokenQueryParamName()) {
                    streamActionApi.openUrlInIncognitoMode(
                            url, openUrlData.getConsistencyTokenQueryParamName());
                } else {
                    streamActionApi.openUrlInIncognitoMode(url);
                }
                break;
            case OPEN_URL_NEW_TAB:
                if (openUrlData.hasConsistencyTokenQueryParamName()) {
                    streamActionApi.openUrlInNewTab(
                            url, openUrlData.getConsistencyTokenQueryParamName());
                } else {
                    streamActionApi.openUrlInNewTab(url);
                }
                break;
            default:
                throw new AssertionError("Unhandled URL type: " + urlType);
        }
        streamActionApi.onClientAction(ActionTypesConverter.convert(urlType));
    }

    @Override
    public boolean canPerformAction(
            FeedActionPayload feedActionPayload, StreamActionApi streamActionApi) {
        return canPerformAction(feedActionPayload.getExtension(FeedAction.feedActionExtension)
                                        .getMetadata()
                                        .getType(),
                streamActionApi);
    }

    private boolean canPerformAction(Type type, StreamActionApi streamActionApi) {
        switch (type) {
            case OPEN_URL:
                return streamActionApi.canOpenUrl();
            case OPEN_URL_NEW_WINDOW:
                return streamActionApi.canOpenUrlInNewWindow();
            case OPEN_URL_INCOGNITO:
                return streamActionApi.canOpenUrlInIncognitoMode();
            case OPEN_URL_NEW_TAB:
                return streamActionApi.canOpenUrlInNewTab();
            case OPEN_CONTEXT_MENU:
                return streamActionApi.canOpenContextMenu();
            case DISMISS:
            case DISMISS_LOCAL:
                // TODO: Once we start logging DISMISS via the feed action end point, DISMISS
                // and DISMISS_LOCAL should not be handled in the exact same way.
                return streamActionApi.canDismiss();
            case DOWNLOAD:
                return mContentMetadata.get() != null && streamActionApi.canDownloadUrl();
            case LEARN_MORE:
                return streamActionApi.canLearnMore();
            case NOT_INTERESTED_IN:
                return streamActionApi.canHandleNotInterestedIn();
            case UNKNOWN:
            default:
                // TODO : Handle the action types introduced in [INTERNAL LINK]
        }
        Logger.e(TAG, "Unhandled feed action type: %s", type);
        return false;
    }
}
