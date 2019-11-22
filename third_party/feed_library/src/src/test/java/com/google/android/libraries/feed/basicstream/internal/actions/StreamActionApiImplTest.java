// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal.actions;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.view.View;

import com.google.android.libraries.feed.api.client.knowncontent.ContentMetadata;
import com.google.android.libraries.feed.api.host.action.ActionApi;
import com.google.android.libraries.feed.api.host.logging.ActionType;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.ContentLoggingData;
import com.google.android.libraries.feed.api.host.stream.TooltipApi;
import com.google.android.libraries.feed.api.host.stream.TooltipCallbackApi;
import com.google.android.libraries.feed.api.host.stream.TooltipCallbackApi.TooltipDismissType;
import com.google.android.libraries.feed.api.host.stream.TooltipInfo;
import com.google.android.libraries.feed.api.internal.actionmanager.ActionManager;
import com.google.android.libraries.feed.api.internal.actionparser.ActionParser;
import com.google.android.libraries.feed.api.internal.actionparser.ActionSource;
import com.google.android.libraries.feed.basicstream.internal.pendingdismiss.ClusterPendingDismissHelper;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.sharedstream.logging.StreamContentLoggingData;
import com.google.android.libraries.feed.sharedstream.pendingdismiss.PendingDismissCallback;
import com.google.android.libraries.feed.testing.sharedstream.contextmenumanager.FakeContextMenuManager;
import com.google.common.collect.ImmutableList;
import com.google.search.now.feed.client.StreamDataProto.StreamDataOperation;
import com.google.search.now.ui.action.FeedActionPayloadProto.FeedActionPayload;
import com.google.search.now.ui.action.FeedActionProto.FeedAction;
import com.google.search.now.ui.action.FeedActionProto.FeedActionMetadata;
import com.google.search.now.ui.action.FeedActionProto.FeedActionMetadata.ElementType;
import com.google.search.now.ui.action.FeedActionProto.FeedActionMetadata.Type;
import com.google.search.now.ui.action.FeedActionProto.LabelledFeedActionData;
import com.google.search.now.ui.action.FeedActionProto.OpenContextMenuData;
import com.google.search.now.ui.action.FeedActionProto.OpenUrlData;
import com.google.search.now.ui.action.FeedActionProto.UndoAction;
import com.google.search.now.ui.stream.StreamStructureProto.BasicLoggingMetadata;
import com.google.search.now.ui.stream.StreamStructureProto.RepresentationData;
import com.google.search.now.wire.feed.ActionPayloadProto.ActionPayload;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Tests for {@link StreamActionApiImpl}. */
@RunWith(RobolectricTestRunner.class)
public class StreamActionApiImplTest {
    private static final String URL = "www.google.com";
    private static final String OPEN_LABEL = "Open";
    private static final String OPEN_IN_NEW_WINDOW_LABEL = "Open in new window";
    private static final String PARAM = "param";
    private static final String NEW_URL = "ooh.shiny.com";
    private static final int INTEREST_TYPE = 2;
    private static final ActionPayload ACTION_PAYLOAD = ActionPayload.getDefaultInstance();
    private static final LabelledFeedActionData OPEN_IN_NEW_WINDOW =
            LabelledFeedActionData.newBuilder()
                    .setLabel(OPEN_IN_NEW_WINDOW_LABEL)
                    .setFeedActionPayload(FeedActionPayload.newBuilder().setExtension(
                            FeedAction.feedActionExtension,
                            FeedAction.newBuilder()
                                    .setMetadata(
                                            FeedActionMetadata.newBuilder()
                                                    .setType(Type.OPEN_URL_NEW_WINDOW)
                                                    .setOpenUrlData(
                                                            OpenUrlData.newBuilder().setUrl(URL)))
                                    .build()))
                    .build();

    private static final String OPEN_IN_INCOGNITO_MODE_LABEL = "Open in incognito mode";

    private static final LabelledFeedActionData OPEN_IN_INCOGNITO_MODE =
            LabelledFeedActionData.newBuilder()
                    .setLabel(OPEN_IN_INCOGNITO_MODE_LABEL)
                    .setFeedActionPayload(FeedActionPayload.newBuilder().setExtension(
                            FeedAction.feedActionExtension,
                            FeedAction.newBuilder()
                                    .setMetadata(
                                            FeedActionMetadata.newBuilder()
                                                    .setType(Type.OPEN_URL_INCOGNITO)
                                                    .setOpenUrlData(
                                                            OpenUrlData.newBuilder().setUrl(URL)))
                                    .build()))
                    .build();

