// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal.drivers;

import static com.google.android.libraries.feed.common.testing.RunnableSubject.assertThatRunnable;
import static com.google.android.libraries.feed.testing.modelprovider.FakeModelCursor.getCardModelFeatureWithCursor;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;

import com.google.android.libraries.feed.api.client.stream.Stream.ContentChangedListener;
import com.google.android.libraries.feed.api.host.action.ActionApi;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.InternalFeedError;
import com.google.android.libraries.feed.api.host.logging.ZeroStateShowReason;
import com.google.android.libraries.feed.api.host.stream.SnackbarApi;
import com.google.android.libraries.feed.api.host.stream.SnackbarCallbackApi;
import com.google.android.libraries.feed.api.host.stream.TooltipApi;
import com.google.android.libraries.feed.api.internal.actionmanager.ActionManager;
import com.google.android.libraries.feed.api.internal.actionparser.ActionParserFactory;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.knowncontent.FeedKnownContent;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild.Type;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.RemoveTrackingFactory;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.State;
import com.google.android.libraries.feed.basicstream.internal.drivers.StreamDriver.StreamContentListener;
import com.google.android.libraries.feed.basicstream.internal.drivers.testing.FakeFeatureDriver;
import com.google.android.libraries.feed.basicstream.internal.drivers.testing.FakeLeafFeatureDriver;
import com.google.android.libraries.feed.basicstream.internal.scroll.BasicStreamScrollMonitor;
import com.google.android.libraries.feed.basicstream.internal.scroll.ScrollRestorer;
import com.google.android.libraries.feed.basicstream.internal.viewloggingupdater.ViewLoggingUpdater;
import com.google.android.libraries.feed.common.concurrent.testing.FakeMainThreadRunner;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.sharedstream.contextmenumanager.ContextMenuManager;
import com.google.android.libraries.feed.sharedstream.offlinemonitor.StreamOfflineMonitor;
import com.google.android.libraries.feed.sharedstream.pendingdismiss.PendingDismissCallback;
import com.google.android.libraries.feed.sharedstream.proto.UiRefreshReasonProto.UiRefreshReason;
import com.google.android.libraries.feed.sharedstream.proto.UiRefreshReasonProto.UiRefreshReason.Reason;
import com.google.android.libraries.feed.sharedstream.removetrackingfactory.StreamRemoveTrackingFactory;
import com.google.android.libraries.feed.testing.modelprovider.FakeModelChild;
import com.google.android.libraries.feed.testing.modelprovider.FakeModelCursor;
import com.google.android.libraries.feed.testing.modelprovider.FakeModelFeature;
import com.google.android.libraries.feed.testing.modelprovider.FeatureChangeBuilder;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.Lists;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;
import com.google.search.now.ui.action.FeedActionProto.UndoAction;
import com.google.search.now.ui.stream.StreamStructureProto.Card;
import com.google.search.now.ui.stream.StreamStructureProto.Cluster;
import com.google.search.now.ui.stream.StreamStructureProto.Content;

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
import java.util.List;

/** Tests for {@link StreamDriver}. */
@RunWith(RobolectricTestRunner.class)
public class StreamDriverTest {
    private static final String CONTENT_ID = "contentID";
    private FakeModelCursor CURSOR_WITH_INVALID_CHILDREN;

    private static final FakeModelCursor EMPTY_MODEL_CURSOR = FakeModelCursor.newBuilder().build();

    private static final ModelFeature UNBOUND_CARD =
            FakeModelFeature.newBuilder()
                    .setStreamFeature(
                            StreamFeature.newBuilder().setCard(Card.getDefaultInstance()).build())
                    .build();

    @Mock
    private ActionApi actionApi;
    @Mock
    private ActionManager actionManager;
    @Mock
    private ActionParserFactory actionParserFactory;
    @Mock
    private ModelFeature streamFeature;
    @Mock
    private ModelProvider modelProvider;
    @Mock
    private ContinuationDriver continuationDriver;
    @Mock
    private NoContentDriver noContentDriver;
    @Mock
    private ZeroStateDriver zeroStateDriver;
    @Mock
    private StreamContentListener contentListener;
    @Mock
    private ContentChangedListener contentChangedListener;
    @Mock
    private SnackbarApi snackbarApi;
    @Mock
    private ScrollRestorer scrollRestorer;
    @Mock
    private BasicLoggingApi basicLoggingApi;
    @Mock
    private FeedKnownContent feedKnownContent;
    @Mock
    private StreamOfflineMonitor streamOfflineMonitor;
    @Mock
    private PendingDismissCallback pendingDismissCallback;
    @Mock
    private TooltipApi tooltipApi;
    @Mock
    private BasicStreamScrollMonitor scrollMonitor;

    @Captor
    private ArgumentCaptor<List<LeafFeatureDriver>> featureDriversCaptor;

    private StreamDriverForTest streamDriver;
    private Configuration configuration = new Configuration.Builder().build();
    private Context context;
    private Clock clock;
    private ThreadUtils threadUtils;
    private final FakeMainThreadRunner mainThreadRunner = FakeMainThreadRunner.queueAllTasks();
    private final ViewLoggingUpdater viewLoggingUpdater = new ViewLoggingUpdater();

    private static FakeModelChild createFakeClusterModelChild() {
        return FakeModelChild.newBuilder()
                .setModelFeature(
                        FakeModelFeature.newBuilder()
                                .setStreamFeature(StreamFeature.newBuilder()
                                                          .setCluster(Cluster.getDefaultInstance())
                                                          .build())
                                .build())
                .build();
    }

