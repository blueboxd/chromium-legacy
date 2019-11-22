// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.PorterDuff.Mode;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.RippleDrawable;
import android.view.View;

import com.google.android.libraries.feed.piet.host.ActionHandler;
import com.google.android.libraries.feed.piet.host.ActionHandler.ActionType;
import com.google.android.libraries.feed.testing.shadows.ExtendedShadowView;
import com.google.search.now.ui.piet.ActionsProto.Action;
import com.google.search.now.ui.piet.ActionsProto.Actions;
import com.google.search.now.ui.piet.ActionsProto.VisibilityAction;
import com.google.search.now.ui.piet.LogDataProto.LogData;
import com.google.search.now.ui.piet.PietProto.Frame;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;

import java.util.HashSet;
import java.util.Set;

/** Tests of the {@link ViewUtils}. */
@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ExtendedShadowView.class})
public class ViewUtilsTest {
    private final Context context = Robolectric.buildActivity(Activity.class).get();

    private static final Frame DEFAULT_FRAME = Frame.newBuilder().setTag("Frame").build();
    private static final Action DEFAULT_ACTION = Action.getDefaultInstance();
    private static final Actions DEFAULT_ACTIONS =
            Actions.newBuilder().setOnClickAction(DEFAULT_ACTION).build();
    private static final Actions LONG_CLICK_ACTIONS =
            Actions.newBuilder().setOnLongClickAction(DEFAULT_ACTION).build();
    private static final Action PARTIAL_VIEW_ACTION = Action.newBuilder().build();
    private static final Action FULL_VIEW_ACTION = Action.newBuilder().build();
    private static final Actions VIEW_ACTIONS =
            Actions.newBuilder()
                    .addOnViewActions(
                            VisibilityAction.newBuilder().setProportionVisible(0.01f).setAction(
                                    PARTIAL_VIEW_ACTION))
                    .addOnViewActions(
                            VisibilityAction.newBuilder().setProportionVisible(1.00f).setAction(
                                    FULL_VIEW_ACTION))
                    .build();

    // Triggers when more than 1% of the view is visible
    private static final VisibilityAction VIEW_ACTION =
            VisibilityAction.newBuilder()
                    .setProportionVisible(0.01f)
                    .setAction(Action.newBuilder().build())
                    .build();
    // Triggers when more than 1% of the view is hidden
    private static final VisibilityAction HIDE_ACTION =
            VisibilityAction.newBuilder()
                    .setProportionVisible(0.99f)
                    .setAction(Action.newBuilder().build())
                    .build();
    private static final Actions VIEW_AND_HIDE_ACTIONS = Actions.newBuilder()
                                                                 .addOnViewActions(VIEW_ACTION)
                                                                 .addOnHideActions(HIDE_ACTION)
                                                                 .build();

    @Mock
    private ActionHandler mockActionHandler;
    @Mock
    private FrameContext mockFrameContext;
    @Mock
    private View.OnClickListener mockListener;
    @Mock
    private View.OnLongClickListener mockLongClickListener;

    private final View view = new View(context);
    private final View viewport = new View(context);

    private final ExtendedShadowView viewShadow = Shadow.extract(view);
    private final ExtendedShadowView viewportShadow = Shadow.extract(viewport);

    private final Set<VisibilityAction> activeActions = new HashSet<>();

    @Before
    public void setUp() {
        initMocks(this);
        when(mockFrameContext.getFrame()).thenReturn(DEFAULT_FRAME);
        when(mockFrameContext.getActionHandler()).thenReturn(mockActionHandler);
    }

    @Test
    public void testSetOnClickActions_success() {
        LogData logData = LogData.newBuilder().build();
        ViewUtils.setOnClickActions(DEFAULT_ACTIONS, view, mockFrameContext, logData);

        assertThat(view.hasOnClickListeners()).isTrue();

        view.callOnClick();
        verify(mockActionHandler)
                .handleAction(eq(DEFAULT_ACTION), eq(ActionType.CLICK), eq(DEFAULT_FRAME), eq(view),
                        same(logData));
        assertThat(view.getForeground()).isInstanceOf(RippleDrawable.class);
    }

    @Test
    public void testSetOnLongClickActions_success() {
        LogData logData = LogData.newBuilder().build();
        ViewUtils.setOnClickActions(LONG_CLICK_ACTIONS, view, mockFrameContext, logData);

        view.performLongClick();
        verify(mockActionHandler)
                .handleAction(eq(DEFAULT_ACTION), eq(ActionType.LONG_CLICK), eq(DEFAULT_FRAME),
                        eq(view), same(logData));
        assertThat(view.getForeground()).isInstanceOf(RippleDrawable.class);
    }