    private static final LabelledFeedActionData NORMAL_OPEN_URL =
            LabelledFeedActionData.newBuilder()
                    .setLabel(OPEN_LABEL)
                    .setFeedActionPayload(FeedActionPayload.newBuilder().setExtension(
                            FeedAction.feedActionExtension,
                            FeedAction.newBuilder()
                                    .setMetadata(
                                            FeedActionMetadata.newBuilder()
                                                    .setType(Type.OPEN_URL)
                                                    .setOpenUrlData(
                                                            OpenUrlData.newBuilder().setUrl(URL)))
                                    .build()))
                    .build();
    private static final String SESSION_ID = "SESSION_ID";
    private static final String CONTENT_ID = "CONTENT_ID";
    private static final ContentMetadata CONTENT_METADATA = new ContentMetadata(URL, "title",
            /* timePublished= */ -1,
            /* imageUrl= */ null,
            /* publisher= */ null,
            /* faviconUrl= */ null,
            /* snippet=*/null);

    @Mock
    private ActionApi actionApi;
    @Mock
    private ActionParser actionParser;
    @Mock
    private ActionManager actionManager;
    @Mock
    private BasicLoggingApi basicLoggingApi;
    @Mock
    private ClusterPendingDismissHelper clusterPendingDismissHelper;
    @Mock
    private ViewElementActionHandler viewElementActionHandler;
    @Mock
    private TooltipApi tooltipApi;

    @Captor
    private ArgumentCaptor<Consumer<String>> consumerCaptor;
    private ContentLoggingData contentLoggingData;
    private FakeContextMenuManager contextMenuManager;
    private StreamActionApiImpl streamActionApi;
    private View view;

    @Before
    public void setup() {
        initMocks(this);

        Context context = Robolectric.buildActivity(Activity.class).get();
        contentLoggingData = new StreamContentLoggingData(0,
                BasicLoggingMetadata.getDefaultInstance(), RepresentationData.getDefaultInstance(),
                /* availableOffline= */ false);
        view = new View(context);
        contextMenuManager = new FakeContextMenuManager();
        streamActionApi =
                new StreamActionApiImpl(actionApi, actionParser, actionManager, basicLoggingApi,
                        ()
                                -> contentLoggingData,
                        contextMenuManager, SESSION_ID, clusterPendingDismissHelper,
                        viewElementActionHandler, CONTENT_ID, tooltipApi);
    }

    @Test
    public void testCanDismiss() {
        assertThat(streamActionApi.canDismiss()).isTrue();
    }

    @Test
    public void testDismiss() {
        String contentId = "contentId";
        List<StreamDataOperation> streamDataOperations =
                Collections.singletonList(StreamDataOperation.getDefaultInstance());
        streamActionApi.dismiss(
                contentId, streamDataOperations, UndoAction.getDefaultInstance(), ACTION_PAYLOAD);

        verify(actionManager)
                .dismissLocal(ImmutableList.of(contentId), streamDataOperations, SESSION_ID);
        verify(basicLoggingApi).onContentDismissed(contentLoggingData, /*wasCommitted =*/true);
    }

    @Test
    public void testDismiss_withSnackbar_onCommitted() {
        testCommittedDismissWithSnackbar(Type.DISMISS);
    }

    @Test
    public void testDismiss_withSnackbar_onReverted() {
        testRevertedDismissWithSnackbar(Type.DISMISS);
    }

    @Test
    public void testDismiss_noSnackbar() {
        testDismissNoSnackbar(Type.DISMISS);
    }

    @Test
    public void testDismissWithSnackbar_dismissLocal_onCommitted() {
        testCommittedDismissWithSnackbar(Type.DISMISS_LOCAL);
    }

    @Test
    public void testDismissWithSnackbar_dismissLocal_onReverted() {
        testRevertedDismissWithSnackbar(Type.DISMISS_LOCAL);
    }

    @Test
    public void testDismissNoSnackbar_dismissLocal_onReverted() {
        testDismissNoSnackbar(Type.DISMISS_LOCAL);
    }

