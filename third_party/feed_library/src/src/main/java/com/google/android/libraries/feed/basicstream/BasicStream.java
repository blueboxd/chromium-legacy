// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream;

import static com.google.android.libraries.feed.common.Validators.checkNotNull;
import static com.google.android.libraries.feed.common.Validators.checkState;

import android.content.Context;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;
import android.support.annotation.IntDef;
import android.support.annotation.VisibleForTesting;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.helper.ItemTouchHelper;
import android.util.Base64;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.View.OnLayoutChangeListener;

import com.google.android.libraries.feed.api.client.stream.Header;
import com.google.android.libraries.feed.api.client.stream.Stream;
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
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.State;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderObserver;
import com.google.android.libraries.feed.basicstream.internal.StreamItemAnimator;
import com.google.android.libraries.feed.basicstream.internal.StreamItemTouchCallbacks;
import com.google.android.libraries.feed.basicstream.internal.StreamRecyclerViewAdapter;
import com.google.android.libraries.feed.basicstream.internal.StreamSavedInstanceStateProto.StreamSavedInstanceState;
import com.google.android.libraries.feed.basicstream.internal.drivers.StreamDriver;
import com.google.android.libraries.feed.basicstream.internal.scroll.BasicStreamScrollMonitor;
import com.google.android.libraries.feed.basicstream.internal.scroll.ScrollRestorer;
import com.google.android.libraries.feed.basicstream.internal.viewloggingupdater.ViewLoggingUpdater;
import com.google.android.libraries.feed.common.concurrent.CancelableTask;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.functional.Suppliers;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.piet.PietManager;
import com.google.android.libraries.feed.piet.host.CustomElementProvider;
import com.google.android.libraries.feed.piet.host.HostBindingProvider;
import com.google.android.libraries.feed.sharedstream.contentchanged.StreamContentChangedListener;
import com.google.android.libraries.feed.sharedstream.contextmenumanager.ContextMenuManager;
import com.google.android.libraries.feed.sharedstream.contextmenumanager.ContextMenuManagerImpl;
import com.google.android.libraries.feed.sharedstream.contextmenumanager.FloatingContextMenuManager;
import com.google.android.libraries.feed.sharedstream.deepestcontenttracker.DeepestContentTracker;
import com.google.android.libraries.feed.sharedstream.logging.UiSessionRequestLogger;
import com.google.android.libraries.feed.sharedstream.offlinemonitor.StreamOfflineMonitor;
import com.google.android.libraries.feed.sharedstream.piet.PietCustomElementProvider;
import com.google.android.libraries.feed.sharedstream.piet.PietEventLogger;
import com.google.android.libraries.feed.sharedstream.piet.PietHostBindingProvider;
import com.google.android.libraries.feed.sharedstream.piet.PietImageLoader;
import com.google.android.libraries.feed.sharedstream.piet.PietStringFormatter;
import com.google.android.libraries.feed.sharedstream.proto.ScrollStateProto.ScrollState;
import com.google.android.libraries.feed.sharedstream.proto.UiRefreshReasonProto.UiRefreshReason;
import com.google.android.libraries.feed.sharedstream.publicapi.menumeasurer.MenuMeasurer;
import com.google.android.libraries.feed.sharedstream.publicapi.scroll.ScrollObservable;
import com.google.android.libraries.feed.sharedstream.scroll.ScrollListenerNotifier;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.search.now.feed.client.StreamDataProto.UiContext;

import java.util.List;

/**
 * A basic implementation of a Feed {@link Stream} that is just able to render a vertical stream of
 * cards.
 */
public class BasicStream implements Stream, ModelProviderObserver, OnLayoutChangeListener {
    private static final String TAG = "BasicStream";

    @VisibleForTesting
    static final String KEY_STREAM_STATE = "stream-state";
    private static final long DEFAULT_LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS = 1000L;
    private static final long DEFAULT_MINIMUM_SPINNER_SHOW_TIME_MS = 500L;
    private static final long DEFAULT_SPINNER_DELAY_TIME_MS = 500L;

