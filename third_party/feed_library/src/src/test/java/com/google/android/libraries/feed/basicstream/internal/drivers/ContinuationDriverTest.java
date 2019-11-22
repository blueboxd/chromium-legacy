// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal.drivers;

import static com.google.android.libraries.feed.common.testing.RunnableSubject.assertThatRunnable;
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

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.view.View.OnClickListener;

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.InternalFeedError;
import com.google.android.libraries.feed.api.host.logging.SpinnerType;
import com.google.android.libraries.feed.api.host.stream.SnackbarApi;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelError;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelError.ErrorType;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelToken;
import com.google.android.libraries.feed.api.internal.modelprovider.TokenCompleted;
import com.google.android.libraries.feed.basicstream.internal.drivers.ContinuationDriver.CursorChangedListener;
import com.google.android.libraries.feed.basicstream.internal.viewholders.ContinuationViewHolder;
import com.google.android.libraries.feed.basicstream.internal.viewholders.R;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.sharedstream.logging.LoggingListener;
import com.google.android.libraries.feed.sharedstream.logging.SpinnerLogger;
import com.google.android.libraries.feed.testing.modelprovider.FakeModelCursor;
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
import org.robolectric.RobolectricTestRunner;

import java.util.List;

/** Tests for {@link ContinuationDriver}.j */
@RunWith(RobolectricTestRunner.class)
public class ContinuationDriverTest {
    private static final int POSITION = 1;
    public static final TokenCompleted EMPTY_TOKEN_COMPLETED =
            new TokenCompleted(FakeModelCursor.newBuilder().build());

    @Mock
    private BasicLoggingApi basicLoggingApi;
    @Mock
    private ModelChild modelChild;
    @Mock
    private ModelProvider modelProvider;
    @Mock
    private ModelToken modelToken;
    @Mock
    private SnackbarApi snackbarApi;
    @Mock
    private ThreadUtils threadUtils;
    @Mock
    private CursorChangedListener cursorChangedListener;
    @Mock
    private ContinuationViewHolder continuationViewHolder;

    private SpinnerLogger spinnerLogger;
    private Clock clock;
    private Context context;
    private ContinuationDriver continuationDriver;
    private Configuration configuration = new Configuration.Builder().build();
    private View view;

    @Before
    public void setup() {
        initMocks(this);
        clock = new FakeClock();
        context = Robolectric.buildActivity(Activity.class).get();
        view = new View(context);
        when(modelProvider.handleToken(any(ModelToken.class))).thenReturn(true);
        when(modelChild.getModelToken()).thenReturn(modelToken);
        spinnerLogger = new SpinnerLogger(basicLoggingApi, clock);

        continuationDriver = new ContinuationDriverForTest(basicLoggingApi, clock, configuration,
                context, cursorChangedListener, modelChild, modelProvider, POSITION, snackbarApi,
                threadUtils,
                /* restoring= */ false);
        continuationDriver.initialize();
    }

    @Test
    public void testHasTokenBeenHandled_returnsTrue_ifSyntheticTokenConsumedImmediately() {
        when(modelToken.isSynthetic()).thenReturn(true);
        continuationDriver = new ContinuationDriverForTest(basicLoggingApi, clock,
                new Configuration.Builder().put(ConfigKey.CONSUME_SYNTHETIC_TOKENS, true).build(),
                context, cursorChangedListener, modelChild, modelProvider, POSITION, snackbarApi,
                threadUtils,
                /* restoring= */ false);
        continuationDriver.initialize();

        assertThat(continuationDriver.hasTokenBeenHandled()).isTrue();
    }

    @Test
    public void testHasTokenBeenHandled_returnsFalse_ifSyntheticTokenNotConsumedImmediately() {
        when(modelToken.isSynthetic()).thenReturn(true);
        continuationDriver = new ContinuationDriverForTest(basicLoggingApi, clock, configuration,
                context, cursorChangedListener, modelChild, modelProvider, POSITION, snackbarApi,
                threadUtils,
                /* restoring= */ false);
        continuationDriver.initialize();

        assertThat(continuationDriver.hasTokenBeenHandled()).isFalse();
    }