    @Before
    public void setup() {
        FakeModelFeature feature =
                FakeModelFeature.newBuilder()
                        .setStreamFeature(StreamFeature.newBuilder()
                                                  .setContent(Content.getDefaultInstance())
                                                  .build())
                        .build();
        CURSOR_WITH_INVALID_CHILDREN =
                FakeModelCursor.newBuilder()
                        .addChild(FakeModelChild.newBuilder().setModelFeature(feature).build())
                        .build();

        initMocks(this);
        context = Robolectric.buildActivity(Activity.class).get();
        clock = new FakeClock();

        when(continuationDriver.getLeafFeatureDriver()).thenReturn(continuationDriver);
        when(noContentDriver.getLeafFeatureDriver()).thenReturn(noContentDriver);
        when(zeroStateDriver.getLeafFeatureDriver()).thenReturn(zeroStateDriver);
        when(zeroStateDriver.isSpinnerShowing()).thenReturn(false);

        when(modelProvider.getRootFeature()).thenReturn(streamFeature);
        when(modelProvider.getCurrentState()).thenReturn(State.READY);
        threadUtils = new ThreadUtils();

        streamDriver = createNonRestoringStreamDriver();

        streamDriver.setStreamContentListener(contentListener);
    }

    @Test
    public void testConstruction() {
        ArgumentCaptor<RemoveTrackingFactory> removeTrackingFactoryArgumentCaptor =
                ArgumentCaptor.forClass(RemoveTrackingFactory.class);

        verify(modelProvider).enableRemoveTracking(removeTrackingFactoryArgumentCaptor.capture());

        assertThat(removeTrackingFactoryArgumentCaptor.getValue())
                .isInstanceOf(StreamRemoveTrackingFactory.class);
    }

    @Test
    public void testBuildChildren() {
        when(streamFeature.getCursor())
                .thenReturn(FakeModelCursor.newBuilder().addCard().addCluster().addToken().build());

        // Causes StreamDriver to build a list of children based on the children from the cursor.
        List<LeafFeatureDriver> leafFeatureDrivers = streamDriver.getLeafFeatureDrivers();

        assertThat(leafFeatureDrivers).hasSize(3);

        assertThat(leafFeatureDrivers.get(0)).isEqualTo(getLeafFeatureDriverFromCard(0));
        assertThat(leafFeatureDrivers.get(1)).isEqualTo(getLeafFeatureDriverFromCluster(1));
        assertThat(leafFeatureDrivers.get(2)).isEqualTo(continuationDriver);

        verify(continuationDriver).initialize();
        verify(streamOfflineMonitor).requestOfflineStatusForNewContent();
    }

    @Test
    public void testBuildChildren_unboundContent() {
        when(streamFeature.getCursor())
                .thenReturn(FakeModelCursor.newBuilder().addChild(UNBOUND_CARD).addCard().build());

        // Causes StreamDriver to build a list of children based on the children from the cursor.
        List<LeafFeatureDriver> leafFeatureDrivers = streamDriver.getLeafFeatureDrivers();

        assertThat(leafFeatureDrivers).hasSize(1);
        assertThat(leafFeatureDrivers.get(0)).isEqualTo(getLeafFeatureDriverFromCard(1));
        verify(basicLoggingApi).onInternalError(InternalFeedError.FAILED_TO_CREATE_LEAF);
    }

    @Test
    public void testBuildChildren_unboundChild_logsInternalError() {
        when(streamFeature.getCursor())
                .thenReturn(FakeModelCursor.newBuilder().addUnboundChild().build());

        streamDriver.getLeafFeatureDrivers();

        verify(basicLoggingApi).onInternalError(InternalFeedError.TOP_LEVEL_UNBOUND_CHILD);
    }

    @Test
    public void testBuildChildren_initializing() {
        when(modelProvider.getRootFeature()).thenReturn(null);
        when(modelProvider.getCurrentState()).thenReturn(State.INITIALIZING);
        List<LeafFeatureDriver> leafFeatureDrivers = streamDriver.getLeafFeatureDrivers();
        assertThat(leafFeatureDrivers).hasSize(1);
        assertThat(streamDriver.getLeafFeatureDrivers().get(0)).isInstanceOf(ZeroStateDriver.class);
    }

    @Test
    public void testBuildChildren_nullRootFeature_logsInternalError() {
        when(modelProvider.getRootFeature()).thenReturn(null);

        streamDriver.getLeafFeatureDrivers();

        verify(basicLoggingApi).onInternalError(InternalFeedError.NO_ROOT_FEATURE);
    }

    @Test
    public void testCreateChildren_invalidFeatureType_logsInternalError() {
        when(streamFeature.getCursor()).thenReturn(CURSOR_WITH_INVALID_CHILDREN);

        streamDriver.getLeafFeatureDrivers();

        verify(basicLoggingApi).onInternalError(InternalFeedError.TOP_LEVEL_INVALID_FEATURE_TYPE);
    }

    @Test
    public void testMaybeRestoreScroll() {
        streamDriver = createRestoringStreamDriver();

        when(streamFeature.getCursor()).thenReturn(FakeModelCursor.newBuilder().addCard().build());

        streamDriver.getLeafFeatureDrivers();

        streamDriver.maybeRestoreScroll();

        verify(scrollRestorer).maybeRestoreScroll();
    }

    @Test
    public void testMaybeRestoreScroll_withToken() {
        streamDriver = createRestoringStreamDriver();

        when(streamFeature.getCursor())
                .thenReturn(FakeModelCursor.newBuilder().addCard().addCluster().addToken().build());

        streamDriver.getLeafFeatureDrivers();

        streamDriver.maybeRestoreScroll();

        // Should restore scroll if a non-synthetic token is last
        verify(scrollRestorer).maybeRestoreScroll();
    }

