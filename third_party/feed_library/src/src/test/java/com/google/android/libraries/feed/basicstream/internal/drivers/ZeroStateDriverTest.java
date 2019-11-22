// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal.drivers;

import static com.google.android.libraries.feed.common.testing.RunnableSubject.assertThatRunnable;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
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
import android.view.View.OnClickListener;
import android.widget.FrameLayout;

import com.google.android.libraries.feed.api.client.stream.Stream.ContentChangedListener;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.RequestReason;
import com.google.android.libraries.feed.api.host.logging.SpinnerType;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.basicstream.internal.viewholders.ZeroStateViewHolder;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.sharedstream.logging.SpinnerLogger;
import com.google.android.libraries.feed.sharedstream.proto.UiRefreshReasonProto.UiRefreshReason;
import com.google.android.libraries.feed.sharedstream.proto.UiRefreshReasonProto.UiRefreshReason.Reason;
import com.google.search.now.feed.client.StreamDataProto.UiContext;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link ZeroStateDriver}. */
@RunWith(RobolectricTestRunner.class)
public class ZeroStateDriverTest {
    private static final UiContext ZERO_STATE_UI_CONTEXT =
            UiContext.newBuilder()
                    .setExtension(UiRefreshReason.uiRefreshReasonExtension,
                            UiRefreshReason.newBuilder().setReason(Reason.ZERO_STATE).build())
                    .build();

    @Mock
    private BasicLoggingApi basicLoggingApi;
    @Mock
    private ContentChangedListener contentChangedListener;
    @Mock
    private ModelProvider modelProvider;
    @Mock
    private SpinnerLogger spinnerLogger;
    @Mock
    private ZeroStateViewHolder zeroStateViewHolder;

    private ZeroStateDriver zeroStateDriver;
    private Clock clock;
    private Context context;

    @Before
    public void setup() {
        initMocks(this);
        clock = new FakeClock();
        context = Robolectric.buildActivity(Activity.class).get();

        zeroStateDriver = new ZeroStateDriverForTest(basicLoggingApi, clock, modelProvider,
                contentChangedListener,
                /* spinnerShown= */ false);
    }

    @Test
    public void testBind() {
        assertThat(zeroStateDriver.isBound()).isFalse();

        zeroStateDriver.bind(zeroStateViewHolder);

        assertThat(zeroStateDriver.isBound()).isTrue();
        verify(zeroStateViewHolder).bind(zeroStateDriver, /* showSpinner= */ false);
    }

    @Test
    public void testBind_whenSpinnerShownTrue() {
        zeroStateDriver = new ZeroStateDriverForTest(basicLoggingApi, clock, modelProvider,
                contentChangedListener,
                /* spinnerShown= */ true);

        zeroStateDriver.bind(zeroStateViewHolder);
        verify(zeroStateViewHolder).bind(zeroStateDriver, /* showSpinner= */ true);
    }

    @Test
    public void testBindAfterOnClick_bindsViewholderWithSpinnerShown() {
        InOrder inOrder = Mockito.inOrder(zeroStateViewHolder);
        zeroStateDriver.bind(zeroStateViewHolder);
        inOrder.verify(zeroStateViewHolder).bind(zeroStateDriver, /* showSpinner= */ false);

        zeroStateDriver.onClick(new FrameLayout(context));
        zeroStateDriver.unbind();

        zeroStateDriver.bind(zeroStateViewHolder);

        inOrder.verify(zeroStateViewHolder).bind(zeroStateDriver, /* showSpinner= */ true);
    }

    @Test
    public void testBind_startsRecordingSpinner_ifSpinnerIsShownAndNotLogged() {
        when(spinnerLogger.isSpinnerActive()).thenReturn(false);
        zeroStateDriver = new ZeroStateDriverForTest(basicLoggingApi, clock, modelProvider,
                contentChangedListener,
                /* spinnerShown= */ true);
        zeroStateDriver.bind(zeroStateViewHolder);

        verify(spinnerLogger).spinnerStarted(SpinnerType.INITIAL_LOAD);
    }

    @Test
    public void testBind_doesNotStartRecordingSpinner_ifSpinnerIsNotShownAndNotLogged() {
        when(spinnerLogger.isSpinnerActive()).thenReturn(false);
        zeroStateDriver.bind(zeroStateViewHolder);

        verifyNoMoreInteractions(spinnerLogger);
    }

    @Test
    public void testBind_doesNotStartRecordingSpinner_ifSpinnerIsShownAndAlreadyLogged() {
        when(spinnerLogger.isSpinnerActive()).thenReturn(true);
        zeroStateDriver = new ZeroStateDriverForTest(basicLoggingApi, clock, modelProvider,
                contentChangedListener,
                /* spinnerShown= */ true);
        zeroStateDriver.bind(zeroStateViewHolder);

        verify(spinnerLogger, never()).spinnerStarted(anyInt());
    }

