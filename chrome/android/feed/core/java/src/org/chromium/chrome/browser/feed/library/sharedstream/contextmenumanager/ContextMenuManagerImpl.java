// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.contextmenumanager;

import static org.chromium.chrome.browser.feed.library.common.Validators.checkNotNull;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewParent;
import android.view.WindowManager.LayoutParams;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.PopupWindow;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.ui.LayoutUtils;
import org.chromium.chrome.browser.feed.library.sharedstream.publicapi.menumeasurer.MenuMeasurer;
import org.chromium.chrome.browser.feed.library.sharedstream.publicapi.menumeasurer.Size;
import org.chromium.chrome.feed.R;

import java.util.List;

/**
 * Implementation of {@link ContextMenuManager} that shows the context menu anchored by a view. This
 * is only supported for N+
 */
@TargetApi(Build.VERSION_CODES.N)
public class ContextMenuManagerImpl implements ContextMenuManager {
    private static final String TAG = "ContextMenuManager";

    // Indicates how deep into the card the menu should be set if the menu is showing below a card.
    // IE 4 indicates that the menu should be placed 1/4 of the way from the edge of the card.
    private static final int FRACTION_FROM_EDGE = 4;

    // Indicates how far from the bottom of the card the menu should be set if the menu is showing
    // above a card.
    private static final int FRACTION_FROM_BOTTOM_EDGE = FRACTION_FROM_EDGE - 1;

    private final MenuMeasurer mMenuMeasurer;
    private final Context mContext;
    @Nullable
    private View mView;

    @Nullable
    private PopupWindow mPopupWindow;

    public ContextMenuManagerImpl(MenuMeasurer menuMeasurer, Context context) {
        this.mMenuMeasurer = menuMeasurer;
        this.mContext = context;
    }

    @Override
    public void setView(View view) {
        this.mView = view;
    }

    /**
     * Opens a context menu if there is currently no open context menu. Returns whether a menu was
     * opened.
     *
     * @param anchorView The {@link View} to position the menu by.
     * @param items The contents to display.
     * @param handler The {@link ContextMenuClickHandler} that handles the user clicking on an
     *         option.
     */
    @Override
    public boolean openContextMenu(
            View anchorView, List<String> items, ContextMenuClickHandler handler) {
        if (menuShowing()) {
            return false;
        }

        ArrayAdapter<String> adapter =
                new ArrayAdapter<>(mContext, R.layout.feed_simple_list_item, items);

        ListView listView = createListView(adapter, mContext);

        Size measurements = mMenuMeasurer.measureAdapterContent(
                listView, adapter, /* windowPadding= */ 0, getStreamWidth(), getStreamHeight());

        PopupWindowWithDimensions popupWindowWithDimensions =
                createPopupWindow(listView, measurements, mContext);

        listView.setOnItemClickListener((parent, view, position, id) -> {
            handler.handleClick(position);
            popupWindowWithDimensions.getPopupWindow().dismiss();
        });

        int yOffset = getYOffsetForContextMenu(
                anchorView, popupWindowWithDimensions.getDimensions().getHeight());

        int xOffset = anchorView.getWidth() / FRACTION_FROM_EDGE;

        // In RtL locals, we want to show the menu 1 / FRACTION_FROM_EDGE from the right of the
        // anchor view. showAsDropDown in RtL aligns the right edge of the menu with the right edge
        // of the anchor view, so we want to shift to the left.
        if (LayoutUtils.isDefaultLocaleRtl()) {
            xOffset *= -1;
        }

        // Note: PopupWindow#showAsDropDown is bugged for android versions below N and should not be
        // used in that context. Instead, showAtLocation should be used, and the menu should be set
        // at an absolute location. However, to get enter animations to properly work,
        // showAsDropDown needs to be used so that Android knows whether the menu is opening above
        // or below the anchor view. This is possible as this class is only used for N+.
        popupWindowWithDimensions.getPopupWindow().showAsDropDown(anchorView, xOffset, yOffset);

        // We want to prevent any more touch events from be used (if a user has yet to end their
        // current touch session). This prevents the possibility of scrolling after the context menu
        // opens.
        ViewParent parent = anchorView.getParent();
        if (parent != null) {
            parent.requestDisallowInterceptTouchEvent(true);
        }

        this.mPopupWindow = popupWindowWithDimensions.getPopupWindow();
        return true;
    }