    private final CardConfiguration cardConfiguration;
    private final Clock clock;
    private final Context context;
    private final ThreadUtils threadUtils;
    private final PietManager pietManager;
    private final ModelProviderFactory modelProviderFactory;
    private final ActionParserFactory actionParserFactory;
    private final ActionApi actionApi;
    private final ActionManager actionManager;
    private final Configuration configuration;
    private final SnackbarApi snackbarApi;
    private final StreamContentChangedListener streamContentChangedListener;
    private final DeepestContentTracker deepestContentTracker;
    private final BasicLoggingApi basicLoggingApi;
    private final long immediateContentThreshold;
    private final StreamOfflineMonitor streamOfflineMonitor;
    private final MainThreadRunner mainThreadRunner;
    private final FeedKnownContent feedKnownContent;
    private final ViewLoggingUpdater viewLoggingUpdater;
    private final TooltipApi tooltipApi;
    private final UiSessionRequestLogger uiSessionRequestLogger;
    private final StreamConfiguration streamConfiguration;
    private final BasicStreamScrollMonitor scrollMonitor;
    private final long minimumSpinnerShowTime;
    private final long spinnerDelayTime;

    private RecyclerView recyclerView;
    private ContextMenuManager contextMenuManager;
    private List<Header> headers;
    private StreamRecyclerViewAdapter adapter;
    private ScrollListenerNotifier scrollListenerNotifier;
    private ScrollRestorer scrollRestorer;
    private long sessionStartTimestamp;
    private long initialLoadingSpinnerStartTime;
    private boolean isInitialLoad = true;
    private boolean isRestoring;
    private boolean isDestroyed;
    private boolean isStreamContentVisible = true;

    @LoggingState
    private int loggingState = LoggingState.STARTING;

    /*@MonotonicNonNull*/ private ModelProvider modelProvider;
    /*@MonotonicNonNull*/ private StreamDriver streamDriver;

    /*@Nullable*/ private String savedSessionId;
    /*@Nullable*/ private CancelableTask cancellableShowSpinnerRunnable;

    // TODO: instead of using a nullable field, pipe UiContext through the creation of
    // ModelProviders to onSessionStart().
    private UiRefreshReason uiRefreshReason = UiRefreshReason.getDefaultInstance();
    private StreamItemAnimator itemAnimator;

    public BasicStream(Context context, StreamConfiguration streamConfiguration,
            CardConfiguration cardConfiguration, ImageLoaderApi imageLoaderApi,
            ActionParserFactory actionParserFactory, ActionApi actionApi,
            /*@Nullable*/ CustomElementProvider customElementProvider, DebugBehavior debugBehavior,
            ThreadUtils threadUtils, List<Header> headers, Clock clock,
            ModelProviderFactory modelProviderFactory,
            /*@Nullable*/ HostBindingProvider hostBindingProvider, ActionManager actionManager,
            Configuration configuration, SnackbarApi snackbarApi, BasicLoggingApi basicLoggingApi,
            OfflineIndicatorApi offlineIndicatorApi, MainThreadRunner mainThreadRunner,
            FeedKnownContent feedKnownContent, TooltipApi tooltipApi, boolean isBackgroundDark) {
        this.cardConfiguration = cardConfiguration;
        this.clock = clock;
        this.threadUtils = threadUtils;
        this.streamOfflineMonitor = new StreamOfflineMonitor(offlineIndicatorApi);
        this.headers = headers;
        this.modelProviderFactory = modelProviderFactory;
        this.streamConfiguration = streamConfiguration;
        this.actionParserFactory = actionParserFactory;
        this.actionApi = actionApi;
        this.actionManager = actionManager;
        this.configuration = configuration;
        this.snackbarApi = snackbarApi;
        this.mainThreadRunner = mainThreadRunner;
        this.streamContentChangedListener = createStreamContentChangedListener();
        this.deepestContentTracker = new DeepestContentTracker();
        this.basicLoggingApi = basicLoggingApi;
        this.immediateContentThreshold =
                configuration.getValueOrDefault(ConfigKey.LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS,
                        DEFAULT_LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS);
        this.feedKnownContent = feedKnownContent;
        viewLoggingUpdater = createViewLoggingUpdater();
        this.uiSessionRequestLogger = new UiSessionRequestLogger(clock, basicLoggingApi);
        this.tooltipApi = tooltipApi;
        this.pietManager = createPietManager(context, cardConfiguration, imageLoaderApi,
                customElementProvider, debugBehavior, clock, hostBindingProvider,
                streamOfflineMonitor, configuration, isBackgroundDark);
        this.context =
                new ContextThemeWrapper(context, (isBackgroundDark ? R.style.Dark : R.style.Light));
        this.scrollMonitor = new BasicStreamScrollMonitor(clock);
        this.minimumSpinnerShowTime = configuration.getValueOrDefault(
                ConfigKey.SPINNER_MINIMUM_SHOW_TIME_MS, DEFAULT_MINIMUM_SPINNER_SHOW_TIME_MS);
        this.spinnerDelayTime = configuration.getValueOrDefault(
                ConfigKey.SPINNER_DELAY_MS, DEFAULT_SPINNER_DELAY_TIME_MS);
    }

