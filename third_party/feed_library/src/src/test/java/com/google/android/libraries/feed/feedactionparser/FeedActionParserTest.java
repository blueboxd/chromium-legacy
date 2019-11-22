// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedactionparser;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.view.View;

import com.google.android.libraries.feed.api.client.knowncontent.ContentMetadata;
import com.google.android.libraries.feed.api.host.action.StreamActionApi;
import com.google.android.libraries.feed.api.host.logging.ActionType;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.InternalFeedError;
import com.google.android.libraries.feed.api.host.stream.TooltipInfo;
import com.google.android.libraries.feed.api.internal.actionparser.ActionSource;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.feedactionparser.internal.PietFeedActionPayloadRetriever;
import com.google.common.collect.Lists;
import com.google.search.now.feed.client.StreamDataProto.StreamDataOperation;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.ui.action.FeedActionPayloadProto.FeedActionPayload;
import com.google.search.now.ui.action.FeedActionProto.DismissData;
import com.google.search.now.ui.action.FeedActionProto.FeedAction;
import com.google.search.now.ui.action.FeedActionProto.FeedActionMetadata;
import com.google.search.now.ui.action.FeedActionProto.FeedActionMetadata.ElementType;
import com.google.search.now.ui.action.FeedActionProto.FeedActionMetadata.Type;
import com.google.search.now.ui.action.FeedActionProto.Insets;
import com.google.search.now.ui.action.FeedActionProto.LabelledFeedActionData;
import com.google.search.now.ui.action.FeedActionProto.NotInterestedInData;
import com.google.search.now.ui.action.FeedActionProto.OpenContextMenuData;
import com.google.search.now.ui.action.FeedActionProto.OpenUrlData;
import com.google.search.now.ui.action.FeedActionProto.TooltipData;
import com.google.search.now.ui.action.FeedActionProto.TooltipData.FeatureName;
import com.google.search.now.ui.action.FeedActionProto.UndoAction;
import com.google.search.now.ui.action.PietExtensionsProto.PietFeedActionPayload;
import com.google.search.now.ui.piet.ActionsProto.Action;
import com.google.search.now.ui.piet.LogDataProto.LogData;
import com.google.search.now.wire.feed.ActionPayloadProto.ActionPayload;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;
import com.google.search.now.wire.feed.DataOperationProto.DataOperation;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

import java.util.List;

/** Tests for {@link FeedActionParser}. */
@RunWith(RobolectricTestRunner.class)
public class FeedActionParserTest {
    private static final String URL = "www.google.com";
    private static final String PARAM = "param";

    // clang-format off

    private static final FeedActionPayload OPEN_URL_FEED_ACTION =
        FeedActionPayload.newBuilder()
        .setExtension(FeedAction.feedActionExtension,
            FeedAction.newBuilder()
            .setMetadata(
                FeedActionMetadata.newBuilder()
                .setType(Type.OPEN_URL)
                .setOpenUrlData(
                    OpenUrlData.newBuilder().setUrl(URL)))
            .build())
        .build();
    private static final FeedActionPayload OPEN_URL_WITH_PARAM_FEED_ACTION =
        FeedActionPayload.newBuilder()
        .setExtension(FeedAction.feedActionExtension,
            FeedAction.newBuilder()
            .setMetadata(
                FeedActionMetadata.newBuilder()
                .setType(Type.OPEN_URL)
                .setOpenUrlData(
                    OpenUrlData.newBuilder()
                    .setUrl(URL)
                    .setConsistencyTokenQueryParamName(
                        PARAM)))
            .build())
        .build();
    private static final FeedActionPayload CONTEXT_MENU_FEED_ACTION =
        FeedActionPayload.newBuilder()
        .setExtension(FeedAction.feedActionExtension,
            FeedAction.newBuilder()
            .setMetadata(
                FeedActionMetadata.newBuilder()
                .setType(Type.OPEN_CONTEXT_MENU)
                .setOpenContextMenuData(
                    OpenContextMenuData.newBuilder().addContextMenuData(
                        LabelledFeedActionData
                        .newBuilder()
                        .setLabel("Open url")
                        .setFeedActionPayload(
                            OPEN_URL_FEED_ACTION))))
            .build())
        .build();