    @Test
    public void testHandleNotInterestedInTopic_onCommitted() {
        testCommittedDismissWithSnackbar(Type.NOT_INTERESTED_IN);
    }

    @Test
    public void testHandleNotInterestedInTopic_onReverted() {
        testRevertedDismissWithSnackbar(Type.NOT_INTERESTED_IN);
    }

    @Test
    public void testHandleNotInterestedInTopic_noSnackbar() {
        testDismissNoSnackbar(Type.NOT_INTERESTED_IN);
    }

    @Test
    public void testOnClientAction() {
        streamActionApi.onClientAction(ActionType.OPEN_URL);

        verify(basicLoggingApi).onClientAction(contentLoggingData, ActionType.OPEN_URL);
    }

    @Test
    public void testOnElementClick_logsElementClicked() {
        streamActionApi.onElementClick(ElementType.INTEREST_HEADER.getNumber());

        verify(basicLoggingApi)
                .onVisualElementClicked(
                        contentLoggingData, ElementType.INTEREST_HEADER.getNumber());
    }

    @Test
    public void testOnElementClick_logsElementClicked_retrievesLoggingDataLazily() {
        contentLoggingData = new StreamContentLoggingData(1,
                BasicLoggingMetadata.getDefaultInstance(), RepresentationData.getDefaultInstance(),
                /* availableOffline= */ true);

        streamActionApi.onElementClick(ElementType.INTEREST_HEADER.getNumber());

        // Should use the new ContentLoggingData defined, not the old one.
        verify(basicLoggingApi)
                .onVisualElementClicked(
                        contentLoggingData, ElementType.INTEREST_HEADER.getNumber());
    }

    @Test
    public void testOnElementView() {
        streamActionApi.onElementView(ElementType.INTEREST_HEADER.getNumber());

        verify(viewElementActionHandler).onElementView(ElementType.INTEREST_HEADER.getNumber());
    }

    @Test
    public void testOnElementHide() {
        streamActionApi.onElementHide(ElementType.INTEREST_HEADER.getNumber());

        verify(viewElementActionHandler).onElementHide(ElementType.INTEREST_HEADER.getNumber());
    }

    @Test
    public void testOpenUrl() {
        streamActionApi.openUrlInNewWindow(URL);
        verify(actionApi).openUrlInNewWindow(URL);
    }

    @Test
    public void testOpenUrl_withParam() {
        streamActionApi.openUrl(URL, PARAM);

        verify(actionManager)
                .uploadAllActionsAndUpdateUrl(eq(URL), eq(PARAM), consumerCaptor.capture());
        consumerCaptor.getValue().accept(NEW_URL);
        verify(actionApi).openUrl(NEW_URL);
    }

    @Test
    public void testCanOpenUrl() {
        when(actionApi.canOpenUrl()).thenReturn(true);
        assertThat(streamActionApi.canOpenUrl()).isTrue();

        when(actionApi.canOpenUrl()).thenReturn(false);
        assertThat(streamActionApi.canOpenUrl()).isFalse();
    }

    @Test
    public void testCanOpenUrlInIncognitoMode() {
        when(actionApi.canOpenUrlInIncognitoMode()).thenReturn(true);
        assertThat(streamActionApi.canOpenUrlInIncognitoMode()).isTrue();

        when(actionApi.canOpenUrlInIncognitoMode()).thenReturn(false);
        assertThat(streamActionApi.canOpenUrlInIncognitoMode()).isFalse();
    }

    @Test
    public void testOpenUrlInIncognitoMode_withParam() {
        streamActionApi.openUrlInIncognitoMode(URL, PARAM);

        verify(actionManager)
                .uploadAllActionsAndUpdateUrl(eq(URL), eq(PARAM), consumerCaptor.capture());
        consumerCaptor.getValue().accept(NEW_URL);
        verify(actionApi).openUrlInIncognitoMode(NEW_URL);
    }

    @Test
    public void testOpenUrlInNewTab() {
        streamActionApi.openUrlInNewTab(URL);
        verify(actionApi).openUrlInNewTab(URL);
    }

