// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream;

import static com.google.android.libraries.feed.api.client.stream.Stream.POSITION_NOT_KNOWN;
import static com.google.android.libraries.feed.basicstream.BasicStream.KEY_STREAM_STATE;
import static com.google.android.libraries.feed.common.testing.RunnableSubject.assertThatRunnable;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static java.nio.charset.StandardCharsets.UTF_8;

import android.app.Activity;
import android.content.Context;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.Base64;
import android.view.View;
import android.widget.FrameLayout;

import com.google.android.libraries.feed.api.client.knowncontent.ContentMetadata;
import com.google.android.libraries.feed.api.client.knowncontent.KnownContent;
import com.google.android.libraries.feed.api.client.stream.Header;
import com.google.android.libraries.feed.api.client.stream.Stream.ContentChangedListener;
import com.google.android.libraries.feed.api.client.stream.Stream.ScrollListener;
import com.google.android.libraries.feed.api.host.action.ActionApi;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.host.config.DebugBehavior;
import com.google.android.libraries.feed.api.host.imageloader.ImageLoaderApi;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.RequestReason;
import com.google.android.libraries.feed.api.host.logging.ZeroStateShowReason;
import com.google.android.libraries.feed.api.host.offlineindicator.OfflineIndicatorApi;
import com.google.android.libraries.feed.api.host.stream.CardConfiguration;
import com.google.android.libraries.feed.api.host.stream.SnackbarApi;
import com.google.android.libraries.feed.api.host.stream.StreamConfiguration;
import com.google.android.libraries.feed.api.host.stream.TooltipApi;
import com.google.android.libraries.feed.api.internal.actionmanager.ActionManager;
import com.google.android.libraries.feed.api.internal.actionparser.ActionParserFactory;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.knowncontent.FeedKnownContent;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelError;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelError.ErrorType;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.State;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.ViewDepthProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.basicstream.internal.StreamItemAnimator;
import com.google.android.libraries.feed.basicstream.internal.StreamRecyclerViewAdapter;
import com.google.android.libraries.feed.basicstream.internal.StreamSavedInstanceStateProto.StreamSavedInstanceState;
import com.google.android.libraries.feed.basicstream.internal.drivers.StreamDriver;
import com.google.android.libraries.feed.basicstream.internal.scroll.BasicStreamScrollMonitor;
import com.google.android.libraries.feed.basicstream.internal.scroll.ScrollRestorer;
import com.google.android.libraries.feed.basicstream.internal.viewloggingupdater.ViewLoggingUpdater;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.testing.FakeMainThreadRunner;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.piet.PietManager;
import com.google.android.libraries.feed.piet.host.CustomElementProvider;
import com.google.android.libraries.feed.piet.host.HostBindingProvider;
import com.google.android.libraries.feed.sharedstream.contentchanged.StreamContentChangedListener;
import com.google.android.libraries.feed.sharedstream.contextmenumanager.ContextMenuManager;
import com.google.android.libraries.feed.sharedstream.contextmenumanager.ContextMenuManagerImpl;
import com.google.android.libraries.feed.sharedstream.deepestcontenttracker.DeepestContentTracker;
import com.google.android.libraries.feed.sharedstream.offlinemonitor.StreamOfflineMonitor;
import com.google.android.libraries.feed.sharedstream.piet.PietEventLogger;
import com.google.android.libraries.feed.sharedstream.proto.ScrollStateProto.ScrollState;
import com.google.android.libraries.feed.sharedstream.proto.UiRefreshReasonProto.UiRefreshReason;
import com.google.android.libraries.feed.sharedstream.proto.UiRefreshReasonProto.UiRefreshReason.Reason;
import com.google.android.libraries.feed.sharedstream.publicapi.menumeasurer.MenuMeasurer;
import com.google.android.libraries.feed.sharedstream.publicapi.scroll.ScrollObservable;
import com.google.android.libraries.feed.sharedstream.scroll.ScrollListenerNotifier;
import com.google.android.libraries.feed.testing.shadows.ShadowRecycledViewPool;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.search.now.feed.client.StreamDataProto.UiContext;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Tests for {@link BasicStream}. */
@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowRecycledViewPool.class})
public class BasicStreamTest {
    private static final int START_PADDING = 1;
    private static final int END_PADDING = 2;
    private static final int TOP_PADDING = 3;
    private static final int BOTTOM_PADDING = 4;
    private static final long LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS = 1000;
    private static final int ADAPTER_HEADER_COUNT = 5;

    private static final String SESSION_ID = "session-id";
    private static final ScrollState SCROLL_STATE =
            ScrollState.newBuilder().setOffset(10).setPosition(10).build();
    private static final StreamSavedInstanceState SAVED_INSTANCE_STATE =
            StreamSavedInstanceState.newBuilder()
                    .setSessionId(SESSION_ID)
                    .setScrollState(SCROLL_STATE)
                    .build();
    private static final long SPINNER_DELAY_MS = 123L;
    private static final long SPINNER_MINIMUM_SHOW_TIME_MS = 655L;
    private static final Configuration CONFIGURATION =
            new Configuration.Builder()
                    .put(ConfigKey.LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS,
                            LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS)
                    .put(ConfigKey.SPINNER_DELAY_MS, SPINNER_DELAY_MS)
                    .put(ConfigKey.SPINNER_MINIMUM_SHOW_TIME_MS, SPINNER_MINIMUM_SHOW_TIME_MS)
                    .build();

    @Mock
    private StreamConfiguration streamConfiguration;
    @Mock
    private ModelFeature modelFeature;
    @Mock
    private ModelProviderFactory modelProviderFactory;
    @Mock
    private ModelProvider initialModelProvider;
    @Mock
    private ModelProvider modelProvider;
    @Mock
    private ModelProvider restoredModelProvider;
    @Mock
    private PietManager pietManager;
    @Mock
    private SnackbarApi snackbarApi;
    @Mock
    private StreamDriver streamDriver;
    @Mock
    private StreamRecyclerViewAdapter adapter;
    @Mock
    private ScrollListenerNotifier scrollListenerNotifier;
    @Mock
    private ScrollRestorer nonRestoringScrollRestorer;
    @Mock
    private ScrollRestorer scrollRestorer;
    @Mock
    private BasicLoggingApi basicLoggingApi;
    @Mock
    private ContextMenuManagerImpl contextMenuManager;
    @Mock
    private ViewLoggingUpdater viewLoggingUpdater;
    @Mock
    private TooltipApi tooltipApi;