    private static final FeedActionPayload OPEN_URL_NEW_WINDOW_FEED_ACTION =
        FeedActionPayload.newBuilder()
        .setExtension(FeedAction.feedActionExtension,
            FeedAction.newBuilder()
            .setMetadata(
                FeedActionMetadata.newBuilder()
                .setType(Type.OPEN_URL_NEW_WINDOW)
                .setOpenUrlData(
                    OpenUrlData.newBuilder().setUrl(URL)))
            .build())
        .build();
    private static final FeedActionPayload OPEN_URL_NEW_WINDOW_WITH_PARAM_FEED_ACTION =
        FeedActionPayload.newBuilder()
        .setExtension(FeedAction.feedActionExtension,
            FeedAction.newBuilder()
            .setMetadata(
                FeedActionMetadata.newBuilder()
                .setType(Type.OPEN_URL_NEW_WINDOW)
                .setOpenUrlData(
                    OpenUrlData.newBuilder()
                    .setUrl(URL)
                    .setConsistencyTokenQueryParamName(
                        PARAM)))
            .build())
        .build();

    private static final FeedActionPayload OPEN_URL_INCOGNITO_FEED_ACTION =
        FeedActionPayload.newBuilder()
        .setExtension(FeedAction.feedActionExtension,
            FeedAction.newBuilder()
            .setMetadata(
                FeedActionMetadata.newBuilder()
                .setType(Type.OPEN_URL_INCOGNITO)
                .setOpenUrlData(
                    OpenUrlData.newBuilder().setUrl(URL)))
            .build())
        .build();
    private static final FeedActionPayload OPEN_URL_INCOGNITO_WITH_PARAM_FEED_ACTION =
        FeedActionPayload.newBuilder()
        .setExtension(FeedAction.feedActionExtension,
            FeedAction.newBuilder()
            .setMetadata(
                FeedActionMetadata.newBuilder()
                .setType(Type.OPEN_URL_INCOGNITO)
                .setOpenUrlData(
                    OpenUrlData.newBuilder()
                    .setUrl(URL)
                    .setConsistencyTokenQueryParamName(
                        PARAM)))
            .build())
        .build();

    private static final FeedActionPayload OPEN_URL_NEW_TAB_FEED_ACTION =
        FeedActionPayload.newBuilder()
        .setExtension(FeedAction.feedActionExtension,
            FeedAction.newBuilder()
            .setMetadata(
                FeedActionMetadata.newBuilder()
                .setType(Type.OPEN_URL_NEW_TAB)
                .setOpenUrlData(
                    OpenUrlData.newBuilder().setUrl(URL)))
            .build())
        .build();
    private static final FeedActionPayload OPEN_URL_NEW_TAB_WITH_PARAM_FEED_ACTION =
        FeedActionPayload.newBuilder()
        .setExtension(FeedAction.feedActionExtension,
            FeedAction.newBuilder()
            .setMetadata(
                FeedActionMetadata.newBuilder()
                .setType(Type.OPEN_URL_NEW_TAB)
                .setOpenUrlData(
                    OpenUrlData.newBuilder()
                    .setUrl(URL)
                    .setConsistencyTokenQueryParamName(
                        PARAM)))
            .build())
        .build();

    private static final FeedActionPayload DOWNLOAD_URL_FEED_ACTION =
        FeedActionPayload.newBuilder()
        .setExtension(FeedAction.feedActionExtension,
            FeedAction.newBuilder()
            .setMetadata(FeedActionMetadata.newBuilder()
              .setType(Type.DOWNLOAD)
              .build())
            .build())
        .build();

    private static final FeedActionPayload LEARN_MORE_FEED_ACTION =
        FeedActionPayload.newBuilder()
        .setExtension(FeedAction.feedActionExtension,
            FeedAction.newBuilder()
            .setMetadata(FeedActionMetadata.newBuilder().setType(
                Type.LEARN_MORE))
            .build())
        .build();

    private static final Action OPEN_URL_ACTION =
        Action.newBuilder()
        .setExtension(PietFeedActionPayload.pietFeedActionPayloadExtension,
            PietFeedActionPayload.newBuilder()
            .setFeedActionPayload(OPEN_URL_FEED_ACTION)
            .build())
        .build();

    private static final Action OPEN_URL_WITH_PARAM_ACTION =
        Action.newBuilder()
        .setExtension(PietFeedActionPayload.pietFeedActionPayloadExtension,
            PietFeedActionPayload.newBuilder()
            .setFeedActionPayload(OPEN_URL_WITH_PARAM_FEED_ACTION)
            .build())
        .build();

