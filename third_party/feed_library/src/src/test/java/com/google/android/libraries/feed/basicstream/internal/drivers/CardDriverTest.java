// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal.drivers;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

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
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.basicstream.internal.pendingdismiss.ClusterPendingDismissHelper;
import com.google.android.libraries.feed.basicstream.internal.viewloggingupdater.ViewLoggingUpdater;
import com.google.android.libraries.feed.common.concurrent.testing.FakeMainThreadRunner;
import com.google.android.libraries.feed.sharedstream.contextmenumanager.ContextMenuManager;
import com.google.android.libraries.feed.sharedstream.offlinemonitor.StreamOfflineMonitor;
import com.google.android.libraries.feed.testing.modelprovider.FakeModelCursor;
import com.google.common.collect.Lists;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;
import com.google.search.now.ui.action.FeedActionPayloadProto.FeedActionPayload;
import com.google.search.now.ui.action.FeedActionProto.DismissData;
import com.google.search.now.ui.action.FeedActionProto.FeedAction;
import com.google.search.now.ui.action.FeedActionProto.FeedActionMetadata;
import com.google.search.now.ui.stream.StreamStructureProto.Card;
import com.google.search.now.ui.stream.StreamStructureProto.Content;
import com.google.search.now.ui.stream.StreamSwipeExtensionProto.SwipeActionExtension;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link CardDriver}. */
@RunWith(RobolectricTestRunner.class)
public class CardDriverTest {
    private static final StreamFeature STREAM_FEATURE =
            StreamFeature.newBuilder().setContent(Content.getDefaultInstance()).build();

    private static final FeedActionPayload SWIPE_ACTION =
            FeedActionPayload.newBuilder()
                    .setExtension(FeedAction.feedActionExtension,
                            FeedAction.newBuilder()
                                    .setMetadata(FeedActionMetadata.newBuilder().setDismissData(
                                            DismissData.getDefaultInstance()))
                                    .build())
                    .build();

    private static final Card CARD_WITH_SWIPE_ACTION =
            Card.newBuilder()
                    .setExtension(SwipeActionExtension.swipeActionExtension,
                            SwipeActionExtension.newBuilder().setSwipeAction(SWIPE_ACTION).build())
                    .build();

    private static final int POSITION;

    @Mock
    private ContentDriver contentDriverChild;
    @Mock
    private ModelFeature cardModelFeature;
    @Mock
    private LeafFeatureDriver leafFeatureDriver;
    @Mock
    private ContentChangedListener contentChangedListener;
    @Mock
    private ClusterPendingDismissHelper clusterPendingDismissHelper;
    @Mock
    private BasicLoggingApi basicLoggingApi;
    @Mock
    private TooltipApi tooltipApi;

    private final Configuration configuration = new Configuration.Builder().build();
    private final FakeMainThreadRunner mainThreadRunner = FakeMainThreadRunner.queueAllTasks();
    private CardDriverForTest cardDriver;
    private FakeModelCursor cardCursor;

    @Before
    public void setup() {
        initMocks(this);

        // Represents payload of the child of the card.
        ModelFeature childModelFeature = mock(ModelFeature.class);
        when(childModelFeature.getStreamFeature()).thenReturn(STREAM_FEATURE);

        // ModelChild containing content.
        ModelChild pietModelChild = mock(ModelChild.class);
        when(pietModelChild.getModelFeature()).thenReturn(childModelFeature);
        when(pietModelChild.getType()).thenReturn(Type.FEATURE);

        // Cursor to transverse the content of a card.
        cardCursor = new FakeModelCursor(Lists.newArrayList(pietModelChild));

        // A ModelFeature representing a card.
        when(cardModelFeature.getCursor()).thenReturn(cardCursor);
        when(cardModelFeature.getStreamFeature())
                .thenReturn(StreamFeature.newBuilder().setCard(Card.getDefaultInstance()).build());
        when(contentDriverChild.getLeafFeatureDriver()).thenReturn(leafFeatureDriver);

        cardDriver = new CardDriverForTest(mock(ActionApi.class), mock(ActionManager.class),
                mock(ActionParserFactory.class), basicLoggingApi, cardModelFeature,
                mock(ModelProvider.class), POSITION, contentDriverChild,
                mock(StreamOfflineMonitor.class), mock(ContextMenuManager.class),
                mock(ViewLoggingUpdater.class));
    }