    @Test
    public void testOpenUrlInNewTab_withParam() {
        streamActionApi.openUrlInNewTab(URL, PARAM);

        verify(actionManager)
                .uploadAllActionsAndUpdateUrl(eq(URL), eq(PARAM), consumerCaptor.capture());
        consumerCaptor.getValue().accept(NEW_URL);
        verify(actionApi).openUrlInNewTab(NEW_URL);
    }

    @Test
    public void testCanOpenUrlInNewTab() {
        when(actionApi.canOpenUrlInNewTab()).thenReturn(true);
        assertThat(streamActionApi.canOpenUrlInNewTab()).isTrue();

        when(actionApi.canOpenUrlInNewTab()).thenReturn(false);
        assertThat(streamActionApi.canOpenUrlInNewTab()).isFalse();
    }

    @Test
    public void testDownloadUrl() {
        streamActionApi.downloadUrl(CONTENT_METADATA);
        verify(actionApi).downloadUrl(CONTENT_METADATA);
    }

    @Test
    public void testCanDownloadUrl() {
        when(actionApi.canDownloadUrl()).thenReturn(true);
        assertThat(streamActionApi.canDownloadUrl()).isTrue();

        when(actionApi.canDownloadUrl()).thenReturn(false);
        assertThat(streamActionApi.canDownloadUrl()).isFalse();
    }

    @Test
    public void testLearnMore() {
        streamActionApi.learnMore();
        verify(actionApi).learnMore();
    }

    @Test
    public void testCanLearnMore() {
        when(actionApi.canLearnMore()).thenReturn(true);
        assertThat(streamActionApi.canLearnMore()).isTrue();

        when(actionApi.canLearnMore()).thenReturn(false);
        assertThat(streamActionApi.canLearnMore()).isFalse();
    }

    @Test
    public void openContextMenuTest() {
        when(actionParser.canPerformAction(any(FeedActionPayload.class), eq(streamActionApi)))
                .thenReturn(true);

        List<LabelledFeedActionData> labelledFeedActionDataList = new ArrayList<>();
        labelledFeedActionDataList.add(NORMAL_OPEN_URL);
        labelledFeedActionDataList.add(OPEN_IN_NEW_WINDOW);
        labelledFeedActionDataList.add(OPEN_IN_INCOGNITO_MODE);

        streamActionApi.openContextMenu(OpenContextMenuData.newBuilder()
                                                .addAllContextMenuData(labelledFeedActionDataList)
                                                .build(),
                view);

        contextMenuManager.performClick(0);
        contextMenuManager.performClick(1);
        contextMenuManager.performClick(2);

        InOrder inOrder = Mockito.inOrder(actionParser);

        inOrder.verify(actionParser)
                .parseFeedActionPayload(NORMAL_OPEN_URL.getFeedActionPayload(), streamActionApi,
                        view, ActionSource.CONTEXT_MENU);
        inOrder.verify(actionParser)
                .parseFeedActionPayload(OPEN_IN_NEW_WINDOW.getFeedActionPayload(), streamActionApi,
                        view, ActionSource.CONTEXT_MENU);
        inOrder.verify(actionParser)
                .parseFeedActionPayload(OPEN_IN_INCOGNITO_MODE.getFeedActionPayload(),
                        streamActionApi, view, ActionSource.CONTEXT_MENU);
    }

    @Test
    public void openContextMenuTest_noNewWindow() {
        when(actionParser.canPerformAction(NORMAL_OPEN_URL.getFeedActionPayload(), streamActionApi))
                .thenReturn(true);
        when(actionParser.canPerformAction(
                     OPEN_IN_INCOGNITO_MODE.getFeedActionPayload(), streamActionApi))
                .thenReturn(true);
        when(actionParser.canPerformAction(
                     OPEN_IN_NEW_WINDOW.getFeedActionPayload(), streamActionApi))
                .thenReturn(false);

        List<LabelledFeedActionData> labelledFeedActionDataList = new ArrayList<>();
        labelledFeedActionDataList.add(NORMAL_OPEN_URL);
        labelledFeedActionDataList.add(OPEN_IN_NEW_WINDOW);
        labelledFeedActionDataList.add(OPEN_IN_INCOGNITO_MODE);

        streamActionApi.openContextMenu(OpenContextMenuData.newBuilder()
                                                .addAllContextMenuData(labelledFeedActionDataList)
                                                .build(),
                view);

        assertThat(contextMenuManager.getMenuOptions())
                .isEqualTo(Arrays.asList(OPEN_LABEL, OPEN_IN_INCOGNITO_MODE_LABEL));
    }