    private static final Action OPEN_INCOGNITO_ACTION =
        Action.newBuilder()
        .setExtension(PietFeedActionPayload.pietFeedActionPayloadExtension,
            PietFeedActionPayload.newBuilder()
            .setFeedActionPayload(OPEN_URL_INCOGNITO_FEED_ACTION)
            .build())
        .build();
    private static final Action OPEN_INCOGNITO_WITH_PARAM_ACTION =
        Action.newBuilder()
        .setExtension(PietFeedActionPayload.pietFeedActionPayloadExtension,
            PietFeedActionPayload.newBuilder()
            .setFeedActionPayload(OPEN_URL_INCOGNITO_WITH_PARAM_FEED_ACTION)
            .build())
        .build();

    private static final Action OPEN_NEW_WINDOW_ACTION =
        Action.newBuilder()
        .setExtension(PietFeedActionPayload.pietFeedActionPayloadExtension,
            PietFeedActionPayload.newBuilder()
            .setFeedActionPayload(OPEN_URL_NEW_WINDOW_FEED_ACTION)
            .build())
        .build();
    private static final Action OPEN_NEW_WINDOW_WITH_PARAM_ACTION =
        Action.newBuilder()
        .setExtension(PietFeedActionPayload.pietFeedActionPayloadExtension,
            PietFeedActionPayload.newBuilder()
            .setFeedActionPayload(
                OPEN_URL_NEW_WINDOW_WITH_PARAM_FEED_ACTION)
            .build())
        .build();
    private static final Action OPEN_NEW_TAB_ACTION =
        Action.newBuilder()
        .setExtension(PietFeedActionPayload.pietFeedActionPayloadExtension,
            PietFeedActionPayload.newBuilder()
            .setFeedActionPayload(OPEN_URL_NEW_TAB_FEED_ACTION)
            .build())
        .build();
    private static final Action OPEN_NEW_TAB_WITH_PARAM_ACTION =
        Action.newBuilder()
        .setExtension(PietFeedActionPayload.pietFeedActionPayloadExtension,
            PietFeedActionPayload.newBuilder()
            .setFeedActionPayload(OPEN_URL_NEW_TAB_WITH_PARAM_FEED_ACTION)
            .build())
        .build();

    private static final Action LEARN_MORE_ACTION =
        Action.newBuilder()
        .setExtension(PietFeedActionPayload.pietFeedActionPayloadExtension,
            PietFeedActionPayload.newBuilder()
            .setFeedActionPayload(LEARN_MORE_FEED_ACTION)
            .build())
        .build();
    private static final ContentId DISMISS_CONTENT_ID = ContentId.newBuilder().setId(123).build();

    private static final String DISMISS_CONTENT_ID_STRING = "dismissContentId";

    private static final UndoAction UNDO_ACTION =
        UndoAction.newBuilder().setConfirmationLabel("confirmation").build();

    private static final FeedActionPayload DISMISS_LOCAL_FEED_ACTION =
        FeedActionPayload.newBuilder()
        .setExtension(FeedAction.feedActionExtension,
            FeedAction.newBuilder()
            .setMetadata(
                FeedActionMetadata.newBuilder()
                .setType(Type.DISMISS_LOCAL)
                .setDismissData(
                    DismissData.newBuilder()
                    .addDataOperations(
                        DataOperation
                        .getDefaultInstance())
                    .setContentId(
                        DISMISS_CONTENT_ID)
                    .setUndoAction(UNDO_ACTION)))
            .build())
        .build();

    private static final FeedActionPayload NOT_INTERESTED_IN =
        FeedActionPayload.newBuilder()
        .setExtension(FeedAction.feedActionExtension,
            FeedAction.newBuilder()
            .setMetadata(
                FeedActionMetadata.newBuilder()
                .setType(Type.NOT_INTERESTED_IN)
                .setNotInterestedInData(
                    NotInterestedInData.newBuilder()
                    .addDataOperations(
                        DataOperation
                        .getDefaultInstance())
                    .setInterestTypeValue(
                        NotInterestedInData
                        .RecordedInterestType
                        .SOURCE
                        .getNumber())
                    .setPayload(
                        ActionPayload
                        .getDefaultInstance())
                    .setUndoAction(UNDO_ACTION)))
            .build())
        .build();

    private static final FeedActionPayload VIEW_ELEMENT =
        FeedActionPayload.newBuilder()
        .setExtension(FeedAction.feedActionExtension,
            FeedAction.newBuilder()
            .setMetadata(
                FeedActionMetadata.newBuilder()
                .setType(Type.VIEW_ELEMENT)
                .setElementTypeValue(ElementType.INTEREST_HEADER
                  .getNumber()))
            .build())
        .build();