    private FakeFeedKnownContent fakeFeedKnownContent;
    private LinearLayoutManagerWithFakePositioning layoutManager;
    private Context context;
    private FakeClock clock;
    private BasicStreamForTest basicStream;
    private FakeMainThreadRunner mainThreadRunner;
    private List<Header> headers;

    @Before
    public void setUp() {
        initMocks(this);

        fakeFeedKnownContent = new FakeFeedKnownContent();
        headers = new ArrayList<>();
        headers.add(mock(Header.class));

        // TODO: Move header orchestration into separate class.
        // Purposely using a different header count here as it is possible for size of headers to
        // change due to swipe to dismiss.  Adapter is source of truth for headers right now.
        // Ideally we should have a drivers specifically for header management but we don't just
        // yet.
        when(adapter.getHeaderCount()).thenReturn(ADAPTER_HEADER_COUNT);

        when(streamConfiguration.getPaddingStart()).thenReturn(START_PADDING);
        when(streamConfiguration.getPaddingEnd()).thenReturn(END_PADDING);
        when(streamConfiguration.getPaddingTop()).thenReturn(TOP_PADDING);
        when(streamConfiguration.getPaddingBottom()).thenReturn(BOTTOM_PADDING);

        when(modelProviderFactory.createNew(any(ViewDepthProvider.class), any(UiContext.class)))
                .thenReturn(initialModelProvider, modelProvider);

        when(initialModelProvider.getSessionId()).thenReturn(SESSION_ID);

        when(scrollRestorer.getScrollStateForScrollRestore(ADAPTER_HEADER_COUNT))
                .thenReturn(SCROLL_STATE);

        when(streamDriver.getLeafFeatureDrivers()).thenReturn(Collections.emptyList());

        context = Robolectric.buildActivity(Activity.class).get();
        clock = new FakeClock();
        mainThreadRunner = FakeMainThreadRunner.create(clock);
        layoutManager = new LinearLayoutManagerWithFakePositioning(context);

        basicStream = createBasicStream(layoutManager);
        basicStream.onCreate((Bundle) null);
    }

    @Test
    public void testRecyclerViewSetup() {
        assertThat(getStreamRecyclerView().getId()).isEqualTo(R.id.feed_stream_recycler_view);
    }

    @Test
    public void testOnSessionStart() {
        basicStream.onShow();
        reset(adapter);

        basicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(streamDriver).onDestroy();
        verify(adapter).setDriver(streamDriver);
        assertThat(basicStream.streamDriverScrollRestorer).isSameInstanceAs(scrollRestorer);
    }

    @Test
    public void testOnSessionStart_logsOnOpenedWithStreamContentAfterOnShow() {
        clock = clock.set(10);
        basicStream.onShow();

        when(streamDriver.hasContent()).thenReturn(true);
        clock = clock.set(40);
        basicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(basicLoggingApi).onOpenedWithContent(30, 0);
    }

    @Test
    public void testOnSessionStart_logsOnOpenedWithStreamContentAfterOnShow_whenRestoring() {
        basicStream.onShow();

        String savedInstanceState = basicStream.getSavedInstanceStateString();

        basicStream.onHide();
        basicStream.onDestroy();

        when(modelProviderFactory.create(SESSION_ID, UiContext.getDefaultInstance()))
                .thenReturn(restoredModelProvider);

        basicStream = createBasicStream(new LinearLayoutManager(context));

        basicStream.onCreate(savedInstanceState);

        clock.set(15L);
        basicStream.onShow();

        when(streamDriver.hasContent()).thenReturn(true);
        clock.advance(5L);
        basicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(basicLoggingApi).onOpenedWithContent(5, 0);
    }

    @Test
    public void testOnSessionStart_doesNotLogOnOpenedWithStreamContentAfterInitialOnShow() {
        basicStream.onShow();
        basicStream.onSessionStart(UiContext.getDefaultInstance());
        reset(basicLoggingApi);

        basicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(basicLoggingApi, never()).onOpenedWithContent(anyInt(), anyInt());
    }

    @Test
    public void testOnSessionStart_doesNotLogOnOpenedWithStreamContent_IfOnErrorLogsNoContent() {
        basicStream.onShow();
        basicStream.onError(
                new ModelError(ErrorType.NO_CARDS_ERROR, /* continuationToken= */ null));
        reset(basicLoggingApi);

        basicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(basicLoggingApi, never()).onOpenedWithContent(anyInt(), anyInt());
    }

    @Test
    public void testOnSessionStart_logsOnOpenedWithNoContent_ifStreamDriverDoesNotHaveContent() {
        basicStream.onShow();

        basicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(basicLoggingApi).onOpenedWithNoContent();
    }

    @Test
    public void
    testOnSessionStart_doesNotUseNewStreamDriver_ifBothStreamDriversAreShowingZeroState() {
        StreamDriver newStreamDriver = mock(StreamDriver.class);
        basicStream.onShow();
        basicStream.onError(
                new ModelError(ErrorType.NO_CARDS_ERROR, /* continuationToken= */ null));
        reset(adapter, streamDriver);
        when(streamDriver.isZeroStateBeingShown()).thenReturn(true);
        when(newStreamDriver.isZeroStateBeingShown()).thenReturn(true);

        basicStream.streamDriver = newStreamDriver;
        basicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(streamDriver).setModelProviderForZeroState(initialModelProvider);
        verify(streamDriver, never()).onDestroy();
        verify(newStreamDriver).onDestroy();
        verify(adapter, never()).setDriver(any(StreamDriver.class));
    }

    @Test
    public void testOnSessionFinished() {
        basicStream.onShow();
        reset(streamDriver);
        basicStream.onSessionFinished(UiContext.getDefaultInstance());

        verify(scrollRestorer).abandonRestoringScroll();
        verify(initialModelProvider).unregisterObserver(basicStream);
        verify(modelProviderFactory, times(2))
                .createNew(any(ViewDepthProvider.class), any(UiContext.class));
        verify(modelProvider).registerObserver(basicStream);
    }