    @Test
    public void testMaybeRestoreScroll_withSyntheticToken() {
        streamDriver = createRestoringStreamDriver();

        when(streamFeature.getCursor())
                .thenReturn(FakeModelCursor.newBuilder()
                                    .addCard()
                                    .addCluster()
                                    .addSyntheticToken()
                                    .build());
        when(continuationDriver.hasTokenBeenHandled()).thenReturn(true);

        streamDriver.getLeafFeatureDrivers();

        streamDriver.maybeRestoreScroll();

        // Should never restore scroll if a synthetic token is last
        verify(scrollRestorer, never()).maybeRestoreScroll();
    }

    @Test
    public void testMaybeRestoreScroll_notRestoring_doesNotScroll() {
        streamDriver = createNonRestoringStreamDriver();

        when(streamFeature.getCursor())
                .thenReturn(FakeModelCursor.newBuilder()
                                    .addCard()
                                    .addCluster()
                                    .addSyntheticToken()
                                    .build());

        streamDriver.getLeafFeatureDrivers();

        streamDriver.maybeRestoreScroll();

        // Should never restore scroll if created in a non-restoring state.
        verify(scrollRestorer, never()).maybeRestoreScroll();
    }

    @Test
    public void testContinuationToken_createsContinuationContentModel() {
        when(streamFeature.getCursor()).thenReturn(FakeModelCursor.newBuilder().addToken().build());

        List<LeafFeatureDriver> leafFeatureDrivers = streamDriver.getLeafFeatureDrivers();
        assertThat(leafFeatureDrivers).hasSize(2);
        assertThat(leafFeatureDrivers.get(0)).isEqualTo(noContentDriver);
        assertThat(leafFeatureDrivers.get(1)).isEqualTo(continuationDriver);
    }

    @Test
    public void testContinuationToken_tokenHandling() {
        streamDriver = createStreamDriver(/* restoring= */ true, /* isInitialLoad= */ false);

        FakeModelCursor initialCursor = FakeModelCursor.newBuilder().addToken().build();
        FakeModelCursor newCursor = FakeModelCursor.newBuilder().addCluster().build();
        when(streamFeature.getCursor()).thenReturn(initialCursor);

        // Causes StreamDriver to build a list of children based on the children from the cursor.
        List<LeafFeatureDriver> leafFeatureDrivers = streamDriver.getLeafFeatureDrivers();
        assertThat(leafFeatureDrivers).hasSize(2);
        assertThat(leafFeatureDrivers.get(0)).isEqualTo(noContentDriver);
        assertThat(leafFeatureDrivers.get(1)).isEqualTo(continuationDriver);

        streamDriver.onNewChildren(initialCursor.getChildAt(0), newCursor.getModelChildren(),
                /* wasSynthetic = */ false);
        leafFeatureDrivers = streamDriver.getLeafFeatureDrivers();
        assertThat(streamDriver.getLeafFeatureDrivers()).hasSize(1);
        assertThat(leafFeatureDrivers.get(0)).isEqualTo(getLeafFeatureDriverFromCluster(1));

        // If the above two assertions pass, this is also guaranteed to pass. This is just to
        // explicitly check that the ContinuationDriver has been removed.
        assertThat(leafFeatureDrivers).doesNotContain(continuationDriver);

        verify(scrollRestorer).maybeRestoreScroll();
    }

    @Test
    public void testContinuationToken_tokenHandling_newSyntheticToken() {
        FakeModelCursor initialCursor = FakeModelCursor.newBuilder().addSyntheticToken().build();
        FakeModelCursor newCursor =
                FakeModelCursor.newBuilder().addCluster().addSyntheticToken().build();
        when(streamFeature.getCursor()).thenReturn(initialCursor);
        when(continuationDriver.hasTokenBeenHandled()).thenReturn(true);

        // Causes StreamDriver to build a list of children based on the children from the cursor.
        List<LeafFeatureDriver> leafFeatureDrivers = streamDriver.getLeafFeatureDrivers();
        assertThat(leafFeatureDrivers).hasSize(2);
        assertThat(leafFeatureDrivers.get(0)).isEqualTo(noContentDriver);
        assertThat(leafFeatureDrivers.get(1)).isEqualTo(continuationDriver);

        streamDriver.onNewChildren(initialCursor.getChildAt(0), newCursor.getModelChildren(),
                /* wasSynthetic = */ true);
        leafFeatureDrivers = streamDriver.getLeafFeatureDrivers();
        assertThat(streamDriver.getLeafFeatureDrivers()).hasSize(2);
        assertThat(leafFeatureDrivers.get(0)).isEqualTo(getLeafFeatureDriverFromCluster(1));
        assertThat(leafFeatureDrivers.get(1)).isEqualTo(continuationDriver);

        verify(scrollRestorer, never()).maybeRestoreScroll();
    }

    @Test
    public void testContinuationToken_tokenHandling_notifiesObservers() {
        FakeModelCursor initialCursor = FakeModelCursor.newBuilder().addCard().addToken().build();
        FakeModelCursor newCursor = FakeModelCursor.newBuilder().addCluster().build();
        when(streamFeature.getCursor()).thenReturn(initialCursor);

        streamDriver.getLeafFeatureDrivers();

        streamDriver.onNewChildren(initialCursor.getChildAt(1), newCursor.getModelChildren(),
                /* wasSynthetic = */ false);

        verify(contentListener).notifyContentRemoved(1);
        verify(contentListener)
                .notifyContentsAdded(1, Lists.newArrayList(getLeafFeatureDriverFromCluster(2)));
        verify(streamOfflineMonitor, times(2)).requestOfflineStatusForNewContent();
    }