    private static final FeedActionPayload HIDE_ELEMENT =
        FeedActionPayload.newBuilder()
        .setExtension(FeedAction.feedActionExtension,
            FeedAction.newBuilder()
            .setMetadata(
                FeedActionMetadata.newBuilder()
                .setType(Type.HIDE_ELEMENT)
                .setElementTypeValue(ElementType.INTEREST_HEADER
                  .getNumber()))
            .build())
        .build();

    private static final FeedActionPayload CLICK_ELEMENT =
        FeedActionPayload.newBuilder()
        .setExtension(FeedAction.feedActionExtension,
            FeedAction.newBuilder()
            .setMetadata(
                FeedActionMetadata.newBuilder()
                .setType(Type.HIDE_ELEMENT)
                .setElementTypeValue(ElementType.INTEREST_HEADER
                  .getNumber()))
            .build())
        .build();

    private static final FeedActionPayload DISMISS_LOCAL_FEED_ACTION_NO_CONTENT_ID =
        FeedActionPayload.newBuilder()
        .setExtension(FeedAction.feedActionExtension,
            FeedAction.newBuilder()
            .setMetadata(
                FeedActionMetadata.newBuilder()
                .setType(Type.DISMISS_LOCAL)
                .setDismissData(
                    DismissData.newBuilder().addDataOperations(
                        DataOperation
                        .getDefaultInstance())))
            .build())
        .build();

    private static final ContentMetadata CONTENT_METADATA = new ContentMetadata(URL, "title",
        /* timePublished= */ -1,
        /* imageUrl= */ null,
        /* publisher= */ null,
        /* faviconUrl= */ null,
        /* snippet= */ null);

    private static final FeedActionPayload OPEN_URL_MISSING_URL =
        FeedActionPayload.newBuilder()
        .setExtension(FeedAction.feedActionExtension,
            FeedAction.newBuilder()
            .setMetadata(FeedActionMetadata.newBuilder()
              .setType(Type.OPEN_URL)
              .setOpenUrlData(
                  OpenUrlData.getDefaultInstance()))
            .build())
        .build();

    @Mock
    private StreamActionApi streamActionApi;
    @Mock
    private ProtocolAdapter protocolAdapter;
    @Mock
    private BasicLoggingApi basicLoggingApi;

    List<StreamDataOperation> streamDataOperations = Lists.newArrayList(
            StreamDataOperation.newBuilder()
                    .setStreamStructure(
                            StreamStructure.newBuilder().setContentId("dataOpContentId"))
                    .build());

    private FeedActionParser feedActionParser;

    // clang-format on

    @Before
    public void setup() {
        initMocks(this);
        when(protocolAdapter.getStreamContentId(DISMISS_CONTENT_ID))
                .thenReturn(DISMISS_CONTENT_ID_STRING);
        feedActionParser = new FeedActionParser(protocolAdapter,
                new PietFeedActionPayloadRetriever(), () -> CONTENT_METADATA, basicLoggingApi);
    }

    @Test
    public void testParseAction() {
        when(streamActionApi.canOpenUrl()).thenReturn(true);
        feedActionParser.parseAction(OPEN_URL_ACTION, streamActionApi,
                /* view= */ null, LogData.getDefaultInstance(), ActionSource.CLICK);

        verify(streamActionApi).openUrl(URL);
        verify(streamActionApi).onClientAction(ActionType.OPEN_URL);
    }

    @Test
    public void testParseAction_openUrlWithParam() {
        when(streamActionApi.canOpenUrl()).thenReturn(true);
        feedActionParser.parseAction(OPEN_URL_WITH_PARAM_ACTION, streamActionApi,
                /* view= */ null, LogData.getDefaultInstance(), ActionSource.CLICK);

        verify(streamActionApi).openUrl(URL, PARAM);
        verify(streamActionApi).onClientAction(ActionType.OPEN_URL);
    }

    @Test
    public void testParseAction_newWindow() {
        when(streamActionApi.canOpenUrlInNewWindow()).thenReturn(true);
        feedActionParser.parseAction(OPEN_NEW_WINDOW_ACTION, streamActionApi,
                /* view= */ null, LogData.getDefaultInstance(), ActionSource.CLICK);

        verify(streamActionApi).openUrlInNewWindow(URL);
        verify(streamActionApi).onClientAction(ActionType.OPEN_URL_NEW_WINDOW);
    }

