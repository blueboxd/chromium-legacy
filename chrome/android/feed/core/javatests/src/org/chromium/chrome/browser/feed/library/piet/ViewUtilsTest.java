// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

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

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;

import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler.ActionType;
import org.chromium.chrome.browser.feed.library.testing.shadows.ExtendedShadowView;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.Action;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.Actions;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.VisibilityAction;
import org.chromium.components.feed.core.proto.ui.piet.LogDataProto.LogData;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Frame;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.HashSet;
import java.util.Set;

/** Tests of the {@link ViewUtils}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ExtendedShadowView.class})
public class ViewUtilsTest {
    private final Context mContext = Robolectric.buildActivity(Activity.class).get();

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
    private ActionHandler mMockActionHandler;
    @Mock
    private FrameContext mMockFrameContext;
    @Mock
    private View.OnClickListener mMockListener;
    @Mock
    private View.OnLongClickListener mMockLongClickListener;

    private final View mView = new View(mContext);
    private final View mViewport = new View(mContext);

    private final ExtendedShadowView mViewShadow = Shadow.extract(mView);
    private final ExtendedShadowView mViewportShadow = Shadow.extract(mViewport);

    private final Set<VisibilityAction> mActiveActions = new HashSet<>();

    @Before
    public void setUp() {
        initMocks(this);
        when(mMockFrameContext.getFrame()).thenReturn(DEFAULT_FRAME);
        when(mMockFrameContext.getActionHandler()).thenReturn(mMockActionHandler);
    }

    @Test
    public void testSetOnClickActions_success() {
        LogData logData = LogData.newBuilder().build();
        ViewUtils.setOnClickActions(DEFAULT_ACTIONS, mView, mMockFrameContext, logData);

        assertThat(mView.hasOnClickListeners()).isTrue();

        mView.callOnClick();
        verify(mMockActionHandler)
                .handleAction(eq(DEFAULT_ACTION), eq(ActionType.CLICK), eq(DEFAULT_FRAME),
                        eq(mView), same(logData));
        assertThat(mView.getForeground()).isInstanceOf(RippleDrawable.class);
    }

    @Test
    public void testSetOnLongClickActions_success() {
        LogData logData = LogData.newBuilder().build();
        ViewUtils.setOnClickActions(LONG_CLICK_ACTIONS, mView, mMockFrameContext, logData);

        mView.performLongClick();
        verify(mMockActionHandler)
                .handleAction(eq(DEFAULT_ACTION), eq(ActionType.LONG_CLICK), eq(DEFAULT_FRAME),
                        eq(mView), same(logData));
        assertThat(mView.getForeground()).isInstanceOf(RippleDrawable.class);
    }

    @Test
    public void testSetOnClickActions_noOnClickActionsDefinedClearsActions() {
        mView.setOnClickListener(mMockListener);
        assertThat(mView.hasOnClickListeners()).isTrue();

        ViewUtils.setOnClickActions(Actions.getDefaultInstance(), mView, mMockFrameContext,
                LogData.getDefaultInstance());

        assertViewNotClickable();
        assertThat(mView.getForeground()).isNull();
    }

    @Test
    public void testSetOnClickActions_noOnLongClickActionsDefinedClearsActions() {
        mView.setOnLongClickListener(mMockLongClickListener);
        assertThat(mView.isLongClickable()).isTrue();

        ViewUtils.setOnClickActions(Actions.getDefaultInstance(), mView, mMockFrameContext,
                LogData.getDefaultInstance());

        assertThat(mView.isLongClickable()).isFalse();
        assertThat(mView.getForeground()).isNull();
    }

    @Test
    public void testClearOnClickActions_success() {
        mView.setOnClickListener(mMockListener);
        assertThat(mView.hasOnClickListeners()).isTrue();

        ViewUtils.clearOnClickActions(mView);

        assertViewNotClickable();
    }

    @Test
    public void testClearOnLongClickActions_success() {
        mView.setOnLongClickListener(mMockLongClickListener);
        assertThat(mView.isLongClickable()).isTrue();

        ViewUtils.clearOnLongClickActions(mView);

        assertThat(mView.isLongClickable()).isFalse();
    }

    @Test
    public void testSetAndClearClickActions() {
        ViewUtils.setOnClickActions(Actions.newBuilder()
                                            .setOnClickAction(DEFAULT_ACTION)
                                            .setOnLongClickAction(DEFAULT_ACTION)
                                            .build(),
                mView, mMockFrameContext, LogData.getDefaultInstance());
        ViewUtils.setOnClickActions(Actions.getDefaultInstance(), mView, mMockFrameContext,
                LogData.getDefaultInstance());

        assertViewNotClickable();
        assertThat(mView.isLongClickable()).isFalse();
        assertThat(mView.getForeground()).isNull();
    }

    @Test
    public void testViewActions_notVisible() {
        setupFullViewScenario();
        mView.setVisibility(View.INVISIBLE);
        ViewUtils.maybeTriggerViewActions(
                mView, mViewport, VIEW_ACTIONS, mMockActionHandler, DEFAULT_FRAME, mActiveActions);
        verifyZeroInteractions(mMockActionHandler);
    }

    @Test
    public void testViewActions_notAttached() {
        setupFullViewScenario();
        mViewShadow.setAttachedToWindow(false);
        ViewUtils.maybeTriggerViewActions(
                mView, mViewport, VIEW_ACTIONS, mMockActionHandler, DEFAULT_FRAME, mActiveActions);
        verifyZeroInteractions(mMockActionHandler);
    }

    @Test
    public void testViewActions_notIntersecting() {
        setupFullViewScenario();
        mViewportShadow.setLocationOnScreen(0, 0);
        mViewportShadow.setHeight(100);
        mViewportShadow.setWidth(100);
        mViewShadow.setLocationOnScreen(1000, 1000);
        ViewUtils.maybeTriggerViewActions(
                mView, mViewport, VIEW_ACTIONS, mMockActionHandler, DEFAULT_FRAME, mActiveActions);
        verifyZeroInteractions(mMockActionHandler);
    }

    @Test
    public void testViewActions_intersectionTriggersPartialView() {
        setupPartialViewScenario();
        ViewUtils.maybeTriggerViewActions(
                mView, mViewport, VIEW_ACTIONS, mMockActionHandler, DEFAULT_FRAME, mActiveActions);
        verify(mMockActionHandler)
                .handleAction(same(PARTIAL_VIEW_ACTION), eq(ActionType.VIEW), same(DEFAULT_FRAME),
                        same(mView), eq(LogData.getDefaultInstance()));
    }

    @Test
    public void testViewActions_fullyOverlappingTriggersFullViewAndPartialView() {
        setupFullViewScenario();
        ViewUtils.maybeTriggerViewActions(
                mView, mViewport, VIEW_ACTIONS, mMockActionHandler, DEFAULT_FRAME, mActiveActions);
        verify(mMockActionHandler)
                .handleAction(same(FULL_VIEW_ACTION), eq(ActionType.VIEW), same(DEFAULT_FRAME),
                        same(mView), eq(LogData.getDefaultInstance()));
        verify(mMockActionHandler)
                .handleAction(same(PARTIAL_VIEW_ACTION), eq(ActionType.VIEW), same(DEFAULT_FRAME),
                        same(mView), eq(LogData.getDefaultInstance()));
    }

    @Test
    public void testViewActions_fullOverlapTriggersActions() {
        setupFullViewScenario();
        mViewShadow.setLocationOnScreen(0, 0);
        mViewShadow.setWidth(100);
        mViewShadow.setHeight(100);
        mViewportShadow.setLocationOnScreen(0, 0);
        mViewportShadow.setWidth(100);
        mViewportShadow.setHeight(100);

        ViewUtils.maybeTriggerViewActions(
                mView, mViewport, VIEW_ACTIONS, mMockActionHandler, DEFAULT_FRAME, mActiveActions);
        verify(mMockActionHandler)
                .handleAction(same(FULL_VIEW_ACTION), eq(ActionType.VIEW), same(DEFAULT_FRAME),
                        same(mView), eq(LogData.getDefaultInstance()));
        verify(mMockActionHandler)
                .handleAction(same(PARTIAL_VIEW_ACTION), eq(ActionType.VIEW), same(DEFAULT_FRAME),
                        same(mView), eq(LogData.getDefaultInstance()));
    }

    @Test
    public void testViewActions_noPartialViewAction() {
        setupFullViewScenario();
        ViewUtils.maybeTriggerViewActions(mView, mViewport,
                Actions.newBuilder()
                        .addOnViewActions(
                                VisibilityAction.newBuilder().setProportionVisible(1.00f).setAction(
                                        FULL_VIEW_ACTION))
                        .build(),
                mMockActionHandler, DEFAULT_FRAME, mActiveActions);
        verify(mMockActionHandler)
                .handleAction(same(FULL_VIEW_ACTION), eq(ActionType.VIEW), same(DEFAULT_FRAME),
                        same(mView), eq(LogData.getDefaultInstance()));
    }

    @Test
    public void testViewActions_noFullViewAction() {
        setupFullViewScenario();
        ViewUtils.maybeTriggerViewActions(mView, mViewport,
                Actions.newBuilder()
                        .addOnViewActions(
                                VisibilityAction.newBuilder().setProportionVisible(0.01f).setAction(
                                        PARTIAL_VIEW_ACTION))
                        .build(),
                mMockActionHandler, DEFAULT_FRAME, mActiveActions);
        verify(mMockActionHandler)
                .handleAction(same(PARTIAL_VIEW_ACTION), eq(ActionType.VIEW), same(DEFAULT_FRAME),
                        same(mView), eq(LogData.getDefaultInstance()));
    }

    @Test
    public void testViewActions_hideActionsNotTriggered() {
        setupFullViewScenario();
        ViewUtils.maybeTriggerViewActions(mView, mViewport,
                Actions.newBuilder()
                        .addOnHideActions(
                                VisibilityAction.newBuilder().setProportionVisible(0.01f).setAction(
                                        PARTIAL_VIEW_ACTION))
                        .build(),
                mMockActionHandler, DEFAULT_FRAME, mActiveActions);
        verifyZeroInteractions(mMockActionHandler);
    }

    @Test
    public void testViewActions_hideActionsTriggered() {
        setupPartialViewScenario();
        ViewUtils.maybeTriggerViewActions(mView, mViewport,
                Actions.newBuilder()
                        .addOnHideActions(
                                VisibilityAction.newBuilder().setProportionVisible(0.90f).setAction(
                                        PARTIAL_VIEW_ACTION))
                        .build(),
                mMockActionHandler, DEFAULT_FRAME, mActiveActions);
        verify(mMockActionHandler)
                .handleAction(same(PARTIAL_VIEW_ACTION), eq(ActionType.VIEW), same(DEFAULT_FRAME),
                        same(mView), eq(LogData.getDefaultInstance()));
    }

    @Test
    public void testViewActions_activeActionsPreventsTriggering_notVisible() {
        mActiveActions.add(VIEW_ACTION);
        mActiveActions.add(HIDE_ACTION);

        setupFullViewScenario();
        mViewShadow.setLocationOnScreen(1000, 1000);
        ViewUtils.maybeTriggerViewActions(mView, mViewport, VIEW_AND_HIDE_ACTIONS,
                mMockActionHandler, DEFAULT_FRAME, mActiveActions);
        verifyZeroInteractions(mMockActionHandler);
    }

    @Test
    public void testViewActions_activeActionsPreventsTriggering_partiallyVisible() {
        setupPartialViewScenario();
        mActiveActions.add(VIEW_ACTION);
        mActiveActions.add(HIDE_ACTION);

        ViewUtils.maybeTriggerViewActions(mView, mViewport, VIEW_AND_HIDE_ACTIONS,
                mMockActionHandler, DEFAULT_FRAME, mActiveActions);
        verifyZeroInteractions(mMockActionHandler);
    }

    @Test
    public void testViewActions_activeActionsPreventsTriggering_fullyVisible() {
        setupFullViewScenario();
        mActiveActions.add(VIEW_ACTION);
        mActiveActions.add(HIDE_ACTION);

        ViewUtils.maybeTriggerViewActions(mView, mViewport, VIEW_AND_HIDE_ACTIONS,
                mMockActionHandler, DEFAULT_FRAME, mActiveActions);
        verifyZeroInteractions(mMockActionHandler);
    }

    @Test
    public void testViewActions_notAttachedUnsetsActiveActions() {
        setupFullViewScenario();
        mViewShadow.setAttachedToWindow(false);
        mActiveActions.add(VIEW_ACTION);

        ViewUtils.maybeTriggerViewActions(mView, mViewport, VIEW_AND_HIDE_ACTIONS,
                mMockActionHandler, DEFAULT_FRAME, mActiveActions);
        assertThat(mActiveActions).containsExactly(HIDE_ACTION);
    }

    @Test
    public void testViewActions_notVisibleUnsetsActiveActions() {
        setupFullViewScenario();
        mView.setVisibility(View.INVISIBLE);
        mActiveActions.add(VIEW_ACTION);

        ViewUtils.maybeTriggerViewActions(mView, mViewport, VIEW_AND_HIDE_ACTIONS,
                mMockActionHandler, DEFAULT_FRAME, mActiveActions);
        assertThat(mActiveActions).containsExactly(HIDE_ACTION);
    }

    @Test
    public void testHideActions() {
        ViewUtils.triggerHideActions(
                mView, VIEW_AND_HIDE_ACTIONS, mMockActionHandler, DEFAULT_FRAME, mActiveActions);
        assertThat(mActiveActions).containsExactly(HIDE_ACTION);
        verify(mMockActionHandler)
                .handleAction(HIDE_ACTION.getAction(), ActionType.VIEW, DEFAULT_FRAME, mView,
                        LogData.getDefaultInstance());
    }

    @Test
    public void testHideActions_deduplicates() {
        ViewUtils.triggerHideActions(
                mView, VIEW_AND_HIDE_ACTIONS, mMockActionHandler, DEFAULT_FRAME, mActiveActions);
        verify(mMockActionHandler)
                .handleAction(HIDE_ACTION.getAction(), ActionType.VIEW, DEFAULT_FRAME, mView,
                        LogData.getDefaultInstance());

        assertThat(mActiveActions).containsExactly(HIDE_ACTION);

        // Hide action is not triggered again.
        ViewUtils.triggerHideActions(
                mView, VIEW_AND_HIDE_ACTIONS, mMockActionHandler, DEFAULT_FRAME, mActiveActions);
        verify(mMockActionHandler, times(1))
                .handleAction(HIDE_ACTION.getAction(), ActionType.VIEW, DEFAULT_FRAME, mView,
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
        mView.setVisibility(View.VISIBLE);
        mViewShadow.setAttachedToWindow(true);
        mViewShadow.setLocationOnScreen(10, 10);
        mViewShadow.setWidth(10);
        mViewShadow.setHeight(10);

        mViewport.setVisibility(View.VISIBLE);
        mViewportShadow.setAttachedToWindow(true);
        mViewportShadow.setLocationOnScreen(0, 0);
        mViewportShadow.setWidth(100);
        mViewportShadow.setHeight(100);

        mActiveActions.clear();
    }

    private void setupPartialViewScenario() {
        setupFullViewScenario();
        mViewShadow.setHeight(1000);
    }

    private void assertViewNotClickable() {
        assertThat(mView.hasOnClickListeners()).isFalse();
        assertThat(mView.isClickable()).isFalse();
    }
}