    @Test
    public void testInitialize_nonSyntheticToken() {
        continuationDriver = new ContinuationDriverForTest(basicLoggingApi, clock,
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, true)
                        .build(),
                context, cursorChangedListener, modelChild, modelProvider, POSITION, snackbarApi,
                threadUtils,
                /* restoring= */ false);
        continuationDriver.initialize();

        verify(modelToken).registerObserver(continuationDriver);
        verify(modelProvider, never()).handleToken(any(ModelToken.class));
    }

    @Test
    public void testInitialize_syntheticTokenConsume() {
        when(modelToken.isSynthetic()).thenReturn(true);
        continuationDriver = new ContinuationDriverForTest(basicLoggingApi, clock,
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, true)
                        .put(ConfigKey.CONSUME_SYNTHETIC_TOKENS, true)
                        .build(),
                context, cursorChangedListener, modelChild, modelProvider, POSITION, snackbarApi,
                threadUtils,
                /* restoring= */ false);
        continuationDriver.initialize();

        // Call initialize again to make sure it doesn't duplicate work
        continuationDriver.initialize();

        verify(modelToken).registerObserver(continuationDriver);
        verify(modelProvider).handleToken(modelToken);

        continuationDriver.bind(continuationViewHolder);
        verify(continuationViewHolder)
                .bind(eq(continuationDriver), any(LoggingListener.class), eq(true));
    }

    @Test
    public void testInitialize_syntheticTokenConsume_logsErrorForUnhandledToken() {
        when(modelToken.isSynthetic()).thenReturn(true);
        when(modelProvider.handleToken(any(ModelToken.class))).thenReturn(false);
        continuationDriver = new ContinuationDriverForTest(basicLoggingApi, clock,
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, true)
                        .put(ConfigKey.CONSUME_SYNTHETIC_TOKENS, true)
                        .build(),
                context, cursorChangedListener, modelChild, modelProvider, POSITION, snackbarApi,
                threadUtils,
                /* restoring= */ false);
        continuationDriver.initialize();

        verify(basicLoggingApi).onInternalError(InternalFeedError.UNHANDLED_TOKEN);
    }

    @Test
    public void testInitialize_syntheticTokenNotConsumed() {
        when(modelToken.isSynthetic()).thenReturn(true);
        continuationDriver = new ContinuationDriverForTest(basicLoggingApi, clock,
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, true)
                        .put(ConfigKey.CONSUME_SYNTHETIC_TOKENS, false)
                        .build(),
                context, cursorChangedListener, modelChild, modelProvider, POSITION, snackbarApi,
                threadUtils,
                /* restoring= */ false);
        continuationDriver.initialize();

        // Call initialize again to make sure it doesn't duplicate work
        continuationDriver.initialize();

        verify(modelToken).registerObserver(continuationDriver);
        verify(modelProvider, never()).handleToken(modelToken);

        continuationDriver.bind(continuationViewHolder);
        verify(continuationViewHolder)
                .bind(eq(continuationDriver), any(LoggingListener.class), eq(true));
    }

    @Test
    public void testInitialize_doesNotAutoConsumeSyntheticTokenOnRestore() {
        when(modelToken.isSynthetic()).thenReturn(true);

        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, false)
                        .put(ConfigKey.CONSUME_SYNTHETIC_TOKENS, false)
                        .put(ConfigKey.CONSUME_SYNTHETIC_TOKENS_WHILE_RESTORING, false)
                        .build();

        continuationDriver = new ContinuationDriverForTest(basicLoggingApi, clock, configuration,
                context, cursorChangedListener, modelChild, modelProvider, POSITION, snackbarApi,
                threadUtils,
                /* restoring= */ true);

        continuationDriver.initialize();

        verify(modelToken).registerObserver(continuationDriver);
        verify(modelProvider, never()).handleToken(modelToken);

        continuationDriver.bind(continuationViewHolder);
        verify(continuationViewHolder)
                .bind(eq(continuationDriver), any(LoggingListener.class), eq(false));
    }

    @Test
    public void testInitialize_autoConsumeSyntheticTokenOnRestore() {
        when(modelToken.isSynthetic()).thenReturn(true);

        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, false)
                        .put(ConfigKey.CONSUME_SYNTHETIC_TOKENS, true)
                        .put(ConfigKey.CONSUME_SYNTHETIC_TOKENS_WHILE_RESTORING, true)
                        .build();

        continuationDriver = new ContinuationDriverForTest(basicLoggingApi, clock, configuration,
                context, cursorChangedListener, modelChild, modelProvider, POSITION, snackbarApi,
                threadUtils,
                /* restoring= */ true);

        continuationDriver.initialize();

        // Token should be immediately handled as we create the ContinuationDriver with
        // forceAutoConsumeSyntheticTokens = true.
        verify(modelProvider).handleToken(modelToken);

        continuationDriver.onTokenCompleted(
                new TokenCompleted(FakeModelCursor.newBuilder().addCard().build()));

        // The listener should be notified that the token was automatically consumed as it was a
        // synthetic token.
        verify(cursorChangedListener)
                .onNewChildren(any(ModelChild.class), ArgumentMatchers.<List<ModelChild>>any(),
                        /* wasSynthetic= */ eq(true));
    }

    @Test
    public void testBind_immediatePaginationOn() {
        continuationDriver = new ContinuationDriverForTest(basicLoggingApi, clock,
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, true)
                        .build(),
                context, cursorChangedListener, modelChild, modelProvider, POSITION, snackbarApi,
                threadUtils,
                /* restoring= */ false);

        continuationDriver.initialize();
        continuationDriver.bind(continuationViewHolder);
        verify(continuationViewHolder)
                .bind(eq(continuationDriver), any(LoggingListener.class),
                        /* showSpinner= */ eq(true));
    }

    @Test
    public void testBind_immediatePaginationOff() {
        continuationDriver = new ContinuationDriverForTest(basicLoggingApi, clock,
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, false)
                        .build(),
                context, cursorChangedListener, modelChild, modelProvider, POSITION, snackbarApi,
                threadUtils,
                /* restoring= */ false);

        continuationDriver.initialize();
        continuationDriver.bind(continuationViewHolder);
        verify(continuationViewHolder)
                .bind(eq(continuationDriver), any(LoggingListener.class),
                        /* showSpinner= */ eq(false));
    }

    @Test
    public void testBind_noInitialization() {
        continuationDriver = new ContinuationDriverForTest(basicLoggingApi, clock,
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, false)
                        .build(),
                context, cursorChangedListener, modelChild, modelProvider, POSITION, snackbarApi,
                threadUtils,
                /* restoring= */ false);

        assertThatRunnable(() -> continuationDriver.bind(continuationViewHolder))
                .throwsAnExceptionOfType(IllegalStateException.class);
    }

    @Test
    public void testShowsSpinnerAfterClick() {
        continuationDriver.bind(continuationViewHolder);

        continuationDriver.onClick(view);

        continuationDriver.unbind();

        continuationDriver.bind(continuationViewHolder);

        verify(continuationViewHolder)
                .bind(eq(continuationDriver), any(LoggingListener.class),
                        /* showSpinner= */ eq(true));
    }

    @Test
    public void testOnDestroy_doesNotUnregisterTokenObserver_ifNotInitialized() {
        continuationDriver = new ContinuationDriverForTest(basicLoggingApi, clock, configuration,
                context, cursorChangedListener, modelChild, modelProvider, POSITION, snackbarApi,
                threadUtils,
                /* restoring= */ false);

        continuationDriver.onDestroy();

        verify(modelToken, never()).unregisterObserver(any());
    }

    @Test
    public void testOnDestroy_unregistersTokenObserver_ifAlreadyInitialized() {
        continuationDriver.onDestroy();

        verify(modelToken).unregisterObserver(continuationDriver);
    }

    @Test
    public void testOnClick() {
        continuationDriver.bind(continuationViewHolder);
        continuationDriver.onClick(view);

        verify(continuationViewHolder).setShowSpinner(true);
        verify(modelProvider).handleToken(modelToken);
    }

    @Test
    public void testBind_listenerLogsMoreButtonClicked() {
        ArgumentCaptor<LoggingListener> captor = ArgumentCaptor.forClass(LoggingListener.class);
        continuationDriver.bind(continuationViewHolder);

        verify(continuationViewHolder).bind(eq(continuationDriver), captor.capture(), anyBoolean());
        captor.getValue().onContentClicked();

        verify(basicLoggingApi).onMoreButtonClicked(POSITION);
    }

    @Test
    public void testBind_listenerLogsMoreButtonViewed() {
        ArgumentCaptor<LoggingListener> captor = ArgumentCaptor.forClass(LoggingListener.class);
        continuationDriver.bind(continuationViewHolder);

        verify(continuationViewHolder).bind(eq(continuationDriver), captor.capture(), anyBoolean());
        captor.getValue().onViewVisible();

        verify(basicLoggingApi).onMoreButtonViewed(POSITION);
    }

    @Test
    public void testBind_listenerDoesNotLogViewTwice() {
        ArgumentCaptor<LoggingListener> captor = ArgumentCaptor.forClass(LoggingListener.class);
        continuationDriver.bind(continuationViewHolder);

        verify(continuationViewHolder).bind(eq(continuationDriver), captor.capture(), anyBoolean());
        captor.getValue().onViewVisible();
        reset(basicLoggingApi);
        captor.getValue().onViewVisible();

        verify(basicLoggingApi, never()).onMoreButtonViewed(anyInt());
    }

    @Test
    public void testBind_listenerDoesNotLogViewIfSpinnerShown() {
        ArgumentCaptor<LoggingListener> captor = ArgumentCaptor.forClass(LoggingListener.class);
        continuationDriver = new ContinuationDriverForTest(basicLoggingApi, clock,
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, true)
                        .build(),
                context, cursorChangedListener, modelChild, modelProvider, POSITION, snackbarApi,
                threadUtils,
                /* restoring= */ false);
        continuationDriver.initialize();
        continuationDriver.bind(continuationViewHolder);

        verify(continuationViewHolder).bind(eq(continuationDriver), captor.capture(), anyBoolean());
        captor.getValue().onViewVisible();

        verify(basicLoggingApi, never()).onMoreButtonViewed(anyInt());
    }

    @Test
    public void testUnbind() {
        continuationDriver.bind(continuationViewHolder);
        assertThat(continuationDriver.isBound()).isTrue();
        continuationDriver.unbind();

        verify(continuationViewHolder).unbind();
        assertThat(continuationDriver.isBound()).isFalse();
    }

    @Test
    public void testOnTokenCompleted_checksMainThread() {
        continuationDriver.bind(continuationViewHolder);

        clickWithTokenCompleted(EMPTY_TOKEN_COMPLETED);

        verify(threadUtils).checkMainThread();
    }

    @Test
    public void testOnTokenCompleted_afterDestroy() {
        continuationDriver.onDestroy();

        continuationDriver.onTokenCompleted(EMPTY_TOKEN_COMPLETED);

        verify(cursorChangedListener, never())
                .onNewChildren(any(ModelChild.class), ArgumentMatchers.<List<ModelChild>>any(),
                        /* wasSynthetic= */ eq(false));
    }

    @Test
    public void testOnTokenCompleted_extractsModelChildren() {
        FakeModelCursor cursor =
                FakeModelCursor.newBuilder().addCard().addCluster().addToken().build();

        continuationDriver.bind(continuationViewHolder);
        clickWithTokenCompleted(new TokenCompleted(cursor));

        verify(cursorChangedListener)
                .onNewChildren(modelChild, cursor.getModelChildren(), /* wasSynthetic= */ false);
        verifyNoMoreInteractions(snackbarApi);
    }

    @Test
    public void testOnTokenCompleted_createsSnackbar_whenCursorReturnsEmptyPage() {
        continuationDriver.bind(continuationViewHolder);

        clickWithTokenCompleted(EMPTY_TOKEN_COMPLETED);

        verify(snackbarApi).show(context.getString(R.string.snackbar_fetch_no_new_suggestions));
        verify(cursorChangedListener)
                .onNewChildren(modelChild, ImmutableList.of(), /* wasSynthetic= */ false);
    }

    @Test
    public void testOnTokenCompleted_createsSnackbar_whenCursorJustReturnsToken() {
        continuationDriver.bind(continuationViewHolder);

        FakeModelCursor cursor = FakeModelCursor.newBuilder().addToken().build();
        clickWithTokenCompleted(new TokenCompleted(cursor));

        verify(snackbarApi).show(context.getString(R.string.snackbar_fetch_no_new_suggestions));
        verify(cursorChangedListener)
                .onNewChildren(modelChild, cursor.getModelChildren(), /* wasSynthetic= */ false);
    }

    @Test
    public void testOnTokenCompleted_logsSpinnerFinished() {
        FakeModelCursor cursor = FakeModelCursor.newBuilder().addToken().build();

        continuationDriver.bind(continuationViewHolder);
        clickWithTokenCompleted(new TokenCompleted(cursor));

        verify(basicLoggingApi)
                .onSpinnerFinished(/* timeShownMs= */ anyInt(), /* spinnerType= */ anyInt());
    }

    @Test
    public void testOnTokenCompleted_doesNotCreateSnackbar_whenTokenFromOthertab() {
        FakeModelCursor cursor = FakeModelCursor.newBuilder().addToken().build();
        continuationDriver.onTokenCompleted(new TokenCompleted(cursor));

        verifyNoMoreInteractions(snackbarApi);
        verify(cursorChangedListener)
                .onNewChildren(modelChild, cursor.getModelChildren(), /* wasSynthetic= */ false);
    }

    @Test
    public void testOnError() {
        continuationDriver.bind(continuationViewHolder);

        clickWithError();

        verify(continuationViewHolder).setShowSpinner(false);
        verify(snackbarApi).show(context.getString(R.string.snackbar_fetch_failed));
    }

    @Test
    public void testOnError_hidesSpinnerAfterBeingBound() {
        continuationDriver = new ContinuationDriverForTest(basicLoggingApi, clock,
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, true)
                        .build(),
                context, cursorChangedListener, modelChild, modelProvider, POSITION, snackbarApi,
                threadUtils,
                /* restoring= */ false);

        continuationDriver.initialize();
        continuationDriver.bind(continuationViewHolder);
        verify(continuationViewHolder)
                .bind(eq(continuationDriver), any(LoggingListener.class), eq(true));

        continuationDriver.unbind();
        continuationDriver.onError(new ModelError(ErrorType.PAGINATION_ERROR, null));
        continuationDriver.bind(continuationViewHolder);

        verify(continuationViewHolder)
                .bind(eq(continuationDriver), any(LoggingListener.class), eq(false));
        verify(continuationViewHolder, never()).setShowSpinner(anyBoolean());
    }

    @Test
    public void testBind_logsSpinnerStarted_afterInitializeWithSyntheticToken() {
        when(modelToken.isSynthetic()).thenReturn(true);
        continuationDriver = new ContinuationDriverForTest(basicLoggingApi, clock,
                new Configuration.Builder().put(ConfigKey.CONSUME_SYNTHETIC_TOKENS, true).build(),
                context, cursorChangedListener, modelChild, modelProvider, POSITION, snackbarApi,
                threadUtils,
                /* restoring= */ true);
        continuationDriver.initialize();

        continuationDriver.bind(continuationViewHolder);

        assertThat(spinnerLogger.getSpinnerType()).isEqualTo(SpinnerType.SYNTHETIC_TOKEN);
    }

    @Test
    public void testBind_logsSpinnerStarted_ifTriggerImmediatePaginationEnabled() {
        continuationDriver = new ContinuationDriverForTest(basicLoggingApi, clock,
                new Configuration.Builder()
                        .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, true)
                        .build(),
                context, cursorChangedListener, modelChild, modelProvider, POSITION, snackbarApi,
                threadUtils,
                /* restoring= */ true);
        continuationDriver.initialize();

        continuationDriver.bind(continuationViewHolder);

        assertThat(spinnerLogger.getSpinnerType()).isEqualTo(SpinnerType.INFINITE_FEED);
    }

    @Test
    public void testOnClick_logsSpinnerStarted() {
        continuationDriver.bind(continuationViewHolder);

        continuationDriver.onClick(view);

        assertThat(spinnerLogger.getSpinnerType()).isEqualTo(SpinnerType.MORE_BUTTON);
    }

    @Test
    public void testOnClick_logsErrorForUnhandledToken() {
        when(modelProvider.handleToken(any(ModelToken.class))).thenReturn(false);
        continuationDriver.bind(continuationViewHolder);

        continuationDriver.onClick(view);

        verify(basicLoggingApi).onInternalError(InternalFeedError.UNHANDLED_TOKEN);
        verify(continuationViewHolder).setShowSpinner(false);
        verify(snackbarApi).show(context.getString(R.string.snackbar_fetch_failed));
    }

    @Test
    public void testOnError_logsSpinnerFinished() {
        continuationDriver.bind(continuationViewHolder);

        clickWithError();

        verify(basicLoggingApi)
                .onSpinnerFinished(/* timeShownMs= */ anyInt(), /* spinnerType= */ anyInt());
    }

    @Test
    public void testOnError_logsTokenError() {
        continuationDriver.bind(continuationViewHolder);

        clickWithError();

        verify(basicLoggingApi)
                .onTokenFailedToComplete(/* wasSynthetic= */ false, /*failureCount=*/1);
    }

    @Test
    public void testOnError_logsTokenError_incrementsFailureCount() {
        continuationDriver.bind(continuationViewHolder);

        clickWithError();
        clickWithError();
        clickWithError();

        InOrder inOrder = Mockito.inOrder(basicLoggingApi);
        inOrder.verify(basicLoggingApi).onTokenFailedToComplete(/* wasSynthetic= */ false, 1);
        inOrder.verify(basicLoggingApi).onTokenFailedToComplete(/* wasSynthetic= */ false, 2);
        inOrder.verify(basicLoggingApi).onTokenFailedToComplete(/* wasSynthetic= */ false, 3);
    }

    @Test
    public void testOnError_syntheticToken_logsTokenError() {
        when(modelToken.isSynthetic()).thenReturn(true);

        continuationDriver.bind(continuationViewHolder);

        clickWithError();

        verify(basicLoggingApi)
                .onTokenFailedToComplete(/* wasSynthetic= */ true, /* failureCount= */ 1);
    }

    @Test
    public void testOnDestroy_logsSpinnerFinished_ifSpinnerActive() {
        continuationDriver.bind(continuationViewHolder);

        continuationDriver.onClick(view);
        continuationDriver.onDestroy();

        verify(basicLoggingApi)
                .onSpinnerDestroyedWithoutCompleting(
                        /* timeShownMs= */ anyInt(), /* spinnerType= */ anyInt());
    }

    @Test
    public void testOnDestroy_doesNotLogSpinnerFinished_ifSpinnerNotActive() {
        when(spinnerLogger.isSpinnerActive()).thenReturn(false);
        continuationDriver.bind(continuationViewHolder);

        continuationDriver.onDestroy();

        verify(basicLoggingApi, never())
                .onSpinnerDestroyedWithoutCompleting(
                        /* timeShownMs= */ anyInt(), /* spinnerType= */ anyInt());
    }

    @Test
    public void testMaybeRebind() {
        continuationDriver.bind(continuationViewHolder);
        continuationDriver.maybeRebind();
        verify(continuationViewHolder, times(2))
                .bind(eq(continuationDriver), any(LoggingListener.class), eq(false));
        verify(continuationViewHolder).unbind();
    }

    @Test
    public void testMaybeRebind_nullViewHolder() {
        // bind/unbind continuationViewHolder so we can then test the driver (and assume the view
        // holder is null)
        continuationDriver.bind(continuationViewHolder);
        continuationDriver.unbind();
        reset(continuationViewHolder);

        continuationDriver.maybeRebind();
        verify(continuationViewHolder, never()).unbind();
        verify(continuationViewHolder, never())
                .bind(any(OnClickListener.class), any(LoggingListener.class), anyBoolean());
    }

    private void clickWithError() {
        continuationDriver.onClick(view);
        continuationDriver.onError(new ModelError(ErrorType.PAGINATION_ERROR, null));
    }

    private void clickWithTokenCompleted(TokenCompleted tokenCompleted) {
        continuationDriver.onClick(view);
        continuationDriver.onTokenCompleted(tokenCompleted);
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
            return spinnerLogger;
        }
    }
}