    @Test
    public void testOnError_showsZeroState() {
        basicStream.onShow();
        reset(streamDriver);

        basicStream.onError(
                new ModelError(ErrorType.NO_CARDS_ERROR, /* continuationToken= */ null));

        verify(scrollRestorer).abandonRestoringScroll();
        verify(streamDriver).showZeroState(ZeroStateShowReason.ERROR);
    }

    @Test
    public void testOnError_logsOnOpenedWithNoContent() {
        when(streamDriver.hasContent()).thenReturn(false);
        clock = clock.set(10);
        basicStream.onShow();

        clock = clock.set(10 + LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS);
        basicStream.onError(
                new ModelError(ErrorType.NO_CARDS_ERROR, /* continuationToken= */ null));

        verify(basicLoggingApi).onOpenedWithNoContent();
    }

    @Test
    public void testOnError_doesNotDoubleLogOnOpenedWithNoContent() {
        when(initialModelProvider.getCurrentState()).thenReturn(State.READY);
        when(initialModelProvider.getRootFeature()).thenReturn(null);

        basicStream.onShow();
        // Trigger onOpenedWithNoContent logging through updating the driver.
        basicStream.onSessionStart(UiContext.getDefaultInstance());
        reset(basicLoggingApi);

        basicStream.onError(
                new ModelError(ErrorType.NO_CARDS_ERROR, /* continuationToken= */ null));

        verify(basicLoggingApi, never()).onOpenedWithNoContent();
    }

    @Test
    public void testOnSessionStart_logsOnOpenedWithNoImmediateContent() {
        basicStream.onShow();

        // Advance so that the spinner starts showing
        clock.advance(SPINNER_DELAY_MS);

        // Advance so that is has taken long enough that onOpenedWithNoImmediateContent is logged.
        clock.advance(LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS);

        basicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(basicLoggingApi).onOpenedWithNoImmediateContent();
    }

    @Test
    public void testOnSessionStart_doesNotLogOnOpenedWithNoImmediateContent_ifNotWithinThreshold() {
        basicStream.onShow();

        clock.advance(LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS - 1);

        basicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(basicLoggingApi, never()).onOpenedWithNoImmediateContent();
    }

    @Test
    public void testOnSessionStart_logsOnOpenedWithNoContent() {
        when(initialModelProvider.getCurrentState()).thenReturn(State.READY);
        when(initialModelProvider.getRootFeature()).thenReturn(null);
        basicStream.onShow();

        basicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(basicLoggingApi).onOpenedWithNoContent();
    }

    @Test
    public void testOnShow_doesNotLogOnOpenedWithNoContent_ifModelProviderNotReady() {
        when(initialModelProvider.getCurrentState()).thenReturn(State.INITIALIZING);
        when(initialModelProvider.getRootFeature()).thenReturn(null);

        basicStream.onShow();

        verify(basicLoggingApi, never()).onOpenedWithNoContent();
    }

    @Test
    public void testOnShow_doesNotLogOnOpenedWithNoContent_ifRootFeatureNotNull() {
        when(initialModelProvider.getCurrentState()).thenReturn(State.READY);
        when(initialModelProvider.getRootFeature()).thenReturn(modelFeature);

        basicStream.onShow();

        verify(basicLoggingApi, never()).onOpenedWithNoContent();
    }

    @Test
    public void testOnSessionStart_doesNotDoubleLogOnOpenedWithNoContent() {
        when(initialModelProvider.getCurrentState()).thenReturn(State.READY);
        when(initialModelProvider.getRootFeature()).thenReturn(null);

        basicStream.onShow();
        // Trigger onOpenedWithNoContent logging through onError.
        basicStream.onError(
                new ModelError(ErrorType.NO_CARDS_ERROR, /* continuationToken= */ null));
        reset(basicLoggingApi);

        basicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(basicLoggingApi, never()).onOpenedWithNoContent();
    }

    @Test
    public void testOnSessionStart_doesNotLogOnOpenedWithNoImmediateContentAfterInitialOnShow() {
        basicStream.onShow();
        basicStream.onSessionStart(UiContext.getDefaultInstance());
        reset(basicLoggingApi);

        basicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(basicLoggingApi, never()).onOpenedWithNoImmediateContent();
    }

    @Test
    public void testOnError_doesNotShowZeroState() {
        basicStream.onShow();

        assertThatRunnable(()
                                   -> basicStream.onError(new ModelError(ErrorType.PAGINATION_ERROR,
                                           /* continuationToken= */ null)))
                .throwsAnExceptionOfType(RuntimeException.class);
    }

    @Test
    public void testLifecycle_onCreateWithBundleCalledOnlyOnce() {
        // onCreate is called once in setup
        assertThatRunnable(() -> basicStream.onCreate(new Bundle()))
                .throwsAnExceptionOfType(IllegalStateException.class);
    }

    @Test
    public void testLifecycle_onCreateWithStringCalledOnlyOnce() {
        // onCreate is called once in setup
        assertThatRunnable(() -> basicStream.onCreate(""))
                .throwsAnExceptionOfType(IllegalStateException.class);
    }

    @Test
    public void testLifecycle_getViewBeforeOnCreateCrashes() {
        // create BasicStream that has not had onCreate() called.
        basicStream = createBasicStream(new LinearLayoutManagerWithFakePositioning(context));
        assertThatRunnable(() -> basicStream.getView())
                .throwsAnExceptionOfType(IllegalStateException.class);
    }

    @Test
    public void testLifecycle_onCreate_onDestroy() {
        basicStream.onDestroy();
        verify(initialModelProvider, never()).invalidate();
    }

    @Test
    public void testLifecycle_onCreate_onShow_onHide_onDestroy() {
        basicStream.onShow();
        basicStream.onHide();
        basicStream.onDestroy();
        verify(adapter).onDestroy();
        verify(initialModelProvider, never()).invalidate();
        verify(initialModelProvider).detachModelProvider();
        assertThat(fakeFeedKnownContent.listeners).isEmpty();
    }

    @Test
    public void testOnDestroy_destroysStreamDriver() {
        basicStream.onShow();
        basicStream.onSessionStart(UiContext.getDefaultInstance());
        reset(streamDriver);

        basicStream.onDestroy();

        verify(streamDriver).onDestroy();
    }