    @Test
    public void testSetOnClickActions_noOnClickActionsDefinedClearsActions() {
        view.setOnClickListener(mockListener);
        assertThat(view.hasOnClickListeners()).isTrue();

        ViewUtils.setOnClickActions(
                Actions.getDefaultInstance(), view, mockFrameContext, LogData.getDefaultInstance());

        assertViewNotClickable();
        assertThat(view.getForeground()).isNull();
    }

    @Test
    public void testSetOnClickActions_noOnLongClickActionsDefinedClearsActions() {
        view.setOnLongClickListener(mockLongClickListener);
        assertThat(view.isLongClickable()).isTrue();

        ViewUtils.setOnClickActions(
                Actions.getDefaultInstance(), view, mockFrameContext, LogData.getDefaultInstance());

        assertThat(view.isLongClickable()).isFalse();
        assertThat(view.getForeground()).isNull();
    }

    @Test
    public void testClearOnClickActions_success() {
        view.setOnClickListener(mockListener);
        assertThat(view.hasOnClickListeners()).isTrue();

        ViewUtils.clearOnClickActions(view);

        assertViewNotClickable();
    }

    @Test
    public void testClearOnLongClickActions_success() {
        view.setOnLongClickListener(mockLongClickListener);
        assertThat(view.isLongClickable()).isTrue();

        ViewUtils.clearOnLongClickActions(view);

        assertThat(view.isLongClickable()).isFalse();
    }

    @Test
    public void testSetAndClearClickActions() {
        ViewUtils.setOnClickActions(Actions.newBuilder()
                                            .setOnClickAction(DEFAULT_ACTION)
                                            .setOnLongClickAction(DEFAULT_ACTION)
                                            .build(),
                view, mockFrameContext, LogData.getDefaultInstance());
        ViewUtils.setOnClickActions(
                Actions.getDefaultInstance(), view, mockFrameContext, LogData.getDefaultInstance());

        assertViewNotClickable();
        assertThat(view.isLongClickable()).isFalse();
        assertThat(view.getForeground()).isNull();
    }

    @Test
    public void testViewActions_notVisible() {
        setupFullViewScenario();
        view.setVisibility(View.INVISIBLE);
        ViewUtils.maybeTriggerViewActions(
                view, viewport, VIEW_ACTIONS, mockActionHandler, DEFAULT_FRAME, activeActions);
        verifyZeroInteractions(mockActionHandler);
    }

    @Test
    public void testViewActions_notAttached() {
        setupFullViewScenario();
        viewShadow.setAttachedToWindow(false);
        ViewUtils.maybeTriggerViewActions(
                view, viewport, VIEW_ACTIONS, mockActionHandler, DEFAULT_FRAME, activeActions);
        verifyZeroInteractions(mockActionHandler);
    }

    @Test
    public void testViewActions_notIntersecting() {
        setupFullViewScenario();
        viewportShadow.setLocationOnScreen(0, 0);
        viewportShadow.setHeight(100);
        viewportShadow.setWidth(100);
        viewShadow.setLocationOnScreen(1000, 1000);
        ViewUtils.maybeTriggerViewActions(
                view, viewport, VIEW_ACTIONS, mockActionHandler, DEFAULT_FRAME, activeActions);
        verifyZeroInteractions(mockActionHandler);
    }

    @Test
    public void testViewActions_intersectionTriggersPartialView() {
        setupPartialViewScenario();
        ViewUtils.maybeTriggerViewActions(
                view, viewport, VIEW_ACTIONS, mockActionHandler, DEFAULT_FRAME, activeActions);
        verify(mockActionHandler)
                .handleAction(same(PARTIAL_VIEW_ACTION), eq(ActionType.VIEW), same(DEFAULT_FRAME),
                        same(view), eq(LogData.getDefaultInstance()));
    }

    @Test
    public void testViewActions_fullyOverlappingTriggersFullViewAndPartialView() {
        setupFullViewScenario();
        ViewUtils.maybeTriggerViewActions(
                view, viewport, VIEW_ACTIONS, mockActionHandler, DEFAULT_FRAME, activeActions);
        verify(mockActionHandler)
                .handleAction(same(FULL_VIEW_ACTION), eq(ActionType.VIEW), same(DEFAULT_FRAME),
                        same(view), eq(LogData.getDefaultInstance()));
        verify(mockActionHandler)
                .handleAction(same(PARTIAL_VIEW_ACTION), eq(ActionType.VIEW), same(DEFAULT_FRAME),
                        same(view), eq(LogData.getDefaultInstance()));
    }