    /**
     * Gets the offset in pixels from the bottom of the anchorview to place the menu.
     *
     * <p>First, we attempt to place the top of the menu {@code 1/FRACTION_FROM_EDGE} down from the
     * top of the anchor view. If the menu cannot fit there, we attempt to put the bottom of the
     * menu
     * {@code 1/FRACTION_FROM_EDGE} up from the bottom of the anchor view. If the menu can fit in
     * neither position it instead is centered in the stream.
     */
    // TODO: Add tests for each case here.
    private int getYOffsetForContextMenu(
            int menuHeight, int anchorViewYInWindow, int anchorViewHeight, int windowHeight) {
        Logger.i(TAG,
                "Getting Y offset for context menu. menuHeight: %s, anchorViewYInWindow: %s, "
                        + "anchorViewHeight: %s, windowHeight: %s",
                menuHeight, anchorViewYInWindow, anchorViewHeight, windowHeight);

        // Check if the menu can fit below the card.
        if (menuHeight + anchorViewYInWindow + anchorViewHeight / FRACTION_FROM_EDGE
                < windowHeight) {
            // Check if 1/FRACTION_FROM_EDGE of the way down the card is visible.
            if (-FRACTION_FROM_EDGE * anchorViewYInWindow < anchorViewHeight) {
                // 1/FRACTION_FROM_EDGE of the way down the card is visible stream, so offset to
                // that point, from the bottom of the anchor view.
                return -FRACTION_FROM_BOTTOM_EDGE * anchorViewHeight / FRACTION_FROM_EDGE;
            } else {
                // 1/FRACTION_FROM_EDGE down from the top of the anchor view is not visible, offset
                // the menu to the top of the stream.
                return -1 * (anchorViewHeight + anchorViewYInWindow);
            }
            // Check if the menu can have the bottom 1/FRACTION_FROM_EDGE off from the bottom of the
            // anchor.
        } else if (anchorViewYInWindow - menuHeight + anchorViewHeight / FRACTION_FROM_EDGE >= 0) {
            // Check if 1/FRACTION_FROM_EDGE from the bottom of the anchor is visible.
            if (anchorViewYInWindow
                            + FRACTION_FROM_BOTTOM_EDGE * anchorViewHeight / FRACTION_FROM_EDGE
                    < windowHeight) {
                // FRACTION_FROM_BOTTOM_EDGE/FRACTION_FROM_EDGE of the way down the card is visible,
                // so position the bottom there.
                return -(menuHeight + anchorViewHeight / FRACTION_FROM_EDGE);
            } else {
                // Less than the top 1/FRACTION_FROM_EDGE of the card is on the screen. Offset so
                // the menu is at the bottom of the screen
                return -(menuHeight + anchorViewHeight - (windowHeight - anchorViewYInWindow));
            }
        } else {
            // The menu will fit neither above, nor below the content. Center it in the middle of
            // the screen.

            // First, get an offset so that the top of the menu will be at the top of the screen.
            int offset = -(anchorViewHeight + anchorViewYInWindow);

            // Then, shift so that the middle of the menu is at the top of the screen.
            offset -= menuHeight / 2;

            // Then, shift so that the center of the menu is at the center of the screen.
            offset += windowHeight / 2;

            return offset;
        }
    }

    private int getYOffsetForContextMenu(View anchorView, int menuHeight) {
        int anchorViewY = getYPosition(anchorView);
        int anchorViewYInWindow = anchorViewY - getStreamYPosition();

        return getYOffsetForContextMenu(
                menuHeight, anchorViewYInWindow, anchorView.getHeight(), getStreamHeight());
    }

    private PopupWindowWithDimensions createPopupWindow(
            ListView listView, Size measurements, Context context) {
        // While using elevation to create shadows should work in lollipop+, the shadow was not
        // appearing in versions below Android N, so we are using ninepatch below N.
        return createPopupWindowWithElevation(listView, measurements, context);
    }