    @Test
    public void testParseAction_newWindowWithParam() {
        when(streamActionApi.canOpenUrlInNewWindow()).thenReturn(true);
        feedActionParser.parseAction(OPEN_NEW_WINDOW_WITH_PARAM_ACTION, streamActionApi,
                /* view= */ null, LogData.getDefaultInstance(), ActionSource.CLICK);

        verify(streamActionApi).openUrlInNewWindow(URL, PARAM);
        verify(streamActionApi).onClientAction(ActionType.OPEN_URL_NEW_WINDOW);
    }

    @Test
    public void testParseAction_incognito() {
        when(streamActionApi.canOpenUrlInIncognitoMode()).thenReturn(true);
        feedActionParser.parseAction(OPEN_INCOGNITO_ACTION, streamActionApi,
                /* view= */ null, LogData.getDefaultInstance(), ActionSource.CLICK);

        verify(streamActionApi).openUrlInIncognitoMode(URL);
        verify(streamActionApi).onClientAction(ActionType.OPEN_URL_INCOGNITO);
    }

    @Test
    public void testParseAction_incognitoWithParam() {
        when(streamActionApi.canOpenUrlInIncognitoMode()).thenReturn(true);
        feedActionParser.parseAction(OPEN_INCOGNITO_WITH_PARAM_ACTION, streamActionApi,
                /* view= */ null, LogData.getDefaultInstance(), ActionSource.CLICK);

        verify(streamActionApi).openUrlInIncognitoMode(URL, PARAM);
        verify(streamActionApi).onClientAction(ActionType.OPEN_URL_INCOGNITO);
    }

    @Test
    public void testParseAction_newTab() {
        when(streamActionApi.canOpenUrlInNewTab()).thenReturn(true);
        feedActionParser.parseAction(OPEN_NEW_TAB_ACTION, streamActionApi,
                /* view= */ null, LogData.getDefaultInstance(), ActionSource.CLICK);

        verify(streamActionApi).openUrlInNewTab(URL);
        verify(streamActionApi).onClientAction(ActionType.OPEN_URL_NEW_TAB);
    }

    @Test
    public void testParseAction_newTabWithParam() {
        when(streamActionApi.canOpenUrlInNewTab()).thenReturn(true);
        feedActionParser.parseAction(OPEN_NEW_TAB_WITH_PARAM_ACTION, streamActionApi,
                /* view= */ null, LogData.getDefaultInstance(), ActionSource.CLICK);

        verify(streamActionApi).openUrlInNewTab(URL, PARAM);
        verify(streamActionApi).onClientAction(ActionType.OPEN_URL_NEW_TAB);
    }

    @Test
    public void testParseAction_contextMenu() {
        Context context = Robolectric.buildActivity(Activity.class).get();
        View view = new View(context);

        when(streamActionApi.canOpenContextMenu()).thenReturn(true);
        PietFeedActionPayload contextMenuPietFeedAction =
                PietFeedActionPayload.newBuilder()
                        .setFeedActionPayload(CONTEXT_MENU_FEED_ACTION)
                        .build();

        feedActionParser.parseAction(
                Action.newBuilder()
                        .setExtension(PietFeedActionPayload.pietFeedActionPayloadExtension,
                                contextMenuPietFeedAction)
                        .build(),
                streamActionApi,
                /* view= */ view, LogData.getDefaultInstance(), ActionSource.CLICK);

        verify(streamActionApi)
                .openContextMenu(contextMenuPietFeedAction.getFeedActionPayload()
                                         .getExtension(FeedAction.feedActionExtension)
                                         .getMetadata()
                                         .getOpenContextMenuData(),
                        view);
    }

    @Test
    public void testParseAction_downloadUrl() {
        when(streamActionApi.canDownloadUrl()).thenReturn(true);
        feedActionParser.parseFeedActionPayload(
                DOWNLOAD_URL_FEED_ACTION, streamActionApi, /* view= */ null, ActionSource.CLICK);
        verify(streamActionApi).downloadUrl(CONTENT_METADATA);
        verify(streamActionApi).onClientAction(ActionType.DOWNLOAD);
    }

    @Test
    public void testCanPerformActionFromContextMenu_openUrl() {
        when(streamActionApi.canOpenUrl()).thenReturn(true);
        assertThat(feedActionParser.canPerformAction(OPEN_URL_FEED_ACTION, streamActionApi))
                .isTrue();
        verify(streamActionApi).canOpenUrl();
        verifyNoMoreInteractions(streamActionApi);
    }

    @Test
    public void testCanPerformActionFromContextMenu_newWindow() {
        when(streamActionApi.canOpenUrlInNewWindow()).thenReturn(true);
        assertThat(
                feedActionParser.canPerformAction(OPEN_URL_NEW_WINDOW_FEED_ACTION, streamActionApi))
                .isTrue();
        verify(streamActionApi).canOpenUrlInNewWindow();
        verifyNoMoreInteractions(streamActionApi);
    }