    @Test
    public void testContinuationToken_tokenChildrenAddedAtTokenPosition() {
        FakeModelCursor initialCursor =
                FakeModelCursor.newBuilder().addCluster().addToken().build();
        FakeModelCursor newCursor = FakeModelCursor.newBuilder().addCluster().addToken().build();
        when(streamFeature.getCursor()).thenReturn(initialCursor);

        streamDriver.getLeafFeatureDrivers();
        streamDriver.onNewChildren(initialCursor.getChildAt(1), newCursor.getModelChildren(),
                /* wasSynthetic = */ false);

        List<LeafFeatureDriver> leafFeatureDrivers = streamDriver.getLeafFeatureDrivers();
        assertThat(leafFeatureDrivers).hasSize(3);
        assertThat(leafFeatureDrivers)
                .containsExactly(getLeafFeatureDriverFromCluster(0),
                        getLeafFeatureDriverFromCluster(2), continuationDriver);
    }

    @Test
    public void testContinuationToken_tokenChildrenAddedAtTokenPosition_tokenNotAtEnd() {
        FakeModelCursor initialCursor =
                FakeModelCursor.newBuilder().addCluster().addToken().addCluster().build();
        FakeModelCursor newCursor =
                FakeModelCursor.newBuilder().addCluster().addCard().addCluster().build();
        when(streamFeature.getCursor()).thenReturn(initialCursor);

        streamDriver.getLeafFeatureDrivers();
        streamDriver.onNewChildren(initialCursor.getChildAt(1), newCursor.getModelChildren(),
                /* wasSynthetic = */ false);

        List<LeafFeatureDriver> leafFeatureDrivers = streamDriver.getLeafFeatureDrivers();
        assertThat(leafFeatureDrivers).hasSize(5);
        assertThat(leafFeatureDrivers)
                .containsExactly(getLeafFeatureDriverFromCluster(0),
                        getLeafFeatureDriverFromCluster(2), getLeafFeatureDriverFromCluster(3),
                        getLeafFeatureDriverFromCard(4), getLeafFeatureDriverFromCluster(5));
    }

    @Test
    public void testContinuationToken_tokenNotFound() {
        FakeModelCursor initialCursor =
                FakeModelCursor.newBuilder().addCluster().addToken().addCluster().build();
        FakeModelCursor badCursor = FakeModelCursor.newBuilder().addCard().build();
        when(streamFeature.getCursor()).thenReturn(initialCursor);

        streamDriver.getLeafFeatureDrivers();
        assertThatRunnable(()
                                   -> streamDriver.onNewChildren(badCursor.getChildAt(0),
                                           badCursor.getModelChildren(),
                                           /* wasSynthetic = */ false))
                .throwsAnExceptionOfType(RuntimeException.class);
    }

    @Test
    public void testOnChange_remove() {
        FakeModelCursor fakeModelCursor =
                FakeModelCursor.newBuilder().addCard().addCard().addCard().addCard().build();

        when(streamFeature.getCursor()).thenReturn(fakeModelCursor);

        List<LeafFeatureDriver> leafFeatureDrivers = streamDriver.getLeafFeatureDrivers();

        assertThat(leafFeatureDrivers).hasSize(4);

        streamDriver.onChange(new FeatureChangeBuilder()
                                      .addChildForRemoval(fakeModelCursor.getChildAt(1))
                                      .addChildForRemoval(fakeModelCursor.getChildAt(2))
                                      .build());

        assertThat(streamDriver.getLeafFeatureDrivers())
                .containsExactly(leafFeatureDrivers.get(0), leafFeatureDrivers.get(3));
    }

    @Test
    public void testOnChange_addsZeroState_whenFeatureDriversEmpty() {
        FakeModelCursor fakeModelCursor = FakeModelCursor.newBuilder().addCard().build();

        initializeStreamDriverAndDismissAllFeatureChildren(fakeModelCursor);

        assertThat(streamDriver.getLeafFeatureDrivers()).hasSize(1);
        assertThat(streamDriver.getLeafFeatureDrivers().get(0)).isInstanceOf(ZeroStateDriver.class);
    }

    @Test
    public void testOnChange_dismissesLastDriver_logsContentDismissed() {
        FakeModelCursor fakeModelCursor = FakeModelCursor.newBuilder().addCard().build();

        initializeStreamDriverAndDismissAllFeatureChildren(fakeModelCursor);

        verify(basicLoggingApi).onZeroStateShown(ZeroStateShowReason.CONTENT_DISMISSED);
    }

    @Test
    public void testGetLeafFeatureDrivers_addsZeroState_withNoModelChildren() {
        when(streamFeature.getCursor()).thenReturn(EMPTY_MODEL_CURSOR);

        List<LeafFeatureDriver> leafFeatureDrivers = streamDriver.getLeafFeatureDrivers();

        assertThat(leafFeatureDrivers).hasSize(1);
        assertThat(leafFeatureDrivers.get(0)).isInstanceOf(ZeroStateDriver.class);
    }

    @Test
    public void testGetLeafFeatureDrivers_noContent_logsNoContent() {
        when(streamFeature.getCursor()).thenReturn(EMPTY_MODEL_CURSOR);

        streamDriver.getLeafFeatureDrivers();

        verify(basicLoggingApi).onZeroStateShown(ZeroStateShowReason.NO_CONTENT);
    }

    @Test
    public void testGetLeafFeatureDrivers_doesNotAddZeroState_ifInitialLoad() {
        streamDriver = createStreamDriver(/* restoring= */ false, /* isInitialLoad= */ true);

        when(streamFeature.getCursor()).thenReturn(EMPTY_MODEL_CURSOR);

        List<LeafFeatureDriver> leafFeatureDrivers = streamDriver.getLeafFeatureDrivers();

        assertThat(leafFeatureDrivers).isEmpty();
    }

