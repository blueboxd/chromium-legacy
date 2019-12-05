// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.drivers;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.common.collect.Lists;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
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
import org.chromium.chrome.browser.feed.library.basicstream.internal.pendingdismiss.PendingDismissHandler;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewloggingupdater.ViewLoggingUpdater;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.chrome.browser.feed.library.sharedstream.contextmenumanager.ContextMenuManager;
import org.chromium.chrome.browser.feed.library.sharedstream.offlinemonitor.StreamOfflineMonitor;
import org.chromium.chrome.browser.feed.library.sharedstream.pendingdismiss.PendingDismissCallback;
import org.chromium.chrome.browser.feed.library.testing.modelprovider.FakeModelCursor;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.UndoAction;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Card;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link ClusterDriver}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ClusterDriverTest {
    private static final StreamFeature CARD_STREAM_FEATURE =
            StreamFeature.newBuilder().setCard(Card.getDefaultInstance()).build();

    @Mock
    private CardDriver mCardDriver;
    @Mock
    private LeafFeatureDriver mLeafFeatureDriver;
    @Mock
    private ModelFeature mClusterModelFeature;
    @Mock
    private ContentChangedListener mContentChangedListener;
    @Mock
    private PendingDismissHandler mPendingDismissHandler;
    @Mock
    private BasicLoggingApi mBasicLoggingApi;
    @Mock
    private TooltipApi mTooltipApi;

    private final Configuration mConfiguration = new Configuration.Builder().build();
    private final FakeMainThreadRunner mMainThreadRunner = FakeMainThreadRunner.queueAllTasks();
    private ClusterDriverForTest mClusterDriver;

    @Before
    public void setup() {
        initMocks(this);

        // Child produced by Cursor representing a card.
        ModelChild cardChild = mock(ModelChild.class);
        when(cardChild.getType()).thenReturn(Type.FEATURE);

        // ModelFeature representing a card.
        ModelFeature cardModelFeature = mock(ModelFeature.class);
        when(cardModelFeature.getStreamFeature()).thenReturn(CARD_STREAM_FEATURE);
        when(cardChild.getModelFeature()).thenReturn(cardModelFeature);

        // Cursor representing the content of the cluster, which is one card.
        FakeModelCursor clusterCursor = new FakeModelCursor(Lists.newArrayList(cardChild));
        when(mClusterModelFeature.getCursor()).thenReturn(clusterCursor);

        when(mCardDriver.getLeafFeatureDriver()).thenReturn(mLeafFeatureDriver);

        mClusterDriver = new ClusterDriverForTest(mock(ActionApi.class), mock(ActionManager.class),
                mock(ActionParserFactory.class), mBasicLoggingApi, mClusterModelFeature,
                mock(ModelProvider.class),
                /* position= */ 0, mCardDriver, mock(StreamOfflineMonitor.class),
                mock(ContextMenuManager.class), mock(ViewLoggingUpdater.class));
    }

    @Test
    public void testGetContentModel() {
        assertThat(mClusterDriver.getLeafFeatureDriver()).isEqualTo(mLeafFeatureDriver);
    }

    @Test
    public void testGetContentModel_reusesPreviousContentModel() {
        mClusterDriver.getLeafFeatureDriver();

        verify(mClusterModelFeature).getCursor();

        mClusterDriver.getLeafFeatureDriver();

        verifyNoMoreInteractions(mClusterModelFeature);
    }

    @Test
    public void testOnDestroy() {
        mClusterDriver.getLeafFeatureDriver();
        mClusterDriver.onDestroy();

        verify(mCardDriver).onDestroy();
    }

    @Test
    public void testTriggerPendingDismiss() {
        StreamFeature streamFeature = StreamFeature.newBuilder().setContentId("contentId").build();
        when(mClusterModelFeature.getStreamFeature()).thenReturn(streamFeature);
        PendingDismissCallback pendingDismissCallback = Mockito.mock(PendingDismissCallback.class);
        mClusterDriver.triggerPendingDismissForCluster(
                UndoAction.getDefaultInstance(), pendingDismissCallback);
        verify(mPendingDismissHandler)
                .triggerPendingDismiss(mClusterModelFeature.getStreamFeature().getContentId(),
                        UndoAction.getDefaultInstance(), pendingDismissCallback);
    }

    @Test
    public void getLeafFeatureDrivers_childNotAFeature_logsInternalError() {
        FakeModelCursor clusterCursor = FakeModelCursor.newBuilder().addUnboundChild().build();
        when(mClusterModelFeature.getCursor()).thenReturn(clusterCursor);

        mClusterDriver.getLeafFeatureDriver();

        verify(mBasicLoggingApi).onInternalError(InternalFeedError.CLUSTER_CHILD_MISSING_FEATURE);
    }

    @Test
    public void getLeafFeatureDrivers_childNotACard_logsInternalError() {
        FakeModelCursor clusterCursor = FakeModelCursor.newBuilder().addCluster().build();
        when(mClusterModelFeature.getCursor()).thenReturn(clusterCursor);

        mClusterDriver.getLeafFeatureDriver();

        verify(mBasicLoggingApi).onInternalError(InternalFeedError.CLUSTER_CHILD_NOT_CARD);
    }

    class ClusterDriverForTest extends ClusterDriver {
        private final CardDriver mCardDriver;

        ClusterDriverForTest(ActionApi actionApi, ActionManager actionManager,
                ActionParserFactory actionParserFactory, BasicLoggingApi basicLoggingApi,
                ModelFeature clusterModel, ModelProvider modelProvider, int position,
                CardDriver cardDriver, StreamOfflineMonitor streamOfflineMonitor,
                ContextMenuManager contextMenuManager, ViewLoggingUpdater viewLoggingUpdater) {
            super(actionApi, actionManager, actionParserFactory, basicLoggingApi, clusterModel,
                    modelProvider, position, mPendingDismissHandler, streamOfflineMonitor,
                    mContentChangedListener, contextMenuManager, mMainThreadRunner, mConfiguration,
                    viewLoggingUpdater, mTooltipApi);
            this.mCardDriver = cardDriver;
        }

        @Override
        CardDriver createCardDriver(ModelFeature content) {
            return mCardDriver;
        }
    }
}
