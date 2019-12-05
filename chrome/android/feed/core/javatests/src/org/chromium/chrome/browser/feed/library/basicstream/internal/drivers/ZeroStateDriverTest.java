// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.drivers;

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

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;

import android.app.Activity;
import android.content.Context;
import android.view.View.OnClickListener;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ContentChangedListener;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.RequestReason;
import org.chromium.chrome.browser.feed.library.api.host.logging.SpinnerType;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.ZeroStateViewHolder;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.sharedstream.logging.SpinnerLogger;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.components.feed.core.proto.libraries.sharedstream.UiRefreshReasonProto.UiRefreshReason;
import org.chromium.components.feed.core.proto.libraries.sharedstream.UiRefreshReasonProto.UiRefreshReason.Reason;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link ZeroStateDriver}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ZeroStateDriverTest {
    private static final UiContext ZERO_STATE_UI_CONTEXT =
            UiContext.newBuilder()
                    .setExtension(UiRefreshReason.uiRefreshReasonExtension,
                            UiRefreshReason.newBuilder().setReason(Reason.ZERO_STATE).build())
                    .build();

    @Mock
    private BasicLoggingApi mBasicLoggingApi;
    @Mock
    private ContentChangedListener mContentChangedListener;
    @Mock
    private ModelProvider mModelProvider;
    @Mock
    private SpinnerLogger mSpinnerLogger;
    @Mock
    private ZeroStateViewHolder mZeroStateViewHolder;

    private ZeroStateDriver mZeroStateDriver;
    private Clock mClock;
    private Context mContext;

    @Before
    public void setup() {
        initMocks(this);
        mClock = new FakeClock();
        mContext = Robolectric.buildActivity(Activity.class).get();

        mZeroStateDriver = new ZeroStateDriverForTest(mBasicLoggingApi, mClock, mModelProvider,
                mContentChangedListener,
                /* spinnerShown= */ false);
    }

    @Test
    public void testBind() {
        assertThat(mZeroStateDriver.isBound()).isFalse();

        mZeroStateDriver.bind(mZeroStateViewHolder);

        assertThat(mZeroStateDriver.isBound()).isTrue();
        verify(mZeroStateViewHolder).bind(mZeroStateDriver, /* showSpinner= */ false);
    }

    @Test
    public void testBind_whenSpinnerShownTrue() {
        mZeroStateDriver = new ZeroStateDriverForTest(mBasicLoggingApi, mClock, mModelProvider,
                mContentChangedListener,
                /* spinnerShown= */ true);

        mZeroStateDriver.bind(mZeroStateViewHolder);
        verify(mZeroStateViewHolder).bind(mZeroStateDriver, /* showSpinner= */ true);
    }

    @Test
    public void testBindAfterOnClick_bindsViewholderWithSpinnerShown() {
        InOrder inOrder = Mockito.inOrder(mZeroStateViewHolder);
        mZeroStateDriver.bind(mZeroStateViewHolder);
        inOrder.verify(mZeroStateViewHolder).bind(mZeroStateDriver, /* showSpinner= */ false);

        mZeroStateDriver.onClick(new FrameLayout(mContext));
        mZeroStateDriver.unbind();

        mZeroStateDriver.bind(mZeroStateViewHolder);

        inOrder.verify(mZeroStateViewHolder).bind(mZeroStateDriver, /* showSpinner= */ true);
    }

    @Test
    public void testBind_startsRecordingSpinner_ifSpinnerIsShownAndNotLogged() {
        when(mSpinnerLogger.isSpinnerActive()).thenReturn(false);
        mZeroStateDriver = new ZeroStateDriverForTest(mBasicLoggingApi, mClock, mModelProvider,
                mContentChangedListener,
                /* spinnerShown= */ true);
        mZeroStateDriver.bind(mZeroStateViewHolder);

        verify(mSpinnerLogger).spinnerStarted(SpinnerType.INITIAL_LOAD);
    }

    @Test
    public void testBind_doesNotStartRecordingSpinner_ifSpinnerIsNotShownAndNotLogged() {
        when(mSpinnerLogger.isSpinnerActive()).thenReturn(false);
        mZeroStateDriver.bind(mZeroStateViewHolder);

        verifyNoMoreInteractions(mSpinnerLogger);
    }

    @Test
    public void testBind_doesNotStartRecordingSpinner_ifSpinnerIsShownAndAlreadyLogged() {
        when(mSpinnerLogger.isSpinnerActive()).thenReturn(true);
        mZeroStateDriver = new ZeroStateDriverForTest(mBasicLoggingApi, mClock, mModelProvider,
                mContentChangedListener,
                /* spinnerShown= */ true);
        mZeroStateDriver.bind(mZeroStateViewHolder);

        verify(mSpinnerLogger, never()).spinnerStarted(anyInt());
    }

    @Test
    public void testUnbind() {
        mZeroStateDriver.bind(mZeroStateViewHolder);
        assertThat(mZeroStateDriver.isBound()).isTrue();

        mZeroStateDriver.unbind();

        assertThat(mZeroStateDriver.isBound()).isFalse();
        verify(mZeroStateViewHolder).unbind();
    }

    @Test
    public void testUnbind_doesNotCallUnbindIfNotBound() {
        assertThat(mZeroStateDriver.isBound()).isFalse();

        mZeroStateDriver.unbind();
        verifyNoMoreInteractions(mZeroStateViewHolder);
    }

    @Test
    public void testOnClick() {
        mZeroStateDriver.bind(mZeroStateViewHolder);

        mZeroStateDriver.onClick(new FrameLayout(mContext));

        verify(mZeroStateViewHolder).showSpinner(true);
        verify(mContentChangedListener).onContentChanged();
        verify(mModelProvider).triggerRefresh(RequestReason.ZERO_STATE, ZERO_STATE_UI_CONTEXT);
    }

    @Test
    public void testOnClick_usesNewModelProvider_afterSettingNewModelProvider() {
        ModelProvider newModelProvider = mock(ModelProvider.class);
        mZeroStateDriver.setModelProvider(newModelProvider);
        mZeroStateDriver.bind(mZeroStateViewHolder);

        mZeroStateDriver.onClick(new FrameLayout(mContext));

        verify(newModelProvider).triggerRefresh(RequestReason.ZERO_STATE, ZERO_STATE_UI_CONTEXT);
    }

    @Test
    public void testOnClick_startsRecordingSpinner_ifSpinnerNotActive() {
        when(mSpinnerLogger.isSpinnerActive()).thenReturn(false);
        mZeroStateDriver.bind(mZeroStateViewHolder);

        mZeroStateDriver.onClick(new FrameLayout(mContext));

        verify(mSpinnerLogger).spinnerStarted(SpinnerType.ZERO_STATE_REFRESH);
    }

    @Test
    public void testOnClick_throwsRuntimeException_ifSpinnerActive() {
        when(mSpinnerLogger.isSpinnerActive()).thenReturn(true);
        mZeroStateDriver = new ZeroStateDriverForTest(mBasicLoggingApi, mClock, mModelProvider,
                mContentChangedListener,
                /* spinnerShown= */ true);

        mZeroStateDriver.bind(mZeroStateViewHolder);
        reset(mSpinnerLogger);

        assertThatRunnable(() -> mZeroStateDriver.onClick(new FrameLayout(mContext)));
    }

    @Test
    public void testOnDestroy_logsSpinnerFinished_ifSpinnerActive() {
        when(mSpinnerLogger.isSpinnerActive()).thenReturn(true);
        mZeroStateDriver = new ZeroStateDriverForTest(mBasicLoggingApi, mClock, mModelProvider,
                mContentChangedListener,
                /* spinnerShown= */ true);
        mZeroStateDriver.bind(mZeroStateViewHolder);

        mZeroStateDriver.onDestroy();

        verify(mSpinnerLogger).spinnerFinished();
    }

    @Test
    public void testOnDestroy_doesNotLogSpinnerFinished_ifSpinnerNotActive() {
        when(mSpinnerLogger.isSpinnerActive()).thenReturn(false);
        mZeroStateDriver.bind(mZeroStateViewHolder);

        mZeroStateDriver.onDestroy();
        verify(mSpinnerLogger, never()).spinnerFinished();
    }

    @Test
    public void testMaybeRebind() {
        mZeroStateDriver.bind(mZeroStateViewHolder);
        assertThat(mZeroStateDriver.isBound()).isTrue();
        mZeroStateDriver.maybeRebind();
        assertThat(mZeroStateDriver.isBound()).isTrue();
        verify(mZeroStateViewHolder, times(2)).bind(mZeroStateDriver, /* showSpinner= */ false);
        verify(mZeroStateViewHolder).unbind();
    }

    @Test
    public void testMaybeRebind_nullViewHolder() {
        mZeroStateDriver.bind(mZeroStateViewHolder);
        mZeroStateDriver.unbind();
        reset(mZeroStateViewHolder);

        assertThat(mZeroStateDriver.isBound()).isFalse();
        mZeroStateDriver.maybeRebind();
        assertThat(mZeroStateDriver.isBound()).isFalse();
        verify(mZeroStateViewHolder, never()).bind(any(OnClickListener.class), anyBoolean());
        verify(mZeroStateViewHolder, never()).unbind();
    }

    private final class ZeroStateDriverForTest extends ZeroStateDriver {
        ZeroStateDriverForTest(BasicLoggingApi basicLoggingApi, Clock clock,
                ModelProvider modelProvider, ContentChangedListener contentChangedListener,
                boolean spinnerShown) {
            super(basicLoggingApi, clock, modelProvider, contentChangedListener, spinnerShown);
        }

        @Override
        SpinnerLogger createSpinnerLogger(BasicLoggingApi basicLoggingApi, Clock clock) {
            return mSpinnerLogger;
        }
    }
}
