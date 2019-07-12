// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.graphics.Bitmap;
import android.support.annotation.DrawableRes;
import android.support.annotation.StringRes;
import android.view.View;
import android.view.View.OnClickListener;

import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.touchless.dialog.TouchlessDialogProperties;
import org.chromium.chrome.browser.touchless.dialog.TouchlessDialogProperties.ActionNames;
import org.chromium.chrome.browser.touchless.dialog.TouchlessDialogProperties.DialogListItemProperties;
import org.chromium.chrome.touchless.R;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;

/**
 * Handles context menu creation for native pages on touchless devices. Utilizes functionality in
 * ContextMenuManager and Delegate to select items to show and lookup their labels.
 */
public class TouchlessContextMenuManager extends ContextMenuManager {
    /**
     * Delegate for touchless-specific context menu items.
     */
    public interface Delegate extends ContextMenuManager.Delegate {
        /**
         * @return Title associated with this delegate.
         */
        String getTitle();

        /**
         * @return Icon associated with this delegate. Used to populate touchless shortcuts.
         */
        Bitmap getIconBitmap();
    }

    /**
     * Empty implementation of Delegate to allow derived classes to only implement methods they
     * need.
     */
    public static class EmptyDelegate extends ContextMenuManager.EmptyDelegate implements Delegate {
        @Override
        public String getTitle() {
            return null;
        }

        @Override
        public Bitmap getIconBitmap() {
            return null;
        }
    }

    private final ChromeActivity mActivity;
    private final ModalDialogManager mDialogManager;
    // This field should be set when showing a dialog, and nulled out when the dialog is closed. We
    // can check if it is null to determine whether we're currently showing our dialog.
    private PropertyModel mTouchlessMenuModel;
    private ModalDialogManager mModalDialogManager;

    public TouchlessContextMenuManager(ChromeActivity activity, ModalDialogManager dialogManager,
            NativePageNavigationDelegate navigationDelegate,
            TouchEnabledDelegate touchEnabledDelegate, Runnable closeContextMenuCallback,
            String userActionPrefix) {
        super(navigationDelegate, touchEnabledDelegate, closeContextMenuCallback, userActionPrefix);
        mActivity = activity;
        mDialogManager = dialogManager;
    }

    /**
     * Creates PropertyModel for touchless context menu and displays it through modalDialogManager.
     *
     * @param modalDialogManager The ModalDialogManager to display context menu.
     * @param delegate Delegate defines filter for displayed menu items and behavior for selects
     *                 item.
     */
    public void showTouchlessContextMenu(
            ModalDialogManager modalDialogManager, ContextMenuManager.Delegate delegate) {
        // Don't create a new dialog if we're already showing one.
        if (mTouchlessMenuModel != null) {
            return;
        }

        ArrayList<PropertyModel> menuItems = new ArrayList<>();
        for (@ContextMenuItemId int itemId = 0; itemId < ContextMenuItemId.NUM_ENTRIES; itemId++) {
            if (!shouldShowItem(itemId, delegate)) continue;

            // Each menu item is assigned its own instance of TouchlessItemClickListener where
            // itemId of this item is maintained.
            PropertyModel menuItem =
                    buildMenuItem(itemId, new TouchlessItemClickListener(delegate, itemId));
            menuItems.add(menuItem);
        }
        if (menuItems.size() == 0) return;

        PropertyModel[] menuItemsArray = new PropertyModel[menuItems.size()];
        menuItemsArray = menuItems.toArray(menuItemsArray);
        mTouchlessMenuModel = buildMenuModel(delegate.getContextMenuTitle(), menuItemsArray);
        mModalDialogManager = modalDialogManager;
        mModalDialogManager.showDialog(mTouchlessMenuModel, ModalDialogManager.ModalDialogType.APP);

        notifyContextMenuShown(delegate);
    }

    @Override
    protected @StringRes int getResourceIdForMenuItem(@ContextMenuItemId int id) {
        if (id == ContextMenuItemId.SEARCH) {
            return org.chromium.chrome.R.string.search_or_type_web_address;
        }
        if (id == ContextMenuItemId.ADD_TO_MY_APPS) {
            return R.string.menu_add_to_apps;
        }

        return super.getResourceIdForMenuItem(id);
    }

    @Override
    protected boolean handleMenuItemClick(
            @ContextMenuItemId int itemId, ContextMenuManager.Delegate delegate) {
        if (itemId == ContextMenuItemId.SEARCH) {
            AppHooks.get().onSearchContextMenuClick();
            return true;
        }
        if (itemId == ContextMenuItemId.ADD_TO_MY_APPS) {
            Delegate touchlessDelegate = (Delegate) delegate;
            TouchlessAddToHomescreenManager touchlessAddToHomescreenManager =
                    new TouchlessAddToHomescreenManager(mActivity, mDialogManager,
                            touchlessDelegate.getUrl(), touchlessDelegate.getTitle(),
                            touchlessDelegate.getIconBitmap());
            touchlessAddToHomescreenManager.start();
            return true;
        }
        return super.handleMenuItemClick(itemId, delegate);
    }

