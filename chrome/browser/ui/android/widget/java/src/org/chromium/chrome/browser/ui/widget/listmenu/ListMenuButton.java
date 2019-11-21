// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget.listmenu;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.ui.widget.R;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * A menu button meant to be used with modern lists throughout Chrome.  Will automatically show and
 * anchor a popup on press and will rely on a delegate for positioning and content of the popup.
 */
public class ListMenuButton
        extends ChromeImageButton implements AnchoredPopupWindow.LayoutObserver {
    /**
     * A listener that is notified when the popup menu is shown.
     */
    @FunctionalInterface
    public interface PopupMenuShownListener {
        void onPopupMenuShown();
    }

    private final int mMenuMaxWidth;
    private final boolean mMenuVerticalOverlapAnchor;
    private final boolean mMenuHorizontalOverlapAnchor;

    private Drawable mMenuBackground;
    private AnchoredPopupWindow mPopupMenu;
    private ListMenuButtonDelegate mDelegate;
    private ObserverList<PopupMenuShownListener> mPopupListeners = new ObserverList<>();

    /**
     * Creates a new {@link ListMenuButton}.
     *
     * @param context The {@link Context} used to build the visuals from.
     * @param attrs The specific {@link AttributeSet} used to build the button.
     */
    public ListMenuButton(Context context, AttributeSet attrs) {
        super(context, attrs);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.ListMenuButton);
        mMenuMaxWidth = a.getDimensionPixelSize(R.styleable.ListMenuButton_menuMaxWidth,
                getResources().getDimensionPixelSize(R.dimen.list_menu_width));
        mMenuBackground = a.getDrawable(R.styleable.ListMenuButton_menuBackground);
        if (mMenuBackground == null) {
            mMenuBackground =
                    ApiCompatibilityUtils.getDrawable(getResources(), R.drawable.popup_bg_tinted);
        }
        mMenuHorizontalOverlapAnchor =
                a.getBoolean(R.styleable.ListMenuButton_menuHorizontalOverlapAnchor, true);
        mMenuVerticalOverlapAnchor =
                a.getBoolean(R.styleable.ListMenuButton_menuVerticalOverlapAnchor, true);

        a.recycle();
    }

    /**
     * Text that represents the item this menu button is related to.  This will affect the content
     * description of the view {@see #setContentDescription(CharSequence)}.
     *
     * @param context The string representation of the list item this button represents.
     */
    public void setContentDescriptionContext(String context) {
        if (context == null) {
            context = "";
        }
        setContentDescription(getContext().getResources().getString(
                R.string.accessibility_list_menu_button, context));
    }

    /**
     * Sets the delegate this menu will rely on for populating the popup menu and handling selection
     * responses.  The menu will not show or work without it.
     *
     * @param delegate The {@link ListMenuButtonDelegate} to use for menu creation and selection
     *         handling.
     */
    public void setDelegate(ListMenuButtonDelegate delegate) {
        dismiss();
        mDelegate = delegate;
    }

    /**
     * Called to dismiss any popup menu that might be showing for this button.
     */
    public void dismiss() {
        if (mPopupMenu != null) {
            mPopupMenu.dismiss();
        }
    }

    /**
     * Shows a popupWindow built by ListMenuButton
     */
    public void showMenu() {
        initPopupWindow();
        mPopupMenu.show();
        notifyPopupListeners();
    }

    /**
     * Init the popup window with provided attributes, called before {@link #showMenu()}
     */
    private void initPopupWindow() {
        if (mDelegate == null) throw new IllegalStateException("Delegate was not set.");

        ListMenu menu = mDelegate.getListMenu();
        menu.addContentViewClickRunnable(this::dismiss);

        mPopupMenu = new AnchoredPopupWindow(getContext(), this, mMenuBackground,
                menu.getContentView(), mDelegate.getRectProvider(this));
        mPopupMenu.setVerticalOverlapAnchor(mMenuVerticalOverlapAnchor);
        mPopupMenu.setHorizontalOverlapAnchor(mMenuHorizontalOverlapAnchor);
        mPopupMenu.setMaxWidth(mMenuMaxWidth);
        mPopupMenu.setFocusable(true);
        mPopupMenu.setLayoutObserver(this);
        mPopupMenu.addOnDismissListener(() -> { mPopupMenu = null; });
    }

    /**
     * Adds a listener which will be notified when the popup menu is shown.
     *
     * @param l The listener of interest.
     */
    public void addPopupListener(PopupMenuShownListener l) {
        mPopupListeners.addObserver(l);
    }

    /**
     * Removes a popup menu listener.
     *
     * @param l The listener of interest.
     */
    public void removePopupListener(PopupMenuShownListener l) {
        mPopupListeners.removeObserver(l);
    }

    // AnchoredPopupWindow.LayoutObserver implementation.
    @Override
    public void onPreLayoutChange(
            boolean positionBelow, int x, int y, int width, int height, Rect anchorRect) {
        mPopupMenu.setAnimationStyle(
                positionBelow ? R.style.OverflowMenuAnim : R.style.OverflowMenuAnimBottom);
    }

    // View implementation.
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        setContentDescriptionContext("");
        setOnClickListener((view) -> showMenu());
    }

    @Override
    protected void onDetachedFromWindow() {
        dismiss();
        super.onDetachedFromWindow();
    }

    /**
     * Notify all of the PopupMenuShownListeners of a popup menu action.
     */
    private void notifyPopupListeners() {
        for (PopupMenuShownListener l : mPopupListeners) {
            l.onPopupMenuShown();
        }
    }
}