    @Test
    public void testOnDestroy_unregistersOnLayoutChangeListener() {
        // initial layout from 0, 0 will not rebind.
        getStreamRecyclerView().layout(0, 0, 100, 300);
        // change the width / height to simulate device rotation
        getStreamRecyclerView().layout(0, 0, 300, 100);
        verify(adapter).rebind();

        reset(adapter);
        basicStream.onDestroy();

        // change the width / height to simulate device rotation
        getStreamRecyclerView().layout(0, 0, 100, 300);
        verify(adapter, never()).rebind();
    }

    @Test
    public void testOnDestroy_deregistersSessionListener() {
        basicStream.onShow();

        basicStream.onDestroy();

        // Once for BasicStream, once for the session listener.
        verify(initialModelProvider, times(2)).unregisterObserver(any());
    }

    @Test
    public void testGetSavedInstanceState() {
        basicStream.onShow();

        Bundle bundle = basicStream.getSavedInstanceState();
        assertThat(bundle.getString(KEY_STREAM_STATE))
                .isEqualTo(
                        Base64.encodeToString(SAVED_INSTANCE_STATE.toByteArray(), Base64.DEFAULT));
    }

    @Test
    public void testGetSavedInstanceStateString_beforeShow() throws InvalidProtocolBufferException {
        StreamSavedInstanceState savedInstanceState = StreamSavedInstanceState.parseFrom(
                decodeSavedInstanceStateString(basicStream.getSavedInstanceStateString()));
        assertThat(savedInstanceState.hasSessionId()).isFalse();
        assertThat(savedInstanceState.getScrollState()).isEqualTo(SCROLL_STATE);
    }

    @Test
    public void testGetSavedInstanceStateString_afterOnShow_beforeSessionStart()
            throws InvalidProtocolBufferException {
        when(initialModelProvider.getSessionId()).thenReturn(null);

        basicStream.onShow();

        StreamSavedInstanceState savedInstanceState = StreamSavedInstanceState.parseFrom(
                decodeSavedInstanceStateString(basicStream.getSavedInstanceStateString()));
        assertThat(savedInstanceState.hasSessionId()).isFalse();
        assertThat(savedInstanceState.getScrollState()).isEqualTo(SCROLL_STATE);
    }

    @Test
    public void testGetSavedInstanceStateString_afterOnShow_afterSessionStart()
            throws InvalidProtocolBufferException {
        basicStream.onShow();

        StreamSavedInstanceState savedInstanceState = StreamSavedInstanceState.parseFrom(
                decodeSavedInstanceStateString(basicStream.getSavedInstanceStateString()));
        assertThat(savedInstanceState.getSessionId()).isEqualTo(SESSION_ID);
        assertThat(savedInstanceState.getScrollState()).isEqualTo(SCROLL_STATE);
    }

    @Test
    public void testGetSavedInstanceStateString_noScrollRestoreBundle()
            throws InvalidProtocolBufferException {
        basicStream.onShow();

        when(scrollRestorer.getScrollStateForScrollRestore(ADAPTER_HEADER_COUNT)).thenReturn(null);
        StreamSavedInstanceState savedInstanceState = StreamSavedInstanceState.parseFrom(
                decodeSavedInstanceStateString(basicStream.getSavedInstanceStateString()));
        assertThat(savedInstanceState.hasScrollState()).isFalse();
    }

    @Test
    public void testRestore() {
        basicStream.onShow();

        Bundle bundle = basicStream.getSavedInstanceState();

        basicStream.onHide();
        basicStream.onDestroy();

        when(modelProviderFactory.create(SESSION_ID, UiContext.getDefaultInstance()))
                .thenReturn(restoredModelProvider);

        basicStream = createBasicStream(new LinearLayoutManagerWithFakePositioning(context));
        basicStream.onCreate(bundle);

        basicStream.onShow();

        verify(restoredModelProvider).registerObserver(basicStream);
    }

    @Test
    public void testRestore_withStringSavedState() {
        basicStream.onShow();

        String savedInstanceState = basicStream.getSavedInstanceStateString();

        basicStream.onHide();
        basicStream.onDestroy();

        when(modelProviderFactory.create(SESSION_ID, UiContext.getDefaultInstance()))
                .thenReturn(restoredModelProvider);

        basicStream = createBasicStream(new LinearLayoutManagerWithFakePositioning(context));
        basicStream.onCreate(savedInstanceState);

        basicStream.onShow();

        verify(restoredModelProvider).registerObserver(basicStream);
    }

    @Test
    public void testRestore_doesNotShowZeroState() {
        basicStream.onShow();

        Bundle bundle = basicStream.getSavedInstanceState();

        basicStream.onHide();
        basicStream.onDestroy();

        when(modelProviderFactory.create(SESSION_ID, UiContext.getDefaultInstance()))
                .thenReturn(restoredModelProvider);

        reset(streamDriver);
        basicStream = createBasicStream(new LinearLayoutManagerWithFakePositioning(context));
        basicStream.onCreate(bundle);

        basicStream.onShow();

        verify(streamDriver, never()).showZeroState(/* zeroStateShowReason= */ anyInt());
        verify(streamDriver, never()).showSpinner();
    }

    @Test
    public void testRestore_showsZeroStateIfNoSessionToRestore() {
        basicStream = createBasicStream(new LinearLayoutManagerWithFakePositioning(context));
        basicStream.onCreate(Bundle.EMPTY);

        basicStream.onShow();

        verify(streamDriver, never()).showSpinner();

        clock.advance(SPINNER_DELAY_MS);

        verify(streamDriver).showSpinner();
    }

    @Test
    public void testRestore_invalidSession() {
        basicStream.onShow();

        Bundle bundle = basicStream.getSavedInstanceState();

        basicStream.onHide();
        basicStream.onDestroy();

        basicStream = createBasicStream(new LinearLayoutManagerWithFakePositioning(context));
        basicStream.onCreate(bundle);
        basicStream.onShow();

        verify(modelProvider).registerObserver(basicStream);
    }

    @Test
    public void testRestore_invalidBase64Encoding() {
        basicStream.onShow();

        Bundle bundle = new Bundle();
        bundle.putString(KEY_STREAM_STATE, "=invalid");

        basicStream.onHide();
        basicStream.onDestroy();

        basicStream = createBasicStream(new LinearLayoutManagerWithFakePositioning(context));
        assertThatRunnable(() -> basicStream.onCreate(bundle))
                .throwsAnExceptionOfType(RuntimeException.class);
    }

