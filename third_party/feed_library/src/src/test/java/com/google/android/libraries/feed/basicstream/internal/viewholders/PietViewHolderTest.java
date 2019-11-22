// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal.viewholders;

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

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.stream.CardConfiguration;
import com.google.android.libraries.feed.api.internal.actionparser.ActionParser;
import com.google.android.libraries.feed.api.internal.actionparser.ActionSource;
import com.google.android.libraries.feed.api.internal.actionparser.ActionSourceConverter;
import com.google.android.libraries.feed.basicstream.internal.actions.StreamActionApiImpl;
import com.google.android.libraries.feed.basicstream.internal.scroll.BasicStreamScrollMonitor;
import com.google.android.libraries.feed.common.functional.Supplier;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.piet.PietManager;
import com.google.android.libraries.feed.piet.host.ActionHandler;
import com.google.android.libraries.feed.piet.host.ActionHandler.ActionType;
import com.google.android.libraries.feed.piet.host.EventLogger;
import com.google.android.libraries.feed.piet.testing.FakeFrameAdapter;
import com.google.android.libraries.feed.sharedstream.logging.LoggingListener;
import com.google.android.libraries.feed.sharedstream.logging.VisibilityMonitor;
import com.google.android.libraries.feed.sharedstream.piet.PietEventLogger;
import com.google.android.libraries.feed.testing.host.stream.FakeCardConfiguration;
import com.google.search.now.ui.action.FeedActionPayloadProto.FeedActionPayload;
import com.google.search.now.ui.action.FeedActionProto.FeedAction;
import com.google.search.now.ui.piet.ActionsProto.Action;
import com.google.search.now.ui.piet.ErrorsProto.ErrorCode;
import com.google.search.now.ui.piet.LogDataProto.LogData;
import com.google.search.now.ui.piet.PietProto.Frame;
import com.google.search.now.ui.piet.PietProto.PietSharedState;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Tests for {@link PietViewHolder}. */
@RunWith(RobolectricTestRunner.class)
public class PietViewHolderTest {
    private static final Action ACTION = Action.getDefaultInstance();
    private static final Frame FRAME = Frame.newBuilder().build();
    private static final FeedActionPayload SWIPE_ACTION =
            FeedActionPayload.newBuilder()
                    .setExtension(FeedAction.feedActionExtension, FeedAction.getDefaultInstance())
                    .build();
    private static final LogData VE_LOGGING_INFO = LogData.getDefaultInstance();

    private CardConfiguration cardConfiguration;
    @Mock
    private ActionParser actionParser;
    @Mock
    private PietManager pietManager;
    @Mock
    private StreamActionApiImpl streamActionApi;
    @Mock
    private LoggingListener loggingListener;
    @Mock
    private VisibilityMonitor visibilityMonitor;
    @Mock
    private BasicLoggingApi basicLoggingApi;

    private BasicStreamScrollMonitor streamScrollMonitor;
    private FakeFrameAdapter frameAdapter;
    private ActionHandler actionHandler;
    private Configuration configuration;
    private PietViewHolder pietViewHolder;
    private FrameLayout frameLayout;
    private View view;
    private View viewport;
    private final List<PietSharedState> pietSharedStates = new ArrayList<>();
    private Context context;

    @Before
    public void setUp() {
        initMocks(this);
        streamScrollMonitor = new BasicStreamScrollMonitor(new FakeClock());
        cardConfiguration = new FakeCardConfiguration();
        context = Robolectric.buildActivity(Activity.class).get();
        frameLayout = new FrameLayout(context);
        frameLayout.setLayoutParams(new MarginLayoutParams(100, 100));
        view = new TextView(context);
        viewport = new FrameLayout(context);

        // TODO: Use FakePietManager once it is implemented.
        when(pietManager.createPietFrameAdapter(ArgumentMatchers.<Supplier<ViewGroup>>any(),
                     any(ActionHandler.class), any(EventLogger.class), eq(context)))
                .thenAnswer(invocation -> {
                    frameAdapter =
                            FakeFrameAdapter.builder(context)
                                    .setActionHandler((ActionHandler) invocation.getArguments()[1])
                                    .addViewAction(Action.getDefaultInstance())
                                    .addHideAction(Action.getDefaultInstance())
                                    .build();
                    return frameAdapter;
                });

        configuration = new Configuration.Builder().build();
        pietViewHolder = new PietViewHolder(cardConfiguration, frameLayout, pietManager,
                streamScrollMonitor, viewport, context, configuration,
                new PietEventLogger(basicLoggingApi)) {
            @Override
            VisibilityMonitor createVisibilityMonitor(View view, Configuration configuration) {
                return visibilityMonitor;
            }
        };

        ArgumentCaptor<ActionHandler> captor = ArgumentCaptor.forClass(ActionHandler.class);
        verify(pietManager)
                .createPietFrameAdapter(ArgumentMatchers.<Supplier<ViewGroup>>any(),
                        captor.capture(), any(EventLogger.class), any(Context.class));
        actionHandler = captor.getValue();
    }

