// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.drivers;

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

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;
import static org.chromium.chrome.browser.feed.library.testing.modelprovider.FakeModelCursor.getCardModelFeatureWithCursor;

import android.app.Activity;
import android.content.Context;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.Lists;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ContentChangedListener;
import org.chromium.chrome.browser.feed.library.api.host.action.ActionApi;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.InternalFeedError;
import org.chromium.chrome.browser.feed.library.api.host.logging.ZeroStateShowReason;
import org.chromium.chrome.browser.feed.library.api.host.stream.SnackbarApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.SnackbarCallbackApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipApi;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionManager;
import org.chromium.chrome.browser.feed.library.api.internal.actionparser.ActionParserFactory;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.knowncontent.FeedKnownContent;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild.Type;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelFeature;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.RemoveTrackingFactory;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.State;
import org.chromium.chrome.browser.feed.library.basicstream.internal.drivers.StreamDriver.StreamContentListener;
import org.chromium.chrome.browser.feed.library.basicstream.internal.drivers.testing.FakeFeatureDriver;
import org.chromium.chrome.browser.feed.library.basicstream.internal.drivers.testing.FakeLeafFeatureDriver;
import org.chromium.chrome.browser.feed.library.basicstream.internal.scroll.BasicStreamScrollMonitor;
import org.chromium.chrome.browser.feed.library.basicstream.internal.scroll.ScrollRestorer;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewloggingupdater.ViewLoggingUpdater;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.sharedstream.contextmenumanager.ContextMenuManager;
import org.chromium.chrome.browser.feed.library.sharedstream.offlinemonitor.StreamOfflineMonitor;
import org.chromium.chrome.browser.feed.library.sharedstream.pendingdismiss.PendingDismissCallback;
import org.chromium.chrome.browser.feed.library.sharedstream.removetrackingfactory.StreamRemoveTrackingFactory;
import org.chromium.chrome.browser.feed.library.testing.modelprovider.FakeModelChild;
import org.chromium.chrome.browser.feed.library.testing.modelprovider.FakeModelCursor;
import org.chromium.chrome.browser.feed.library.testing.modelprovider.FakeModelFeature;
import org.chromium.chrome.browser.feed.library.testing.modelprovider.FeatureChangeBuilder;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.components.feed.core.proto.libraries.sharedstream.UiRefreshReasonProto.UiRefreshReason;
import org.chromium.components.feed.core.proto.libraries.sharedstream.UiRefreshReasonProto.UiRefreshReason.Reason;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.UndoAction;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Card;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Cluster;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Content;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link StreamDriver}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StreamDriverTest {
    private static final String CONTENT_ID = "contentID";
    private FakeModelCursor mCursorWithInvalidChildren;

    private static final FakeModelCursor EMPTY_MODEL_CURSOR = FakeModelCursor.newBuilder().build();

    private static final ModelFeature UNBOUND_CARD =
            FakeModelFeature.newBuilder()
                    .setStreamFeature(
                            StreamFeature.newBuilder().setCard(Card.getDefaultInstance()).build())
                    .build();

    @Mock
    private ActionApi mActionApi;
    @Mock
    private ActionManager mActionManager;
    @Mock
    private ActionParserFactory mActionParserFactory;
    @Mock
    private ModelFeature mStreamFeature;
    @Mock
    private ModelProvider mModelProvider;
    @Mock
    private ContinuationDriver mContinuationDriver;
    @Mock
    private NoContentDriver mNoContentDriver;
    @Mock
    private ZeroStateDriver mZeroStateDriver;
    @Mock
    private StreamContentListener mContentlistener;
    @Mock
    private ContentChangedListener mContentChangedListener;
    @Mock
    private SnackbarApi mSnackbarApi;
    @Mock
    private ScrollRestorer mScrollRestorer;
    @Mock
    private BasicLoggingApi mBasicLoggingApi;
    @Mock
    private FeedKnownContent mFeedKnownContent;
    @Mock
    private StreamOfflineMonitor mStreamOfflineMonitor;
    @Mock
    private PendingDismissCallback mPendingDismissCallback;
    @Mock
    private TooltipApi mTooltipApi;
    @Mock
    private BasicStreamScrollMonitor mScrollmonitor;

    @Captor
    private ArgumentCaptor<List<LeafFeatureDriver>> mFeatureDriversCaptor;

    private StreamDriverForTest mStreamDriver;
    private Configuration mConfiguration = new Configuration.Builder().build();
    private Context mContext;
    private Clock mClock;
    private ThreadUtils mThreadUtils;
    private final FakeMainThreadRunner mMainThreadRunner = FakeMainThreadRunner.queueAllTasks();
    private final ViewLoggingUpdater mViewLoggingUpdater = new ViewLoggingUpdater();

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
        mCursorWithInvalidChildren =
                FakeModelCursor.newBuilder()
                        .addChild(FakeModelChild.newBuilder().setModelFeature(feature).build())
                        .build();

        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        mClock = new FakeClock();

        when(mContinuationDriver.getLeafFeatureDriver()).thenReturn(mContinuationDriver);
        when(mNoContentDriver.getLeafFeatureDriver()).thenReturn(mNoContentDriver);
        when(mZeroStateDriver.getLeafFeatureDriver()).thenReturn(mZeroStateDriver);
        when(mZeroStateDriver.isSpinnerShowing()).thenReturn(false);

        when(mModelProvider.getRootFeature()).thenReturn(mStreamFeature);
        when(mModelProvider.getCurrentState()).thenReturn(State.READY);
        mThreadUtils = new ThreadUtils();

        mStreamDriver = createNonRestoringStreamDriver();

        mStreamDriver.setStreamContentListener(mContentlistener);
    }

    @Test
    public void testConstruction() {
        ArgumentCaptor<RemoveTrackingFactory> removeTrackingFactoryArgumentCaptor =
                ArgumentCaptor.forClass(RemoveTrackingFactory.class);

        verify(mModelProvider).enableRemoveTracking(removeTrackingFactoryArgumentCaptor.capture());

        assertThat(removeTrackingFactoryArgumentCaptor.getValue())
                .isInstanceOf(StreamRemoveTrackingFactory.class);
    }

    @Test
    public void testBuildChildren() {
        when(mStreamFeature.getCursor())
                .thenReturn(FakeModelCursor.newBuilder().addCard().addCluster().addToken().build());

        // Causes StreamDriver to build a list of children based on the children from the cursor.
        List<LeafFeatureDriver> leafFeatureDrivers = mStreamDriver.getLeafFeatureDrivers();

        assertThat(leafFeatureDrivers).hasSize(3);

        assertThat(leafFeatureDrivers.get(0)).isEqualTo(getLeafFeatureDriverFromCard(0));
        assertThat(leafFeatureDrivers.get(1)).isEqualTo(getLeafFeatureDriverFromCluster(1));
        assertThat(leafFeatureDrivers.get(2)).isEqualTo(mContinuationDriver);

        verify(mContinuationDriver).initialize();
        verify(mStreamOfflineMonitor).requestOfflineStatusForNewContent();
    }

    @Test
    public void testBuildChildren_unboundContent() {
        when(mStreamFeature.getCursor())
                .thenReturn(FakeModelCursor.newBuilder().addChild(UNBOUND_CARD).addCard().build());

        // Causes StreamDriver to build a list of children based on the children from the cursor.
        List<LeafFeatureDriver> leafFeatureDrivers = mStreamDriver.getLeafFeatureDrivers();

        assertThat(leafFeatureDrivers).hasSize(1);
        assertThat(leafFeatureDrivers.get(0)).isEqualTo(getLeafFeatureDriverFromCard(1));
        verify(mBasicLoggingApi).onInternalError(InternalFeedError.FAILED_TO_CREATE_LEAF);
    }

    @Test
    public void testBuildChildren_unboundChild_logsInternalError() {
        when(mStreamFeature.getCursor())
                .thenReturn(FakeModelCursor.newBuilder().addUnboundChild().build());

        mStreamDriver.getLeafFeatureDrivers();

        verify(mBasicLoggingApi).onInternalError(InternalFeedError.TOP_LEVEL_UNBOUND_CHILD);
    }

    @Test
    public void testBuildChildren_initializing() {
        when(mModelProvider.getRootFeature()).thenReturn(null);
        when(mModelProvider.getCurrentState()).thenReturn(State.INITIALIZING);
        List<LeafFeatureDriver> leafFeatureDrivers = mStreamDriver.getLeafFeatureDrivers();
        assertThat(leafFeatureDrivers).hasSize(1);
        assertThat(mStreamDriver.getLeafFeatureDrivers().get(0))
                .isInstanceOf(ZeroStateDriver.class);
    }

    @Test
    public void testBuildChildren_nullRootFeature_logsInternalError() {
        when(mModelProvider.getRootFeature()).thenReturn(null);

        mStreamDriver.getLeafFeatureDrivers();

        verify(mBasicLoggingApi).onInternalError(InternalFeedError.NO_ROOT_FEATURE);
    }

    @Test
    public void testCreateChildren_invalidFeatureType_logsInternalError() {
        when(mStreamFeature.getCursor()).thenReturn(mCursorWithInvalidChildren);

        mStreamDriver.getLeafFeatureDrivers();

        verify(mBasicLoggingApi).onInternalError(InternalFeedError.TOP_LEVEL_INVALID_FEATURE_TYPE);
    }

    @Test
    public void testMaybeRestoreScroll() {
        mStreamDriver = createRestoringStreamDriver();

        when(mStreamFeature.getCursor()).thenReturn(FakeModelCursor.newBuilder().addCard().build());

        mStreamDriver.getLeafFeatureDrivers();

        mStreamDriver.maybeRestoreScroll();

        verify(mScrollRestorer).maybeRestoreScroll();
    }

    @Test
    public void testMaybeRestoreScroll_withToken() {
        mStreamDriver = createRestoringStreamDriver();

        when(mStreamFeature.getCursor())
                .thenReturn(FakeModelCursor.newBuilder().addCard().addCluster().addToken().build());

        mStreamDriver.getLeafFeatureDrivers();

        mStreamDriver.maybeRestoreScroll();

        // Should restore scroll if a non-synthetic token is last
        verify(mScrollRestorer).maybeRestoreScroll();
    }

    @Test
    public void testMaybeRestoreScroll_withSyntheticToken() {
        mStreamDriver = createRestoringStreamDriver();

        when(mStreamFeature.getCursor())
                .thenReturn(FakeModelCursor.newBuilder()
                                    .addCard()
                                    .addCluster()
                                    .addSyntheticToken()
                                    .build());
        when(mContinuationDriver.hasTokenBeenHandled()).thenReturn(true);

        mStreamDriver.getLeafFeatureDrivers();

        mStreamDriver.maybeRestoreScroll();

        // Should never restore scroll if a synthetic token is last
        verify(mScrollRestorer, never()).maybeRestoreScroll();
    }

    @Test
    public void testMaybeRestoreScroll_notRestoring_doesNotScroll() {
        mStreamDriver = createNonRestoringStreamDriver();

        when(mStreamFeature.getCursor())
                .thenReturn(FakeModelCursor.newBuilder()
                                    .addCard()
                                    .addCluster()
                                    .addSyntheticToken()
                                    .build());

        mStreamDriver.getLeafFeatureDrivers();

        mStreamDriver.maybeRestoreScroll();

        // Should never restore scroll if created in a non-restoring state.
        verify(mScrollRestorer, never()).maybeRestoreScroll();
    }

    @Test
    public void testContinuationToken_createsContinuationContentModel() {
        when(mStreamFeature.getCursor())
                .thenReturn(FakeModelCursor.newBuilder().addToken().build());

        List<LeafFeatureDriver> leafFeatureDrivers = mStreamDriver.getLeafFeatureDrivers();
        assertThat(leafFeatureDrivers).hasSize(2);
        assertThat(leafFeatureDrivers.get(0)).isEqualTo(mNoContentDriver);
        assertThat(leafFeatureDrivers.get(1)).isEqualTo(mContinuationDriver);
    }

    @Test
    public void testContinuationToken_tokenHandling() {
        mStreamDriver = createStreamDriver(/* restoring= */ true, /* isInitialLoad= */ false);

        FakeModelCursor initialCursor = FakeModelCursor.newBuilder().addToken().build();
        FakeModelCursor newCursor = FakeModelCursor.newBuilder().addCluster().build();
        when(mStreamFeature.getCursor()).thenReturn(initialCursor);

        // Causes StreamDriver to build a list of children based on the children from the cursor.
        List<LeafFeatureDriver> leafFeatureDrivers = mStreamDriver.getLeafFeatureDrivers();
        assertThat(leafFeatureDrivers).hasSize(2);
        assertThat(leafFeatureDrivers.get(0)).isEqualTo(mNoContentDriver);
        assertThat(leafFeatureDrivers.get(1)).isEqualTo(mContinuationDriver);

        mStreamDriver.onNewChildren(initialCursor.getChildAt(0), newCursor.getModelChildren(),
                /* wasSynthetic = */ false);
        leafFeatureDrivers = mStreamDriver.getLeafFeatureDrivers();
        assertThat(mStreamDriver.getLeafFeatureDrivers()).hasSize(1);
        assertThat(leafFeatureDrivers.get(0)).isEqualTo(getLeafFeatureDriverFromCluster(1));

        // If the above two assertions pass, this is also guaranteed to pass. This is just to
        // explicitly check that the ContinuationDriver has been removed.
        assertThat(leafFeatureDrivers).doesNotContain(mContinuationDriver);

        verify(mScrollRestorer).maybeRestoreScroll();
    }

    @Test
    public void testContinuationToken_tokenHandling_newSyntheticToken() {
        FakeModelCursor initialCursor = FakeModelCursor.newBuilder().addSyntheticToken().build();
        FakeModelCursor newCursor =
                FakeModelCursor.newBuilder().addCluster().addSyntheticToken().build();
        when(mStreamFeature.getCursor()).thenReturn(initialCursor);
        when(mContinuationDriver.hasTokenBeenHandled()).thenReturn(true);

        // Causes StreamDriver to build a list of children based on the children from the cursor.
        List<LeafFeatureDriver> leafFeatureDrivers = mStreamDriver.getLeafFeatureDrivers();
        assertThat(leafFeatureDrivers).hasSize(2);
        assertThat(leafFeatureDrivers.get(0)).isEqualTo(mNoContentDriver);
        assertThat(leafFeatureDrivers.get(1)).isEqualTo(mContinuationDriver);

        mStreamDriver.onNewChildren(initialCursor.getChildAt(0), newCursor.getModelChildren(),
                /* wasSynthetic = */ true);
        leafFeatureDrivers = mStreamDriver.getLeafFeatureDrivers();
        assertThat(mStreamDriver.getLeafFeatureDrivers()).hasSize(2);
        assertThat(leafFeatureDrivers.get(0)).isEqualTo(getLeafFeatureDriverFromCluster(1));
        assertThat(leafFeatureDrivers.get(1)).isEqualTo(mContinuationDriver);

        verify(mScrollRestorer, never()).maybeRestoreScroll();
    }

    @Test
    public void testContinuationToken_tokenHandling_notifiesObservers() {
        FakeModelCursor initialCursor = FakeModelCursor.newBuilder().addCard().addToken().build();
        FakeModelCursor newCursor = FakeModelCursor.newBuilder().addCluster().build();
        when(mStreamFeature.getCursor()).thenReturn(initialCursor);

        mStreamDriver.getLeafFeatureDrivers();

        mStreamDriver.onNewChildren(initialCursor.getChildAt(1), newCursor.getModelChildren(),
                /* wasSynthetic = */ false);

        verify(mContentlistener).notifyContentRemoved(1);
        verify(mContentlistener)
                .notifyContentsAdded(1, Lists.newArrayList(getLeafFeatureDriverFromCluster(2)));
        verify(mStreamOfflineMonitor, times(2)).requestOfflineStatusForNewContent();
    }

    @Test
    public void testContinuationToken_tokenChildrenAddedAtTokenPosition() {
        FakeModelCursor initialCursor =
                FakeModelCursor.newBuilder().addCluster().addToken().build();
        FakeModelCursor newCursor = FakeModelCursor.newBuilder().addCluster().addToken().build();
        when(mStreamFeature.getCursor()).thenReturn(initialCursor);

        mStreamDriver.getLeafFeatureDrivers();
        mStreamDriver.onNewChildren(initialCursor.getChildAt(1), newCursor.getModelChildren(),
                /* wasSynthetic = */ false);

        List<LeafFeatureDriver> leafFeatureDrivers = mStreamDriver.getLeafFeatureDrivers();
        assertThat(leafFeatureDrivers).hasSize(3);
        assertThat(leafFeatureDrivers)
                .containsExactly(getLeafFeatureDriverFromCluster(0),
                        getLeafFeatureDriverFromCluster(2), mContinuationDriver);
    }

    @Test
    public void testContinuationToken_tokenChildrenAddedAtTokenPosition_tokenNotAtEnd() {
        FakeModelCursor initialCursor =
                FakeModelCursor.newBuilder().addCluster().addToken().addCluster().build();
        FakeModelCursor newCursor =
                FakeModelCursor.newBuilder().addCluster().addCard().addCluster().build();
        when(mStreamFeature.getCursor()).thenReturn(initialCursor);

        mStreamDriver.getLeafFeatureDrivers();
        mStreamDriver.onNewChildren(initialCursor.getChildAt(1), newCursor.getModelChildren(),
                /* wasSynthetic = */ false);

        List<LeafFeatureDriver> leafFeatureDrivers = mStreamDriver.getLeafFeatureDrivers();
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
        when(mStreamFeature.getCursor()).thenReturn(initialCursor);

        mStreamDriver.getLeafFeatureDrivers();
        assertThatRunnable(()
                                   -> mStreamDriver.onNewChildren(badCursor.getChildAt(0),
                                           badCursor.getModelChildren(),
                                           /* wasSynthetic = */ false))
                .throwsAnExceptionOfType(RuntimeException.class);
    }

    @Test
    public void testOnChange_remove() {
        FakeModelCursor fakeModelCursor =
                FakeModelCursor.newBuilder().addCard().addCard().addCard().addCard().build();

        when(mStreamFeature.getCursor()).thenReturn(fakeModelCursor);

        List<LeafFeatureDriver> leafFeatureDrivers = mStreamDriver.getLeafFeatureDrivers();

        assertThat(leafFeatureDrivers).hasSize(4);

        mStreamDriver.onChange(new FeatureChangeBuilder()
                                       .addChildForRemoval(fakeModelCursor.getChildAt(1))
                                       .addChildForRemoval(fakeModelCursor.getChildAt(2))
                                       .build());

        assertThat(mStreamDriver.getLeafFeatureDrivers())
                .containsExactly(leafFeatureDrivers.get(0), leafFeatureDrivers.get(3));
    }

    @Test
    public void testOnChange_addsZeroState_whenFeatureDriversEmpty() {
        FakeModelCursor fakeModelCursor = FakeModelCursor.newBuilder().addCard().build();

        initializeStreamDriverAndDismissAllFeatureChildren(fakeModelCursor);

        assertThat(mStreamDriver.getLeafFeatureDrivers()).hasSize(1);
        assertThat(mStreamDriver.getLeafFeatureDrivers().get(0))
                .isInstanceOf(ZeroStateDriver.class);
    }

    @Test
    public void testOnChange_dismissesLastDriver_logsContentDismissed() {
        FakeModelCursor fakeModelCursor = FakeModelCursor.newBuilder().addCard().build();

        initializeStreamDriverAndDismissAllFeatureChildren(fakeModelCursor);

        verify(mBasicLoggingApi).onZeroStateShown(ZeroStateShowReason.CONTENT_DISMISSED);
    }

    @Test
    public void testGetLeafFeatureDrivers_addsZeroState_withNoModelChildren() {
        when(mStreamFeature.getCursor()).thenReturn(EMPTY_MODEL_CURSOR);

        List<LeafFeatureDriver> leafFeatureDrivers = mStreamDriver.getLeafFeatureDrivers();

        assertThat(leafFeatureDrivers).hasSize(1);
        assertThat(leafFeatureDrivers.get(0)).isInstanceOf(ZeroStateDriver.class);
    }

    @Test
    public void testGetLeafFeatureDrivers_noContent_logsNoContent() {
        when(mStreamFeature.getCursor()).thenReturn(EMPTY_MODEL_CURSOR);

        mStreamDriver.getLeafFeatureDrivers();

        verify(mBasicLoggingApi).onZeroStateShown(ZeroStateShowReason.NO_CONTENT);
    }

    @Test
    public void testGetLeafFeatureDrivers_doesNotAddZeroState_ifInitialLoad() {
        mStreamDriver = createStreamDriver(/* restoring= */ false, /* isInitialLoad= */ true);

        when(mStreamFeature.getCursor()).thenReturn(EMPTY_MODEL_CURSOR);

        List<LeafFeatureDriver> leafFeatureDrivers = mStreamDriver.getLeafFeatureDrivers();

        assertThat(leafFeatureDrivers).isEmpty();
    }

    @Test
    public void testGetLeafFeatureDrivers_doesNotAddZeroState_ifRestoring() {
        mStreamDriver = createStreamDriver(/* restoring= */ true, /* isInitialLoad= */ false);

        when(mStreamFeature.getCursor()).thenReturn(EMPTY_MODEL_CURSOR);

        List<LeafFeatureDriver> leafFeatureDrivers = mStreamDriver.getLeafFeatureDrivers();

        assertThat(leafFeatureDrivers).isEmpty();
    }

    @Test
    public void testShowZeroState_createsZeroState() {
        FakeModelCursor fakeModelCursor = FakeModelCursor.newBuilder().addCard().addCard().build();

        when(mStreamFeature.getCursor()).thenReturn(fakeModelCursor);
        assertThat(mStreamDriver.getLeafFeatureDrivers()).hasSize(2);

        mStreamDriver.showZeroState(ZeroStateShowReason.ERROR);

        List<LeafFeatureDriver> leafFeatureDrivers = mStreamDriver.getLeafFeatureDrivers();

        assertThat(leafFeatureDrivers).hasSize(1);
        assertThat(leafFeatureDrivers.get(0)).isInstanceOf(ZeroStateDriver.class);
    }

    @Test
    public void testShowZeroState_logsZeroStateReason() {
        FakeModelCursor fakeModelCursor = FakeModelCursor.newBuilder().addCard().addCard().build();
        when(mStreamFeature.getCursor()).thenReturn(fakeModelCursor);

        mStreamDriver.getLeafFeatureDrivers();
        mStreamDriver.showZeroState(ZeroStateShowReason.ERROR);

        verify(mBasicLoggingApi).onZeroStateShown(ZeroStateShowReason.ERROR);
    }

    @Test
    public void testShowZeroState_notifiesContentsCleared() {
        FakeModelCursor fakeModelCursor = FakeModelCursor.newBuilder().addCard().addCard().build();

        when(mStreamFeature.getCursor()).thenReturn(fakeModelCursor);

        mStreamDriver.showSpinner();

        verify(mContentlistener).notifyContentsCleared();
    }

    @Test
    public void testShowZeroState_destroysFeatureDrivers() {
        FakeModelCursor fakeModelCursor = FakeModelCursor.newBuilder().addToken().build();

        when(mStreamFeature.getCursor()).thenReturn(fakeModelCursor);
        mStreamDriver.getLeafFeatureDrivers();

        mStreamDriver.showSpinner();

        verify(mContinuationDriver).onDestroy();
    }

    @Test
    public void testOnChange_addsNoContentCard_withJustContinuationDriver() {
        FakeModelCursor fakeModelCursor = FakeModelCursor.newBuilder().addCard().addToken().build();

        initializeStreamDriverAndDismissAllFeatureChildren(fakeModelCursor);

        assertThat(mStreamDriver.getLeafFeatureDrivers()).hasSize(2);
        assertThat(mStreamDriver.getLeafFeatureDrivers().get(0))
                .isInstanceOf(NoContentDriver.class);
    }

    @Test
    public void testContinuationToken_removesNoContentCard() {
        FakeModelCursor fakeModelCursor = FakeModelCursor.newBuilder().addCard().addToken().build();

        initializeStreamDriverAndDismissAllFeatureChildren(fakeModelCursor);

        assertThat(mStreamDriver.getLeafFeatureDrivers()).hasSize(2);
        assertThat(mStreamDriver.getLeafFeatureDrivers().get(0))
                .isInstanceOf(NoContentDriver.class);

        FakeModelCursor newCursor = FakeModelCursor.newBuilder().addCard().build();
        mStreamDriver.onNewChildren(fakeModelCursor.getChildAt(1), newCursor.getModelChildren(),
                /* wasSynthetic = */ false);

        assertThat(mStreamDriver.getLeafFeatureDrivers()).hasSize(1);
        assertThat(mStreamDriver.getLeafFeatureDrivers().get(0))
                .isInstanceOf(FakeLeafFeatureDriver.class);
    }

    @Test
    public void testContinuationToken_replacesNoContentCardWithZeroState() {
        FakeModelCursor initialCursor = FakeModelCursor.newBuilder().addCard().addToken().build();
        initializeStreamDriverAndDismissAllFeatureChildren(initialCursor);
        mStreamDriver.onNewChildren(
                initialCursor.getChildAt(1), ImmutableList.of(), /* wasSynthetic = */ false);

        verify(mNoContentDriver).onDestroy();
        assertThat(mStreamDriver.getLeafFeatureDrivers()).hasSize(1);
        assertThat(mStreamDriver.getLeafFeatureDrivers().get(0))
                .isInstanceOf(ZeroStateDriver.class);
    }

    @Test
    public void testContinuationToken_logsContinuationTokenPayload() {
        FakeModelCursor initialCursor = FakeModelCursor.newBuilder().addCard().addToken().build();
        when(mStreamFeature.getCursor()).thenReturn(initialCursor);

        mStreamDriver.getLeafFeatureDrivers();
        mStreamDriver.onNewChildren(initialCursor.getChildAt(1),
                FakeModelCursor.newBuilder()
                        .addCluster()
                        .addCard()
                        .addCard()
                        .addToken()
                        .build()
                        .getModelChildren(),
                true);

        verify(mBasicLoggingApi)
                .onTokenCompleted(
                        /* wasSynthetic= */ true, /* contentCount= */ 3, /* tokenCount= */ 1);
    }

    @Test
    public void testOnChange_remove_notifiesListener() {
        FakeModelCursor fakeModelCursor = FakeModelCursor.newBuilder().addCard().addCard().build();

        when(mStreamFeature.getCursor()).thenReturn(fakeModelCursor);

        mStreamDriver.getLeafFeatureDrivers();

        mStreamDriver.onChange(new FeatureChangeBuilder()
                                       .addChildForRemoval(fakeModelCursor.getChildAt(0))
                                       .build());

        verify(mContentlistener).notifyContentRemoved(0);
        verifyNoMoreInteractions(mContentlistener);
    }

    @Test
    public void testOnChange_addAndRemoveContent() {
        FakeModelCursor fakeModelCursor = FakeModelCursor.newBuilder().addCard().addCard().build();

        when(mStreamFeature.getCursor()).thenReturn(fakeModelCursor);

        // Causes StreamDriver to build a list of children based on the children from the cursor.
        mStreamDriver.getLeafFeatureDrivers();

        mStreamDriver.onChange(new FeatureChangeBuilder()
                                       .addChildForRemoval(fakeModelCursor.getChildAt(0))
                                       .addChildForAppending(createFakeClusterModelChild())
                                       .build());

        InOrder inOrder = Mockito.inOrder(mContentlistener);

        inOrder.verify(mContentlistener).notifyContentRemoved(0);
        inOrder.verify(mContentlistener)
                .notifyContentsAdded(eq(1), mFeatureDriversCaptor.capture());
        inOrder.verifyNoMoreInteractions();

        assertThat(mFeatureDriversCaptor.getValue()).hasSize(1);
        assertThat(mFeatureDriversCaptor.getValue().get(0))
                .isEqualTo(getLeafFeatureDriverFromCluster(2));
    }

    @Test
    public void testOnChange_addContent() {
        FakeModelCursor fakeModelCursor = FakeModelCursor.newBuilder().addCard().addCard().build();

        when(mStreamFeature.getCursor()).thenReturn(fakeModelCursor);

        // Causes StreamDriver to build a list of children based on the children from the cursor.
        mStreamDriver.getLeafFeatureDrivers();

        reset(mContentlistener);

        mStreamDriver.onChange(new FeatureChangeBuilder()
                                       .addChildForAppending(createFakeClusterModelChild())
                                       .build());

        InOrder inOrder = Mockito.inOrder(mContentlistener);

        inOrder.verify(mContentlistener)
                .notifyContentsAdded(eq(2), mFeatureDriversCaptor.capture());
        inOrder.verifyNoMoreInteractions();

        assertThat(mFeatureDriversCaptor.getValue()).hasSize(1);
        assertThat(mFeatureDriversCaptor.getValue().get(0))
                .isEqualTo(getLeafFeatureDriverFromCluster(2));
        verify(mStreamOfflineMonitor, times(2)).requestOfflineStatusForNewContent();
    }

    @Test
    public void testOnDestroy() {
        FakeModelCursor cursor = FakeModelCursor.newBuilder().addCard().addToken().build();
        initializeStreamDriverAndDismissAllFeatureChildren(cursor);

        assertThat(mStreamDriver.getLeafFeatureDrivers())
                .containsExactly(mNoContentDriver, mContinuationDriver);
        mStreamDriver.onDestroy();

        verify(mNoContentDriver).onDestroy();
        verify(mContinuationDriver).onDestroy();
        verify(mStreamFeature).unregisterObserver(mStreamDriver);
    }

    @Test
    public void testOnDestroy_clearsFeatureDrivers() {
        mStreamDriver = createStreamDriver(false, true);
        when(mStreamFeature.getCursor()).thenReturn(FakeModelCursor.newBuilder().addCard().build());

        assertThat(mStreamDriver.getLeafFeatureDrivers()).hasSize(1);
        mStreamDriver.onDestroy();

        assertThat(mStreamDriver.getLeafFeatureDrivers()).isEmpty();
    }

    @Test
    public void testHasContent_returnsTrue() {
        FakeModelCursor cursor = FakeModelCursor.newBuilder().addCard().addToken().build();
        when(mStreamFeature.getCursor()).thenReturn(cursor);
        mStreamDriver.getLeafFeatureDrivers();

        assertThat(mStreamDriver.hasContent()).isTrue();
    }

    @Test
    public void testHasContent_returnsFalse_withNoContentCard() {
        FakeModelCursor cursor = FakeModelCursor.newBuilder().addCard().addToken().build();
        initializeStreamDriverAndDismissAllFeatureChildren(cursor);

        assertThat(mStreamDriver.hasContent()).isFalse();
    }

    @Test
    public void testHasContent_returnsFalse_withZeroState() {
        when(mStreamFeature.getCursor()).thenReturn(EMPTY_MODEL_CURSOR);

        mStreamDriver.getLeafFeatureDrivers();

        assertThat(mStreamDriver.hasContent()).isFalse();
    }

    @Test
    public void testAutoConsumeSyntheticTokensOnRestore() {
        FakeModelCursor initialCursor = FakeModelCursor.newBuilder().addCard().addToken().build();
        FakeModelCursor newCursor = FakeModelCursor.newBuilder().addToken().build();

        when(mStreamFeature.getCursor()).thenReturn(initialCursor);

        mStreamDriver = createStreamDriver(/* restoring= */ true, /* isInitialLoad= */ false);

        mStreamDriver.getLeafFeatureDrivers();

        assertThat(mStreamDriver.mWasRestoringDuringLastContinuationDriver).isTrue();

        // Restoring scroll indicates that restore is over, and further continuation drivers will
        // not be forced to consume synthetic tokens.
        mStreamDriver.maybeRestoreScroll();

        mStreamDriver.onNewChildren(initialCursor.getChildAt(1), newCursor.getModelChildren(),
                /* wasSynthetic = */ false);

        assertThat(mStreamDriver.mWasRestoringDuringLastContinuationDriver).isFalse();
    }

    @Test
    public void testTriggerPendingDismiss_noAction() {
        setCards(CONTENT_ID);
        SnackbarCallbackApi snackbarCallbackApi = triggerPendingDismiss(
                UndoAction.newBuilder().setConfirmationLabel("confirmation").build(), CONTENT_ID);

        assertThat(mStreamDriver.isZeroStateBeingShown()).isTrue();

        snackbarCallbackApi.onDismissNoAction();

        verify(mPendingDismissCallback).onDismissCommitted();
        assertThat(mStreamDriver.isZeroStateBeingShown()).isTrue();
    }

    @Test
    public void testTriggerPendingDismiss_actionTaken_lastCard() {
        setCards(CONTENT_ID);
        SnackbarCallbackApi snackbarCallbackApi = triggerPendingDismiss(
                UndoAction.newBuilder().setConfirmationLabel("confirmation").build(), CONTENT_ID);

        List<LeafFeatureDriver> leafFeatureDrivers = mStreamDriver.getLeafFeatureDrivers();
        assertThat(leafFeatureDrivers).hasSize(1);
        assertThat(mStreamDriver.isZeroStateBeingShown()).isTrue();

        snackbarCallbackApi.onDismissedWithAction();

        verify(mPendingDismissCallback).onDismissReverted();
        leafFeatureDrivers = mStreamDriver.getLeafFeatureDrivers();
        assertThat(leafFeatureDrivers).hasSize(1);
        assertThat(mStreamDriver.isZeroStateBeingShown()).isFalse();
    }

    @Test
    public void testTriggerPendingDismiss_actionTaken_multipleCards() {
        setCards(CONTENT_ID, CONTENT_ID + "1");
        SnackbarCallbackApi snackbarCallbackApi = triggerPendingDismiss(
                UndoAction.newBuilder().setConfirmationLabel("confirmation").build(), CONTENT_ID);

        List<LeafFeatureDriver> leafFeatureDrivers = mStreamDriver.getLeafFeatureDrivers();
        assertThat(leafFeatureDrivers).hasSize(1);
        assertThat(mStreamDriver.isZeroStateBeingShown()).isFalse();

        snackbarCallbackApi.onDismissedWithAction();

        verify(mPendingDismissCallback).onDismissReverted();
        leafFeatureDrivers = mStreamDriver.getLeafFeatureDrivers();
        assertThat(leafFeatureDrivers).hasSize(2);
        assertThat(mStreamDriver.isZeroStateBeingShown()).isFalse();
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
        when(mStreamFeature.getCursor()).thenReturn(EMPTY_MODEL_CURSOR);
        mStreamDriver.getLeafFeatureDrivers();

        assertThat(mStreamDriver.isZeroStateBeingShown()).isTrue();
    }

    @Test
    public void testIsZeroStateBeingShown_returnsFalse_whenNoContentCardShowing() {
        FakeModelCursor cursor = FakeModelCursor.newBuilder().addCard().addToken().build();

        // Dismisses all features, but not the token, so the no-content state will be showing.
        initializeStreamDriverAndDismissAllFeatureChildren(cursor);

        assertThat(mStreamDriver.isZeroStateBeingShown()).isFalse();
    }

    @Test
    public void testIsZeroStateBeingShown_returnsFalse_ifZeroStateIsSpinner() {
        when(mStreamFeature.getCursor())
                .thenReturn(FakeModelCursor.newBuilder().addCard().addToken().build());

        mStreamDriver.getLeafFeatureDrivers();

        mStreamDriver.showSpinner();

        assertThat(mStreamDriver.isZeroStateBeingShown()).isFalse();
    }

    @Test
    public void testSetModelProvider_setsModelProviderOnTheZeroState() {
        when(mStreamFeature.getCursor()).thenReturn(EMPTY_MODEL_CURSOR);
        mStreamDriver.getLeafFeatureDrivers();

        mStreamDriver.setModelProviderForZeroState(mModelProvider);

        verify(mZeroStateDriver).setModelProvider(mModelProvider);
    }

    @Test
    public void testRefreshFromZeroState_resultsInZeroState_logsNoNewContent() {
        mStreamDriver = createStreamDriverFromZeroStateRefresh();

        when(mStreamFeature.getCursor()).thenReturn(EMPTY_MODEL_CURSOR);
        mStreamDriver.getLeafFeatureDrivers();

        verify(mBasicLoggingApi)
                .onZeroStateRefreshCompleted(/* newContentCount= */ 0, /* newTokenCount= */ 0);
    }

    @Test
    public void testRefreshFromZeroState_addsContent_logsContent() {
        mStreamDriver = createStreamDriverFromZeroStateRefresh();

        when(mStreamFeature.getCursor())
                .thenReturn(FakeModelCursor.newBuilder().addClusters(10).addToken().build());
        mStreamDriver.getLeafFeatureDrivers();

        verify(mBasicLoggingApi)
                .onZeroStateRefreshCompleted(/* newContentCount= */ 10, /* newTokenCount= */ 1);
    }

    private void initializeStreamDriverAndDismissAllFeatureChildren(
            FakeModelCursor fakeModelCursor) {
        when(mStreamFeature.getCursor()).thenReturn(fakeModelCursor);

        mStreamDriver.getLeafFeatureDrivers();

        FeatureChangeBuilder dismissAllChildrenBuilder = new FeatureChangeBuilder();

        for (ModelChild child : fakeModelCursor.getModelChildren()) {
            if (child.getType() == Type.FEATURE) {
                dismissAllChildrenBuilder.addChildForRemoval(child);
            }
        }

        mStreamDriver.onChange(dismissAllChildrenBuilder.build());
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
                mModelProvider, mThreadUtils, restoring, isInitialLoad, uiRefreshReason);
    }

    private StreamDriverForTest createStreamDriver(boolean restoring, boolean isInitialLoad) {
        return new StreamDriverForTest(mModelProvider, mThreadUtils, restoring, isInitialLoad,
                UiRefreshReason.getDefaultInstance());
    }

    // TODO: Instead of just checking that the ModelFeature is of the correct type, check
    // that it is the one created by the FakeModelCursor.Builder.
    private LeafFeatureDriver getLeafFeatureDriverFromCard(int i) {
        FakeFeatureDriver featureDriver = (FakeFeatureDriver) mStreamDriver.mChildrenCreated.get(i);
        assertThat(featureDriver.getModelFeature().getStreamFeature().hasCard()).isTrue();
        return mStreamDriver.mChildrenCreated.get(i).getLeafFeatureDriver();
    }

    // TODO: Instead of just checking that the ModelFeature is of the correct type, check
    // that it is the one created by the FakeModelCursor.Builder.
    private LeafFeatureDriver getLeafFeatureDriverFromCluster(int i) {
        FakeFeatureDriver featureDriver = (FakeFeatureDriver) mStreamDriver.mChildrenCreated.get(i);
        assertThat(featureDriver.getModelFeature().getStreamFeature().hasCluster()).isTrue();
        return mStreamDriver.mChildrenCreated.get(i).getLeafFeatureDriver();
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
        when(mStreamFeature.getCursor()).thenReturn(cursor.build());
    }

    private SnackbarCallbackApi triggerPendingDismiss(UndoAction undoAction, String contentId) {
        ArgumentCaptor<SnackbarCallbackApi> snackbarCallbackApi =
                ArgumentCaptor.forClass(SnackbarCallbackApi.class);

        // Causes StreamDriver to build a list of children based on the children from the cursor.
        mStreamDriver.getLeafFeatureDrivers();

        mStreamDriver.triggerPendingDismiss(contentId, undoAction, mPendingDismissCallback);

        verify(mContentlistener).notifyContentRemoved(0);
        verify(mSnackbarApi)
                .show(eq(undoAction.getConfirmationLabel()),
                        undoAction.hasUndoLabel() ? eq(undoAction.getUndoLabel())
                                                  : eq(mContext.getResources().getString(
                                                          R.string.snackbar_default_action)),
                        snackbarCallbackApi.capture());

        return snackbarCallbackApi.getValue();
    }

    private class StreamDriverForTest extends StreamDriver {
        // TODO: create a fake for ContinuationDriver so that this can be
        // List<FakeFeatureDriver>
        private List<FeatureDriver> mChildrenCreated;
        private boolean mWasRestoringDuringLastContinuationDriver;

        StreamDriverForTest(ModelProvider modelProvider, ThreadUtils threadUtils, boolean restoring,
                boolean isInitialLoad, UiRefreshReason uiRefreshReason) {
            super(mActionApi, mActionManager, mActionParserFactory, modelProvider, threadUtils,
                    mClock, mConfiguration, mContext, mSnackbarApi, mContentChangedListener,
                    mScrollRestorer, mBasicLoggingApi, mStreamOfflineMonitor, mFeedKnownContent,
                    mock(ContextMenuManager.class), restoring, isInitialLoad, mMainThreadRunner,
                    mViewLoggingUpdater, mTooltipApi, uiRefreshReason, mScrollmonitor);
            mChildrenCreated = new ArrayList<>();
        }

        @Override
        ContinuationDriver createContinuationDriver(BasicLoggingApi basicLoggingApi, Clock clock,
                Configuration configuration, Context context, ModelChild modelChild,
                ModelProvider modelProvider, int position, SnackbarApi snackbarApi,
                boolean restoring) {
            this.mWasRestoringDuringLastContinuationDriver = restoring;
            mChildrenCreated.add(mContinuationDriver);
            return mContinuationDriver;
        }

        @Override
        FeatureDriver createClusterDriver(ModelFeature modelFeature, int position) {
            FeatureDriver featureDriver =
                    new FakeFeatureDriver.Builder().setModelFeature(modelFeature).build();
            mChildrenCreated.add(featureDriver);
            return featureDriver;
        }

        @Override
        FeatureDriver createCardDriver(ModelFeature modelFeature, int position) {
            if (modelFeature != UNBOUND_CARD) {
                FeatureDriver featureDriver =
                        new FakeFeatureDriver.Builder().setModelFeature(modelFeature).build();
                mChildrenCreated.add(featureDriver);
                return featureDriver;
            } else {
                FeatureDriver featureDriver = new FakeFeatureDriver.Builder()
                                                      .setModelFeature(modelFeature)
                                                      .setLeafFeatureDriver(null)
                                                      .build();
                mChildrenCreated.add(featureDriver);
                return featureDriver;
            }
        }

        @Override
        NoContentDriver createNoContentDriver() {
            return mNoContentDriver;
        }

        @Override
        ZeroStateDriver createZeroStateDriver() {
            return mZeroStateDriver;
        }
    }
}