    @Test
    public void testCanPerformActionFromContextMenu_incognito() {
        when(streamActionApi.canOpenUrlInIncognitoMode()).thenReturn(true);
        assertThat(
                feedActionParser.canPerformAction(OPEN_URL_INCOGNITO_FEED_ACTION, streamActionApi))
                .isTrue();
        verify(streamActionApi).canOpenUrlInIncognitoMode();
        verifyNoMoreInteractions(streamActionApi);
    }

    @Test
    public void testCanPerformActionFromContextMenu_nestedContextMenu() {
        when(streamActionApi.canOpenContextMenu()).thenReturn(true);

        assertThat(feedActionParser.canPerformAction(CONTEXT_MENU_FEED_ACTION, streamActionApi))
                .isTrue();
        verify(streamActionApi).canOpenContextMenu();
        verifyNoMoreInteractions(streamActionApi);
    }

    @Test
    public void testCanPerformActionFromContextMenu_downloadUrl() {
        when(streamActionApi.canDownloadUrl()).thenReturn(true);
        assertThat(feedActionParser.canPerformAction(DOWNLOAD_URL_FEED_ACTION, streamActionApi))
                .isTrue();
        verify(streamActionApi).canDownloadUrl();
        verifyNoMoreInteractions(streamActionApi);
    }

    @Test
    public void testCanPerformActionFromContextMenu_learnMore() {
        when(streamActionApi.canLearnMore()).thenReturn(true);
        assertThat(feedActionParser.canPerformAction(LEARN_MORE_FEED_ACTION, streamActionApi))
                .isTrue();
        verify(streamActionApi).canLearnMore();
        verifyNoMoreInteractions(streamActionApi);
    }

    @Test
    public void testDownloadUrl_noContentMetadata() {
        feedActionParser =
                new FeedActionParser(protocolAdapter, new PietFeedActionPayloadRetriever(),
                        /* contentMetadata= */ () -> null, basicLoggingApi);
        when(streamActionApi.canDownloadUrl()).thenReturn(true);
        feedActionParser.parseFeedActionPayload(
                DOWNLOAD_URL_FEED_ACTION, streamActionApi, /* view= */ null, ActionSource.CLICK);
        verify(streamActionApi, times(0)).downloadUrl(any(ContentMetadata.class));
    }

    @Test
    public void testDownloadUrl_noHostSupport() {
        when(streamActionApi.canDownloadUrl()).thenReturn(false);
        feedActionParser.parseFeedActionPayload(
                DOWNLOAD_URL_FEED_ACTION, streamActionApi, /* view= */ null, ActionSource.CLICK);
        verify(streamActionApi, times(0)).downloadUrl(any(ContentMetadata.class));
    }

    @Test
    public void testDismiss_noApiSupport() {
        when(streamActionApi.canDismiss()).thenReturn(false);
        when(protocolAdapter.createOperations(
                     DISMISS_LOCAL_FEED_ACTION.getExtension(FeedAction.feedActionExtension)
                             .getMetadata()
                             .getDismissData()
                             .getDataOperationsList()))
                .thenReturn(Result.success(streamDataOperations));
        feedActionParser.parseFeedActionPayload(
                DISMISS_LOCAL_FEED_ACTION, streamActionApi, /* view= */ null, ActionSource.CLICK);
        verify(streamActionApi, never())
                .dismiss(anyString(), ArgumentMatchers.anyList(), any(UndoAction.class),
                        any(ActionPayload.class));
    }

    @Test
    public void testNotInterestedIn_noApiSupport() {
        when(streamActionApi.canHandleNotInterestedIn()).thenReturn(false);
        when(protocolAdapter.createOperations(
                     NOT_INTERESTED_IN.getExtension(FeedAction.feedActionExtension)
                             .getMetadata()
                             .getNotInterestedInData()
                             .getDataOperationsList()))
                .thenReturn(Result.success(streamDataOperations));
        feedActionParser.parseFeedActionPayload(
                DISMISS_LOCAL_FEED_ACTION, streamActionApi, /* view= */ null, ActionSource.CLICK);
        verify(streamActionApi, never())
                .handleNotInterestedIn(ArgumentMatchers.anyList(), any(UndoAction.class),
                        any(ActionPayload.class), anyInt());
    }