    @Test
    public void testCardViewSetup() {
        assertThat(frameLayout.getId()).isEqualTo(R.id.feed_content_card);
    }

    @Test
    public void testBind_clearsPadding() {
        frameLayout.setPadding(1, 2, 3, 4);

        pietViewHolder.bind(Frame.getDefaultInstance(), pietSharedStates, streamActionApi,
                FeedActionPayload.getDefaultInstance(), loggingListener, actionParser);

        assertThat(frameLayout.getPaddingLeft()).isEqualTo(0);
        assertThat(frameLayout.getPaddingRight()).isEqualTo(0);
        assertThat(frameLayout.getPaddingTop()).isEqualTo(0);
        assertThat(frameLayout.getPaddingBottom()).isEqualTo(0);
    }

    @Test
    public void testBind_setsBackground() {
        pietViewHolder.bind(Frame.getDefaultInstance(), pietSharedStates, streamActionApi,
                FeedActionPayload.getDefaultInstance(), loggingListener, actionParser);

        assertThat(((ColorDrawable) frameLayout.getBackground()).getColor())
                .isEqualTo(((ColorDrawable) cardConfiguration.getCardBackground()).getColor());
    }

    @Test
    public void testBind_setsMargins() {
        pietViewHolder.bind(Frame.getDefaultInstance(), pietSharedStates, streamActionApi,
                FeedActionPayload.getDefaultInstance(), loggingListener, actionParser);

        MarginLayoutParams marginLayoutParams =
                (MarginLayoutParams) pietViewHolder.itemView.getLayoutParams();
        assertThat(marginLayoutParams.bottomMargin)
                .isEqualTo(cardConfiguration.getCardBottomMargin());
        assertThat(marginLayoutParams.leftMargin).isEqualTo(cardConfiguration.getCardStartMargin());
        assertThat(marginLayoutParams.rightMargin).isEqualTo(cardConfiguration.getCardEndMargin());
    }

    @Test
    public void testBind_bindsModel() {
        pietViewHolder.bind(Frame.getDefaultInstance(), pietSharedStates, streamActionApi,
                FeedActionPayload.getDefaultInstance(), loggingListener, actionParser);

        assertThat(frameAdapter.isBound()).isTrue();
    }

    @Test
    public void testBind_onlyBindsOnce() {
        pietViewHolder.bind(Frame.getDefaultInstance(), pietSharedStates, streamActionApi,
                FeedActionPayload.getDefaultInstance(), loggingListener, actionParser);

        pietViewHolder.bind(Frame.getDefaultInstance(), pietSharedStates, streamActionApi,
                FeedActionPayload.getDefaultInstance(), loggingListener, actionParser);

        // Should not crash.
    }

    @Test
    public void testBind_setsListener() {
        pietViewHolder.bind(Frame.getDefaultInstance(), pietSharedStates, streamActionApi,
                FeedActionPayload.getDefaultInstance(), loggingListener, actionParser);

        verify(visibilityMonitor).setListener(loggingListener);
    }

    @Test
    public void testBind_setsScrollListener() {
        pietViewHolder.bind(Frame.getDefaultInstance(), pietSharedStates, streamActionApi,
                FeedActionPayload.getDefaultInstance(), loggingListener, actionParser);

        assertThat(streamScrollMonitor.getObserverCount()).isEqualTo(1);
    }

    @Test
    public void testUnbind() {
        pietViewHolder.bind(Frame.getDefaultInstance(), pietSharedStates, streamActionApi,
                FeedActionPayload.getDefaultInstance(), loggingListener, actionParser);

        pietViewHolder.unbind();

        assertThat(frameAdapter.isBound()).isFalse();
    }

    @Test
    public void testUnbind_setsListenerToNull() {
        pietViewHolder.bind(Frame.getDefaultInstance(), pietSharedStates, streamActionApi,
                FeedActionPayload.getDefaultInstance(), loggingListener, actionParser);

        pietViewHolder.unbind();

        verify(visibilityMonitor).setListener(null);
    }

    @Test
    public void testUnbind_unsetsScrollListener() {
        pietViewHolder.bind(Frame.getDefaultInstance(), pietSharedStates, streamActionApi,
                FeedActionPayload.getDefaultInstance(), loggingListener, actionParser);

        pietViewHolder.unbind();

        assertThat(streamScrollMonitor.getObserverCount()).isEqualTo(0);
    }