    @Test
    public void testGetLeafFeatureDrivers_doesNotAddZeroState_ifRestoring() {
        streamDriver = createStreamDriver(/* restoring= */ true, /* isInitialLoad= */ false);

        when(streamFeature.getCursor()).thenReturn(EMPTY_MODEL_CURSOR);

        List<LeafFeatureDriver> leafFeatureDrivers = streamDriver.getLeafFeatureDrivers();

        assertThat(leafFeatureDrivers).isEmpty();
    }

    @Test
    public void testShowZeroState_createsZeroState() {
        FakeModelCursor fakeModelCursor = FakeModelCursor.newBuilder().addCard().addCard().build();

        when(streamFeature.getCursor()).thenReturn(fakeModelCursor);
        assertThat(streamDriver.getLeafFeatureDrivers()).hasSize(2);

        streamDriver.showZeroState(ZeroStateShowReason.ERROR);

        List<LeafFeatureDriver> leafFeatureDrivers = streamDriver.getLeafFeatureDrivers();

        assertThat(leafFeatureDrivers).hasSize(1);
        assertThat(leafFeatureDrivers.get(0)).isInstanceOf(ZeroStateDriver.class);
    }

    @Test
    public void testShowZeroState_logsZeroStateReason() {
        FakeModelCursor fakeModelCursor = FakeModelCursor.newBuilder().addCard().addCard().build();
        when(streamFeature.getCursor()).thenReturn(fakeModelCursor);

        streamDriver.getLeafFeatureDrivers();
        streamDriver.showZeroState(ZeroStateShowReason.ERROR);

        verify(basicLoggingApi).onZeroStateShown(ZeroStateShowReason.ERROR);
    }

    @Test
    public void testShowZeroState_notifiesContentsCleared() {
        FakeModelCursor fakeModelCursor = FakeModelCursor.newBuilder().addCard().addCard().build();

        when(streamFeature.getCursor()).thenReturn(fakeModelCursor);

        streamDriver.showSpinner();

        verify(contentListener).notifyContentsCleared();
    }

    @Test
    public void testShowZeroState_destroysFeatureDrivers() {
        FakeModelCursor fakeModelCursor = FakeModelCursor.newBuilder().addToken().build();

        when(streamFeature.getCursor()).thenReturn(fakeModelCursor);
        streamDriver.getLeafFeatureDrivers();

        streamDriver.showSpinner();

        verify(continuationDriver).onDestroy();
    }

    @Test
    public void testOnChange_addsNoContentCard_withJustContinuationDriver() {
        FakeModelCursor fakeModelCursor = FakeModelCursor.newBuilder().addCard().addToken().build();

        initializeStreamDriverAndDismissAllFeatureChildren(fakeModelCursor);

        assertThat(streamDriver.getLeafFeatureDrivers()).hasSize(2);
        assertThat(streamDriver.getLeafFeatureDrivers().get(0)).isInstanceOf(NoContentDriver.class);
    }

    @Test
    public void testContinuationToken_removesNoContentCard() {
        FakeModelCursor fakeModelCursor = FakeModelCursor.newBuilder().addCard().addToken().build();

        initializeStreamDriverAndDismissAllFeatureChildren(fakeModelCursor);

        assertThat(streamDriver.getLeafFeatureDrivers()).hasSize(2);
        assertThat(streamDriver.getLeafFeatureDrivers().get(0)).isInstanceOf(NoContentDriver.class);

        FakeModelCursor newCursor = FakeModelCursor.newBuilder().addCard().build();
        streamDriver.onNewChildren(fakeModelCursor.getChildAt(1), newCursor.getModelChildren(),
                /* wasSynthetic = */ false);

        assertThat(streamDriver.getLeafFeatureDrivers()).hasSize(1);
        assertThat(streamDriver.getLeafFeatureDrivers().get(0))
                .isInstanceOf(FakeLeafFeatureDriver.class);
    }

    @Test
    public void testContinuationToken_replacesNoContentCardWithZeroState() {
        FakeModelCursor initialCursor = FakeModelCursor.newBuilder().addCard().addToken().build();
        initializeStreamDriverAndDismissAllFeatureChildren(initialCursor);
        streamDriver.onNewChildren(
                initialCursor.getChildAt(1), ImmutableList.of(), /* wasSynthetic = */ false);

        verify(noContentDriver).onDestroy();
        assertThat(streamDriver.getLeafFeatureDrivers()).hasSize(1);
        assertThat(streamDriver.getLeafFeatureDrivers().get(0)).isInstanceOf(ZeroStateDriver.class);
    }

    @Test
    public void testContinuationToken_logsContinuationTokenPayload() {
        FakeModelCursor initialCursor = FakeModelCursor.newBuilder().addCard().addToken().build();
        when(streamFeature.getCursor()).thenReturn(initialCursor);

        streamDriver.getLeafFeatureDrivers();
        streamDriver.onNewChildren(initialCursor.getChildAt(1),
                FakeModelCursor.newBuilder()
                        .addCluster()
                        .addCard()
                        .addCard()
                        .addToken()
                        .build()
                        .getModelChildren(),
                true);

        verify(basicLoggingApi)
                .onTokenCompleted(
                        /* wasSynthetic= */ true, /* contentCount= */ 3, /* tokenCount= */ 1);
    }

    @Test
    public void testOnChange_remove_notifiesListener() {
        FakeModelCursor fakeModelCursor = FakeModelCursor.newBuilder().addCard().addCard().build();

        when(streamFeature.getCursor()).thenReturn(fakeModelCursor);

        streamDriver.getLeafFeatureDrivers();

        streamDriver.onChange(new FeatureChangeBuilder()
                                      .addChildForRemoval(fakeModelCursor.getChildAt(0))
                                      .build());

        verify(contentListener).notifyContentRemoved(0);
        verifyNoMoreInteractions(contentListener);
    }

