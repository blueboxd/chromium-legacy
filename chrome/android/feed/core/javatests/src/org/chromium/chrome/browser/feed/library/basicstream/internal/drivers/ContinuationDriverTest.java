// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.drivers;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.view.View.OnClickListener;

import com.google.common.collect.ImmutableList;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.InternalFeedError;
import org.chromium.chrome.browser.feed.library.api.host.logging.SpinnerType;
import org.chromium.chrome.browser.feed.library.api.host.stream.SnackbarApi;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelError;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelError.ErrorType;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelToken;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.TokenCompleted;
import org.chromium.chrome.browser.feed.library.basicstream.internal.drivers.ContinuationDriver.CursorChangedListener;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.ContinuationViewHolder;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.sharedstream.logging.LoggingListener;
import org.chromium.chrome.browser.feed.library.sharedstream.logging.SpinnerLogger;
import org.chromium.chrome.browser.feed.library.testing.modelprovider.FakeModelCursor;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.List;

/** Tests for {@link ContinuationDriver}.j */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContinuationDriverTest {
    private static final int POSITION = 1;
    public static final TokenCompleted EMPTY_TOKEN_COMPLETED =
            new TokenCompleted(FakeModelCursor.newBuilder().build());

    @Mock
    private BasicLoggingApi mBasicLoggingApi;
    @Mock
    private ModelChild mModelChild;
    @Mock
    private ModelProvider mModelProvider;
    @Mock
    private ModelToken mModelToken;
    @Mock
    private SnackbarApi mSnackbarApi;
    @Mock
    private ThreadUtils mThreadUtils;
    @Mock
    private CursorChangedListener mCursorChangedListener;
    @Mock
    private ContinuationViewHolder mContinuationViewHolder;

    private SpinnerLogger mSpinnerLogger;
    private Clock mClock;
    private Context mContext;
    private ContinuationDriver mContinuationDriver;
    private Configuration mConfiguration = new Configuration.Builder().build();
    private View mView;

    @Before
    public void setup() {
        initMocks(this);
        mClock = new FakeClock();
        mContext = Robolectric.buildActivity(Activity.class).get();
        mView = new View(mContext);
        when(mModelProvider.handleToken(any(ModelToken.class))).thenReturn(true);
        when(mModelChild.getModelToken()).thenReturn(mModelToken);
        mSpinnerLogger = new SpinnerLogger(mBasicLoggingApi, mClock);

        mContinuationDriver = new ContinuationDriverForTest(mBasicLoggingApi, mClock,
                mConfiguration, mContext, mCursorChangedListener, mModelChild, mModelProvider,
                POSITION, mSnackbarApi, mThreadUtils,
                /* restoring= */ false);
        mContinuationDriver.initialize();
    }

    @Test
    public void testHasTokenBeenHandled_returnsTrue_ifSyntheticTokenConsumedImmediately() {
        when(mModelToken.isSynthetic()).thenReturn(true);
        mContinuationDriver = new ContinuationDriverForTest(mBasicLoggingApi, mClock,
                new Configuration.Builder().put(ConfigKey.CONSUME_SYNTHETIC_TOKENS, true).build(),
                mContext, mCursorChangedListener, mModelChild, mModelProvider, POSITION,
                mSnackbarApi, mThreadUtils,
                /* restoring= */ false);
        mContinuationDriver.initialize();

        assertThat(mContinuationDriver.hasTokenBeenHandled()).isTrue();
    }

    @Test
    public void testHasTokenBeenHandled_returnsFalse_ifSyntheticTokenNotConsumedImmediately() {
        when(mModelToken.isSynthetic()).thenReturn(true);
        mContinuationDriver = new ContinuationDriverForTest(mBasicLoggingApi, mClock,
                mConfiguration, mContext, mCursorChangedListener, mModelChild, mModelProvider,
                POSITION, mSnackbarApi, mThreadUtils,
                /* restoring= */ false);
        mContinuationDriver.initialize();

        assertThat(mContinuationDriver.hasTokenBeenHandled()).isFalse();
    }

    @Test
    public void testInitialize_nonSyntheticToken() {
        mContinuationDriver = new ContinuationDriverForTest(mBasicLoggingApi, mClock,
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, true)
                        .build(),
                mContext, mCursorChangedListener, mModelChild, mModelProvider, POSITION,
                mSnackbarApi, mThreadUtils,
                /* restoring= */ false);
        mContinuationDriver.initialize();

        verify(mModelToken).registerObserver(mContinuationDriver);
        verify(mModelProvider, never()).handleToken(any(ModelToken.class));
    }

    @Test
    public void testInitialize_syntheticTokenConsume() {
        when(mModelToken.isSynthetic()).thenReturn(true);
        mContinuationDriver = new ContinuationDriverForTest(mBasicLoggingApi, mClock,
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, true)
                        .put(ConfigKey.CONSUME_SYNTHETIC_TOKENS, true)
                        .build(),
                mContext, mCursorChangedListener, mModelChild, mModelProvider, POSITION,
                mSnackbarApi, mThreadUtils,
                /* restoring= */ false);
        mContinuationDriver.initialize();

        // Call initialize again to make sure it doesn't duplicate work
        mContinuationDriver.initialize();

        verify(mModelToken).registerObserver(mContinuationDriver);
        verify(mModelProvider).handleToken(mModelToken);

        mContinuationDriver.bind(mContinuationViewHolder);
        verify(mContinuationViewHolder)
                .bind(eq(mContinuationDriver), any(LoggingListener.class), eq(true));
    }

    @Test
    public void testInitialize_syntheticTokenConsume_logsErrorForUnhandledToken() {
        when(mModelToken.isSynthetic()).thenReturn(true);
        when(mModelProvider.handleToken(any(ModelToken.class))).thenReturn(false);
        mContinuationDriver = new ContinuationDriverForTest(mBasicLoggingApi, mClock,
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, true)
                        .put(ConfigKey.CONSUME_SYNTHETIC_TOKENS, true)
                        .build(),
                mContext, mCursorChangedListener, mModelChild, mModelProvider, POSITION,
                mSnackbarApi, mThreadUtils,
                /* restoring= */ false);
        mContinuationDriver.initialize();

        verify(mBasicLoggingApi).onInternalError(InternalFeedError.UNHANDLED_TOKEN);
    }

    @Test
    public void testInitialize_syntheticTokenNotConsumed() {
        when(mModelToken.isSynthetic()).thenReturn(true);
        mContinuationDriver = new ContinuationDriverForTest(mBasicLoggingApi, mClock,
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, true)
                        .put(ConfigKey.CONSUME_SYNTHETIC_TOKENS, false)
                        .build(),
                mContext, mCursorChangedListener, mModelChild, mModelProvider, POSITION,
                mSnackbarApi, mThreadUtils,
                /* restoring= */ false);
        mContinuationDriver.initialize();

        // Call initialize again to make sure it doesn't duplicate work
        mContinuationDriver.initialize();

        verify(mModelToken).registerObserver(mContinuationDriver);
        verify(mModelProvider, never()).handleToken(mModelToken);

        mContinuationDriver.bind(mContinuationViewHolder);
        verify(mContinuationViewHolder)
                .bind(eq(mContinuationDriver), any(LoggingListener.class), eq(true));
    }

    @Test
    public void testInitialize_doesNotAutoConsumeSyntheticTokenOnRestore() {
        when(mModelToken.isSynthetic()).thenReturn(true);

        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, false)
                        .put(ConfigKey.CONSUME_SYNTHETIC_TOKENS, false)
                        .put(ConfigKey.CONSUME_SYNTHETIC_TOKENS_WHILE_RESTORING, false)
                        .build();

        mContinuationDriver = new ContinuationDriverForTest(mBasicLoggingApi, mClock, configuration,
                mContext, mCursorChangedListener, mModelChild, mModelProvider, POSITION,
                mSnackbarApi, mThreadUtils,
                /* restoring= */ true);

        mContinuationDriver.initialize();

        verify(mModelToken).registerObserver(mContinuationDriver);
        verify(mModelProvider, never()).handleToken(mModelToken);

        mContinuationDriver.bind(mContinuationViewHolder);
        verify(mContinuationViewHolder)
                .bind(eq(mContinuationDriver), any(LoggingListener.class), eq(false));
    }

    @Test
    public void testInitialize_autoConsumeSyntheticTokenOnRestore() {
        when(mModelToken.isSynthetic()).thenReturn(true);

        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, false)
                        .put(ConfigKey.CONSUME_SYNTHETIC_TOKENS, true)
                        .put(ConfigKey.CONSUME_SYNTHETIC_TOKENS_WHILE_RESTORING, true)
                        .build();

        mContinuationDriver = new ContinuationDriverForTest(mBasicLoggingApi, mClock, configuration,
                mContext, mCursorChangedListener, mModelChild, mModelProvider, POSITION,
                mSnackbarApi, mThreadUtils,
                /* restoring= */ true);

        mContinuationDriver.initialize();

        // Token should be immediately handled as we create the ContinuationDriver with
        // forceAutoConsumeSyntheticTokens = true.
        verify(mModelProvider).handleToken(mModelToken);

        mContinuationDriver.onTokenCompleted(
                new TokenCompleted(FakeModelCursor.newBuilder().addCard().build()));

        // The listener should be notified that the token was automatically consumed as it was a
        // synthetic token.
        verify(mCursorChangedListener)
                .onNewChildren(any(ModelChild.class), ArgumentMatchers.<List<ModelChild>>any(),
                        /* wasSynthetic= */ eq(true));
    }

    @Test
    public void testBind_immediatePaginationOn() {
        mContinuationDriver = new ContinuationDriverForTest(mBasicLoggingApi, mClock,
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, true)
                        .build(),
                mContext, mCursorChangedListener, mModelChild, mModelProvider, POSITION,
                mSnackbarApi, mThreadUtils,
                /* restoring= */ false);

        mContinuationDriver.initialize();
        mContinuationDriver.bind(mContinuationViewHolder);
        verify(mContinuationViewHolder)
                .bind(eq(mContinuationDriver), any(LoggingListener.class),
                        /* showSpinner= */ eq(true));
    }

    @Test
    public void testBind_immediatePaginationOff() {
        mContinuationDriver = new ContinuationDriverForTest(mBasicLoggingApi, mClock,
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, false)
                        .build(),
                mContext, mCursorChangedListener, mModelChild, mModelProvider, POSITION,
                mSnackbarApi, mThreadUtils,
                /* restoring= */ false);

        mContinuationDriver.initialize();
        mContinuationDriver.bind(mContinuationViewHolder);
        verify(mContinuationViewHolder)
                .bind(eq(mContinuationDriver), any(LoggingListener.class),
                        /* showSpinner= */ eq(false));
    }

    @Test
    public void testBind_noInitialization() {
        mContinuationDriver = new ContinuationDriverForTest(mBasicLoggingApi, mClock,
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, false)
                        .build(),
                mContext, mCursorChangedListener, mModelChild, mModelProvider, POSITION,
                mSnackbarApi, mThreadUtils,
                /* restoring= */ false);

        assertThatRunnable(() -> mContinuationDriver.bind(mContinuationViewHolder))
                .throwsAnExceptionOfType(IllegalStateException.class);
    }

    @Test
    public void testShowsSpinnerAfterClick() {
        mContinuationDriver.bind(mContinuationViewHolder);

        mContinuationDriver.onClick(mView);

        mContinuationDriver.unbind();

        mContinuationDriver.bind(mContinuationViewHolder);

        verify(mContinuationViewHolder)
                .bind(eq(mContinuationDriver), any(LoggingListener.class),
                        /* showSpinner= */ eq(true));
    }

    @Test
    public void testOnDestroy_doesNotUnregisterTokenObserver_ifNotInitialized() {
        mContinuationDriver = new ContinuationDriverForTest(mBasicLoggingApi, mClock,
                mConfiguration, mContext, mCursorChangedListener, mModelChild, mModelProvider,
                POSITION, mSnackbarApi, mThreadUtils,
                /* restoring= */ false);

        mContinuationDriver.onDestroy();

        verify(mModelToken, never()).unregisterObserver(any());
    }

    @Test
    public void testOnDestroy_unregistersTokenObserver_ifAlreadyInitialized() {
        mContinuationDriver.onDestroy();

        verify(mModelToken).unregisterObserver(mContinuationDriver);
    }

    @Test
    public void testOnClick() {
        mContinuationDriver.bind(mContinuationViewHolder);
        mContinuationDriver.onClick(mView);

        verify(mContinuationViewHolder).setShowSpinner(true);
        verify(mModelProvider).handleToken(mModelToken);
    }

    @Test
    public void testBind_listenerLogsMoreButtonClicked() {
        ArgumentCaptor<LoggingListener> captor = ArgumentCaptor.forClass(LoggingListener.class);
        mContinuationDriver.bind(mContinuationViewHolder);

        verify(mContinuationViewHolder)
                .bind(eq(mContinuationDriver), captor.capture(), anyBoolean());
        captor.getValue().onContentClicked();

        verify(mBasicLoggingApi).onMoreButtonClicked(POSITION);
    }

    @Test
    public void testBind_listenerLogsMoreButtonViewed() {
        ArgumentCaptor<LoggingListener> captor = ArgumentCaptor.forClass(LoggingListener.class);
        mContinuationDriver.bind(mContinuationViewHolder);

        verify(mContinuationViewHolder)
                .bind(eq(mContinuationDriver), captor.capture(), anyBoolean());
        captor.getValue().onViewVisible();

        verify(mBasicLoggingApi).onMoreButtonViewed(POSITION);
    }

    @Test
    public void testBind_listenerDoesNotLogViewTwice() {
        ArgumentCaptor<LoggingListener> captor = ArgumentCaptor.forClass(LoggingListener.class);
        mContinuationDriver.bind(mContinuationViewHolder);

        verify(mContinuationViewHolder)
                .bind(eq(mContinuationDriver), captor.capture(), anyBoolean());
        captor.getValue().onViewVisible();
        reset(mBasicLoggingApi);
        captor.getValue().onViewVisible();

        verify(mBasicLoggingApi, never()).onMoreButtonViewed(anyInt());
    }

    @Test
    public void testBind_listenerDoesNotLogViewIfSpinnerShown() {
        ArgumentCaptor<LoggingListener> captor = ArgumentCaptor.forClass(LoggingListener.class);
        mContinuationDriver = new ContinuationDriverForTest(mBasicLoggingApi, mClock,
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, true)
                        .build(),
                mContext, mCursorChangedListener, mModelChild, mModelProvider, POSITION,
                mSnackbarApi, mThreadUtils,
                /* restoring= */ false);
        mContinuationDriver.initialize();
        mContinuationDriver.bind(mContinuationViewHolder);

        verify(mContinuationViewHolder)
                .bind(eq(mContinuationDriver), captor.capture(), anyBoolean());
        captor.getValue().onViewVisible();

        verify(mBasicLoggingApi, never()).onMoreButtonViewed(anyInt());
    }

    @Test
    public void testUnbind() {
        mContinuationDriver.bind(mContinuationViewHolder);
        assertThat(mContinuationDriver.isBound()).isTrue();
        mContinuationDriver.unbind();

        verify(mContinuationViewHolder).unbind();
        assertThat(mContinuationDriver.isBound()).isFalse();
    }

    @Test
    public void testOnTokenCompleted_checksMainThread() {
        mContinuationDriver.bind(mContinuationViewHolder);

        clickWithTokenCompleted(EMPTY_TOKEN_COMPLETED);

        verify(mThreadUtils).checkMainThread();
    }

    @Test
    public void testOnTokenCompleted_afterDestroy() {
        mContinuationDriver.onDestroy();

        mContinuationDriver.onTokenCompleted(EMPTY_TOKEN_COMPLETED);

        verify(mCursorChangedListener, never())
                .onNewChildren(any(ModelChild.class), ArgumentMatchers.<List<ModelChild>>any(),
                        /* wasSynthetic= */ eq(false));
    }

    @Test
    public void testOnTokenCompleted_extractsModelChildren() {
        FakeModelCursor cursor =
                FakeModelCursor.newBuilder().addCard().addCluster().addToken().build();

        mContinuationDriver.bind(mContinuationViewHolder);
        clickWithTokenCompleted(new TokenCompleted(cursor));

        verify(mCursorChangedListener)
                .onNewChildren(mModelChild, cursor.getModelChildren(), /* wasSynthetic= */ false);
        verifyNoMoreInteractions(mSnackbarApi);
    }

    @Test
    public void testOnTokenCompleted_createsSnackbar_whenCursorReturnsEmptyPage() {
        mContinuationDriver.bind(mContinuationViewHolder);

        clickWithTokenCompleted(EMPTY_TOKEN_COMPLETED);

        verify(mSnackbarApi)
                .show(mContext.getString(R.string.ntp_suggestions_fetch_no_new_suggestions));
        verify(mCursorChangedListener)
                .onNewChildren(mModelChild, ImmutableList.of(), /* wasSynthetic= */ false);
    }

    @Test
    public void testOnTokenCompleted_createsSnackbar_whenCursorJustReturnsToken() {
        mContinuationDriver.bind(mContinuationViewHolder);

        FakeModelCursor cursor = FakeModelCursor.newBuilder().addToken().build();
        clickWithTokenCompleted(new TokenCompleted(cursor));

        verify(mSnackbarApi)
                .show(mContext.getString(R.string.ntp_suggestions_fetch_no_new_suggestions));
        verify(mCursorChangedListener)
                .onNewChildren(mModelChild, cursor.getModelChildren(), /* wasSynthetic= */ false);
    }

    @Test
    public void testOnTokenCompleted_logsSpinnerFinished() {
        FakeModelCursor cursor = FakeModelCursor.newBuilder().addToken().build();

        mContinuationDriver.bind(mContinuationViewHolder);
        clickWithTokenCompleted(new TokenCompleted(cursor));

        verify(mBasicLoggingApi)
                .onSpinnerFinished(/* timeShownMs= */ anyInt(), /* spinnerType= */ anyInt());
    }

    @Test
    public void testOnTokenCompleted_doesNotCreateSnackbar_whenTokenFromOthertab() {
        FakeModelCursor cursor = FakeModelCursor.newBuilder().addToken().build();
        mContinuationDriver.onTokenCompleted(new TokenCompleted(cursor));

        verifyNoMoreInteractions(mSnackbarApi);
        verify(mCursorChangedListener)
                .onNewChildren(mModelChild, cursor.getModelChildren(), /* wasSynthetic= */ false);
    }

    @Test
    public void testOnError() {
        mContinuationDriver.bind(mContinuationViewHolder);

        clickWithError();

        verify(mContinuationViewHolder).setShowSpinner(false);
        verify(mSnackbarApi).show(mContext.getString(R.string.ntp_suggestions_fetch_failed));
    }

    @Test
    public void testOnError_hidesSpinnerAfterBeingBound() {
        mContinuationDriver = new ContinuationDriverForTest(mBasicLoggingApi, mClock,
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, true)
                        .build(),
                mContext, mCursorChangedListener, mModelChild, mModelProvider, POSITION,
                mSnackbarApi, mThreadUtils,
                /* restoring= */ false);

        mContinuationDriver.initialize();
        mContinuationDriver.bind(mContinuationViewHolder);
        verify(mContinuationViewHolder)
                .bind(eq(mContinuationDriver), any(LoggingListener.class), eq(true));

        mContinuationDriver.unbind();
        mContinuationDriver.onError(new ModelError(ErrorType.PAGINATION_ERROR, null));
        mContinuationDriver.bind(mContinuationViewHolder);

        verify(mContinuationViewHolder)
                .bind(eq(mContinuationDriver), any(LoggingListener.class), eq(false));
        verify(mContinuationViewHolder, never()).setShowSpinner(anyBoolean());
    }

    @Test
    public void testBind_logsSpinnerStarted_afterInitializeWithSyntheticToken() {
        when(mModelToken.isSynthetic()).thenReturn(true);
        mContinuationDriver = new ContinuationDriverForTest(mBasicLoggingApi, mClock,
                new Configuration.Builder().put(ConfigKey.CONSUME_SYNTHETIC_TOKENS, true).build(),
                mContext, mCursorChangedListener, mModelChild, mModelProvider, POSITION,
                mSnackbarApi, mThreadUtils,
                /* restoring= */ true);
        mContinuationDriver.initialize();

        mContinuationDriver.bind(mContinuationViewHolder);

        assertThat(mSpinnerLogger.getSpinnerType()).isEqualTo(SpinnerType.SYNTHETIC_TOKEN);
    }

    @Test
    public void testBind_logsSpinnerStarted_ifTriggerImmediatePaginationEnabled() {
        mContinuationDriver = new ContinuationDriverForTest(mBasicLoggingApi, mClock,
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, true)
                        .build(),
                mContext, mCursorChangedListener, mModelChild, mModelProvider, POSITION,
                mSnackbarApi, mThreadUtils,
                /* restoring= */ true);
        mContinuationDriver.initialize();

        mContinuationDriver.bind(mContinuationViewHolder);

        assertThat(mSpinnerLogger.getSpinnerType()).isEqualTo(SpinnerType.INFINITE_FEED);
    }

    @Test
    public void testOnClick_logsSpinnerStarted() {
        mContinuationDriver.bind(mContinuationViewHolder);

        mContinuationDriver.onClick(mView);

        assertThat(mSpinnerLogger.getSpinnerType()).isEqualTo(SpinnerType.MORE_BUTTON);
    }

    @Test
    public void testOnClick_logsErrorForUnhandledToken() {
        when(mModelProvider.handleToken(any(ModelToken.class))).thenReturn(false);
        mContinuationDriver.bind(mContinuationViewHolder);

        mContinuationDriver.onClick(mView);

        verify(mBasicLoggingApi).onInternalError(InternalFeedError.UNHANDLED_TOKEN);
        verify(mContinuationViewHolder).setShowSpinner(false);
        verify(mSnackbarApi).show(mContext.getString(R.string.ntp_suggestions_fetch_failed));
    }

    @Test
    public void testOnError_logsSpinnerFinished() {
        mContinuationDriver.bind(mContinuationViewHolder);

        clickWithError();

        verify(mBasicLoggingApi)
                .onSpinnerFinished(/* timeShownMs= */ anyInt(), /* spinnerType= */ anyInt());
    }

    @Test
    public void testOnError_logsTokenError() {
        mContinuationDriver.bind(mContinuationViewHolder);

        clickWithError();

        verify(mBasicLoggingApi)
                .onTokenFailedToComplete(/* wasSynthetic= */ false, /*failureCount=*/1);
    }

    @Test
    public void testOnError_logsTokenError_incrementsFailureCount() {
        mContinuationDriver.bind(mContinuationViewHolder);

        clickWithError();
        clickWithError();
        clickWithError();

        InOrder inOrder = Mockito.inOrder(mBasicLoggingApi);
        inOrder.verify(mBasicLoggingApi).onTokenFailedToComplete(/* wasSynthetic= */ false, 1);
        inOrder.verify(mBasicLoggingApi).onTokenFailedToComplete(/* wasSynthetic= */ false, 2);
        inOrder.verify(mBasicLoggingApi).onTokenFailedToComplete(/* wasSynthetic= */ false, 3);
    }

    @Test
    public void testOnError_syntheticToken_logsTokenError() {
        when(mModelToken.isSynthetic()).thenReturn(true);

        mContinuationDriver.bind(mContinuationViewHolder);

        clickWithError();

        verify(mBasicLoggingApi)
                .onTokenFailedToComplete(/* wasSynthetic= */ true, /* failureCount= */ 1);
    }

    @Test
    public void testOnDestroy_logsSpinnerFinished_ifSpinnerActive() {
        mContinuationDriver.bind(mContinuationViewHolder);

        mContinuationDriver.onClick(mView);
        mContinuationDriver.onDestroy();

        verify(mBasicLoggingApi)
                .onSpinnerDestroyedWithoutCompleting(
                        /* timeShownMs= */ anyInt(), /* spinnerType= */ anyInt());
    }

    @Test
    public void testOnDestroy_doesNotLogSpinnerFinished_ifSpinnerNotActive() {
        when(mSpinnerLogger.isSpinnerActive()).thenReturn(false);
        mContinuationDriver.bind(mContinuationViewHolder);

        mContinuationDriver.onDestroy();

        verify(mBasicLoggingApi, never())
                .onSpinnerDestroyedWithoutCompleting(
                        /* timeShownMs= */ anyInt(), /* spinnerType= */ anyInt());
    }

    @Test
    public void testMaybeRebind() {
        mContinuationDriver.bind(mContinuationViewHolder);
        mContinuationDriver.maybeRebind();
        verify(mContinuationViewHolder, times(2))
                .bind(eq(mContinuationDriver), any(LoggingListener.class), eq(false));
        verify(mContinuationViewHolder).unbind();
    }

    @Test
    public void testMaybeRebind_nullViewHolder() {
        // bind/unbind continuationViewHolder so we can then test the driver (and assume the view
        // holder is null)
        mContinuationDriver.bind(mContinuationViewHolder);
        mContinuationDriver.unbind();
        reset(mContinuationViewHolder);

        mContinuationDriver.maybeRebind();
        verify(mContinuationViewHolder, never()).unbind();
        verify(mContinuationViewHolder, never())
                .bind(any(OnClickListener.class), any(LoggingListener.class), anyBoolean());
    }

    private void clickWithError() {
        mContinuationDriver.onClick(mView);
        mContinuationDriver.onError(new ModelError(ErrorType.PAGINATION_ERROR, null));
    }

    private void clickWithTokenCompleted(TokenCompleted tokenCompleted) {
        mContinuationDriver.onClick(mView);
        mContinuationDriver.onTokenCompleted(tokenCompleted);
    }

    private final class ContinuationDriverForTest extends ContinuationDriver {
        ContinuationDriverForTest(BasicLoggingApi basicLoggingApi, Clock clock,
                Configuration configuration, Context context,
                CursorChangedListener cursorChangedListener, ModelChild modelChild,
                ModelProvider modelProvider, int position, SnackbarApi snackbarApi,
                ThreadUtils threadUtils, boolean restoring) {
            super(basicLoggingApi, clock, configuration, context, cursorChangedListener, modelChild,
                    modelProvider, position, snackbarApi, threadUtils, restoring);
        }

        @Override
        SpinnerLogger createSpinnerLogger(BasicLoggingApi basicLoggingApi, Clock clock) {
            return mSpinnerLogger;
        }
    }
}