    @Test
    public void testSwipe_cantSwipeWithDefaultInstance() {
        pietViewHolder.bind(Frame.getDefaultInstance(), pietSharedStates, streamActionApi,
                FeedActionPayload.getDefaultInstance(), loggingListener, actionParser);

        assertThat(pietViewHolder.canSwipe()).isFalse();
    }

    @Test
    public void testSwipe_canSwipeWithNonDefaultInstance() {
        pietViewHolder.bind(Frame.getDefaultInstance(), pietSharedStates, streamActionApi,
                SWIPE_ACTION, loggingListener, actionParser);

        assertThat(pietViewHolder.canSwipe()).isTrue();
    }

    @Test
    public void testSwipeToDismissPerformed() {
        pietViewHolder.bind(Frame.getDefaultInstance(), pietSharedStates, streamActionApi,
                SWIPE_ACTION, loggingListener, actionParser);

        pietViewHolder.onSwiped();

        verify(loggingListener).onContentSwiped();
        verify(actionParser)
                .parseFeedActionPayload(
                        SWIPE_ACTION, streamActionApi, pietViewHolder.itemView, ActionSource.SWIPE);
    }

    @Test
    public void testActionHandler_logsClickOnClickAction() {
        pietViewHolder.bind(Frame.getDefaultInstance(), pietSharedStates, streamActionApi,
                FeedActionPayload.getDefaultInstance(), loggingListener, actionParser);

        actionHandler.handleAction(ACTION, ActionType.CLICK, FRAME, view, VE_LOGGING_INFO);

        verify(loggingListener).onContentClicked();
        verify(actionParser)
                .parseAction(ACTION, streamActionApi, view, VE_LOGGING_INFO, ActionSource.CLICK);
    }

    @Test
    public void testActionHandler_doesNotLogClickOnLongClickAction() {
        pietViewHolder.bind(Frame.getDefaultInstance(), pietSharedStates, streamActionApi,
                FeedActionPayload.getDefaultInstance(), loggingListener, actionParser);

        actionHandler.handleAction(ACTION, ActionType.LONG_CLICK, FRAME, view, VE_LOGGING_INFO);

        verify(loggingListener, never()).onContentClicked();
        verify(actionParser)
                .parseAction(
                        ACTION, streamActionApi, view, VE_LOGGING_INFO, ActionSource.LONG_CLICK);
    }

    @Test
    public void testActionHandler_doesNotLogClickOnViewAction() {
        pietViewHolder.bind(Frame.getDefaultInstance(), pietSharedStates, streamActionApi,
                FeedActionPayload.getDefaultInstance(), loggingListener, actionParser);

        actionHandler.handleAction(ACTION, ActionType.VIEW, FRAME, view, VE_LOGGING_INFO);

        verify(loggingListener, never()).onContentClicked();
        verify(actionParser)
                .parseAction(ACTION, streamActionApi, view, VE_LOGGING_INFO, ActionSource.VIEW);
    }

    @Test
    public void testHideActionsOnUnbind() {
        pietViewHolder.bind(Frame.getDefaultInstance(), pietSharedStates, streamActionApi,
                FeedActionPayload.getDefaultInstance(), loggingListener, actionParser);

        // Triggers view actions.
        streamScrollMonitor.onScrollStateChanged(
                new RecyclerView(context), RecyclerView.SCROLL_STATE_IDLE);

        // Triggers hide actions associated with those views
        pietViewHolder.unbind();

        // TODO: Instead of using the default instance twice, create an extension for test
        // proto.
        // Once on the view, once on the hide.
        verify(actionParser, times(2))
                .parseAction(Action.getDefaultInstance(), streamActionApi,
                        frameAdapter.getFrameContainer(), LogData.getDefaultInstance(),
                        ActionSourceConverter.convertPietAction(ActionType.VIEW));
    }

    @Test
    public void testPietError_loggedToHost() {
        ArgumentCaptor<EventLogger> pietEventLoggerCaptor =
                ArgumentCaptor.forClass(EventLogger.class);

        verify(pietManager)
                .createPietFrameAdapter(ArgumentMatchers.<Supplier<ViewGroup>>any(),
                        eq(actionHandler), pietEventLoggerCaptor.capture(), eq(context));

        pietEventLoggerCaptor.getValue().logEvents(
                Collections.singletonList(ErrorCode.ERR_MISSING_BINDING_VALUE));

        verify(basicLoggingApi)
                .onPietFrameRenderingEvent(
                        Collections.singletonList(ErrorCode.ERR_MISSING_BINDING_VALUE.getNumber()));
    }
}