    @Test
    public void testDismiss_noContentId() {
        when(streamActionApi.canDismiss()).thenReturn(true);
        when(protocolAdapter.createOperations(
                     DISMISS_LOCAL_FEED_ACTION.getExtension(FeedAction.feedActionExtension)
                             .getMetadata()
                             .getDismissData()
                             .getDataOperationsList()))
                .thenReturn(Result.success(streamDataOperations));
        feedActionParser.parseFeedActionPayload(DISMISS_LOCAL_FEED_ACTION_NO_CONTENT_ID,
                streamActionApi,
                /* view= */ null, ActionSource.CLICK);
        verify(streamActionApi, never())
                .dismiss(anyString(), ArgumentMatchers.anyList(), any(UndoAction.class),
                        any(ActionPayload.class));
    }

    @Test
    public void testDismiss_unsuccessfulCreateOperations() {
        when(streamActionApi.canDismiss()).thenReturn(true);

        when(protocolAdapter.createOperations(
                     DISMISS_LOCAL_FEED_ACTION.getExtension(FeedAction.feedActionExtension)
                             .getMetadata()
                             .getDismissData()
                             .getDataOperationsList()))
                .thenReturn(Result.failure());

        feedActionParser.parseFeedActionPayload(
                DISMISS_LOCAL_FEED_ACTION, streamActionApi, /* view= */ null, ActionSource.CLICK);

        verify(streamActionApi, never())
                .dismiss(anyString(), ArgumentMatchers.anyList(), any(UndoAction.class),
                        any(ActionPayload.class));
    }

    @Test
    public void testDismiss() {
        when(streamActionApi.canDismiss()).thenReturn(true);

        when(protocolAdapter.createOperations(
                     DISMISS_LOCAL_FEED_ACTION.getExtension(FeedAction.feedActionExtension)
                             .getMetadata()
                             .getDismissData()
                             .getDataOperationsList()))
                .thenReturn(Result.success(streamDataOperations));

        feedActionParser.parseFeedActionPayload(
                DISMISS_LOCAL_FEED_ACTION, streamActionApi, /* view= */ null, ActionSource.CLICK);

        verify(streamActionApi)
                .dismiss(DISMISS_CONTENT_ID_STRING, streamDataOperations, UNDO_ACTION,
                        ActionPayload.getDefaultInstance());
    }

    @Test
    public void testNotInterestedIn() {
        when(streamActionApi.canDismiss()).thenReturn(true);
        when(streamActionApi.canHandleNotInterestedIn()).thenReturn(true);

        when(protocolAdapter.createOperations(
                     NOT_INTERESTED_IN.getExtension(FeedAction.feedActionExtension)
                             .getMetadata()
                             .getNotInterestedInData()
                             .getDataOperationsList()))
                .thenReturn(Result.success(streamDataOperations));

        feedActionParser.parseFeedActionPayload(
                NOT_INTERESTED_IN, streamActionApi, /* view= */ null, ActionSource.CLICK);

        verify(streamActionApi)
                .handleNotInterestedIn(streamDataOperations, UNDO_ACTION,
                        ActionPayload.getDefaultInstance(),
                        NotInterestedInData.RecordedInterestType.SOURCE.getNumber());
    }

    @Test
    public void testLearnMore() {
        when(streamActionApi.canLearnMore()).thenReturn(true);
        feedActionParser.parseAction(LEARN_MORE_ACTION, streamActionApi,
                /* view= */ null, /* veLoggingToken- */
                null, ActionSource.CLICK);

        verify(streamActionApi).learnMore();
        verify(streamActionApi).onClientAction(ActionType.LEARN_MORE);
    }

    @Test
    public void testLearnMore_noApiSupport() {
        when(streamActionApi.canLearnMore()).thenReturn(false);
        feedActionParser.parseAction(LEARN_MORE_ACTION, streamActionApi,
                /* view= */ null, /* veLoggingToken- */
                null, ActionSource.CLICK);

        verify(streamActionApi, never()).learnMore();
    }

    @Test
    public void testOnHide() {
        when(streamActionApi.canHandleElementHide()).thenReturn(true);
        feedActionParser.parseFeedActionPayload(
                HIDE_ELEMENT, streamActionApi, /* view= */ null, ActionSource.VIEW);

        verify(streamActionApi).onElementHide(ElementType.INTEREST_HEADER.getNumber());

        verify(streamActionApi, never()).onElementClick(ElementType.INTEREST_HEADER.getNumber());
    }