    @Test
    public void testRestore_invalidProtocolBuffer() {
        basicStream.onShow();

        Bundle bundle = new Bundle();
        bundle.putString(
                KEY_STREAM_STATE, Base64.encodeToString("invalid".getBytes(UTF_8), Base64.DEFAULT));

        basicStream.onHide();
        basicStream.onDestroy();

        basicStream = createBasicStream(new LinearLayoutManagerWithFakePositioning(context));
        assertThatRunnable(() -> basicStream.onCreate(bundle))
                .throwsAnExceptionOfType(RuntimeException.class);
    }

    @Test
    public void testRestore_createsStreamDriver() {
        basicStream.onShow();

        Bundle bundle = basicStream.getSavedInstanceState();

        basicStream.onHide();
        basicStream.onDestroy();

        when(modelProviderFactory.create(SESSION_ID, UiContext.getDefaultInstance()))
                .thenReturn(restoredModelProvider);

        basicStream = createBasicStream(new LinearLayoutManagerWithFakePositioning(context));
        basicStream.onCreate(bundle);

        basicStream.onShow();

        assertThat(basicStream.streamDriverRestoring).isTrue();
    }

    @Test
    public void testRestore_createsStreamDriver_afterFailure() {
        basicStream.onShow();

        Bundle bundle = basicStream.getSavedInstanceState();

        basicStream.onHide();
        basicStream.onDestroy();

        when(modelProviderFactory.create(SESSION_ID, UiContext.getDefaultInstance()))
                .thenReturn(restoredModelProvider);

        basicStream = createBasicStream(new LinearLayoutManagerWithFakePositioning(context));
        basicStream.onCreate(bundle);

        // onSessionFinish indicates the restore has failed.
        basicStream.onSessionFinished(UiContext.getDefaultInstance());

        basicStream.onShow();

        assertThat(basicStream.streamDriverRestoring).isFalse();
    }

    @Test
    @Config(sdk = VERSION_CODES.JELLY_BEAN)
    public void testPadding_jellyBean() {
        // Padding is setup in constructor.
        View view = basicStream.getView();

        assertThat(view.getPaddingLeft()).isEqualTo(START_PADDING);
        assertThat(view.getPaddingRight()).isEqualTo(END_PADDING);
        assertThat(view.getPaddingTop()).isEqualTo(TOP_PADDING);
        assertThat(view.getPaddingBottom()).isEqualTo(BOTTOM_PADDING);
    }

    @Test
    @Config(sdk = VERSION_CODES.KITKAT)
    public void testPadding_kitKat() {
        // Padding is setup in constructor.
        View view = basicStream.getView();

        assertThat(view.getPaddingStart()).isEqualTo(START_PADDING);
        assertThat(view.getPaddingEnd()).isEqualTo(END_PADDING);
        assertThat(view.getPaddingTop()).isEqualTo(TOP_PADDING);
        assertThat(view.getPaddingBottom()).isEqualTo(BOTTOM_PADDING);
    }

    @Test
    public void testTrim() {
        ShadowRecycledViewPool viewPool =
                Shadow.extract(getStreamRecyclerView().getRecycledViewPool());

        // RecyclerView ends up clearing the pool initially when the adapter is set on the
        // RecyclerView. Verify that has happened before anything else
        assertThat(viewPool.getClearCallCount()).isEqualTo(1);

        basicStream.trim();

        verify(pietManager).purgeRecyclerPools();

        // We expect the clear() call to be 2 as RecyclerView ends up clearing the pool initially
        // when the adapter is set on the RecyclerView.  So one call for that and one call for
        // trim() call on stream.
        assertThat(viewPool.getClearCallCount()).isEqualTo(2);
    }

    @Test
    public void testLayoutChanges() {
        // initial layout from 0, 0 will not rebind.
        getStreamRecyclerView().layout(0, 0, 100, 300);
        verify(adapter, never()).rebind();
        // change the width / height to simulate device rotation
        getStreamRecyclerView().layout(0, 0, 300, 100);
        verify(adapter).rebind();
    }

    @Test
    public void testAddScrollListener() {
        ScrollListener scrollListener = mock(ScrollListener.class);
        basicStream.addScrollListener(scrollListener);
        verify(scrollListenerNotifier).addScrollListener(scrollListener);
    }

    @Test
    public void testRemoveScrollListener() {
        ScrollListener scrollListener = mock(ScrollListener.class);
        basicStream.removeScrollListener(scrollListener);
        verify(scrollListenerNotifier).removeScrollListener(scrollListener);
    }

    @Test
    public void testIsChildAtPositionVisible() {
        layoutManager.firstVisiblePosition = 0;
        layoutManager.lastVisiblePosition = 1;
        assertThat(basicStream.isChildAtPositionVisible(-2)).isFalse();
        assertThat(basicStream.isChildAtPositionVisible(-1)).isFalse();
        assertThat(basicStream.isChildAtPositionVisible(0)).isTrue();
        assertThat(basicStream.isChildAtPositionVisible(1)).isTrue();
        assertThat(basicStream.isChildAtPositionVisible(2)).isFalse();
    }

    @Test
    public void testIsChildAtPositionVisible_nothingVisible() {
        assertThat(basicStream.isChildAtPositionVisible(0)).isFalse();
    }

    @Test
    public void testIsChildAtPositionVisible_validTop() {
        layoutManager.firstVisiblePosition = 0;
        assertThat(basicStream.isChildAtPositionVisible(0)).isFalse();
    }

    @Test
    public void testIsChildAtPositionVisible_validBottom() {
        layoutManager.lastVisiblePosition = 1;
        assertThat(basicStream.isChildAtPositionVisible(0)).isFalse();
    }

    @Test
    public void testGetChildTopAt_noVisibleChild() {
        assertThat(basicStream.getChildTopAt(0)).isEqualTo(POSITION_NOT_KNOWN);
    }

    @Test
    public void testGetChildTopAt_noChild() {
        layoutManager.firstVisiblePosition = 0;
        layoutManager.lastVisiblePosition = 1;
        assertThat(basicStream.getChildTopAt(0)).isEqualTo(POSITION_NOT_KNOWN);
    }