    @Test
    public void testOnChange_addAndRemoveContent() {
        FakeModelCursor fakeModelCursor = FakeModelCursor.newBuilder().addCard().addCard().build();

        when(streamFeature.getCursor()).thenReturn(fakeModelCursor);

        // Causes StreamDriver to build a list of children based on the children from the cursor.
        streamDriver.getLeafFeatureDrivers();

        streamDriver.onChange(new FeatureChangeBuilder()
                                      .addChildForRemoval(fakeModelCursor.getChildAt(0))
                                      .addChildForAppending(createFakeClusterModelChild())
                                      .build());

        InOrder inOrder = Mockito.inOrder(contentListener);

        inOrder.verify(contentListener).notifyContentRemoved(0);
        inOrder.verify(contentListener).notifyContentsAdded(eq(1), featureDriversCaptor.capture());
        inOrder.verifyNoMoreInteractions();

        assertThat(featureDriversCaptor.getValue()).hasSize(1);
        assertThat(featureDriversCaptor.getValue().get(0))
                .isEqualTo(getLeafFeatureDriverFromCluster(2));
    }

    @Test
    public void testOnChange_addContent() {
        FakeModelCursor fakeModelCursor = FakeModelCursor.newBuilder().addCard().addCard().build();

        when(streamFeature.getCursor()).thenReturn(fakeModelCursor);

        // Causes StreamDriver to build a list of children based on the children from the cursor.
        streamDriver.getLeafFeatureDrivers();

        reset(contentListener);

        streamDriver.onChange(new FeatureChangeBuilder()
                                      .addChildForAppending(createFakeClusterModelChild())
                                      .build());

        InOrder inOrder = Mockito.inOrder(contentListener);

        inOrder.verify(contentListener).notifyContentsAdded(eq(2), featureDriversCaptor.capture());
        inOrder.verifyNoMoreInteractions();

        assertThat(featureDriversCaptor.getValue()).hasSize(1);
        assertThat(featureDriversCaptor.getValue().get(0))
                .isEqualTo(getLeafFeatureDriverFromCluster(2));
        verify(streamOfflineMonitor, times(2)).requestOfflineStatusForNewContent();
    }

    @Test
    public void testOnDestroy() {
        FakeModelCursor cursor = FakeModelCursor.newBuilder().addCard().addToken().build();
        initializeStreamDriverAndDismissAllFeatureChildren(cursor);

        assertThat(streamDriver.getLeafFeatureDrivers())
                .containsExactly(noContentDriver, continuationDriver);
        streamDriver.onDestroy();

        verify(noContentDriver).onDestroy();
        verify(continuationDriver).onDestroy();
        verify(streamFeature).unregisterObserver(streamDriver);
    }

    @Test
    public void testOnDestroy_clearsFeatureDrivers() {
        streamDriver = createStreamDriver(false, true);
        when(streamFeature.getCursor()).thenReturn(FakeModelCursor.newBuilder().addCard().build());

        assertThat(streamDriver.getLeafFeatureDrivers()).hasSize(1);
        streamDriver.onDestroy();

        assertThat(streamDriver.getLeafFeatureDrivers()).isEmpty();
    }

    @Test
    public void testHasContent_returnsTrue() {
        FakeModelCursor cursor = FakeModelCursor.newBuilder().addCard().addToken().build();
        when(streamFeature.getCursor()).thenReturn(cursor);
        streamDriver.getLeafFeatureDrivers();

        assertThat(streamDriver.hasContent()).isTrue();
    }

    @Test
    public void testHasContent_returnsFalse_withNoContentCard() {
        FakeModelCursor cursor = FakeModelCursor.newBuilder().addCard().addToken().build();
        initializeStreamDriverAndDismissAllFeatureChildren(cursor);

        assertThat(streamDriver.hasContent()).isFalse();
    }

    @Test
    public void testHasContent_returnsFalse_withZeroState() {
        when(streamFeature.getCursor()).thenReturn(EMPTY_MODEL_CURSOR);

        streamDriver.getLeafFeatureDrivers();

        assertThat(streamDriver.hasContent()).isFalse();
    }

    @Test
    public void testAutoConsumeSyntheticTokensOnRestore() {
        FakeModelCursor initialCursor = FakeModelCursor.newBuilder().addCard().addToken().build();
        FakeModelCursor newCursor = FakeModelCursor.newBuilder().addToken().build();

        when(streamFeature.getCursor()).thenReturn(initialCursor);

        streamDriver = createStreamDriver(/* restoring= */ true, /* isInitialLoad= */ false);

        streamDriver.getLeafFeatureDrivers();

        assertThat(streamDriver.wasRestoringDuringLastContinuationDriver).isTrue();

        // Restoring scroll indicates that restore is over, and further continuation drivers will
        // not be forced to consume synthetic tokens.
        streamDriver.maybeRestoreScroll();

        streamDriver.onNewChildren(initialCursor.getChildAt(1), newCursor.getModelChildren(),
                /* wasSynthetic = */ false);

        assertThat(streamDriver.wasRestoringDuringLastContinuationDriver).isFalse();
    }

    @Test
    public void testTriggerPendingDismiss_noAction() {
        setCards(CONTENT_ID);
        SnackbarCallbackApi snackbarCallbackApi = triggerPendingDismiss(
                UndoAction.newBuilder().setConfirmationLabel("confirmation").build(), CONTENT_ID);

        assertThat(streamDriver.isZeroStateBeingShown()).isTrue();

        snackbarCallbackApi.onDismissNoAction();

        verify(pendingDismissCallback).onDismissCommitted();
        assertThat(streamDriver.isZeroStateBeingShown()).isTrue();
    }