    @Test
    public void testViewActions_fullOverlapTriggersActions() {
        setupFullViewScenario();
        viewShadow.setLocationOnScreen(0, 0);
        viewShadow.setWidth(100);
        viewShadow.setHeight(100);
        viewportShadow.setLocationOnScreen(0, 0);
        viewportShadow.setWidth(100);
        viewportShadow.setHeight(100);

        ViewUtils.maybeTriggerViewActions(
                view, viewport, VIEW_ACTIONS, mockActionHandler, DEFAULT_FRAME, activeActions);
        verify(mockActionHandler)
                .handleAction(same(FULL_VIEW_ACTION), eq(ActionType.VIEW), same(DEFAULT_FRAME),
                        same(view), eq(LogData.getDefaultInstance()));
        verify(mockActionHandler)
                .handleAction(same(PARTIAL_VIEW_ACTION), eq(ActionType.VIEW), same(DEFAULT_FRAME),
                        same(view), eq(LogData.getDefaultInstance()));
    }

    @Test
    public void testViewActions_noPartialViewAction() {
        setupFullViewScenario();
        ViewUtils.maybeTriggerViewActions(view, viewport,
                Actions.newBuilder()
                        .addOnViewActions(
                                VisibilityAction.newBuilder().setProportionVisible(1.00f).setAction(
                                        FULL_VIEW_ACTION))
                        .build(),
                mockActionHandler, DEFAULT_FRAME, activeActions);
        verify(mockActionHandler)
                .handleAction(same(FULL_VIEW_ACTION), eq(ActionType.VIEW), same(DEFAULT_FRAME),
                        same(view), eq(LogData.getDefaultInstance()));
    }

    @Test
    public void testViewActions_noFullViewAction() {
        setupFullViewScenario();
        ViewUtils.maybeTriggerViewActions(view, viewport,
                Actions.newBuilder()
                        .addOnViewActions(
                                VisibilityAction.newBuilder().setProportionVisible(0.01f).setAction(
                                        PARTIAL_VIEW_ACTION))
                        .build(),
                mockActionHandler, DEFAULT_FRAME, activeActions);
        verify(mockActionHandler)
                .handleAction(same(PARTIAL_VIEW_ACTION), eq(ActionType.VIEW), same(DEFAULT_FRAME),
                        same(view), eq(LogData.getDefaultInstance()));
    }

    @Test
    public void testViewActions_hideActionsNotTriggered() {
        setupFullViewScenario();
        ViewUtils.maybeTriggerViewActions(view, viewport,
                Actions.newBuilder()
                        .addOnHideActions(
                                VisibilityAction.newBuilder().setProportionVisible(0.01f).setAction(
                                        PARTIAL_VIEW_ACTION))
                        .build(),
                mockActionHandler, DEFAULT_FRAME, activeActions);
        verifyZeroInteractions(mockActionHandler);
    }

    @Test
    public void testViewActions_hideActionsTriggered() {
        setupPartialViewScenario();
        ViewUtils.maybeTriggerViewActions(view, viewport,
                Actions.newBuilder()
                        .addOnHideActions(
                                VisibilityAction.newBuilder().setProportionVisible(0.90f).setAction(
                                        PARTIAL_VIEW_ACTION))
                        .build(),
                mockActionHandler, DEFAULT_FRAME, activeActions);
        verify(mockActionHandler)
                .handleAction(same(PARTIAL_VIEW_ACTION), eq(ActionType.VIEW), same(DEFAULT_FRAME),
                        same(view), eq(LogData.getDefaultInstance()));
    }

    @Test
    public void testViewActions_activeActionsPreventsTriggering_notVisible() {
        activeActions.add(VIEW_ACTION);
        activeActions.add(HIDE_ACTION);

        setupFullViewScenario();
        viewShadow.setLocationOnScreen(1000, 1000);
        ViewUtils.maybeTriggerViewActions(view, viewport, VIEW_AND_HIDE_ACTIONS, mockActionHandler,
                DEFAULT_FRAME, activeActions);
        verifyZeroInteractions(mockActionHandler);
    }

    @Test
    public void testViewActions_activeActionsPreventsTriggering_partiallyVisible() {
        setupPartialViewScenario();
        activeActions.add(VIEW_ACTION);
        activeActions.add(HIDE_ACTION);

        ViewUtils.maybeTriggerViewActions(view, viewport, VIEW_AND_HIDE_ACTIONS, mockActionHandler,
                DEFAULT_FRAME, activeActions);
        verifyZeroInteractions(mockActionHandler);
    }

    @Test
    public void testViewActions_activeActionsPreventsTriggering_fullyVisible() {
        setupFullViewScenario();
        activeActions.add(VIEW_ACTION);
        activeActions.add(HIDE_ACTION);

        ViewUtils.maybeTriggerViewActions(view, viewport, VIEW_AND_HIDE_ACTIONS, mockActionHandler,
                DEFAULT_FRAME, activeActions);
        verifyZeroInteractions(mockActionHandler);
    }