    @Test
    public void testGetChildTopAt() {
        layoutManager.firstVisiblePosition = 0;
        layoutManager.lastVisiblePosition = 1;
        View view = new FrameLayout(context);
        layoutManager.addChildToPosition(0, view);

        assertThat(basicStream.getChildTopAt(0)).isEqualTo(view.getTop());
    }

    @Test
    public void testStreamContentVisible() {
        basicStream.setStreamContentVisibility(false);
        verify(adapter).setStreamContentVisible(false);

        reset(adapter);
        basicStream.setStreamContentVisibility(true);
        verify(adapter).setStreamContentVisible(true);
    }

    @Test
    public void testStreamContentVisible_notifiesItemAnimator_notVisible() {
        basicStream.setStreamContentVisibility(false);

        assertThat(((StreamItemAnimator) getStreamRecyclerView().getItemAnimator())
                           .getStreamContentVisibility())
                .isFalse();
    }

    @Test
    public void testStreamContentVisible_notifiesItemAnimator_visible() {
        basicStream.setStreamContentVisibility(true);

        assertThat(((StreamItemAnimator) getStreamRecyclerView().getItemAnimator())
                           .getStreamContentVisibility())
                .isTrue();
    }

    @Test
    public void testSetStreamContentVisibility_createsModelProvider_ifContentNotVisible() {
        basicStream.setStreamContentVisibility(false);
        basicStream.onShow();

        verifyNoMoreInteractions(modelProvider);

        basicStream.setStreamContentVisibility(true);

        verify(modelProviderFactory).createNew(any(ViewDepthProvider.class), any(UiContext.class));
    }

    @Test
    public void testSetStreamContentVisibility_resetsViewLogging() {
        basicStream.setStreamContentVisibility(false);
        basicStream.onShow();

        basicStream.setStreamContentVisibility(true);

        verify(viewLoggingUpdater).resetViewTracking();
    }

    @Test
    public void testSetStreamContentVisibility_trueMultipleTimes_doesNotResetViewLogging() {
        basicStream.setStreamContentVisibility(true);
        basicStream.onShow();

        basicStream.setStreamContentVisibility(true);

        verify(viewLoggingUpdater, never()).resetViewTracking();
    }

    @Test
    public void testTriggerRefresh() {
        basicStream.onShow();

        reset(adapter, streamDriver);

        basicStream.triggerRefresh();

        verify(initialModelProvider).triggerRefresh(RequestReason.HOST_REQUESTED);
        verify(streamDriver).showSpinner();
    }

    @Test
    public void testTriggerRefresh_beforeOnShow() {
        basicStream.triggerRefresh();

        verify(initialModelProvider, never()).triggerRefresh(anyInt());
        verify(streamDriver, never()).showSpinner();
    }

    @Test
    public void testAddOnContentChangedListener() {
        ContentChangedListener contentChangedListener = mock(ContentChangedListener.class);

        basicStream.addOnContentChangedListener(contentChangedListener);
        basicStream.contentChangedListener.onContentChanged();

        verify(contentChangedListener).onContentChanged();
    }

    @Test
    public void testRemoveOnContentChangedListener() {
        ContentChangedListener contentChangedListener = mock(ContentChangedListener.class);

        basicStream.addOnContentChangedListener(contentChangedListener);
        basicStream.removeOnContentChangedListener(contentChangedListener);

        basicStream.contentChangedListener.onContentChanged();

        verify(contentChangedListener, never()).onContentChanged();
    }

    @Test
    public void testOnShow_nonRestoringRestorer() {
        basicStream.onShow();

        assertThat(basicStream.streamDriverScrollRestorer).isEqualTo(nonRestoringScrollRestorer);
    }

    @Test
    public void testOnShow_restoringScrollRestorer() {
        when(initialModelProvider.getCurrentState()).thenReturn(State.READY);
        basicStream.onShow();

        assertThat(basicStream.streamDriverScrollRestorer).isEqualTo(scrollRestorer);
    }

    @Test
    public void testOnShow_setShown() {
        basicStream.onShow();

        verify(adapter).setShown(true);
    }

    @Test
    public void testOnShow_doesNotCreateModelProvider_ifStreamContentNotVisible() {
        basicStream.setStreamContentVisibility(false);

        basicStream.onShow();

        verifyNoMoreInteractions(modelProviderFactory);
        verify(adapter, never()).setDriver(any(StreamDriver.class));
    }

    @Test
    public void testOnShow_restoresScrollPosition_ifStreamContentNotVisible() {
        basicStream.setStreamContentVisibility(false);

        basicStream.onShow();

        verify(scrollRestorer).maybeRestoreScroll();
    }

    @Test
    public void testOnSessionStart_showsContentImmediately_ifNotInitialLoad() {
        basicStream.onShow();
        basicStream.onSessionStart(UiContext.getDefaultInstance());
        reset(adapter);

        basicStream.onSessionStart(UiContext.getDefaultInstance());
        verify(adapter).setDriver(streamDriver);
    }

    @Test
    public void testOnSessionStart_showsContentImmediately_ifSpinnerTimeElapsed() {
        basicStream.onShow();
        reset(adapter);

        // Advance so the spinner is shown.
        clock.advance(SPINNER_DELAY_MS);

        // Advance so that it has shown long enough
        clock.advance(SPINNER_MINIMUM_SHOW_TIME_MS);

        basicStream.onSessionStart(UiContext.getDefaultInstance());

        // The adapter should be immediately set in onSesssionStart
        verify(adapter).setDriver(streamDriver);
    }

    @Test
    public void testOnSessionStart_showsContentWithDelay_ifSpinnerTimeNotElapsed() {
        basicStream.onShow();

        // Advance so the spinner is shown.
        clock.advance(SPINNER_DELAY_MS);

        reset(adapter);

        basicStream.onSessionStart(UiContext.getDefaultInstance());
        verify(adapter, never()).setDriver(any(StreamDriver.class));

        // Advance so that it has shown long enough.
        clock.advance(SPINNER_MINIMUM_SHOW_TIME_MS);

        verify(adapter).setDriver(streamDriver);
    }

