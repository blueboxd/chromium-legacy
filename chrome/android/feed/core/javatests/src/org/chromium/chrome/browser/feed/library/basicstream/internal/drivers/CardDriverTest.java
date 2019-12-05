// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.drivers;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.common.collect.Lists;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ContentChangedListener;
import org.chromium.chrome.browser.feed.library.api.host.action.ActionApi;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.InternalFeedError;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipApi;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionManager;
import org.chromium.chrome.browser.feed.library.api.internal.actionparser.ActionParserFactory;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild.Type;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelFeature;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.basicstream.internal.pendingdismiss.ClusterPendingDismissHelper;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewloggingupdater.ViewLoggingUpdater;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.chrome.browser.feed.library.sharedstream.contextmenumanager.ContextMenuManager;
import org.chromium.chrome.browser.feed.library.sharedstream.offlinemonitor.StreamOfflineMonitor;
import org.chromium.chrome.browser.feed.library.testing.modelprovider.FakeModelCursor;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.components.feed.core.proto.ui.action.FeedActionPayloadProto.FeedActionPayload;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.DismissData;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedAction;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Card;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Content;
import org.chromium.components.feed.core.proto.ui.stream.StreamSwipeExtensionProto.SwipeActionExtension;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link CardDriver}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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

    private static final int POSITION = 0;

    @Mock
    private ContentDriver mContentDriverChild;
    @Mock
    private ModelFeature mCardModelFeature;
    @Mock
    private LeafFeatureDriver mLeafFeatureDriver;
    @Mock
    private ContentChangedListener mContentChangedListener;
    @Mock
    private ClusterPendingDismissHelper mClusterPendingDismissHelper;
    @Mock
    private BasicLoggingApi mBasicLoggingApi;
    @Mock
    private TooltipApi mTooltipApi;

    private final Configuration mConfiguration = new Configuration.Builder().build();
    private final FakeMainThreadRunner mMainThreadRunner = FakeMainThreadRunner.queueAllTasks();
    private CardDriverForTest mCardDriver;
    private FakeModelCursor mCardCursor;

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
        mCardCursor = new FakeModelCursor(Lists.newArrayList(pietModelChild));

        // A ModelFeature representing a card.
        when(mCardModelFeature.getCursor()).thenReturn(mCardCursor);
        when(mCardModelFeature.getStreamFeature())
                .thenReturn(StreamFeature.newBuilder().setCard(Card.getDefaultInstance()).build());
        when(mContentDriverChild.getLeafFeatureDriver()).thenReturn(mLeafFeatureDriver);

        mCardDriver = new CardDriverForTest(mock(ActionApi.class), mock(ActionManager.class),
                mock(ActionParserFactory.class), mBasicLoggingApi, mCardModelFeature,
                mock(ModelProvider.class), POSITION, mContentDriverChild,
                mock(StreamOfflineMonitor.class), mock(ContextMenuManager.class),
                mock(ViewLoggingUpdater.class));
    }

    @Test
    public void testGetLeafFeatureDriver() {
        assertThat((mCardDriver.getLeafFeatureDriver())).isEqualTo(mLeafFeatureDriver);
    }

    @Test
    public void testGetLeafFeatureDriver_reusesPreviousLeafFeatureDriver() {
        mCardDriver.getLeafFeatureDriver();

        verify(mCardModelFeature).getCursor();

        reset(mCardModelFeature);

        mCardDriver.getLeafFeatureDriver();

        verify(mCardModelFeature, never()).getCursor();
    }

    @Test
    public void testGetLeafFeatureDriver_notSwipeable() {
        when(mCardModelFeature.getStreamFeature())
                .thenReturn(StreamFeature.newBuilder().setCard(Card.getDefaultInstance()).build());

        mCardDriver.getLeafFeatureDriver();

        assertThat(mCardDriver.mSwipeActionArg).isEqualTo(FeedActionPayload.getDefaultInstance());
    }

    @Test
    public void testGetLeafFeatureDriver_swipeable() {
        when(mCardModelFeature.getStreamFeature())
                .thenReturn(StreamFeature.newBuilder().setCard(CARD_WITH_SWIPE_ACTION).build());

        mCardDriver.getLeafFeatureDriver();

        assertThat(mCardDriver.mSwipeActionArg).isEqualTo(SWIPE_ACTION);
    }

    @Test
    public void testOnDestroy() {
        mCardDriver.getLeafFeatureDriver();
        mCardDriver.onDestroy();

        verify(mContentDriverChild).onDestroy();
    }

    @Test
    public void testUnboundChild() {
        ModelChild pietModelChild = mock(ModelChild.class);
        when(pietModelChild.getType()).thenReturn(Type.UNBOUND);

        // Override setup with an unbound child in the cursor
        mCardCursor.setModelChildren(Lists.newArrayList(pietModelChild));

        assertThat(mCardDriver.getLeafFeatureDriver()).isNull();
    }

    @Test
    public void getLeafFeatureDrivers_childNotAFeature_logsInternalError() {
        mCardCursor = FakeModelCursor.newBuilder().addUnboundChild().build();
        when(mCardModelFeature.getCursor()).thenReturn(mCardCursor);

        mCardDriver.getLeafFeatureDriver();

        verify(mBasicLoggingApi).onInternalError(InternalFeedError.CARD_CHILD_MISSING_FEATURE);
    }

    public class CardDriverForTest extends CardDriver {
        private final ContentDriver mChild;
        private FeedActionPayload mSwipeActionArg;

        CardDriverForTest(ActionApi actionApi, ActionManager actionManager,
                ActionParserFactory actionParserFactory, BasicLoggingApi basicLoggingApi,
                ModelFeature cardModel, ModelProvider modelProvider, int position,
                ContentDriver childContentDriver, StreamOfflineMonitor streamOfflineMonitor,
                ContextMenuManager contextMenuManager, ViewLoggingUpdater viewLoggingUpdater) {
            super(actionApi, actionManager, actionParserFactory, basicLoggingApi, cardModel,
                    modelProvider, position, mClusterPendingDismissHelper, streamOfflineMonitor,
                    mContentChangedListener, contextMenuManager, mMainThreadRunner, mConfiguration,
                    viewLoggingUpdater, mTooltipApi);
            this.mChild = childContentDriver;
        }

        @Override
        ContentDriver createContentDriver(
                ModelFeature contentModel, FeedActionPayload swipeAction) {
            this.mSwipeActionArg = swipeAction;
            return mChild;
        }
    }
}