    @Test
    public void testTriggerPendingDismiss_actionTaken_lastCard() {
        setCards(CONTENT_ID);
        SnackbarCallbackApi snackbarCallbackApi = triggerPendingDismiss(
                UndoAction.newBuilder().setConfirmationLabel("confirmation").build(), CONTENT_ID);

        List<LeafFeatureDriver> leafFeatureDrivers = streamDriver.getLeafFeatureDrivers();
        assertThat(leafFeatureDrivers).hasSize(1);
        assertThat(streamDriver.isZeroStateBeingShown()).isTrue();

        snackbarCallbackApi.onDismissedWithAction();

        verify(pendingDismissCallback).onDismissReverted();
        leafFeatureDrivers = streamDriver.getLeafFeatureDrivers();
        assertThat(leafFeatureDrivers).hasSize(1);
        assertThat(streamDriver.isZeroStateBeingShown()).isFalse();
    }

    @Test
    public void testTriggerPendingDismiss_actionTaken_multipleCards() {
        setCards(CONTENT_ID, CONTENT_ID + "1");
        SnackbarCallbackApi snackbarCallbackApi = triggerPendingDismiss(
                UndoAction.newBuilder().setConfirmationLabel("confirmation").build(), CONTENT_ID);

        List<LeafFeatureDriver> leafFeatureDrivers = streamDriver.getLeafFeatureDrivers();
        assertThat(leafFeatureDrivers).hasSize(1);
        assertThat(streamDriver.isZeroStateBeingShown()).isFalse();

        snackbarCallbackApi.onDismissedWithAction();

        verify(pendingDismissCallback).onDismissReverted();
        leafFeatureDrivers = streamDriver.getLeafFeatureDrivers();
        assertThat(leafFeatureDrivers).hasSize(2);
        assertThat(streamDriver.isZeroStateBeingShown()).isFalse();
    }

    @Test
    public void testTriggerPendingDismiss_actionStringSent() {
        setCards(CONTENT_ID);
        triggerPendingDismiss(UndoAction.newBuilder()
                                      .setConfirmationLabel("conf")
                                      .setUndoLabel("undo label")
                                      .build(),
                CONTENT_ID);
    }

    @Test
    public void testIsZeroStateBeingShown_returnsTrue_ifZeroState() {
        when(streamFeature.getCursor()).thenReturn(EMPTY_MODEL_CURSOR);
        streamDriver.getLeafFeatureDrivers();

        assertThat(streamDriver.isZeroStateBeingShown()).isTrue();
    }

    @Test
    public void testIsZeroStateBeingShown_returnsFalse_whenNoContentCardShowing() {
        FakeModelCursor cursor = FakeModelCursor.newBuilder().addCard().addToken().build();

        // Dismisses all features, but not the token, so the no-content state will be showing.
        initializeStreamDriverAndDismissAllFeatureChildren(cursor);

        assertThat(streamDriver.isZeroStateBeingShown()).isFalse();
    }

    @Test
    public void testIsZeroStateBeingShown_returnsFalse_ifZeroStateIsSpinner() {
        when(streamFeature.getCursor())
                .thenReturn(FakeModelCursor.newBuilder().addCard().addToken().build());

        streamDriver.getLeafFeatureDrivers();

        streamDriver.showSpinner();

        assertThat(streamDriver.isZeroStateBeingShown()).isFalse();
    }

    @Test
    public void testSetModelProvider_setsModelProviderOnTheZeroState() {
        when(streamFeature.getCursor()).thenReturn(EMPTY_MODEL_CURSOR);
        streamDriver.getLeafFeatureDrivers();

        streamDriver.setModelProviderForZeroState(modelProvider);

        verify(zeroStateDriver).setModelProvider(modelProvider);
    }

    @Test
    public void testRefreshFromZeroState_resultsInZeroState_logsNoNewContent() {
        streamDriver = createStreamDriverFromZeroStateRefresh();

        when(streamFeature.getCursor()).thenReturn(EMPTY_MODEL_CURSOR);
        streamDriver.getLeafFeatureDrivers();

        verify(basicLoggingApi)
                .onZeroStateRefreshCompleted(/* newContentCount= */ 0, /* newTokenCount= */ 0);
    }

    @Test
    public void testRefreshFromZeroState_addsContent_logsContent() {
        streamDriver = createStreamDriverFromZeroStateRefresh();

        when(streamFeature.getCursor())
                .thenReturn(FakeModelCursor.newBuilder().addClusters(10).addToken().build());
        streamDriver.getLeafFeatureDrivers();

        verify(basicLoggingApi)
                .onZeroStateRefreshCompleted(/* newContentCount= */ 10, /* newTokenCount= */ 1);
    }

    private void initializeStreamDriverAndDismissAllFeatureChildren(
            FakeModelCursor fakeModelCursor) {
        when(streamFeature.getCursor()).thenReturn(fakeModelCursor);

        streamDriver.getLeafFeatureDrivers();

        FeatureChangeBuilder dismissAllChildrenBuilder = new FeatureChangeBuilder();

        for (ModelChild child : fakeModelCursor.getModelChildren()) {
            if (child.getType() == Type.FEATURE) {
                dismissAllChildrenBuilder.addChildForRemoval(child);
            }
        }

        streamDriver.onChange(dismissAllChildrenBuilder.build());
    }

    private StreamDriverForTest createNonRestoringStreamDriver() {
        return createStreamDriver(/* restoring= */ false, /* isInitialLoad= */ false);
    }

    private StreamDriverForTest createRestoringStreamDriver() {
        return createStreamDriver(/* restoring= */ true, /* isInitialLoad= */ true);
    }

