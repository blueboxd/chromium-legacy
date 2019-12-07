// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.ColorDrawable;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Supplier;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.CardConfiguration;
import org.chromium.chrome.browser.feed.library.api.internal.actionparser.ActionParser;
import org.chromium.chrome.browser.feed.library.api.internal.actionparser.ActionSource;
import org.chromium.chrome.browser.feed.library.api.internal.actionparser.ActionSourceConverter;
import org.chromium.chrome.browser.feed.library.basicstream.internal.actions.StreamActionApiImpl;
import org.chromium.chrome.browser.feed.library.basicstream.internal.scroll.BasicStreamScrollMonitor;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.piet.PietManager;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler.ActionType;
import org.chromium.chrome.browser.feed.library.piet.host.EventLogger;
import org.chromium.chrome.browser.feed.library.piet.testing.FakeFrameAdapter;
import org.chromium.chrome.browser.feed.library.sharedstream.logging.LoggingListener;
import org.chromium.chrome.browser.feed.library.sharedstream.logging.VisibilityMonitor;
import org.chromium.chrome.browser.feed.library.sharedstream.piet.PietEventLogger;
import org.chromium.chrome.browser.feed.library.testing.host.stream.FakeCardConfiguration;
import org.chromium.components.feed.core.proto.ui.action.FeedActionPayloadProto.FeedActionPayload;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedAction;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.Action;
import org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode;
import org.chromium.components.feed.core.proto.ui.piet.LogDataProto.LogData;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Frame;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.PietSharedState;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Tests for {@link PietViewHolder}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PietViewHolderTest {
    private static final Action ACTION = Action.getDefaultInstance();
    private static final Frame FRAME = Frame.newBuilder().build();
    private static final FeedActionPayload SWIPE_ACTION =
            FeedActionPayload.newBuilder()
                    .setExtension(FeedAction.feedActionExtension, FeedAction.getDefaultInstance())
                    .build();
    private static final LogData VE_LOGGING_INFO = LogData.getDefaultInstance();

    private CardConfiguration mCardConfiguration;
    @Mock
    private ActionParser mActionParser;
    @Mock
    private PietManager mPietManager;
    @Mock
    private StreamActionApiImpl mStreamActionApi;
    @Mock
    private LoggingListener mLoggingListener;
    @Mock
    private VisibilityMonitor mVisibilityMonitor;
    @Mock
    private BasicLoggingApi mBasicLoggingApi;

    private BasicStreamScrollMonitor mStreamScrollMonitor;
    private FakeFrameAdapter mFrameAdapter;
    private ActionHandler mActionHandler;
    private Configuration mConfiguration;
    private PietViewHolder mPietViewHolder;
    private FrameLayout mFrameLayout;
    private View mView;
    private View mViewport;
    private final List<PietSharedState> mPietSharedStates = new ArrayList<>();
    private Context mContext;

    @Before
    public void setUp() {
        initMocks(this);
        mStreamScrollMonitor = new BasicStreamScrollMonitor(new FakeClock());
        mCardConfiguration = new FakeCardConfiguration();
        mContext = Robolectric.buildActivity(Activity.class).get();
        mFrameLayout = new FrameLayout(mContext);
        mFrameLayout.setLayoutParams(new MarginLayoutParams(100, 100));
        mView = new TextView(mContext);
        mViewport = new FrameLayout(mContext);

        // TODO: Use FakePietManager once it is implemented.
        when(mPietManager.createPietFrameAdapter(ArgumentMatchers.<Supplier<ViewGroup>>any(),
                     any(ActionHandler.class), any(EventLogger.class), eq(mContext)))
                .thenAnswer(invocation -> {
                    mFrameAdapter =
                            FakeFrameAdapter.builder(mContext)
                                    .setActionHandler((ActionHandler) invocation.getArguments()[1])
                                    .addViewAction(Action.getDefaultInstance())
                                    .addHideAction(Action.getDefaultInstance())
                                    .build();
                    return mFrameAdapter;
                });

        mConfiguration = new Configuration.Builder().build();
        mPietViewHolder = new PietViewHolder(mCardConfiguration, mFrameLayout, mPietManager,
                mStreamScrollMonitor, mViewport, mContext, mConfiguration,
                new PietEventLogger(mBasicLoggingApi)) {
            @Override
            VisibilityMonitor createVisibilityMonitor(View view, Configuration configuration) {
                return mVisibilityMonitor;
            }
        };

        ArgumentCaptor<ActionHandler> captor = ArgumentCaptor.forClass(ActionHandler.class);
        verify(mPietManager)
                .createPietFrameAdapter(ArgumentMatchers.<Supplier<ViewGroup>>any(),
                        captor.capture(), any(EventLogger.class), any(Context.class));
        mActionHandler = captor.getValue();
    }

    @Test
    public void testCardViewSetup() {
        assertThat(mFrameLayout.getId()).isEqualTo(R.id.feed_content_card);
    }

    @Test
    public void testBind_clearsPadding() {
        mFrameLayout.setPadding(1, 2, 3, 4);

        mPietViewHolder.bind(Frame.getDefaultInstance(), mPietSharedStates, mStreamActionApi,
                FeedActionPayload.getDefaultInstance(), mLoggingListener, mActionParser);

        assertThat(mFrameLayout.getPaddingLeft()).isEqualTo(0);
        assertThat(mFrameLayout.getPaddingRight()).isEqualTo(0);
        assertThat(mFrameLayout.getPaddingTop()).isEqualTo(0);
        assertThat(mFrameLayout.getPaddingBottom()).isEqualTo(0);
    }

    @Test
    public void testBind_setsBackground() {
        mPietViewHolder.bind(Frame.getDefaultInstance(), mPietSharedStates, mStreamActionApi,
                FeedActionPayload.getDefaultInstance(), mLoggingListener, mActionParser);

        assertThat(((ColorDrawable) mFrameLayout.getBackground()).getColor())
                .isEqualTo(((ColorDrawable) mCardConfiguration.getCardBackground()).getColor());
    }

    @Test
    public void testBind_setsMargins() {
        mPietViewHolder.bind(Frame.getDefaultInstance(), mPietSharedStates, mStreamActionApi,
                FeedActionPayload.getDefaultInstance(), mLoggingListener, mActionParser);

        MarginLayoutParams marginLayoutParams =
                (MarginLayoutParams) mPietViewHolder.itemView.getLayoutParams();
        assertThat(marginLayoutParams.bottomMargin)
                .isEqualTo(mCardConfiguration.getCardBottomMargin());
        assertThat(marginLayoutParams.leftMargin)
                .isEqualTo(mCardConfiguration.getCardStartMargin());
        assertThat(marginLayoutParams.rightMargin).isEqualTo(mCardConfiguration.getCardEndMargin());
    }

    @Test
    public void testBind_bindsModel() {
        mPietViewHolder.bind(Frame.getDefaultInstance(), mPietSharedStates, mStreamActionApi,
                FeedActionPayload.getDefaultInstance(), mLoggingListener, mActionParser);

        assertThat(mFrameAdapter.isBound()).isTrue();
    }

    @Test
    public void testBind_onlyBindsOnce() {
        mPietViewHolder.bind(Frame.getDefaultInstance(), mPietSharedStates, mStreamActionApi,
                FeedActionPayload.getDefaultInstance(), mLoggingListener, mActionParser);

        mPietViewHolder.bind(Frame.getDefaultInstance(), mPietSharedStates, mStreamActionApi,
                FeedActionPayload.getDefaultInstance(), mLoggingListener, mActionParser);

        // Should not crash.
    }

    @Test
    public void testBind_setsListener() {
        mPietViewHolder.bind(Frame.getDefaultInstance(), mPietSharedStates, mStreamActionApi,
                FeedActionPayload.getDefaultInstance(), mLoggingListener, mActionParser);

        verify(mVisibilityMonitor).setListener(mLoggingListener);
    }

    @Test
    public void testBind_setsScrollListener() {
        mPietViewHolder.bind(Frame.getDefaultInstance(), mPietSharedStates, mStreamActionApi,
                FeedActionPayload.getDefaultInstance(), mLoggingListener, mActionParser);

        assertThat(mStreamScrollMonitor.getObserverCount()).isEqualTo(1);
    }

    @Test
    public void testUnbind() {
        mPietViewHolder.bind(Frame.getDefaultInstance(), mPietSharedStates, mStreamActionApi,
                FeedActionPayload.getDefaultInstance(), mLoggingListener, mActionParser);

        mPietViewHolder.unbind();

        assertThat(mFrameAdapter.isBound()).isFalse();
    }

    @Test
    public void testUnbind_setsListenerToNull() {
        mPietViewHolder.bind(Frame.getDefaultInstance(), mPietSharedStates, mStreamActionApi,
                FeedActionPayload.getDefaultInstance(), mLoggingListener, mActionParser);

        mPietViewHolder.unbind();

        verify(mVisibilityMonitor).setListener(null);
    }

    @Test
    public void testUnbind_unsetsScrollListener() {
        mPietViewHolder.bind(Frame.getDefaultInstance(), mPietSharedStates, mStreamActionApi,
                FeedActionPayload.getDefaultInstance(), mLoggingListener, mActionParser);

        mPietViewHolder.unbind();

        assertThat(mStreamScrollMonitor.getObserverCount()).isEqualTo(0);
    }

    @Test
    public void testSwipe_cantSwipeWithDefaultInstance() {
        mPietViewHolder.bind(Frame.getDefaultInstance(), mPietSharedStates, mStreamActionApi,
                FeedActionPayload.getDefaultInstance(), mLoggingListener, mActionParser);

        assertThat(mPietViewHolder.canSwipe()).isFalse();
    }

    @Test
    public void testSwipe_canSwipeWithNonDefaultInstance() {
        mPietViewHolder.bind(Frame.getDefaultInstance(), mPietSharedStates, mStreamActionApi,
                SWIPE_ACTION, mLoggingListener, mActionParser);

        assertThat(mPietViewHolder.canSwipe()).isTrue();
    }

    @Test
    public void testSwipeToDismissPerformed() {
        mPietViewHolder.bind(Frame.getDefaultInstance(), mPietSharedStates, mStreamActionApi,
                SWIPE_ACTION, mLoggingListener, mActionParser);

        mPietViewHolder.onSwiped();

        verify(mLoggingListener).onContentSwiped();
        verify(mActionParser)
                .parseFeedActionPayload(SWIPE_ACTION, mStreamActionApi, mPietViewHolder.itemView,
                        ActionSource.SWIPE);
    }

    @Test
    public void testActionHandler_logsClickOnClickAction() {
        mPietViewHolder.bind(Frame.getDefaultInstance(), mPietSharedStates, mStreamActionApi,
                FeedActionPayload.getDefaultInstance(), mLoggingListener, mActionParser);

        mActionHandler.handleAction(ACTION, ActionType.CLICK, FRAME, mView, VE_LOGGING_INFO);

        verify(mLoggingListener).onContentClicked();
        verify(mActionParser)
                .parseAction(ACTION, mStreamActionApi, mView, VE_LOGGING_INFO, ActionSource.CLICK);
    }

    @Test
    public void testActionHandler_doesNotLogClickOnLongClickAction() {
        mPietViewHolder.bind(Frame.getDefaultInstance(), mPietSharedStates, mStreamActionApi,
                FeedActionPayload.getDefaultInstance(), mLoggingListener, mActionParser);

        mActionHandler.handleAction(ACTION, ActionType.LONG_CLICK, FRAME, mView, VE_LOGGING_INFO);

        verify(mLoggingListener, never()).onContentClicked();
        verify(mActionParser)
                .parseAction(
                        ACTION, mStreamActionApi, mView, VE_LOGGING_INFO, ActionSource.LONG_CLICK);
    }

    @Test
    public void testActionHandler_doesNotLogClickOnViewAction() {
        mPietViewHolder.bind(Frame.getDefaultInstance(), mPietSharedStates, mStreamActionApi,
                FeedActionPayload.getDefaultInstance(), mLoggingListener, mActionParser);

        mActionHandler.handleAction(ACTION, ActionType.VIEW, FRAME, mView, VE_LOGGING_INFO);

        verify(mLoggingListener, never()).onContentClicked();
        verify(mActionParser)
                .parseAction(ACTION, mStreamActionApi, mView, VE_LOGGING_INFO, ActionSource.VIEW);
    }

    @Test
    public void testHideActionsOnUnbind() {
        mPietViewHolder.bind(Frame.getDefaultInstance(), mPietSharedStates, mStreamActionApi,
                FeedActionPayload.getDefaultInstance(), mLoggingListener, mActionParser);

        // Triggers view actions.
        mStreamScrollMonitor.onScrollStateChanged(
                new RecyclerView(mContext), RecyclerView.SCROLL_STATE_IDLE);

        // Triggers hide actions associated with those views
        mPietViewHolder.unbind();

        // TODO: Instead of using the default instance twice, create an extension for test
        // proto.
        // Once on the view, once on the hide.
        verify(mActionParser, times(2))
                .parseAction(Action.getDefaultInstance(), mStreamActionApi,
                        mFrameAdapter.getFrameContainer(), LogData.getDefaultInstance(),
                        ActionSourceConverter.convertPietAction(ActionType.VIEW));
    }

    @Test
    public void testPietError_loggedToHost() {
        ArgumentCaptor<EventLogger> pietEventLoggerCaptor =
                ArgumentCaptor.forClass(EventLogger.class);

        verify(mPietManager)
                .createPietFrameAdapter(ArgumentMatchers.<Supplier<ViewGroup>>any(),
                        eq(mActionHandler), pietEventLoggerCaptor.capture(), eq(mContext));

        pietEventLoggerCaptor.getValue().logEvents(
                Collections.singletonList(ErrorCode.ERR_MISSING_BINDING_VALUE));

        verify(mBasicLoggingApi)
                .onPietFrameRenderingEvent(
                        Collections.singletonList(ErrorCode.ERR_MISSING_BINDING_VALUE.getNumber()));
    }
}