    @VisibleForTesting
    PietManager createPietManager(
            /*@UnderInitialization*/ BasicStream this, Context context,
            CardConfiguration cardConfiguration, ImageLoaderApi imageLoaderApi,
            /*@Nullable*/ CustomElementProvider customElementProvider, DebugBehavior debugBehavior,
            Clock clock,
            /*@Nullable*/ HostBindingProvider hostBindingProvider,
            StreamOfflineMonitor streamOfflineMonitor, Configuration configuration,
            boolean isBackgroundDark) {
        return PietManager.builder()
                .setImageLoader(new PietImageLoader(imageLoaderApi))
                .setStringFormatter(new PietStringFormatter(clock))
                .setFadeImageThresholdMs(Suppliers.of(
                        configuration.getValueOrDefault(ConfigKey.FADE_IMAGE_THRESHOLD_MS, 80L)))
                .setDefaultCornerRadius(() -> cardConfiguration.getDefaultCornerRadius())
                .setDebugBehavior(debugBehavior)
                .setCustomElementProvider(
                        new PietCustomElementProvider(context, customElementProvider))
                .setHostBindingProvider(
                        new PietHostBindingProvider(hostBindingProvider, streamOfflineMonitor))
                .setClock(clock)
                .setIsDarkTheme(Suppliers.of(isBackgroundDark))
                .build();
    }

    @Override
    public void onCreate(/*@Nullable*/ Bundle savedInstanceState) {
        if (savedInstanceState == null) {
            onCreate((String) null);
            return;
        }

        onCreate(savedInstanceState.getString(KEY_STREAM_STATE));
    }

    @Override
    public void onCreate(/*@Nullable*/ String savedInstanceState) {
        checkState(recyclerView == null, "Can't call onCreate() multiple times.");
        setupRecyclerView();

        if (savedInstanceState == null) {
            scrollRestorer =
                    createScrollRestorer(configuration, recyclerView, scrollListenerNotifier, null);
            return;
        }

        try {
            StreamSavedInstanceState streamSavedInstanceState = StreamSavedInstanceState.parseFrom(
                    Base64.decode(savedInstanceState, Base64.DEFAULT));

            if (streamSavedInstanceState.hasSessionId()) {
                savedSessionId = streamSavedInstanceState.getSessionId();
            }

            scrollRestorer = createScrollRestorer(configuration, recyclerView,
                    scrollListenerNotifier, streamSavedInstanceState.getScrollState());
        } catch (IllegalArgumentException | InvalidProtocolBufferException e) {
            Logger.wtf(TAG, "Could not parse saved instance state String.");
            scrollRestorer =
                    createScrollRestorer(configuration, recyclerView, scrollListenerNotifier, null);
        }
    }

    @Override
    public void onShow() {
        // Only create model provider if Stream content is visible.
        if (isStreamContentVisible) {
            createModelProviderAndStreamDriver();
        } else {
            if (loggingState == LoggingState.STARTING) {
                basicLoggingApi.onOpenedWithNoContent();
                loggingState = LoggingState.LOGGED_NO_CONTENT;
            }

            // If Stream content is not visible, we will not create the StreamDriver and restore the
            // scroll position automatically. So we try to restore the scroll position before.
            scrollRestorer.maybeRestoreScroll();
        }
        adapter.setShown(true);
    }

    @Override
    public void onActive() {}