    private StreamDriverForTest createStreamDriverFromZeroStateRefresh() {
        return createStreamDriver(
                /* restoring= */ false,
                /* isInitialLoad= */ false,
                UiRefreshReason.newBuilder().setReason(Reason.ZERO_STATE).build());
    }

    private StreamDriverForTest createStreamDriver(
            boolean restoring, boolean isInitialLoad, UiRefreshReason uiRefreshReason) {
        return new StreamDriverForTest(
                modelProvider, threadUtils, restoring, isInitialLoad, uiRefreshReason);
    }

    private StreamDriverForTest createStreamDriver(boolean restoring, boolean isInitialLoad) {
        return new StreamDriverForTest(modelProvider, threadUtils, restoring, isInitialLoad,
                UiRefreshReason.getDefaultInstance());
    }

    // TODO: Instead of just checking that the ModelFeature is of the correct type, check
    // that it is the one created by the FakeModelCursor.Builder.
    private LeafFeatureDriver getLeafFeatureDriverFromCard(int i) {
        FakeFeatureDriver featureDriver = (FakeFeatureDriver) streamDriver.childrenCreated.get(i);
        assertThat(featureDriver.getModelFeature().getStreamFeature().hasCard()).isTrue();
        return streamDriver.childrenCreated.get(i).getLeafFeatureDriver();
    }

    // TODO: Instead of just checking that the ModelFeature is of the correct type, check
    // that it is the one created by the FakeModelCursor.Builder.
    private LeafFeatureDriver getLeafFeatureDriverFromCluster(int i) {
        FakeFeatureDriver featureDriver = (FakeFeatureDriver) streamDriver.childrenCreated.get(i);
        assertThat(featureDriver.getModelFeature().getStreamFeature().hasCluster()).isTrue();
        return streamDriver.childrenCreated.get(i).getLeafFeatureDriver();
    }

    private void setCards(String... contentId) {
        FakeModelCursor.Builder cursor = FakeModelCursor.newBuilder();
        for (String id : contentId) {
            cursor.addChild(
                    FakeModelChild.newBuilder()
                            .setModelFeature(getCardModelFeatureWithCursor(EMPTY_MODEL_CURSOR))
                            .setContentId(id)
                            .build());
        }
        when(streamFeature.getCursor()).thenReturn(cursor.build());
    }

    private SnackbarCallbackApi triggerPendingDismiss(UndoAction undoAction, String contentId) {
        ArgumentCaptor<SnackbarCallbackApi> snackbarCallbackApi =
                ArgumentCaptor.forClass(SnackbarCallbackApi.class);

        // Causes StreamDriver to build a list of children based on the children from the cursor.
        streamDriver.getLeafFeatureDrivers();

        streamDriver.triggerPendingDismiss(contentId, undoAction, pendingDismissCallback);

        verify(contentListener).notifyContentRemoved(0);
        verify(snackbarApi)
                .show(eq(undoAction.getConfirmationLabel()),
                        undoAction.hasUndoLabel() ? eq(undoAction.getUndoLabel())
                                                  : eq(context.getResources().getString(
                                                          R.string.snackbar_default_action)),
                        snackbarCallbackApi.capture());

        return snackbarCallbackApi.getValue();
    }

    private class StreamDriverForTest extends StreamDriver {
        // TODO: create a fake for ContinuationDriver so that this can be
        // List<FakeFeatureDriver>
        private List<FeatureDriver> childrenCreated;
        private boolean wasRestoringDuringLastContinuationDriver;

        StreamDriverForTest(ModelProvider modelProvider, ThreadUtils threadUtils, boolean restoring,
                boolean isInitialLoad, UiRefreshReason uiRefreshReason) {
            super(actionApi, actionManager, actionParserFactory, modelProvider, threadUtils, clock,
                    configuration, context, snackbarApi, contentChangedListener, scrollRestorer,
                    basicLoggingApi, streamOfflineMonitor, feedKnownContent,
                    mock(ContextMenuManager.class), restoring, isInitialLoad, mainThreadRunner,
                    viewLoggingUpdater, tooltipApi, uiRefreshReason, scrollMonitor);
            childrenCreated = new ArrayList<>();
        }

        @Override
        ContinuationDriver createContinuationDriver(BasicLoggingApi basicLoggingApi, Clock clock,
                Configuration configuration, Context context, ModelChild modelChild,
                ModelProvider modelProvider, int position, SnackbarApi snackbarApi,
                boolean restoring) {
            this.wasRestoringDuringLastContinuationDriver = restoring;
            childrenCreated.add(continuationDriver);
            return continuationDriver;
        }

        @Override
        FeatureDriver createClusterDriver(ModelFeature modelFeature, int position) {
            FeatureDriver featureDriver =
                    new FakeFeatureDriver.Builder().setModelFeature(modelFeature).build();
            childrenCreated.add(featureDriver);
            return featureDriver;
        }

        @Override
        FeatureDriver createCardDriver(ModelFeature modelFeature, int position) {
            if (modelFeature != UNBOUND_CARD) {
                FeatureDriver featureDriver =
                        new FakeFeatureDriver.Builder().setModelFeature(modelFeature).build();
                childrenCreated.add(featureDriver);
                return featureDriver;
            } else {
                FeatureDriver featureDriver = new FakeFeatureDriver.Builder()
                                                      .setModelFeature(modelFeature)
                                                      .setLeafFeatureDriver(null)
                                                      .build();
                childrenCreated.add(featureDriver);
                return featureDriver;
            }
        }

        @Override
        NoContentDriver createNoContentDriver() {
            return noContentDriver;
        }

        @Override
        ZeroStateDriver createZeroStateDriver() {
            return zeroStateDriver;
        }
    }
}