    @Test
    public void testGetLeafFeatureDriver() {
        assertThat((cardDriver.getLeafFeatureDriver())).isEqualTo(leafFeatureDriver);
    }

    @Test
    public void testGetLeafFeatureDriver_reusesPreviousLeafFeatureDriver() {
        cardDriver.getLeafFeatureDriver();

        verify(cardModelFeature).getCursor();

        reset(cardModelFeature);

        cardDriver.getLeafFeatureDriver();

        verify(cardModelFeature, never()).getCursor();
    }

    @Test
    public void testGetLeafFeatureDriver_notSwipeable() {
        when(cardModelFeature.getStreamFeature())
                .thenReturn(StreamFeature.newBuilder().setCard(Card.getDefaultInstance()).build());

        cardDriver.getLeafFeatureDriver();

        assertThat(cardDriver.swipeActionArg).isEqualTo(FeedActionPayload.getDefaultInstance());
    }

    @Test
    public void testGetLeafFeatureDriver_swipeable() {
        when(cardModelFeature.getStreamFeature())
                .thenReturn(StreamFeature.newBuilder().setCard(CARD_WITH_SWIPE_ACTION).build());

        cardDriver.getLeafFeatureDriver();

        assertThat(cardDriver.swipeActionArg).isEqualTo(SWIPE_ACTION);
    }

    @Test
    public void testOnDestroy() {
        cardDriver.getLeafFeatureDriver();
        cardDriver.onDestroy();

        verify(contentDriverChild).onDestroy();
    }

    @Test
    public void testUnboundChild() {
        ModelChild pietModelChild = mock(ModelChild.class);
        when(pietModelChild.getType()).thenReturn(Type.UNBOUND);

        // Override setup with an unbound child in the cursor
        cardCursor.setModelChildren(Lists.newArrayList(pietModelChild));

        assertThat(cardDriver.getLeafFeatureDriver()).isNull();
    }

    @Test
    public void getLeafFeatureDrivers_childNotAFeature_logsInternalError() {
        cardCursor = FakeModelCursor.newBuilder().addUnboundChild().build();
        when(cardModelFeature.getCursor()).thenReturn(cardCursor);

        cardDriver.getLeafFeatureDriver();

        verify(basicLoggingApi).onInternalError(InternalFeedError.CARD_CHILD_MISSING_FEATURE);
    }

    public class CardDriverForTest extends CardDriver {
        private final ContentDriver child;
        private FeedActionPayload swipeActionArg;

        CardDriverForTest(ActionApi actionApi, ActionManager actionManager,
                ActionParserFactory actionParserFactory, BasicLoggingApi basicLoggingApi,
                ModelFeature cardModel, ModelProvider modelProvider, int position,
                ContentDriver childContentDriver, StreamOfflineMonitor streamOfflineMonitor,
                ContextMenuManager contextMenuManager, ViewLoggingUpdater viewLoggingUpdater) {
            super(actionApi, actionManager, actionParserFactory, basicLoggingApi, cardModel,
                    modelProvider, position, clusterPendingDismissHelper, streamOfflineMonitor,
                    contentChangedListener, contextMenuManager, mainThreadRunner, configuration,
                    viewLoggingUpdater, tooltipApi);
            this.child = childContentDriver;
        }

        @Override
        ContentDriver createContentDriver(
                ModelFeature contentModel, FeedActionPayload swipeAction) {
            this.swipeActionArg = swipeAction;
            return child;
        }
    }
}