    @Override
    public void onInactive() {}

    @Override
    public void onHide() {
        adapter.setShown(false);
        contextMenuManager.dismissPopup();
    }

    @Override
    public void onDestroy() {
        if (isDestroyed) {
            Logger.e(TAG, "onDestroy() called multiple times.");
            return;
        }
        adapter.onDestroy();
        recyclerView.removeOnLayoutChangeListener(this);
        if (modelProvider != null) {
            modelProvider.unregisterObserver(this);
            modelProvider.detachModelProvider();
        }
        if (streamDriver != null) {
            streamDriver.onDestroy();
        }
        streamOfflineMonitor.onDestroy();
        uiSessionRequestLogger.onDestroy();
        isDestroyed = true;
    }

    @Override
    public Bundle getSavedInstanceState() {
        Bundle bundle = new Bundle();
        bundle.putString(KEY_STREAM_STATE, getSavedInstanceStateString());
        return bundle;
    }

    @Override
    public String getSavedInstanceStateString() {
        StreamSavedInstanceState.Builder builder = StreamSavedInstanceState.newBuilder();
        if (modelProvider != null && modelProvider.getSessionId() != null) {
            builder.setSessionId(checkNotNull(modelProvider.getSessionId()));
        }

        ScrollState scrollState =
                scrollRestorer.getScrollStateForScrollRestore(adapter.getHeaderCount());
        if (scrollState != null) {
            builder.setScrollState(scrollState);
        }

        return convertStreamSavedInstanceStateToString(builder.build());
    }

    @Override
    public View getView() {
        checkState(recyclerView != null, "Must call onCreate() before getView()");
        return recyclerView;
    }

    @VisibleForTesting
    StreamRecyclerViewAdapter getAdapter() {
        return adapter;
    }

    @Override
    public void setHeaderViews(List<Header> headers) {
        Logger.i(TAG, "Setting %s header views, currently have %s headers", headers.size(),
                this.headers.size());

        this.headers = headers;
        adapter.setHeaders(headers);
    }

    @Override
    public void setStreamContentVisibility(boolean visible) {
        checkNotNull(adapter, "onCreate must be called before setStreamContentVisibility");

        if (visible == isStreamContentVisible) {
            return;
        }

        isStreamContentVisible = visible;

        if (isStreamContentVisible) {
            viewLoggingUpdater.resetViewTracking();
        }

        // If Stream content was previously not visible, ModelProvider might need to be created.
        if (isStreamContentVisible && modelProvider == null) {
            createModelProviderAndStreamDriver();
        }

        itemAnimator.setStreamVisibility(isStreamContentVisible);
        adapter.setStreamContentVisible(isStreamContentVisible);
    }

    @Override
    public void trim() {
        pietManager.purgeRecyclerPools();
        recyclerView.getRecycledViewPool().clear();
    }

    @Override
    public void smoothScrollBy(int dx, int dy) {
        recyclerView.smoothScrollBy(dx, dy);
    }

    @Override
    public int getChildTopAt(int position) {
        if (!isChildAtPositionVisible(position)) {
            return POSITION_NOT_KNOWN;
        }

        LinearLayoutManager layoutManager = (LinearLayoutManager) recyclerView.getLayoutManager();
        if (layoutManager == null) {
            return POSITION_NOT_KNOWN;
        }

        View view = layoutManager.findViewByPosition(position);
        if (view == null) {
            return POSITION_NOT_KNOWN;
        }

        return view.getTop();
    }

    @Override
    public boolean isChildAtPositionVisible(int position) {
        LinearLayoutManager layoutManager = (LinearLayoutManager) recyclerView.getLayoutManager();
        if (layoutManager == null) {
            return false;
        }

        int firstItemPosition = layoutManager.findFirstVisibleItemPosition();
        int lastItemPosition = layoutManager.findLastVisibleItemPosition();
        if (firstItemPosition == RecyclerView.NO_POSITION
                || lastItemPosition == RecyclerView.NO_POSITION) {
            return false;
        }

        return position >= firstItemPosition && position <= lastItemPosition;
    }

    @Override
    public void addScrollListener(ScrollListener listener) {
        scrollListenerNotifier.addScrollListener(listener);
    }