    @Test
    public void testOnSessionStart_doesNotShowContent_ifSessionFinishesBeforeSpinnerTimeElapsed() {
        clock.set(10);

        basicStream.onShow();

        reset(adapter);

        // Advance so the spinner is shown.
        clock.advance(SPINNER_DELAY_MS);

        basicStream.onSessionStart(UiContext.getDefaultInstance());
        basicStream.onSessionFinished(UiContext.getDefaultInstance());

        // Advance so that it has shown long enough.
        clock.advance(SPINNER_MINIMUM_SHOW_TIME_MS);

        verify(adapter, never()).setDriver(streamDriver);
    }

    @Test
    public void testOnSessionStart_showsZeroState_ifFeatureDriversEmpty() {
        basicStream.onShow();

        basicStream.onSessionStart(UiContext.getDefaultInstance());

        verify(streamDriver).showZeroState(ZeroStateShowReason.NO_CONTENT);
    }

    @Test
    public void testOnShow_delaysShowingZeroState_onInitialLoad() {
        basicStream.onShow();

        verify(streamDriver, never()).showZeroState(/* zeroStateShowReason= */ anyInt());
        verify(streamDriver, never()).showSpinner();

        // Advance so the spinner is shown.
        clock.advance(SPINNER_DELAY_MS);

        verify(streamDriver).showSpinner();
    }

    @Test
    public void testOnShow_delaysShowingZeroState_configured_delay_time() {
        basicStream.onShow();

        verify(streamDriver, never()).showZeroState(/* zeroStateShowReason= */ anyInt());
        verify(streamDriver, never()).showSpinner();

        // Advance so the spinner is shown.
        clock.advance(SPINNER_DELAY_MS);

        verify(streamDriver).showSpinner();
    }

    @Test
    public void testOnHide_setShown() {
        basicStream.onShow();
        reset(adapter);

        basicStream.onHide();

        verify(adapter).setShown(false);
    }

    @Test
    public void testOnHide_dismissesPopup() {
        basicStream.onHide();
        verify(contextMenuManager).dismissPopup();
    }

    @Test
    public void onSessionFinished_afterOnDestroy_unregistersOnce() {
        basicStream.onShow();

        basicStream.onDestroy();
        basicStream.onSessionFinished(UiContext.getDefaultInstance());

        // Should only occur once between onDestroy() and onSessionFinished()
        verify(initialModelProvider).unregisterObserver(basicStream);
    }

    @Test
    public void onDestroyTwice_unregistersOnce() {
        basicStream.onShow();

        basicStream.onDestroy();
        basicStream.onDestroy();

        // Should only occur once between multiple onDestroy() calls
        verify(initialModelProvider).unregisterObserver(basicStream);
    }

    @Test
    public void onSessionStart_propagatesUiContext() {
        basicStream.onShow();

        UiRefreshReason uiRefreshReason =
                UiRefreshReason.newBuilder().setReason(Reason.ZERO_STATE).build();
        basicStream.onSessionStart(UiContext.getDefaultInstance());
        basicStream.onSessionFinished(
                UiContext.newBuilder()
                        .setExtension(UiRefreshReason.uiRefreshReasonExtension, uiRefreshReason)
                        .build());
        basicStream.onSessionStart(UiContext.getDefaultInstance());

        assertThat(basicStream.streamDriverUiRefreshReason).isEqualTo(uiRefreshReason);
    }

    @Test
    public void onShow_whileStreamContentNotVisible_logsOpenedWithNoContent() {
        basicStream.setStreamContentVisibility(false);

        basicStream.onShow();

        verify(basicLoggingApi).onOpenedWithNoContent();
    }

    @Test
    public void onShow_whileStreamContentNotVisible_twice_logsOpenedWithNoContentOnce() {
        basicStream.setStreamContentVisibility(false);

        basicStream.onShow();
        basicStream.onHide();
        basicStream.onShow();

        verify(basicLoggingApi).onOpenedWithNoContent();
    }

    @Test
    public void failToRestoreSession_doesntShowSpinnerImmediately() {
        basicStream.onShow();

        basicStream.onSessionFinished(UiContext.getDefaultInstance());

        verify(streamDriver, never()).showSpinner();
    }

    @Test
    public void failToRestoreSession_showsSpinnerAfterDelay() {
        basicStream.onShow();

        basicStream.onSessionFinished(UiContext.getDefaultInstance());

        clock.advance(SPINNER_DELAY_MS);

        verify(streamDriver).showSpinner();
    }

    @Test
    public void startSession_quicklyFinish_doesntShowInitialSpinner() {
        basicStream.onShow();

        // Advance, but not enough so that the spinner shows.
        clock.advance(SPINNER_DELAY_MS / 2);

        // Start the session. At this point, the spinner enqueued by onShow() should never be shown.
        basicStream.onSessionStart(UiContext.getDefaultInstance());

        // Finish the session immediately. As we had started a session and the user had presumably
        // seen content, we shouldn't trigger the spinner enqueued by onShow().
        basicStream.onSessionFinished(UiContext.getDefaultInstance());

        // It has now been long enough since onShow that the spinner would show if onSessionStart()
        // hadn't been called first.
        clock.advance(SPINNER_DELAY_MS / 2 + 1);

        verify(streamDriver, never()).showSpinner();
    }

    @Test
    public void startSession_thenFinish_showsSpinnerAfterDelay() {
        basicStream.onShow();

        basicStream.onSessionStart(UiContext.getDefaultInstance());
        basicStream.onSessionFinished(UiContext.getDefaultInstance());

        verify(streamDriver, never()).showSpinner();

        clock.advance(SPINNER_DELAY_MS);

        verify(streamDriver).showSpinner();
    }

    private byte[] decodeSavedInstanceStateString(String savedInstanceState) {
        return Base64.decode(savedInstanceState, Base64.DEFAULT);
    }

    private RecyclerView getStreamRecyclerView() {
        return (RecyclerView) basicStream.getView();
    }

    private BasicStreamForTest createBasicStream(LinearLayoutManager layoutManager) {
        return new BasicStreamForTest(context, streamConfiguration, mock(CardConfiguration.class),
                mock(ImageLoaderApi.class), mock(ActionParserFactory.class), mock(ActionApi.class),
                mock(CustomElementProvider.class), DebugBehavior.VERBOSE, new ThreadUtils(),
                headers, clock, modelProviderFactory, new HostBindingProvider(),
                mock(ActionManager.class), CONFIGURATION, layoutManager,
                mock(OfflineIndicatorApi.class), streamDriver);
    }