    @Test
    public void testUnbind() {
        zeroStateDriver.bind(zeroStateViewHolder);
        assertThat(zeroStateDriver.isBound()).isTrue();

        zeroStateDriver.unbind();

        assertThat(zeroStateDriver.isBound()).isFalse();
        verify(zeroStateViewHolder).unbind();
    }

    @Test
    public void testUnbind_doesNotCallUnbindIfNotBound() {
        assertThat(zeroStateDriver.isBound()).isFalse();

        zeroStateDriver.unbind();
        verifyNoMoreInteractions(zeroStateViewHolder);
    }

    @Test
    public void testOnClick() {
        zeroStateDriver.bind(zeroStateViewHolder);

        zeroStateDriver.onClick(new FrameLayout(context));

        verify(zeroStateViewHolder).showSpinner(true);
        verify(contentChangedListener).onContentChanged();
        verify(modelProvider).triggerRefresh(RequestReason.ZERO_STATE, ZERO_STATE_UI_CONTEXT);
    }

    @Test
    public void testOnClick_usesNewModelProvider_afterSettingNewModelProvider() {
        ModelProvider newModelProvider = mock(ModelProvider.class);
        zeroStateDriver.setModelProvider(newModelProvider);
        zeroStateDriver.bind(zeroStateViewHolder);

        zeroStateDriver.onClick(new FrameLayout(context));

        verify(newModelProvider).triggerRefresh(RequestReason.ZERO_STATE, ZERO_STATE_UI_CONTEXT);
    }

    @Test
    public void testOnClick_startsRecordingSpinner_ifSpinnerNotActive() {
        when(spinnerLogger.isSpinnerActive()).thenReturn(false);
        zeroStateDriver.bind(zeroStateViewHolder);

        zeroStateDriver.onClick(new FrameLayout(context));

        verify(spinnerLogger).spinnerStarted(SpinnerType.ZERO_STATE_REFRESH);
    }

    @Test
    public void testOnClick_throwsRuntimeException_ifSpinnerActive() {
        when(spinnerLogger.isSpinnerActive()).thenReturn(true);
        zeroStateDriver = new ZeroStateDriverForTest(basicLoggingApi, clock, modelProvider,
                contentChangedListener,
                /* spinnerShown= */ true);

        zeroStateDriver.bind(zeroStateViewHolder);
        reset(spinnerLogger);

        assertThatRunnable(() -> zeroStateDriver.onClick(new FrameLayout(context)));
    }

    @Test
    public void testOnDestroy_logsSpinnerFinished_ifSpinnerActive() {
        when(spinnerLogger.isSpinnerActive()).thenReturn(true);
        zeroStateDriver = new ZeroStateDriverForTest(basicLoggingApi, clock, modelProvider,
                contentChangedListener,
                /* spinnerShown= */ true);
        zeroStateDriver.bind(zeroStateViewHolder);

        zeroStateDriver.onDestroy();

        verify(spinnerLogger).spinnerFinished();
    }

    @Test
    public void testOnDestroy_doesNotLogSpinnerFinished_ifSpinnerNotActive() {
        when(spinnerLogger.isSpinnerActive()).thenReturn(false);
        zeroStateDriver.bind(zeroStateViewHolder);

        zeroStateDriver.onDestroy();
        verify(spinnerLogger, never()).spinnerFinished();
    }

    @Test
    public void testMaybeRebind() {
        zeroStateDriver.bind(zeroStateViewHolder);
        assertThat(zeroStateDriver.isBound()).isTrue();
        zeroStateDriver.maybeRebind();
        assertThat(zeroStateDriver.isBound()).isTrue();
        verify(zeroStateViewHolder, times(2)).bind(zeroStateDriver, /* showSpinner= */ false);
        verify(zeroStateViewHolder).unbind();
    }

    @Test
    public void testMaybeRebind_nullViewHolder() {
        zeroStateDriver.bind(zeroStateViewHolder);
        zeroStateDriver.unbind();
        reset(zeroStateViewHolder);

        assertThat(zeroStateDriver.isBound()).isFalse();
        zeroStateDriver.maybeRebind();
        assertThat(zeroStateDriver.isBound()).isFalse();
        verify(zeroStateViewHolder, never()).bind(any(OnClickListener.class), anyBoolean());
        verify(zeroStateViewHolder, never()).unbind();
    }

    private final class ZeroStateDriverForTest extends ZeroStateDriver {
        ZeroStateDriverForTest(BasicLoggingApi basicLoggingApi, Clock clock,
                ModelProvider modelProvider, ContentChangedListener contentChangedListener,
                boolean spinnerShown) {
            super(basicLoggingApi, clock, modelProvider, contentChangedListener, spinnerShown);
        }

        @Override
        SpinnerLogger createSpinnerLogger(BasicLoggingApi basicLoggingApi, Clock clock) {
            return spinnerLogger;
        }
    }
}