    @Override
    public void removeScrollListener(ScrollListener listener) {
        scrollListenerNotifier.removeScrollListener(listener);
    }

    @Override
    public void addOnContentChangedListener(ContentChangedListener listener) {
        streamContentChangedListener.addContentChangedListener(listener);
    }

    @Override
    public void removeOnContentChangedListener(ContentChangedListener listener) {
        streamContentChangedListener.removeContentChangedListener(listener);
    }

    @Override
    public void triggerRefresh() {
        if (streamDriver == null || modelProvider == null) {
            Logger.w(TAG,
                    "Refresh requested before Stream was shown.  Scheduler should be used instead "
                            + "in this instance.");
            return;
        }

        // This invalidates the modelProvider, which results in onSessionFinished() then
        // onSessionStart() being called, leading to recreating the entire stream.
        streamDriver.showSpinner();
        modelProvider.triggerRefresh(RequestReason.HOST_REQUESTED);
    }

    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        if ((oldLeft != 0 && left != oldLeft) || (oldRight != 0 && right != oldRight)) {
            checkNotNull(adapter, "onCreate must be called before so that adapter is set.")
                    .rebind();
        }

        contextMenuManager.dismissPopup();
    }

    private void setupRecyclerView() {
        recyclerView = new RecyclerView(context);
        scrollListenerNotifier = createScrollListenerNotifier(
                streamContentChangedListener, scrollMonitor, mainThreadRunner);
        recyclerView.addOnScrollListener(scrollMonitor);
        adapter = createRecyclerViewAdapter(context, cardConfiguration, pietManager,
                deepestContentTracker, streamContentChangedListener, scrollMonitor, configuration,
                new PietEventLogger(basicLoggingApi));
        adapter.setHeaders(headers);
        recyclerView.setId(R.id.feed_stream_recycler_view);
        recyclerView.setLayoutManager(createRecyclerViewLayoutManager(context));
        contextMenuManager = createContextMenuManager(recyclerView, new MenuMeasurer(context));
        new ItemTouchHelper(new StreamItemTouchCallbacks()).attachToRecyclerView(recyclerView);
        recyclerView.setAdapter(adapter);
        recyclerView.setClipToPadding(false);
        if (VERSION.SDK_INT > VERSION_CODES.JELLY_BEAN) {
            recyclerView.setPaddingRelative(streamConfiguration.getPaddingStart(),
                    streamConfiguration.getPaddingTop(), streamConfiguration.getPaddingEnd(),
                    streamConfiguration.getPaddingBottom());
        } else {
            recyclerView.setPadding(streamConfiguration.getPaddingStart(),
                    streamConfiguration.getPaddingTop(), streamConfiguration.getPaddingEnd(),
                    streamConfiguration.getPaddingBottom());
        }

        itemAnimator = new StreamItemAnimator(streamContentChangedListener);
        itemAnimator.setStreamVisibility(isStreamContentVisible);

        recyclerView.setItemAnimator(itemAnimator);
        recyclerView.addOnLayoutChangeListener(this);
    }

    private void updateAdapterAfterSessionStart(ModelProvider modelProvider) {
        StreamDriver newStreamDriver = createStreamDriver(actionApi, actionManager,
                actionParserFactory, modelProvider, threadUtils, clock, configuration, context,
                snackbarApi, streamContentChangedListener, scrollRestorer, basicLoggingApi,
                streamOfflineMonitor, feedKnownContent, contextMenuManager, isRestoring,
                /* isInitialLoad= */ false, mainThreadRunner, tooltipApi, uiRefreshReason,
                scrollListenerNotifier);

        uiRefreshReason = UiRefreshReason.getDefaultInstance();

        // If after starting a new session the Stream is still empty, we should show the zero state.
        if (newStreamDriver.getLeafFeatureDrivers().isEmpty()) {
            newStreamDriver.showZeroState(ZeroStateShowReason.NO_CONTENT);
        }
        if (loggingState == LoggingState.STARTING && modelProvider.getCurrentState() == State.READY
                && modelProvider.getRootFeature() == null) {
            basicLoggingApi.onOpenedWithNoContent();
            loggingState = LoggingState.LOGGED_NO_CONTENT;
        }

        // If old and new stream driver are both showing the zero state, do not replace the old
        // stream driver. This prevents the zero state flashing if the old and new stream drivers
        // are both displaying the same content. The old stream driver will be updated with the new
        // model provider.
        if (streamDriver != null && streamDriver.isZeroStateBeingShown()
                && newStreamDriver.isZeroStateBeingShown()) {
            streamDriver.setModelProviderForZeroState(modelProvider);
            newStreamDriver.onDestroy();
            return;
        }
        if (streamDriver != null) {
            streamDriver.onDestroy();
        }
        streamDriver = newStreamDriver;
        adapter.setDriver(newStreamDriver);
        deepestContentTracker.reset();

        if (streamDriver.hasContent()) {
            isInitialLoad = false;
        }

        logContent();
    }

    @Override
    public void onSessionStart(UiContext uiContext) {
        threadUtils.checkMainThread();

        if (cancellableShowSpinnerRunnable != null) {
            cancellableShowSpinnerRunnable.cancel();
            cancellableShowSpinnerRunnable = null;
        }

        ModelProvider localModelProvider =
                checkNotNull(modelProvider, "Model Provider must be set if a session is active");
        // On initial load, if a loading spinner is currently being shown, the spinner must be shown
        // for at least the time specified in MINIMUM_SPINNER_SHOW_TIME.
        if (isInitialLoad && initialLoadingSpinnerStartTime != 0L) {
            long spinnerDisplayTime = clock.currentTimeMillis() - initialLoadingSpinnerStartTime;
            // If MINIMUM_SPINNER_SHOW_TIME has elapsed, the new content can be shown immediately.
            if (spinnerDisplayTime >= minimumSpinnerShowTime) {
                updateAdapterAfterSessionStart(localModelProvider);
            } else {
                // If MINIMUM_SPINNER_SHOW_TIME has not elapsed, the new content should only be
                // shown once the remaining time has been fulfilled.
                mainThreadRunner.executeWithDelay(TAG + " onSessionStart", () -> {
                    // Only show content if model providers are the same. If they are different,
                    // this indicates that the session finished before the spinner show time
                    // elapsed.
                    if (modelProvider == localModelProvider) {
                        updateAdapterAfterSessionStart(localModelProvider);
                    }
                }, minimumSpinnerShowTime - spinnerDisplayTime);
            }
        } else {
            updateAdapterAfterSessionStart(localModelProvider);
        }
    }

    private void logContent() {
        if (loggingState == LoggingState.STARTING) {
            long timeToPopulateMs = clock.currentTimeMillis() - sessionStartTimestamp;
            if (timeToPopulateMs > immediateContentThreshold) {
                basicLoggingApi.onOpenedWithNoImmediateContent();
            }

            if (checkNotNull(streamDriver).hasContent()) {
                basicLoggingApi.onOpenedWithContent((int) timeToPopulateMs,
                        checkNotNull(streamDriver).getLeafFeatureDrivers().size());
                // onOpenedWithContent should only be logged the first time the Stream is opened up.
                loggingState = LoggingState.LOGGED_CONTENT_SHOWN;
            } else {
                basicLoggingApi.onOpenedWithNoContent();
                loggingState = LoggingState.LOGGED_NO_CONTENT;
            }
        }
    }

    @Override
    public void onSessionFinished(UiContext uiContext) {
        if (isDestroyed) {
            // This seems to be getting called after onDestroy(), resulting in unregistering from
            // the ModelProvider twice, which causes a crash.
            Logger.e(TAG, "onSessionFinished called after onDestroy()");
            return;
        }

        // Our previous session isn't valid anymore.  There are some circumstances we could probably
        // restore our scroll (say if scroll was in headers), other times, if we were to restore
        // scroll it would be to a card which is no longer present.  For simplicity just abandon
        // scroll restoring for now.  We can improve logic if this doesn't prove to be sufficient
        // enough.
        scrollRestorer.abandonRestoringScroll();

        // At this point, the StreamDriver shouldn't be null. However, the
        // cancellableShowSpinnerRunnable could be null or not, depending on whether this spinner is
        // finishing because a failed restore or because the session started. If a spinner is queued
        // to show, we want to show that one with its delay, otherwise we show a new one with a new
        // delay.
        if (streamDriver != null && cancellableShowSpinnerRunnable == null) {
            showSpinnerWithDelay();
        }

        isRestoring = false;

        if (modelProvider != null) {
            modelProvider.unregisterObserver(this);
        }
        uiRefreshReason = uiContext.getExtension(UiRefreshReason.uiRefreshReasonExtension);

        // TODO: Instead of setting the refresh reseason, pipe the UiContext through here.
        modelProvider = modelProviderFactory.createNew(
                deepestContentTracker, UiContext.getDefaultInstance());

        registerObserversOnModelProvider(modelProvider);
    }

    @Override
    public void onError(ModelError modelError) {
        if (modelError.getErrorType() != ErrorType.NO_CARDS_ERROR) {
            Logger.wtf(TAG, "Not expecting non NO_CARDS_ERROR type.");
        }

        if (loggingState == LoggingState.STARTING) {
            basicLoggingApi.onOpenedWithNoContent();
            loggingState = LoggingState.LOGGED_NO_CONTENT;
        }

        scrollRestorer.abandonRestoringScroll();
        if (streamDriver != null) {
            streamDriver.showZeroState(ZeroStateShowReason.ERROR);
        }
    }

    private void createModelProviderAndStreamDriver() {
        if (modelProvider == null) {
            // For nullness checker
            ModelProvider localModelProvider = null;
            String localSavedSessionId = savedSessionId;
            if (localSavedSessionId != null) {
                isRestoring = true;
                Logger.d(TAG, "Attempting to restoring session with id: %s.", localSavedSessionId);
                localModelProvider = modelProviderFactory.create(
                        localSavedSessionId, UiContext.getDefaultInstance());
            }

            if (localModelProvider == null) {
                // If a session is no longer valid then a ModelProvider will not have been created
                // above.
                Logger.d(TAG, "Creating new session for showing.");
                localModelProvider = modelProviderFactory.createNew(
                        deepestContentTracker, UiContext.getDefaultInstance());
            }

            sessionStartTimestamp = clock.currentTimeMillis();
            modelProvider = localModelProvider;

            registerObserversOnModelProvider(modelProvider);
        }

        if (streamDriver == null) {
            // If the ModelProvider is not ready we don't want to restore the Stream at all. Instead
            // we need to wait for it to become active and we can reset the StreamDriver with the
            // correct scroll restorer in order to finally restore scroll position.
            ScrollRestorer initialScrollRestorer = modelProvider.getCurrentState() == State.READY
                    ? scrollRestorer
                    : createNonRestoringScrollRestorer(
                            configuration, recyclerView, scrollListenerNotifier);

            streamDriver = createStreamDriver(actionApi, actionManager, actionParserFactory,
                    modelProvider, threadUtils, clock, configuration, context, snackbarApi,
                    streamContentChangedListener, initialScrollRestorer, basicLoggingApi,
                    streamOfflineMonitor, feedKnownContent, contextMenuManager, isRestoring,
                    isInitialLoad, mainThreadRunner, tooltipApi,
                    UiRefreshReason.getDefaultInstance(), scrollListenerNotifier);

            showSpinnerWithDelay();
            adapter.setDriver(streamDriver);
        }
    }

    private void showSpinnerWithDelay() {
        cancellableShowSpinnerRunnable = mainThreadRunner.executeWithDelay(
                TAG + " onShow", new ShowSpinnerRunnable(), spinnerDelayTime);
    }

    private String convertStreamSavedInstanceStateToString(
            StreamSavedInstanceState savedInstanceState) {
        return Base64.encodeToString(savedInstanceState.toByteArray(), Base64.DEFAULT);
    }

    private void registerObserversOnModelProvider(ModelProvider modelProvider) {
        modelProvider.registerObserver(this);
        uiSessionRequestLogger.onSessionRequested(modelProvider);
    }

    @VisibleForTesting
    StreamDriver createStreamDriver(ActionApi actionApi, ActionManager actionManager,
            ActionParserFactory actionParserFactory, ModelProvider modelProvider,
            ThreadUtils threadUtils, Clock clock, Configuration configuration, Context context,
            SnackbarApi snackbarApi, ContentChangedListener contentChangedListener,
            ScrollRestorer scrollRestorer, BasicLoggingApi basicLoggingApi,
            StreamOfflineMonitor streamOfflineMonitor, FeedKnownContent feedKnownContent,
            ContextMenuManager contextMenuManager, boolean restoring, boolean isInitialLoad,
            MainThreadRunner mainThreadRunner, TooltipApi tooltipApi,
            UiRefreshReason uiRefreshReason, ScrollListenerNotifier scrollListenerNotifier) {
        return new StreamDriver(actionApi, actionManager, actionParserFactory, modelProvider,
                threadUtils, clock, configuration, context, snackbarApi, contentChangedListener,
                scrollRestorer, basicLoggingApi, streamOfflineMonitor, feedKnownContent,
                contextMenuManager, restoring, isInitialLoad, mainThreadRunner, viewLoggingUpdater,
                tooltipApi, uiRefreshReason, scrollMonitor);
    }

    @VisibleForTesting
    StreamRecyclerViewAdapter createRecyclerViewAdapter(Context context,
            CardConfiguration cardConfiguration, PietManager pietManager,
            DeepestContentTracker deepestContentTracker,
            StreamContentChangedListener streamContentChangedListener,
            ScrollObservable scrollObservable, Configuration configuration,
            PietEventLogger pietEventLogger) {
        return new StreamRecyclerViewAdapter(context, recyclerView, cardConfiguration, pietManager,
                deepestContentTracker, streamContentChangedListener, scrollObservable,
                configuration, pietEventLogger);
    }

    @VisibleForTesting
    ScrollListenerNotifier createScrollListenerNotifier(
            ContentChangedListener contentChangedListener, BasicStreamScrollMonitor scrollMonitor,
            MainThreadRunner mainThreadRunner) {
        return new ScrollListenerNotifier(contentChangedListener, scrollMonitor, mainThreadRunner);
    }

    @VisibleForTesting
    LinearLayoutManager createRecyclerViewLayoutManager(Context context) {
        return new LinearLayoutManager(context);
    }

    @VisibleForTesting
    StreamContentChangedListener createStreamContentChangedListener(
            /*@UnderInitialization*/ BasicStream this) {
        return new StreamContentChangedListener();
    }

    @VisibleForTesting
    ScrollRestorer createScrollRestorer(Configuration configuration, RecyclerView recyclerView,
            ScrollListenerNotifier scrollListenerNotifier,
            /*@Nullable*/ ScrollState scrollState) {
        return new ScrollRestorer(configuration, recyclerView, scrollListenerNotifier, scrollState);
    }

    @VisibleForTesting
    ScrollRestorer createNonRestoringScrollRestorer(Configuration configuration,
            RecyclerView recyclerView, ScrollListenerNotifier scrollListenerNotifier) {
        return ScrollRestorer.nonRestoringRestorer(
                configuration, recyclerView, scrollListenerNotifier);
    }

    @VisibleForTesting
    ContextMenuManager createContextMenuManager(
            RecyclerView recyclerView, MenuMeasurer menuMeasurer) {
        ContextMenuManager manager;
        if (VERSION.SDK_INT > VERSION_CODES.M) {
            manager = new ContextMenuManagerImpl(menuMeasurer, context);
        } else {
            manager = new FloatingContextMenuManager(context);
        }
        manager.setView(recyclerView);
        return manager;
    }

    @VisibleForTesting
    ViewLoggingUpdater createViewLoggingUpdater(/*@UnderInitialization*/ BasicStream this) {
        return new ViewLoggingUpdater();
    }

    @IntDef({LoggingState.STARTING, LoggingState.LOGGED_NO_CONTENT,
            LoggingState.LOGGED_CONTENT_SHOWN})
    @interface LoggingState {
        int STARTING = 0;
        int LOGGED_NO_CONTENT = 1;
        int LOGGED_CONTENT_SHOWN = 2;
    }

    private final class ShowSpinnerRunnable implements Runnable {
        @Override
        public void run() {
            checkNotNull(streamDriver).showSpinner();
            initialLoadingSpinnerStartTime = clock.currentTimeMillis();
            BasicStream.this.cancellableShowSpinnerRunnable = null;
        }
    }
}