    @Test
    public void openContextMenuTest_noIncognitoWindow() {
        when(actionParser.canPerformAction(NORMAL_OPEN_URL.getFeedActionPayload(), streamActionApi))
                .thenReturn(true);
        when(actionParser.canPerformAction(
                     OPEN_IN_NEW_WINDOW.getFeedActionPayload(), streamActionApi))
                .thenReturn(true);
        when(actionParser.canPerformAction(
                     OPEN_IN_INCOGNITO_MODE.getFeedActionPayload(), streamActionApi))
                .thenReturn(false);

        List<LabelledFeedActionData> labelledFeedActionDataList = new ArrayList<>();
        labelledFeedActionDataList.add(NORMAL_OPEN_URL);
        labelledFeedActionDataList.add(OPEN_IN_NEW_WINDOW);
        labelledFeedActionDataList.add(OPEN_IN_INCOGNITO_MODE);

        streamActionApi.openContextMenu(OpenContextMenuData.newBuilder()
                                                .addAllContextMenuData(labelledFeedActionDataList)
                                                .build(),
                view);

        assertThat(contextMenuManager.getMenuOptions())
                .isEqualTo(Arrays.asList(OPEN_LABEL, OPEN_IN_NEW_WINDOW_LABEL));
    }

    @Test
    public void openContextMenuTest_logsContentContextMenuOpened() {
        when(actionParser.canPerformAction(any(FeedActionPayload.class), eq(streamActionApi)))
                .thenReturn(true);

        streamActionApi.openContextMenu(
                OpenContextMenuData.newBuilder()
                        .addAllContextMenuData(Collections.singletonList(NORMAL_OPEN_URL))
                        .build(),
                view);

        // First context menu succeeds in opening and is logged.
        verify(basicLoggingApi).onContentContextMenuOpened(contentLoggingData);

        reset(basicLoggingApi);

        streamActionApi.openContextMenu(
                OpenContextMenuData.newBuilder()
                        .addAllContextMenuData(Collections.singletonList(NORMAL_OPEN_URL))
                        .build(),
                view);

        // Second context menu fails in opening and is not logged.
        verifyZeroInteractions(basicLoggingApi);
    }

    @Test
    public void testMaybeShowTooltip() {
        TooltipInfo info = mock(TooltipInfo.class);
        ArgumentCaptor<TooltipCallbackApi> callbackCaptor =
                ArgumentCaptor.forClass(TooltipCallbackApi.class);

        streamActionApi.maybeShowTooltip(info, view);

        verify(tooltipApi).maybeShowHelpUi(eq(info), eq(view), callbackCaptor.capture());

        callbackCaptor.getValue().onShow();
        verify(viewElementActionHandler).onElementView(ElementType.TOOLTIP.getNumber());

        callbackCaptor.getValue().onHide(TooltipDismissType.TIMEOUT);
        verify(viewElementActionHandler).onElementHide(ElementType.TOOLTIP.getNumber());
    }

    private void testCommittedDismissWithSnackbar(Type actionType) {
        ArgumentCaptor<PendingDismissCallback> pendingDismissCallback =
                ArgumentCaptor.forClass(PendingDismissCallback.class);
        List<StreamDataOperation> streamDataOperations =
                Collections.singletonList(StreamDataOperation.getDefaultInstance());
        UndoAction undoAction =
                UndoAction.newBuilder().setConfirmationLabel("confirmation").build();
        switch (actionType) {
            case DISMISS:
            case DISMISS_LOCAL:
                streamActionApi.dismiss(
                        CONTENT_ID, streamDataOperations, undoAction, ACTION_PAYLOAD);
                break;
            case NOT_INTERESTED_IN:
                streamActionApi.handleNotInterestedIn(
                        streamDataOperations, undoAction, ACTION_PAYLOAD, INTEREST_TYPE);
                break;
            default:
                break;
        }

        verify(clusterPendingDismissHelper)
                .triggerPendingDismissForCluster(eq(undoAction), pendingDismissCallback.capture());

        pendingDismissCallback.getValue().onDismissCommitted();
        switch (actionType) {
            case DISMISS:
                verify(actionManager)
                        .dismissLocal(
                                ImmutableList.of(CONTENT_ID), streamDataOperations, SESSION_ID);
                verify(basicLoggingApi)
                        .onContentDismissed(contentLoggingData, /*wasCommitted =*/true);
                verify(actionManager).createAndUploadAction(CONTENT_ID, ACTION_PAYLOAD);
                break;
            case DISMISS_LOCAL:
                verify(actionManager)
                        .dismissLocal(
                                ImmutableList.of(CONTENT_ID), streamDataOperations, SESSION_ID);
                verify(basicLoggingApi)
                        .onContentDismissed(contentLoggingData, /*wasCommitted =*/true);
                break;
            case NOT_INTERESTED_IN:
                verify(actionManager).dismiss(streamDataOperations, SESSION_ID);
                verify(basicLoggingApi)
                        .onNotInterestedIn(
                                INTEREST_TYPE, contentLoggingData, /*wasCommitted =*/true);
                verify(actionManager).createAndUploadAction(CONTENT_ID, ACTION_PAYLOAD);
                break;
            default:
                break;
        }
    }