    @Override
    protected boolean shouldShowItem(
            @ContextMenuItemId int itemId, ContextMenuManager.Delegate delegate) {
        // Here we filter out any item IDs that don't make sense in touchless.
        switch (itemId) {
            case ContextMenuItemId.SEARCH:
                return delegate.isItemSupported(itemId);
            case ContextMenuItemId.REMOVE:
                // fall through
            case ContextMenuItemId.LEARN_MORE:
                return super.shouldShowItem(itemId, delegate);
            case ContextMenuItemId.SAVE_FOR_OFFLINE:
                // fall through
            case ContextMenuItemId.OPEN_IN_NEW_TAB:
                // fall through
            case ContextMenuItemId.OPEN_IN_INCOGNITO_TAB:
                // fall through
            case ContextMenuItemId.OPEN_IN_NEW_WINDOW:
                return false;
            case ContextMenuItemId.ADD_TO_MY_APPS:
                if (delegate instanceof Delegate) {
                    return delegate.isItemSupported(itemId);
                } else {
                    return false;
                }
        }

        assert false : "Encountered unexpected touchless context menu item type";
        return false;
    }

    /**
     * Builds PropertyModel for individual menu item. String for item's label is loaded from
     * resources by PropertyModel.Builder.
     */
    private PropertyModel buildMenuItem(@ContextMenuItemId int itemId, OnClickListener listener) {
        return new PropertyModel.Builder(DialogListItemProperties.ALL_KEYS)
                .with(DialogListItemProperties.TEXT, mActivity.getResources(),
                        getResourceIdForMenuItem(itemId))
                .with(DialogListItemProperties.ICON, mActivity, getIconIdForMenuItem(itemId))
                .with(DialogListItemProperties.CLICK_LISTENER, listener)
                .build();
    }

    /** Builds PropertyModel for context menu from list of PropertyModels for individual items. */
    private PropertyModel buildMenuModel(String title, PropertyModel[] menuItems) {
        PropertyModel.Builder builder =
                new PropertyModel.Builder(TouchlessDialogProperties.ALL_DIALOG_KEYS);
        ActionNames names = new ActionNames();
        names.cancel = org.chromium.chrome.R.string.cancel;
        names.select = org.chromium.chrome.R.string.select;
        names.alt = 0;
        builder.with(TouchlessDialogProperties.IS_FULLSCREEN, true)
                .with(ModalDialogProperties.CONTROLLER,
                        new ModalDialogProperties.Controller() {
                            @Override
                            public void onClick(PropertyModel model, int buttonType) {}

                            @Override
                            public void onDismiss(PropertyModel model, int dismissalCause) {
                                mTouchlessMenuModel = null;
                            }
                        })
                .with(TouchlessDialogProperties.ACTION_NAMES, names)
                .with(TouchlessDialogProperties.CANCEL_ACTION, (v) -> closeTouchlessContextMenu())
                .with(TouchlessDialogProperties.LIST_MODELS, menuItems)
                .with(TouchlessDialogProperties.PRIORITY, TouchlessDialogProperties.Priority.HIGH)
                .with(TouchlessDialogProperties.FORCE_SINGLE_LINE_TITLE, true);
        if (title != null) {
            builder.with(ModalDialogProperties.TITLE, title);
        }
        return builder.build();
    }

    /**
     * Returns resource id of an icon to be displayed for menu item with given item id.
     */
    private @DrawableRes int getIconIdForMenuItem(@ContextMenuItemId int itemId) {
        switch (itemId) {
            case ContextMenuItemId.SEARCH:
                return R.drawable.ic_search;
            case ContextMenuItemId.REMOVE:
                return R.drawable.ic_remove_circle_outline_24dp;
            case ContextMenuItemId.LEARN_MORE:
                return R.drawable.ic_help_outline_24dp;
            case ContextMenuItemId.ADD_TO_MY_APPS:
                return R.drawable.ic_add_circle_outline_24dp;
            default:
                return 0;
        }
    }

    private void closeTouchlessContextMenu() {
        mModalDialogManager.dismissDialog(mTouchlessMenuModel, 0);
    }

    private class TouchlessItemClickListener implements OnClickListener {
        private final ContextMenuManager.Delegate mDelegate;
        private final @ContextMenuItemId int mItemId;

        public TouchlessItemClickListener(
                ContextMenuManager.Delegate delegate, @ContextMenuItemId int itemId) {
            mDelegate = delegate;
            mItemId = itemId;
        }

        @Override
        public void onClick(View view) {
            closeTouchlessContextMenu();
            handleMenuItemClick(mItemId, mDelegate);
        }
    }
}
