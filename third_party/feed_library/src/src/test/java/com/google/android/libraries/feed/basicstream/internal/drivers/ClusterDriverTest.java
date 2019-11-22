// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal.drivers;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
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
import com.google.android.libraries.feed.basicstream.internal.pendingdismiss.PendingDismissHandler;
import com.google.android.libraries.feed.basicstream.internal.viewloggingupdater.ViewLoggingUpdater;
import com.google.android.libraries.feed.common.concurrent.testing.FakeMainThreadRunner;
import com.google.android.libraries.feed.sharedstream.contextmenumanager.ContextMenuManager;
import com.google.android.libraries.feed.sharedstream.offlinemonitor.StreamOfflineMonitor;
import com.google.android.libraries.feed.sharedstream.pendingdismiss.PendingDismissCallback;
import com.google.android.libraries.feed.testing.modelprovider.FakeModelCursor;
import com.google.common.collect.Lists;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;
import com.google.search.now.ui.action.FeedActionProto.UndoAction;
import com.google.search.now.ui.stream.StreamStructureProto.Card;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link ClusterDriver}. */
@RunWith(RobolectricTestRunner.class)
public class ClusterDriverTest {
    private static final StreamFeature CARD_STREAM_FEATURE =
            StreamFeature.newBuilder().setCard(Card.getDefaultInstance()).build();

    @Mock
    private CardDriver cardDriver;
    @Mock
    private LeafFeatureDriver leafFeatureDriver;
    @Mock
    private ModelFeature clusterModelFeature;
    @Mock
    private ContentChangedListener contentChangedListener;
    @Mock
    private PendingDismissHandler pendingDismissHandler;
    @Mock
    private BasicLoggingApi basicLoggingApi;
    @Mock
    private TooltipApi tooltipApi;

    private final Configuration configuration = new Configuration.Builder().build();
    private final FakeMainThreadRunner mainThreadRunner = FakeMainThreadRunner.queueAllTasks();
    private ClusterDriverForTest clusterDriver;

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
        when(clusterModelFeature.getCursor()).thenReturn(clusterCursor);

        when(cardDriver.getLeafFeatureDriver()).thenReturn(leafFeatureDriver);

        clusterDriver = new ClusterDriverForTest(mock(ActionApi.class), mock(ActionManager.class),
                mock(ActionParserFactory.class), basicLoggingApi, clusterModelFeature,
                mock(ModelProvider.class),
                /* position= */ 0, cardDriver, mock(StreamOfflineMonitor.class),
                mock(ContextMenuManager.class), mock(ViewLoggingUpdater.class));
    }

    @Test
    public void testGetContentModel() {
        assertThat(clusterDriver.getLeafFeatureDriver()).isEqualTo(leafFeatureDriver);
    }

    @Test
    public void testGetContentModel_reusesPreviousContentModel() {
        clusterDriver.getLeafFeatureDriver();

        verify(clusterModelFeature).getCursor();

        clusterDriver.getLeafFeatureDriver();

        verifyNoMoreInteractions(clusterModelFeature);
    }

    @Test
    public void testOnDestroy() {
        clusterDriver.getLeafFeatureDriver();
        clusterDriver.onDestroy();

        verify(cardDriver).onDestroy();
    }

    @Test
    public void testTriggerPendingDismiss() {
        StreamFeature streamFeature = StreamFeature.newBuilder().setContentId("contentId").build();
        when(clusterModelFeature.getStreamFeature()).thenReturn(streamFeature);
        PendingDismissCallback pendingDismissCallback = Mockito.mock(PendingDismissCallback.class);
        clusterDriver.triggerPendingDismissForCluster(
                UndoAction.getDefaultInstance(), pendingDismissCallback);
        verify(pendingDismissHandler)
                .triggerPendingDismiss(clusterModelFeature.getStreamFeature().getContentId(),
                        UndoAction.getDefaultInstance(), pendingDismissCallback);
    }

    @Test
    public void getLeafFeatureDrivers_childNotAFeature_logsInternalError() {
        FakeModelCursor clusterCursor = FakeModelCursor.newBuilder().addUnboundChild().build();
        when(clusterModelFeature.getCursor()).thenReturn(clusterCursor);

        clusterDriver.getLeafFeatureDriver();

        verify(basicLoggingApi).onInternalError(InternalFeedError.CLUSTER_CHILD_MISSING_FEATURE);
    }

    @Test
    public void getLeafFeatureDrivers_childNotACard_logsInternalError() {
        FakeModelCursor clusterCursor = FakeModelCursor.newBuilder().addCluster().build();
        when(clusterModelFeature.getCursor()).thenReturn(clusterCursor);

        clusterDriver.getLeafFeatureDriver();

        verify(basicLoggingApi).onInternalError(InternalFeedError.CLUSTER_CHILD_NOT_CARD);
    }

    class ClusterDriverForTest extends ClusterDriver {
        private final CardDriver cardDriver;

        ClusterDriverForTest(ActionApi actionApi, ActionManager actionManager,
                ActionParserFactory actionParserFactory, BasicLoggingApi basicLoggingApi,
                ModelFeature clusterModel, ModelProvider modelProvider, int position,
                CardDriver cardDriver, StreamOfflineMonitor streamOfflineMonitor,
                ContextMenuManager contextMenuManager, ViewLoggingUpdater viewLoggingUpdater) {
            super(actionApi, actionManager, actionParserFactory, basicLoggingApi, clusterModel,
                    modelProvider, position, pendingDismissHandler, streamOfflineMonitor,
                    contentChangedListener, contextMenuManager, mainThreadRunner, configuration,
                    viewLoggingUpdater, tooltipApi);
            this.cardDriver = cardDriver;
        }

        @Override
        CardDriver createCardDriver(ModelFeature content) {
            return cardDriver;
        }
    }
}