    // TODO: Remove the nullness suppression.
    @SuppressWarnings("nullness:argument.type.incompatible")
    private PopupWindowWithDimensions createPopupWindowWithElevation(
            ListView listView, Size measurements, Context context) {
        // Note: We are intentionally using contextPopupMenuStyle, which allows hosts to modify the
        // style of context menus. This should not be changed without speaking with host teams.
        PopupWindow popupWindow = new PopupWindow(
                getBaseContext(context), null, android.R.attr.contextPopupMenuStyle);
        Drawable background = popupWindow.getBackground();
        Rect backgroundPadding = new Rect();
        background.getPadding(backgroundPadding);
        popupWindow.setContentView(listView);

        // We want the menu to wrap content.
        popupWindow.setHeight(LayoutParams.WRAP_CONTENT);

        // Material design specifies the width of a menu, so we set it specifically, instead of
        // wrapping content.
        popupWindow.setWidth(measurements.getWidth() + backgroundPadding.width());

        popupWindow.setFocusable(true);

        // So this is weird. In our situation, the menu does overlap with the anchor. Setting this
        // to false doesn't actually make it impossible to overlap with the anchor, it just changes
        // the default behavior of PopupWindow#showAsDropDown. All of the positioning logic written
        // to this point has assumed that the default position of a drop down menu is with the top
        // left of the menu touching the bottom right of the anchor view. Setting overlap anchor to
        // true changes the default behavior so that the top left corner of the menu is over the top
        // left corner of the anchor view. As the logic has been written assuming overlap anchor is
        // false, we set it false.
        popupWindow.setOverlapAnchor(false);

        // Our positioning code uses this sizing to properly position the menus. The measurement we
        // need here is from the top of the menu, to the bottom of where the shadow ends (and
        // similarly for width). So we divide the width and height of the shadows by two to now
        // include the shadows on both sides.
        Size popupAndBackgroundSize = new Size(measurements.getWidth()
                        + (backgroundPadding.width() + background.getMinimumWidth()) / 2,
                measurements.getHeight()
                        + (backgroundPadding.height() + background.getMinimumHeight()) / 2);

        return new PopupWindowWithDimensions(popupWindow, popupAndBackgroundSize);
    }

    private Context getBaseContext(Context context) {
        if (context instanceof ContextThemeWrapper) {
            return ((ContextThemeWrapper) context).getBaseContext();
        }
        return context;
    }

    private ListView createListView(ArrayAdapter<String> adapter, Context context) {
        ListView listView = new ListView(context);
        listView.setAdapter(adapter);
        listView.setDivider(null);
        listView.setDividerHeight(0);
        return listView;
    }

    /**
     * Gets the height of the Stream. This is specifically the height visible on screen, not
     * including anything below the screen.
     */
    int getStreamHeight() {
        return checkNotNull(mView).getHeight();
    }

    /** Gets the width of the Stream. */
    private int getStreamWidth() {
        return checkNotNull(mView).getWidth();
    }

    /** Gets the Y coordinate of the position of the Stream. */
    private int getStreamYPosition() {
        return getYPosition(checkNotNull(mView));
    }

    private boolean menuShowing() {
        return mPopupWindow != null && mPopupWindow.isShowing();
    }

    @Override
    public void dismissPopup() {
        if (mPopupWindow == null) {
            return;
        }

        mPopupWindow.dismiss();
        mPopupWindow = null;
    }

    private int getYPosition(View view) {
        int[] viewLocation = new int[2];
        view.getLocationInWindow(viewLocation);

        return viewLocation[1];
    }

    /** Represents a {@link PopupWindow} that accounts for padding caused by shadows. */
    private static class PopupWindowWithDimensions {
        private final PopupWindow mPopupWindow;
        private final Size mSize;

        PopupWindowWithDimensions(PopupWindow popupWindow, Size size) {
            this.mPopupWindow = popupWindow;
            this.mSize = size;
        }

        PopupWindow getPopupWindow() {
            return mPopupWindow;
        }

        Size getDimensions() {
            return mSize;
        }
    }
}
