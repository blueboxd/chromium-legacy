// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedactionparser;

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

import com.google.common.collect.Lists;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.client.knowncontent.ContentMetadata;
import org.chromium.chrome.browser.feed.library.api.host.action.StreamActionApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.ActionType;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.InternalFeedError;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipInfo;
import org.chromium.chrome.browser.feed.library.api.internal.actionparser.ActionSource;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.feedactionparser.internal.PietFeedActionPayloadRetriever;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.ui.action.FeedActionPayloadProto.FeedActionPayload;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.BlockContentData;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.DismissData;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedAction;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata.ElementType;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata.Type;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.Insets;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.LabelledFeedActionData;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.NotInterestedInData;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.OpenContextMenuData;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.OpenUrlData;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.TooltipData;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.TooltipData.FeatureName;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.UndoAction;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.ViewReportData;
import org.chromium.components.feed.core.proto.ui.action.PietExtensionsProto.PietFeedActionPayload;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.Action;
import org.chromium.components.feed.core.proto.ui.piet.LogDataProto.LogData;
import org.chromium.components.feed.core.proto.wire.ActionPayloadProto.ActionPayload;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.components.feed.core.proto.wire.DataOperationProto.DataOperation;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.List;

/** Tests for {@link FeedActionParser}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedActionParserTest {
    private static final String URL = "www.google.com";
    private static final String PARAM = "param";

    // clang-format off

    private static final ContentId CONTENT_ID = ContentId.newBuilder().setId(123).build();

    private static final String CONTENT_ID_STRING = "contentId";

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

    private static final FeedActionPayload OPEN_URL_WITH_CLICK_PAYLOAD_FEED_ACTION =
        FeedActionPayload.newBuilder()
        .setExtension(FeedAction.feedActionExtension,
            FeedAction.newBuilder()
            .setMetadata(
                FeedActionMetadata.newBuilder()
                .setType(Type.OPEN_URL)
                .setOpenUrlData(
                    OpenUrlData.newBuilder()
                    .setUrl(URL)
                    .setContentId(CONTENT_ID)
                    .setPayload(ActionPayload.getDefaultInstance())))
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

    private static final FeedActionPayload OPEN_URL_INCOGNITO_WITH_CLICK_PAYLOAD_FEED_ACTION =
        FeedActionPayload.newBuilder()
        .setExtension(FeedAction.feedActionExtension,
            FeedAction.newBuilder()
            .setMetadata(
                FeedActionMetadata.newBuilder()
                .setType(Type.OPEN_URL_INCOGNITO)
                .setOpenUrlData(
                    OpenUrlData.newBuilder()
                    .setUrl(URL)
                    .setContentId(CONTENT_ID)
                    .setPayload(ActionPayload.getDefaultInstance())))
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

    private static final FeedActionPayload MANAGE_INTERESTS_FEED_ACTION =
        FeedActionPayload.newBuilder()
        .setExtension(FeedAction.feedActionExtension,
            FeedAction.newBuilder()
            .setMetadata(
                FeedActionMetadata.newBuilder()
                .setType(Type.OPEN_URL)
                .setOpenUrlData(
                    OpenUrlData.newBuilder().setUrl(
                        FeedActionParser.EXPECTED_MANAGE_INTERESTS_URL)))
                .build())
            .build();

    private static final FeedActionPayload SEND_FEEDBACK_FEED_ACTION =
        FeedActionPayload.newBuilder()
        .setExtension(FeedAction.feedActionExtension,
            FeedAction.newBuilder()
            .setMetadata(FeedActionMetadata.newBuilder().setType(
                Type.SEND_FEEDBACK))
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

       private static final Action OPEN_URL_WITH_CLICK_PAYLOAD_ACTION =
        Action.newBuilder()
        .setExtension(PietFeedActionPayload.pietFeedActionPayloadExtension,
            PietFeedActionPayload.newBuilder()
            .setFeedActionPayload(OPEN_URL_WITH_CLICK_PAYLOAD_FEED_ACTION)
            .build())
        .build();

    private static final Action OPEN_INCOGNITO_ACTION =
        Action.newBuilder()
        .setExtension(PietFeedActionPayload.pietFeedActionPayloadExtension,
            PietFeedActionPayload.newBuilder()
            .setFeedActionPayload(OPEN_URL_INCOGNITO_FEED_ACTION)
            .build())
        .build();

    private static final Action OPEN_INCOGNITO_WITH_CLICK_PAYLOAD_ACTION =
        Action.newBuilder()
        .setExtension(PietFeedActionPayload.pietFeedActionPayloadExtension,
            PietFeedActionPayload.newBuilder()
            .setFeedActionPayload(OPEN_URL_INCOGNITO_WITH_CLICK_PAYLOAD_FEED_ACTION)
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

    private static final Action MANAGE_INTERESTS_ACTION =
        Action.newBuilder()
        .setExtension(PietFeedActionPayload.pietFeedActionPayloadExtension,
            PietFeedActionPayload.newBuilder()
            .setFeedActionPayload(MANAGE_INTERESTS_FEED_ACTION)
            .build())
        .build();

    private static final Action SEND_FEEDBACK_ACTION =
        Action.newBuilder()
        .setExtension(PietFeedActionPayload.pietFeedActionPayloadExtension,
            PietFeedActionPayload.newBuilder()
            .setFeedActionPayload(SEND_FEEDBACK_FEED_ACTION)
            .build())
        .build();

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
                        CONTENT_ID)
                    .setUndoAction(UNDO_ACTION)))
            .build())
        .build();

     private static final FeedActionPayload BLOCK_CONTENT_ACTION =
        FeedActionPayload.newBuilder()
        .setExtension(FeedAction.feedActionExtension,
            FeedAction.newBuilder()
            .setMetadata(
                FeedActionMetadata.newBuilder()
                .setType(Type.BLOCK_CONTENT)
                .setBlockContentData(
                    BlockContentData.newBuilder()
                    .addDataOperations(
                        DataOperation
                        .getDefaultInstance())
                    .setPayload(ActionPayload.getDefaultInstance())
                    ))
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
    private StreamActionApi mStreamActionApi;
    @Mock
    private ProtocolAdapter mProtocolAdapter;
    @Mock
    private BasicLoggingApi mBasicLoggingApi;

    List<StreamDataOperation> mStreamDataOperations = Lists.newArrayList(
            StreamDataOperation.newBuilder()
                    .setStreamStructure(
                            StreamStructure.newBuilder().setContentId("dataOpContentId"))
                    .build());

    private FeedActionParser mFeedActionParser;

    // clang-format on

    @Before
    public void setup() {
        initMocks(this);
        when(mProtocolAdapter.getStreamContentId(CONTENT_ID)).thenReturn(CONTENT_ID_STRING);
        mFeedActionParser = new FeedActionParser(mProtocolAdapter,
                new PietFeedActionPayloadRetriever(), () -> CONTENT_METADATA, mBasicLoggingApi);
    }

    @Test
    public void testParseAction() {
        when(mStreamActionApi.canOpenUrl()).thenReturn(true);
        mFeedActionParser.parseAction(OPEN_URL_ACTION, mStreamActionApi,
                /* view= */ null, LogData.getDefaultInstance(), ActionSource.CLICK);

        verify(mStreamActionApi).openUrl(URL);
        verify(mStreamActionApi).onClientAction(ActionType.OPEN_URL);
    }

    @Test
    public void testParseAction_manageInterests() {
        when(mStreamActionApi.canOpenUrl()).thenReturn(true);
        mFeedActionParser.parseAction(MANAGE_INTERESTS_ACTION, mStreamActionApi,
                /* view= */ null, LogData.getDefaultInstance(), ActionSource.CLICK);

        verify(mStreamActionApi).openUrl(FeedActionParser.EXPECTED_MANAGE_INTERESTS_URL);
        verify(mStreamActionApi).onClientAction(ActionType.MANAGE_INTERESTS);
    }

    @Test
    public void testParseAction_openUrlWithParam() {
        when(mStreamActionApi.canOpenUrl()).thenReturn(true);
        mFeedActionParser.parseAction(OPEN_URL_WITH_PARAM_ACTION, mStreamActionApi,
                /* view= */ null, LogData.getDefaultInstance(), ActionSource.CLICK);

        verify(mStreamActionApi).openUrl(URL, PARAM);
        verify(mStreamActionApi).onClientAction(ActionType.OPEN_URL);
    }

    @Test
    public void testParseAction_newWindow() {
        when(mStreamActionApi.canOpenUrlInNewWindow()).thenReturn(true);
        mFeedActionParser.parseAction(OPEN_NEW_WINDOW_ACTION, mStreamActionApi,
                /* view= */ null, LogData.getDefaultInstance(), ActionSource.CLICK);

        verify(mStreamActionApi).openUrlInNewWindow(URL);
        verify(mStreamActionApi).onClientAction(ActionType.OPEN_URL_NEW_WINDOW);
    }

    @Test
    public void testParseAction_newWindowWithParam() {
        when(mStreamActionApi.canOpenUrlInNewWindow()).thenReturn(true);
        mFeedActionParser.parseAction(OPEN_NEW_WINDOW_WITH_PARAM_ACTION, mStreamActionApi,
                /* view= */ null, LogData.getDefaultInstance(), ActionSource.CLICK);

        verify(mStreamActionApi).openUrlInNewWindow(URL, PARAM);
        verify(mStreamActionApi).onClientAction(ActionType.OPEN_URL_NEW_WINDOW);
    }

    @Test
    public void testParseAction_incognito() {
        when(mStreamActionApi.canOpenUrlInIncognitoMode()).thenReturn(true);
        mFeedActionParser.parseAction(OPEN_INCOGNITO_ACTION, mStreamActionApi,
                /* view= */ null, LogData.getDefaultInstance(), ActionSource.CLICK);

        verify(mStreamActionApi).openUrlInIncognitoMode(URL);
        verify(mStreamActionApi).onClientAction(ActionType.OPEN_URL_INCOGNITO);
    }

    @Test
    public void testParseAction_incognitoWithParam() {
        when(mStreamActionApi.canOpenUrlInIncognitoMode()).thenReturn(true);
        mFeedActionParser.parseAction(OPEN_INCOGNITO_WITH_PARAM_ACTION, mStreamActionApi,
                /* view= */ null, LogData.getDefaultInstance(), ActionSource.CLICK);

        verify(mStreamActionApi).openUrlInIncognitoMode(URL, PARAM);
        verify(mStreamActionApi).onClientAction(ActionType.OPEN_URL_INCOGNITO);
    }

    @Test
    public void testParseAction_incognitoNoClickAction() {
        when(mStreamActionApi.canOpenUrlInIncognitoMode()).thenReturn(true);
        mFeedActionParser.parseAction(OPEN_INCOGNITO_ACTION, mStreamActionApi,
                /* view= */ null, LogData.getDefaultInstance(), ActionSource.CLICK);

        verify(mStreamActionApi, never()).reportClickAction(anyString(), any(ActionPayload.class));

        verify(mStreamActionApi).openUrlInIncognitoMode(URL);
        verify(mStreamActionApi).onClientAction(ActionType.OPEN_URL_INCOGNITO);
    }

    @Test
    public void testParseAction_newTab() {
        when(mStreamActionApi.canOpenUrlInNewTab()).thenReturn(true);
        mFeedActionParser.parseAction(OPEN_NEW_TAB_ACTION, mStreamActionApi,
                /* view= */ null, LogData.getDefaultInstance(), ActionSource.CLICK);

        verify(mStreamActionApi).openUrlInNewTab(URL);
        verify(mStreamActionApi).onClientAction(ActionType.OPEN_URL_NEW_TAB);
    }

    @Test
    public void testParseAction_newTabWithParam() {
        when(mStreamActionApi.canOpenUrlInNewTab()).thenReturn(true);
        mFeedActionParser.parseAction(OPEN_NEW_TAB_WITH_PARAM_ACTION, mStreamActionApi,
                /* view= */ null, LogData.getDefaultInstance(), ActionSource.CLICK);

        verify(mStreamActionApi).openUrlInNewTab(URL, PARAM);
        verify(mStreamActionApi).onClientAction(ActionType.OPEN_URL_NEW_TAB);
    }

    @Test
    public void testParseAction_withClickAction() {
        when(mStreamActionApi.canOpenUrl()).thenReturn(true);
        mFeedActionParser.parseAction(OPEN_URL_WITH_CLICK_PAYLOAD_ACTION, mStreamActionApi,
                /* view= */ null, LogData.getDefaultInstance(), ActionSource.CLICK);

        verify(mStreamActionApi)
                .reportClickAction(CONTENT_ID_STRING, ActionPayload.getDefaultInstance());

        verify(mStreamActionApi).openUrl(URL);
        verify(mStreamActionApi).onClientAction(ActionType.OPEN_URL);
    }

    @Test
    public void testParseAction_contextMenu() {
        Context context = Robolectric.buildActivity(Activity.class).get();
        View view = new View(context);

        when(mStreamActionApi.canOpenContextMenu()).thenReturn(true);
        PietFeedActionPayload contextMenuPietFeedAction =
                PietFeedActionPayload.newBuilder()
                        .setFeedActionPayload(CONTEXT_MENU_FEED_ACTION)
                        .build();

        mFeedActionParser.parseAction(
                Action.newBuilder()
                        .setExtension(PietFeedActionPayload.pietFeedActionPayloadExtension,
                                contextMenuPietFeedAction)
                        .build(),
                mStreamActionApi,
                /* view= */ view, LogData.getDefaultInstance(), ActionSource.CLICK);

        verify(mStreamActionApi)
                .openContextMenu(contextMenuPietFeedAction.getFeedActionPayload()
                                         .getExtension(FeedAction.feedActionExtension)
                                         .getMetadata()
                                         .getOpenContextMenuData(),
                        view);
    }

    @Test
    public void testParseAction_downloadUrl() {
        when(mStreamActionApi.canDownloadUrl()).thenReturn(true);
        mFeedActionParser.parseFeedActionPayload(
                DOWNLOAD_URL_FEED_ACTION, mStreamActionApi, /* view= */ null, ActionSource.CLICK);
        verify(mStreamActionApi).downloadUrl(CONTENT_METADATA);
        verify(mStreamActionApi).onClientAction(ActionType.DOWNLOAD);
    }

    @Test
    public void testParseAction_reportViewVisible() {
        FeedAction reportViewAction =
                FeedAction.newBuilder()
                        .setMetadata(
                                FeedActionMetadata.newBuilder()
                                        .setType(Type.REPORT_VIEW)
                                        .setViewReportData(
                                                ViewReportData.newBuilder()
                                                        .setContentId(CONTENT_ID)
                                                        .setVisibility(
                                                                ViewReportData.Visibility.SHOW)
                                                        .setPayload(ActionPayload
                                                                            .getDefaultInstance())))
                        .build();

        FeedActionPayload reportViewActionPayload =
                FeedActionPayload.newBuilder()
                        .setExtension(FeedAction.feedActionExtension, reportViewAction)
                        .build();

        View view = new View(Robolectric.buildActivity(Activity.class).get());

        mFeedActionParser.parseFeedActionPayload(
                reportViewActionPayload, mStreamActionApi, view, ActionSource.VIEW);
        verify(mStreamActionApi)
                .reportViewVisible(view, CONTENT_ID_STRING, ActionPayload.getDefaultInstance());
    }

    @Test
    public void testParseAction_reportViewHidden() {
        FeedAction reportViewAction =
                FeedAction.newBuilder()
                        .setMetadata(
                                FeedActionMetadata.newBuilder()
                                        .setType(Type.REPORT_VIEW)
                                        .setViewReportData(
                                                ViewReportData.newBuilder()
                                                        .setContentId(CONTENT_ID)
                                                        .setVisibility(
                                                                ViewReportData.Visibility.HIDE)
                                                        .setPayload(ActionPayload
                                                                            .getDefaultInstance())))
                        .build();

        FeedActionPayload reportViewActionPayload =
                FeedActionPayload.newBuilder()
                        .setExtension(FeedAction.feedActionExtension, reportViewAction)
                        .build();

        View view = new View(Robolectric.buildActivity(Activity.class).get());

        mFeedActionParser.parseFeedActionPayload(
                reportViewActionPayload, mStreamActionApi, view, ActionSource.VIEW);
        verify(mStreamActionApi).reportViewHidden(view, CONTENT_ID_STRING);
    }

    @Test
    public void testCanPerformActionFromContextMenu_openUrl() {
        when(mStreamActionApi.canOpenUrl()).thenReturn(true);
        assertThat(mFeedActionParser.canPerformAction(OPEN_URL_FEED_ACTION, mStreamActionApi))
                .isTrue();
        verify(mStreamActionApi).canOpenUrl();
        verifyNoMoreInteractions(mStreamActionApi);
    }

    @Test
    public void testCanPerformActionFromContextMenu_newWindow() {
        when(mStreamActionApi.canOpenUrlInNewWindow()).thenReturn(true);
        assertThat(mFeedActionParser.canPerformAction(
                           OPEN_URL_NEW_WINDOW_FEED_ACTION, mStreamActionApi))
                .isTrue();
        verify(mStreamActionApi).canOpenUrlInNewWindow();
        verifyNoMoreInteractions(mStreamActionApi);
    }

    @Test
    public void testCanPerformActionFromContextMenu_incognito() {
        when(mStreamActionApi.canOpenUrlInIncognitoMode()).thenReturn(true);
        assertThat(mFeedActionParser.canPerformAction(
                           OPEN_URL_INCOGNITO_FEED_ACTION, mStreamActionApi))
                .isTrue();
        verify(mStreamActionApi).canOpenUrlInIncognitoMode();
        verifyNoMoreInteractions(mStreamActionApi);
    }

    @Test
    public void testCanPerformActionFromContextMenu_nestedContextMenu() {
        when(mStreamActionApi.canOpenContextMenu()).thenReturn(true);

        assertThat(mFeedActionParser.canPerformAction(CONTEXT_MENU_FEED_ACTION, mStreamActionApi))
                .isTrue();
        verify(mStreamActionApi).canOpenContextMenu();
        verifyNoMoreInteractions(mStreamActionApi);
    }

    @Test
    public void testCanPerformActionFromContextMenu_downloadUrl() {
        when(mStreamActionApi.canDownloadUrl()).thenReturn(true);
        assertThat(mFeedActionParser.canPerformAction(DOWNLOAD_URL_FEED_ACTION, mStreamActionApi))
                .isTrue();
        verify(mStreamActionApi).canDownloadUrl();
        verifyNoMoreInteractions(mStreamActionApi);
    }

    @Test
    public void testCanPerformActionFromContextMenu_learnMore() {
        when(mStreamActionApi.canLearnMore()).thenReturn(true);
        assertThat(mFeedActionParser.canPerformAction(LEARN_MORE_FEED_ACTION, mStreamActionApi))
                .isTrue();
        verify(mStreamActionApi).canLearnMore();
        verifyNoMoreInteractions(mStreamActionApi);
    }

    @Test
    public void testDownloadUrl_noContentMetadata() {
        mFeedActionParser =
                new FeedActionParser(mProtocolAdapter, new PietFeedActionPayloadRetriever(),
                        /* contentMetadata= */ () -> null, mBasicLoggingApi);
        when(mStreamActionApi.canDownloadUrl()).thenReturn(true);
        mFeedActionParser.parseFeedActionPayload(
                DOWNLOAD_URL_FEED_ACTION, mStreamActionApi, /* view= */ null, ActionSource.CLICK);
        verify(mStreamActionApi, times(0)).downloadUrl(any(ContentMetadata.class));
    }

    @Test
    public void testDownloadUrl_noHostSupport() {
        when(mStreamActionApi.canDownloadUrl()).thenReturn(false);
        mFeedActionParser.parseFeedActionPayload(
                DOWNLOAD_URL_FEED_ACTION, mStreamActionApi, /* view= */ null, ActionSource.CLICK);
        verify(mStreamActionApi, times(0)).downloadUrl(any(ContentMetadata.class));
    }

    @Test
    public void testDismiss_noApiSupport() {
        when(mStreamActionApi.canDismiss()).thenReturn(false);
        when(mProtocolAdapter.createOperations(
                     DISMISS_LOCAL_FEED_ACTION.getExtension(FeedAction.feedActionExtension)
                             .getMetadata()
                             .getDismissData()
                             .getDataOperationsList()))
                .thenReturn(mStreamDataOperations);
        mFeedActionParser.parseFeedActionPayload(
                DISMISS_LOCAL_FEED_ACTION, mStreamActionApi, /* view= */ null, ActionSource.CLICK);
        verify(mStreamActionApi, never())
                .dismiss(anyString(), ArgumentMatchers.anyList(), any(UndoAction.class),
                        any(ActionPayload.class));
    }

    @Test
    public void testBlockContent() {
        when(mProtocolAdapter.createOperations(
                     BLOCK_CONTENT_ACTION.getExtension(FeedAction.feedActionExtension)
                             .getMetadata()
                             .getBlockContentData()
                             .getDataOperationsList()))
                .thenReturn(mStreamDataOperations);
        mFeedActionParser.parseFeedActionPayload(
                BLOCK_CONTENT_ACTION, mStreamActionApi, /* view= */ null, ActionSource.CLICK);
        verify(mStreamActionApi)
                .handleBlockContent(mStreamDataOperations, ActionPayload.getDefaultInstance());
    }

    @Test
    public void testNotInterestedIn_noApiSupport() {
        when(mStreamActionApi.canHandleNotInterestedIn()).thenReturn(false);
        when(mProtocolAdapter.createOperations(
                     NOT_INTERESTED_IN.getExtension(FeedAction.feedActionExtension)
                             .getMetadata()
                             .getNotInterestedInData()
                             .getDataOperationsList()))
                .thenReturn(mStreamDataOperations);
        mFeedActionParser.parseFeedActionPayload(
                DISMISS_LOCAL_FEED_ACTION, mStreamActionApi, /* view= */ null, ActionSource.CLICK);
        verify(mStreamActionApi, never())
                .handleNotInterestedIn(ArgumentMatchers.anyList(), any(UndoAction.class),
                        any(ActionPayload.class), anyInt());
    }

    @Test
    public void testDismiss_noContentId() {
        when(mStreamActionApi.canDismiss()).thenReturn(true);
        when(mProtocolAdapter.createOperations(
                     DISMISS_LOCAL_FEED_ACTION.getExtension(FeedAction.feedActionExtension)
                             .getMetadata()
                             .getDismissData()
                             .getDataOperationsList()))
                .thenReturn(mStreamDataOperations);
        mFeedActionParser.parseFeedActionPayload(DISMISS_LOCAL_FEED_ACTION_NO_CONTENT_ID,
                mStreamActionApi,
                /* view= */ null, ActionSource.CLICK);
        verify(mStreamActionApi, never())
                .dismiss(anyString(), ArgumentMatchers.anyList(), any(UndoAction.class),
                        any(ActionPayload.class));
    }

    @Test
    public void testDismiss() {
        when(mStreamActionApi.canDismiss()).thenReturn(true);

        when(mProtocolAdapter.createOperations(
                     DISMISS_LOCAL_FEED_ACTION.getExtension(FeedAction.feedActionExtension)
                             .getMetadata()
                             .getDismissData()
                             .getDataOperationsList()))
                .thenReturn(mStreamDataOperations);

        mFeedActionParser.parseFeedActionPayload(
                DISMISS_LOCAL_FEED_ACTION, mStreamActionApi, /* view= */ null, ActionSource.CLICK);

        verify(mStreamActionApi)
                .dismiss(CONTENT_ID_STRING, mStreamDataOperations, UNDO_ACTION,
                        ActionPayload.getDefaultInstance());
    }

    @Test
    public void testNotInterestedIn() {
        when(mStreamActionApi.canDismiss()).thenReturn(true);
        when(mStreamActionApi.canHandleNotInterestedIn()).thenReturn(true);

        when(mProtocolAdapter.createOperations(
                     NOT_INTERESTED_IN.getExtension(FeedAction.feedActionExtension)
                             .getMetadata()
                             .getNotInterestedInData()
                             .getDataOperationsList()))
                .thenReturn(mStreamDataOperations);

        mFeedActionParser.parseFeedActionPayload(
                NOT_INTERESTED_IN, mStreamActionApi, /* view= */ null, ActionSource.CLICK);

        verify(mStreamActionApi)
                .handleNotInterestedIn(mStreamDataOperations, UNDO_ACTION,
                        ActionPayload.getDefaultInstance(),
                        NotInterestedInData.RecordedInterestType.SOURCE.getNumber());
    }

    @Test
    public void testLearnMore() {
        when(mStreamActionApi.canLearnMore()).thenReturn(true);
        mFeedActionParser.parseAction(LEARN_MORE_ACTION, mStreamActionApi,
                /* view= */ null, /* veLoggingToken- */
                null, ActionSource.CLICK);

        verify(mStreamActionApi).learnMore();
        verify(mStreamActionApi).onClientAction(ActionType.LEARN_MORE);
    }

    @Test
    public void testLearnMore_noApiSupport() {
        when(mStreamActionApi.canLearnMore()).thenReturn(false);
        mFeedActionParser.parseAction(LEARN_MORE_ACTION, mStreamActionApi,
                /* view= */ null, /* veLoggingToken- */
                null, ActionSource.CLICK);

        verify(mStreamActionApi, never()).learnMore();
    }

    @Test
    public void testSendFeedback() {
        when(mStreamActionApi.canLearnMore()).thenReturn(true);
        mFeedActionParser.parseAction(SEND_FEEDBACK_ACTION, mStreamActionApi,
                /* view= */ null, /* veLoggingToken- */
                null, ActionSource.CLICK);

        // TODO(petewil): Figure out how to verify that it worked as expected.
    }

    @Test
    public void testOnHide() {
        when(mStreamActionApi.canHandleElementHide()).thenReturn(true);
        mFeedActionParser.parseFeedActionPayload(
                HIDE_ELEMENT, mStreamActionApi, /* view= */ null, ActionSource.VIEW);

        verify(mStreamActionApi).onElementHide(ElementType.INTEREST_HEADER.getNumber());

        verify(mStreamActionApi, never()).onElementClick(ElementType.INTEREST_HEADER.getNumber());
    }

    @Test
    public void testOnView() {
        when(mStreamActionApi.canHandleElementView()).thenReturn(true);
        mFeedActionParser.parseFeedActionPayload(
                VIEW_ELEMENT, mStreamActionApi, /* view= */ null, ActionSource.VIEW);

        verify(mStreamActionApi).onElementView(ElementType.INTEREST_HEADER.getNumber());

        verify(mStreamActionApi, never()).onElementClick(ElementType.INTEREST_HEADER.getNumber());
    }

    @Test
    public void testOnClick() {
        when(mStreamActionApi.canHandleElementClick()).thenReturn(true);
        mFeedActionParser.parseFeedActionPayload(
                CLICK_ELEMENT, mStreamActionApi, /* view= */ null, ActionSource.CLICK);

        verify(mStreamActionApi).onElementClick(ElementType.INTEREST_HEADER.getNumber());
    }

    @Test
    public void testOnClick_notLogClick() {
        when(mStreamActionApi.canHandleElementClick()).thenReturn(true);
        mFeedActionParser.parseFeedActionPayload(
                VIEW_ELEMENT, mStreamActionApi, /* view= */ null, ActionSource.VIEW);

        verify(mStreamActionApi, never()).onElementClick(ElementType.INTEREST_HEADER.getNumber());
    }

    @Test
    public void testOpenUrl_noUrl_logsError() {
        when(mStreamActionApi.canOpenUrl()).thenReturn(true);

        mFeedActionParser.parseFeedActionPayload(
                OPEN_URL_MISSING_URL, mStreamActionApi, /* view= */ null, ActionSource.CLICK);

        verify(mBasicLoggingApi).onInternalError(InternalFeedError.NO_URL_FOR_OPEN);
    }

    @Test
    public void testShowTooltip() {
        String tooltipLabel = "tooltip";
        String accessibilityLabel = "tool";
        int topInsert = 1;
        int bottomInsert = 3;
        when(mStreamActionApi.canShowTooltip()).thenReturn(true);
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
        mFeedActionParser.parseFeedActionPayload(
                tooltipData, mStreamActionApi, view, ActionSource.VIEW);
        verify(mStreamActionApi).maybeShowTooltip(tooltipInfoArgumentCaptor.capture(), eq(view));
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
        when(mStreamActionApi.canShowTooltip()).thenReturn(false);

        FeedActionPayload tooltipData =
                FeedActionPayload.newBuilder()
                        .setExtension(FeedAction.feedActionExtension,
                                FeedAction.newBuilder()
                                        .setMetadata(FeedActionMetadata.newBuilder().setType(
                                                Type.SHOW_TOOLTIP))
                                        .build())
                        .build();
        View view = new View(Robolectric.buildActivity(Activity.class).get());
        mFeedActionParser.parseFeedActionPayload(
                tooltipData, mStreamActionApi, view, ActionSource.VIEW);
        verify(mStreamActionApi, never()).maybeShowTooltip(any(TooltipInfo.class), any(View.class));
    }
}