    private void testRevertedDismissWithSnackbar(Type actionType) {
        ArgumentCaptor<PendingDismissCallback> pendingDismissCallback =
                ArgumentCaptor.forClass(PendingDismissCallback.class);
        String contentId = "contentId";
        List<StreamDataOperation> streamDataOperations =
                Collections.singletonList(StreamDataOperation.getDefaultInstance());
        UndoAction undoAction =
                UndoAction.newBuilder().setConfirmationLabel("confirmation").build();
        switch (actionType) {
            case DISMISS:
            case DISMISS_LOCAL:
                streamActionApi.dismiss(
                        contentId, streamDataOperations, undoAction, ACTION_PAYLOAD);
                break;
            case NOT_INTERESTED_IN:
                streamActionApi.handleNotInterestedIn(
                        streamDataOperations, undoAction, ACTION_PAYLOAD, INTEREST_TYPE);
                break;
            default:
                break;
        }

        verify(clusterPendingDismissHelper)
                .triggerPendingDismissForCluster(eq(undoAction), pendingDismissCallback.capture());

        pendingDismissCallback.getValue().onDismissReverted();

        verify(actionManager, never())
                .dismissLocal(ImmutableList.of(contentId), streamDataOperations, SESSION_ID);
        switch (actionType) {
            case DISMISS:
            case DISMISS_LOCAL:
                verify(basicLoggingApi)
                        .onContentDismissed(contentLoggingData, /*wasCommitted =*/false);
                break;
            case NOT_INTERESTED_IN:
                verify(basicLoggingApi)
                        .onNotInterestedIn(
                                INTEREST_TYPE, contentLoggingData, /*wasCommitted =*/false);
                break;
            default:
                break;
        }
    }

    private void testDismissNoSnackbar(Type actionType) {
        String contentId = "contentId";
        List<StreamDataOperation> streamDataOperations =
                Collections.singletonList(StreamDataOperation.getDefaultInstance());
        UndoAction undoAction = UndoAction.getDefaultInstance();
        switch (actionType) {
            case DISMISS:
            case DISMISS_LOCAL:
                streamActionApi.dismiss(
                        contentId, streamDataOperations, undoAction, ACTION_PAYLOAD);
                break;
            case NOT_INTERESTED_IN:
                streamActionApi.handleNotInterestedIn(
                        streamDataOperations, undoAction, ACTION_PAYLOAD, INTEREST_TYPE);
                break;
            default:
                break;
        }

        verify(clusterPendingDismissHelper, never()).triggerPendingDismissForCluster(any(), any());

        switch (actionType) {
            case DISMISS:
            case DISMISS_LOCAL:
                verify(actionManager)
                        .dismissLocal(
                                ImmutableList.of(contentId), streamDataOperations, SESSION_ID);
                verify(basicLoggingApi)
                        .onContentDismissed(contentLoggingData, /*wasCommitted =*/true);
                break;
            case NOT_INTERESTED_IN:
                verify(actionManager).dismiss(streamDataOperations, SESSION_ID);
                verify(basicLoggingApi)
                        .onNotInterestedIn(
                                INTEREST_TYPE, contentLoggingData, /*wasCommitted =*/true);
                break;
            default:
                break;
        }
    }
}