    @Test
    public void testViewActions_notAttachedUnsetsActiveActions() {
        setupFullViewScenario();
        viewShadow.setAttachedToWindow(false);
        activeActions.add(VIEW_ACTION);

        ViewUtils.maybeTriggerViewActions(view, viewport, VIEW_AND_HIDE_ACTIONS, mockActionHandler,
                DEFAULT_FRAME, activeActions);
        assertThat(activeActions).containsExactly(HIDE_ACTION);
    }

    @Test
    public void testViewActions_notVisibleUnsetsActiveActions() {
        setupFullViewScenario();
        view.setVisibility(View.INVISIBLE);
        activeActions.add(VIEW_ACTION);

        ViewUtils.maybeTriggerViewActions(view, viewport, VIEW_AND_HIDE_ACTIONS, mockActionHandler,
                DEFAULT_FRAME, activeActions);
        assertThat(activeActions).containsExactly(HIDE_ACTION);
    }

    @Test
    public void testHideActions() {
        ViewUtils.triggerHideActions(
                view, VIEW_AND_HIDE_ACTIONS, mockActionHandler, DEFAULT_FRAME, activeActions);
        assertThat(activeActions).containsExactly(HIDE_ACTION);
        verify(mockActionHandler)
                .handleAction(HIDE_ACTION.getAction(), ActionType.VIEW, DEFAULT_FRAME, view,
                        LogData.getDefaultInstance());
    }

    @Test
    public void testHideActions_deduplicates() {
        ViewUtils.triggerHideActions(
                view, VIEW_AND_HIDE_ACTIONS, mockActionHandler, DEFAULT_FRAME, activeActions);
        verify(mockActionHandler)
                .handleAction(HIDE_ACTION.getAction(), ActionType.VIEW, DEFAULT_FRAME, view,
                        LogData.getDefaultInstance());

        assertThat(activeActions).containsExactly(HIDE_ACTION);

        // Hide action is not triggered again.
        ViewUtils.triggerHideActions(
                view, VIEW_AND_HIDE_ACTIONS, mockActionHandler, DEFAULT_FRAME, activeActions);
        verify(mockActionHandler, times(1))
                .handleAction(HIDE_ACTION.getAction(), ActionType.VIEW, DEFAULT_FRAME, view,
                        LogData.getDefaultInstance());
    }

    @Test
    public void testApplyOverlayColor_setsColorFilter() {
        int overlayColor1 = 0xFFEEDDCC;
        int overlayColor2 = 0xCCDDEEFF;
        Drawable original =
                new BitmapDrawable(Bitmap.createBitmap(12, 34, Bitmap.Config.ARGB_8888));

        Drawable result1 = ViewUtils.applyOverlayColor(original, overlayColor1);
        Drawable result2 = ViewUtils.applyOverlayColor(original, overlayColor2);

        assertThat(result1).isNotSameInstanceAs(original);
        assertThat(result1.getColorFilter())
                .isEqualTo(new PorterDuffColorFilter(overlayColor1, Mode.SRC_IN));

        assertThat(result2).isNotSameInstanceAs(original);
        assertThat(result2.getColorFilter())
                .isEqualTo(new PorterDuffColorFilter(overlayColor2, Mode.SRC_IN));
    }

    @Test
    public void testApplyOverlayColor_nullIsNoOp() {
        Drawable original =
                new BitmapDrawable(Bitmap.createBitmap(12, 34, Bitmap.Config.ARGB_8888));

        Drawable result1 = ViewUtils.applyOverlayColor(original, null);

        assertThat(result1).isSameInstanceAs(original);
        assertThat(result1.getColorFilter()).isNull();
    }

    /** Sets up view and viewport so that view should be fully visible. */
    private void setupFullViewScenario() {
        view.setVisibility(View.VISIBLE);
        viewShadow.setAttachedToWindow(true);
        viewShadow.setLocationOnScreen(10, 10);
        viewShadow.setWidth(10);
        viewShadow.setHeight(10);

        viewport.setVisibility(View.VISIBLE);
        viewportShadow.setAttachedToWindow(true);
        viewportShadow.setLocationOnScreen(0, 0);
        viewportShadow.setWidth(100);
        viewportShadow.setHeight(100);

        activeActions.clear();
    }

    private void setupPartialViewScenario() {
        setupFullViewScenario();
        viewShadow.setHeight(1000);
    }

    private void assertViewNotClickable() {
        assertThat(view.hasOnClickListeners()).isFalse();
        assertThat(view.isClickable()).isFalse();
    }
}