    private class BasicStreamForTest extends BasicStream {
        private final LinearLayoutManager layoutManager;
        private StreamDriver streamDriver;
        private boolean streamDriverRestoring;

        private ScrollRestorer streamDriverScrollRestorer;
        private StreamContentChangedListener contentChangedListener;
        private UiRefreshReason streamDriverUiRefreshReason;

        public BasicStreamForTest(Context context, StreamConfiguration streamConfiguration,
                CardConfiguration cardConfiguration, ImageLoaderApi imageLoaderApi,
                ActionParserFactory actionParserFactory, ActionApi actionApi,
                /*@Nullable*/ CustomElementProvider customElementProvider,
                DebugBehavior debugBehavior, ThreadUtils threadUtils, List<Header> headers,
                Clock clock, ModelProviderFactory modelProviderFactory,
                /*@Nullable*/ HostBindingProvider hostBindingProvider, ActionManager actionManager,
                Configuration configuration, LinearLayoutManager layoutManager,
                OfflineIndicatorApi offlineIndicatorApi, StreamDriver streamDriver) {
            super(context, streamConfiguration, cardConfiguration, imageLoaderApi,
                    actionParserFactory, actionApi, customElementProvider, debugBehavior,
                    threadUtils, headers, clock, modelProviderFactory, hostBindingProvider,
                    actionManager, configuration, snackbarApi, basicLoggingApi, offlineIndicatorApi,
                    mainThreadRunner, fakeFeedKnownContent, tooltipApi,
                    /* isBackgroundDark= */ false);
            this.layoutManager = layoutManager;
            this.streamDriver = streamDriver;
        }

        @Override
        PietManager createPietManager(Context context, CardConfiguration cardConfiguration,
                ImageLoaderApi imageLoaderApi,
                /*@Nullable*/ CustomElementProvider customElementProvider,
                DebugBehavior debugBehavior, Clock clock,
                /*@Nullable*/ HostBindingProvider hostBindingProvider,
                StreamOfflineMonitor streamOfflineMonitor, Configuration config,
                boolean isBackgroundDark) {
            return pietManager;
        }

        @Override
        StreamDriver createStreamDriver(ActionApi actionApi, ActionManager actionManager,
                ActionParserFactory actionParserFactory, ModelProvider modelProvider,
                ThreadUtils threadUtils, Clock clock, Configuration configuration, Context context,
                SnackbarApi snackbarApi, ContentChangedListener contentChangedListener,
                ScrollRestorer scrollRestorer, BasicLoggingApi basicLoggingApi,
                StreamOfflineMonitor streamOfflineMonitor, FeedKnownContent feedKnownContent,
                ContextMenuManager contextMenuManager, boolean restoring, boolean isInitialLoad,
                MainThreadRunner mainThreadRunner, TooltipApi tooltipApi,
                UiRefreshReason uiRefreshReason, ScrollListenerNotifier scrollListenerNotifier) {
            streamDriverScrollRestorer = scrollRestorer;
            streamDriverRestoring = restoring;
            this.streamDriverUiRefreshReason = uiRefreshReason;
            return streamDriver;
        }

        @Override
        StreamContentChangedListener createStreamContentChangedListener() {
            contentChangedListener = new StreamContentChangedListener();
            return contentChangedListener;
        }

        @Override
        StreamRecyclerViewAdapter createRecyclerViewAdapter(Context context,
                CardConfiguration cardConfiguration, PietManager pietManager,
                DeepestContentTracker deepestContentTracker,
                StreamContentChangedListener streamContentChangedListener,
                ScrollObservable scrollObservable, Configuration configuration,
                PietEventLogger pietEventLogger) {
            return adapter;
        }

        @Override
        ScrollListenerNotifier createScrollListenerNotifier(
                ContentChangedListener contentChangedListener,
                BasicStreamScrollMonitor scrollMonitor, MainThreadRunner mainThreadRunner) {
            return scrollListenerNotifier;
        }

        @Override
        LinearLayoutManager createRecyclerViewLayoutManager(Context context) {
            return layoutManager;
        }

        @Override
        ScrollRestorer createScrollRestorer(Configuration configuration, RecyclerView recyclerView,
                ScrollListenerNotifier scrollListenerNotifier,
                /*@Nullable*/ ScrollState scrollState) {
            return scrollRestorer;
        }

        @Override
        ScrollRestorer createNonRestoringScrollRestorer(Configuration configuration,
                RecyclerView recyclerView, ScrollListenerNotifier scrollListenerNotifier) {
            return nonRestoringScrollRestorer;
        }

        @Override
        ContextMenuManagerImpl createContextMenuManager(
                RecyclerView recyclerView, MenuMeasurer menuMeasurer) {
            return contextMenuManager;
        }

        @Override
        ViewLoggingUpdater createViewLoggingUpdater() {
            return viewLoggingUpdater;
        }
    }

    private class LinearLayoutManagerWithFakePositioning extends LinearLayoutManager {
        private final List<View> childMap;
        private int firstVisiblePosition = RecyclerView.NO_POSITION;
        private int lastVisiblePosition = RecyclerView.NO_POSITION;

        public LinearLayoutManagerWithFakePositioning(Context context) {
            super(context);
            childMap = new ArrayList<>();
        }

        @Override
        public int findFirstVisibleItemPosition() {
            return firstVisiblePosition;
        }

        @Override
        public int findLastVisibleItemPosition() {
            return lastVisiblePosition;
        }

        @Override
        public View findViewByPosition(int i) {
            if (i < 0 || i >= childMap.size()) {
                return null;
            }
            return childMap.get(i);
        }

        private void addChildToPosition(int position, View child) {
            childMap.add(position, child);
        }
    }

    private static class FakeFeedKnownContent implements FeedKnownContent {
        private final Set<KnownContent.Listener> listeners = new HashSet<>();

        @Override
        public void getKnownContent(Consumer<List<ContentMetadata>> knownContentConsumer) {}

        @Override
        public void addListener(KnownContent.Listener listener) {
            listeners.add(listener);
        }

        @Override
        public void removeListener(KnownContent.Listener listener) {
            listeners.remove(listener);
        }

        @Override
        public Listener getKnownContentHostNotifier() {
            return null;
        }
    }
}