    @Test
    public void testOnView() {
        when(streamActionApi.canHandleElementView()).thenReturn(true);
        feedActionParser.parseFeedActionPayload(
                VIEW_ELEMENT, streamActionApi, /* view= */ null, ActionSource.VIEW);

        verify(streamActionApi).onElementView(ElementType.INTEREST_HEADER.getNumber());

        verify(streamActionApi, never()).onElementClick(ElementType.INTEREST_HEADER.getNumber());
    }

    @Test
    public void testOnClick() {
        when(streamActionApi.canHandleElementClick()).thenReturn(true);
        feedActionParser.parseFeedActionPayload(
                CLICK_ELEMENT, streamActionApi, /* view= */ null, ActionSource.CLICK);

        verify(streamActionApi).onElementClick(ElementType.INTEREST_HEADER.getNumber());
    }

    @Test
    public void testOnClick_notLogClick() {
        when(streamActionApi.canHandleElementClick()).thenReturn(true);
        feedActionParser.parseFeedActionPayload(
                VIEW_ELEMENT, streamActionApi, /* view= */ null, ActionSource.VIEW);

        verify(streamActionApi, never()).onElementClick(ElementType.INTEREST_HEADER.getNumber());
    }

    @Test
    public void testOpenUrl_noUrl_logsError() {
        when(streamActionApi.canOpenUrl()).thenReturn(true);

        feedActionParser.parseFeedActionPayload(
                OPEN_URL_MISSING_URL, streamActionApi, /* view= */ null, ActionSource.CLICK);

        verify(basicLoggingApi).onInternalError(InternalFeedError.NO_URL_FOR_OPEN);
    }

    @Test
    public void testShowTooltip() {
        String tooltipLabel = "tooltip";
        String accessibilityLabel = "tool";
        int topInsert = 1;
        int bottomInsert = 3;
        when(streamActionApi.canShowTooltip()).thenReturn(true);
        ArgumentCaptor<TooltipInfo> tooltipInfoArgumentCaptor =
                ArgumentCaptor.forClass(TooltipInfo.class);
        FeedActionPayload tooltipData =
                FeedActionPayload.newBuilder()
                        .setExtension(FeedAction.feedActionExtension,
                                FeedAction.newBuilder()
                                        .setMetadata(
                                                FeedActionMetadata.newBuilder()
                                                        .setType(Type.SHOW_TOOLTIP)
                                                        .setTooltipData(
                                                                // clang-format off
                                          TooltipData.newBuilder()
                                                  .setLabel(tooltipLabel)
                                                  .setAccessibilityLabel(
                                                          accessibilityLabel)
                                                  .setFeatureName(
                                                          FeatureName
                                                                  .CARD_MENU)
                                                  .setInsets(
                                                          Insets.newBuilder()
                                                                  .setTop(topInsert)
                                                                  .setBottom(
                                                                          bottomInsert)
                                                                  .build())
                                                                // clang-format on
                                                                ))
                                        .build())
                        .build();
        View view = new View(Robolectric.buildActivity(Activity.class).get());
        feedActionParser.parseFeedActionPayload(
                tooltipData, streamActionApi, view, ActionSource.VIEW);
        verify(streamActionApi).maybeShowTooltip(tooltipInfoArgumentCaptor.capture(), eq(view));
        TooltipInfo tooltipInfo = tooltipInfoArgumentCaptor.getValue();
        assertThat(tooltipInfo.getLabel()).isEqualTo(tooltipLabel);
        assertThat(tooltipInfo.getAccessibilityLabel()).isEqualTo(accessibilityLabel);
        assertThat(tooltipInfo.getFeatureName())
                .isEqualTo(TooltipInfo.FeatureName.CARD_MENU_TOOLTIP);
        assertThat(tooltipInfo.getTopInset()).isEqualTo(topInsert);
        assertThat(tooltipInfo.getBottomInset()).isEqualTo(bottomInsert);
    }

    @Test
    public void testShowTooltip_notSupported() {
        when(streamActionApi.canShowTooltip()).thenReturn(false);

        FeedActionPayload tooltipData =
                FeedActionPayload.newBuilder()
                        .setExtension(FeedAction.feedActionExtension,
                                FeedAction.newBuilder()
                                        .setMetadata(FeedActionMetadata.newBuilder().setType(
                                                Type.SHOW_TOOLTIP))
                                        .build())
                        .build();
        View view = new View(Robolectric.buildActivity(Activity.class).get());
        feedActionParser.parseFeedActionPayload(
                tooltipData, streamActionApi, view, ActionSource.VIEW);
        verify(streamActionApi, never()).maybeShowTooltip(any(TooltipInfo.class), any(View.class));
    }
}
