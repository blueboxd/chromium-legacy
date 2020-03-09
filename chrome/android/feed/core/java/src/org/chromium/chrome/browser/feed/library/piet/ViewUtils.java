// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import android.graphics.PorterDuff.Mode;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;

import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler.ActionType;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.Actions;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.VisibilityAction;
import org.chromium.components.feed.core.proto.ui.piet.LogDataProto.LogData;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Frame;

import java.util.Set;

/** Utility class, providing useful methods to interact with Views. */
public class ViewUtils {
    private static final String TAG = "ViewUtils";

    /**
     * Attaches the onClick action from actions to the view, executed by the handler. In Android M+,
     * a RippleDrawable is added to the foreground of the view, so that a ripple animation happens
     * on each click.
     */
    static void setOnClickActions(
            Actions actions, View view, FrameContext frameContext, LogData logData) {
        ActionHandler handler = frameContext.getActionHandler();
        if (actions.hasOnLongClickAction()) {
            view.setOnLongClickListener(v -> {
                handler.handleAction(actions.getOnLongClickAction(), ActionType.LONG_CLICK,
                        frameContext.getFrame(), view, logData);
                return true;
            });
        } else {
            clearOnLongClickActions(view);
        }
        if (actions.hasOnClickAction()) {
            view.setOnClickListener(v -> {
                handler.handleAction(actions.getOnClickAction(), ActionType.CLICK,
                        frameContext.getFrame(), view, logData);
            });
        } else {
            clearOnClickActions(view);
        }
        // TODO: Implement alternative support for older versions
        if (Build.VERSION.SDK_INT >= VERSION_CODES.M) {
            if (actions.hasOnClickAction() || actions.hasOnLongClickAction()) {
                // CAUTION: View.setForeground() is only available in L+
                view.setForeground(view.getContext().getDrawable(R.drawable.piet_clickable_ripple));
            } else {
                view.setForeground(null);
            }
        }
    }

    static void clearOnLongClickActions(View view) {
        view.setOnLongClickListener(null);
        view.setLongClickable(false);
    }

    /** Sets clickability to false. */
    static void clearOnClickActions(View view) {
        if (view.hasOnClickListeners()) {
            view.setOnClickListener(null);
        }

        view.setClickable(false);
    }

    /**
     * Check if this view is visible, trigger actions accordingly, and update set of active actions.
     *
     * <p>Actions are added to activeActions when they trigger, and removed when the condition that
     * caused them to trigger is no longer true. (Ex. a view action will be removed when the view
     * goes off screen)
     *
     * @param view this adapter's view
     * @param viewport the visible viewport
     * @param actions this element's actions, which might be triggered
     * @param actionHandler host-provided handler to execute actions
     * @param frame the parent frame
     * @param activeActions mutable set of currently-triggered actions; this will get updated by
     *         this
     *     method as new actions are triggered and old actions are reset.
     */
    static void maybeTriggerViewActions(View view, View viewport, Actions actions,
            ActionHandler actionHandler, Frame frame, Set<VisibilityAction> activeActions) {
        if (actions.getOnViewActionsCount() == 0 && actions.getOnHideActionsCount() == 0) {
            return;
        }
        // For invisible views, short-cut triggering of hide/show actions.
        if (view.getVisibility() != View.VISIBLE || !ViewCompat.isAttachedToWindow(view)) {
            triggerHideActions(view, actions, actionHandler, frame, activeActions);
            return;
        }

        // Figure out overlap of viewport and view, and trigger based on proportion overlap.
        Rect viewRect = getViewRectOnScreen(view);
        Rect viewportRect = getViewRectOnScreen(viewport);

        if (viewportRect.intersect(viewRect)) {
            int viewArea = viewRect.height() * viewRect.width();
            int visibleArea = viewportRect.height() * viewportRect.width();
            float proportionVisible = ((float) visibleArea) / viewArea;

            for (VisibilityAction visibilityAction : actions.getOnViewActionsList()) {
                if (proportionVisible >= visibilityAction.getProportionVisible()) {
                    if (activeActions.add(visibilityAction)) {
                        actionHandler.handleAction(visibilityAction.getAction(), ActionType.VIEW,
                                frame, view, LogData.getDefaultInstance());
                    }
                } else {
                    activeActions.remove(visibilityAction);
                }
            }

            for (VisibilityAction visibilityAction : actions.getOnHideActionsList()) {
                if (proportionVisible < visibilityAction.getProportionVisible()) {
                    if (activeActions.add(visibilityAction)) {
                        actionHandler.handleAction(visibilityAction.getAction(), ActionType.VIEW,
                                frame, view, LogData.getDefaultInstance());
                    }
                } else {
                    activeActions.remove(visibilityAction);
                }
            }
        }
    }

    static void triggerHideActions(View view, Actions actions, ActionHandler actionHandler,
            Frame frame, Set<VisibilityAction> activeActions) {
        activeActions.removeAll(actions.getOnViewActionsList());
        for (VisibilityAction visibilityAction : actions.getOnHideActionsList()) {
            if (activeActions.add(visibilityAction)) {
                actionHandler.handleAction(visibilityAction.getAction(), ActionType.VIEW, frame,
                        view, LogData.getDefaultInstance());
            }
        }
    }

    /**
     * Replaces all non-transparent pixels of a {@link Drawable} with overlayColor and returns the
     * new
     * {@link Drawable}; If overlayColor is null, returns the original {@link Drawable}.
     */
    static Drawable applyOverlayColor(Drawable drawable, @Nullable Integer overlayColor) {
        if (overlayColor == null) {
            return drawable;
        }
        Drawable drawableWithOverlay = drawable.mutate();
        drawableWithOverlay.setColorFilter(overlayColor, Mode.SRC_IN);
        return drawableWithOverlay;
    }

    private static Rect getViewRectOnScreen(View view) {
        int[] viewLocation = new int[2];
        view.getLocationOnScreen(viewLocation);

        return new Rect(viewLocation[0], viewLocation[1], viewLocation[0] + view.getWidth(),
                viewLocation[1] + view.getHeight());
    }

    /** Private constructor to prevent instantiation. */
    private ViewUtils() {}
}
