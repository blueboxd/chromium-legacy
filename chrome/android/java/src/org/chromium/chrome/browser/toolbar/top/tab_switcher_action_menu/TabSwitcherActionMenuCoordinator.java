// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top.tab_switcher_action_menu;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnLongClickListener;
import android.widget.ListView;
import android.widget.PopupWindow;

import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.StringRes;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The main coordinator for the Tab Switcher Action Menu, responsible for creating the popup menu
 * (popup window) in general and building a list of menu items
 */
public class TabSwitcherActionMenuCoordinator {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ListItemType.DIVIDER, ListItemType.MENU_ITEM})
    public @interface ListItemType {
        int DIVIDER = 0;
        int MENU_ITEM = 1;
    }

    private ListView mListView;
    private AnchoredPopupWindow mPopup;
    private View mContentView;

    /**
     * @param onItemClicked  The clicked listener handling clicks on TabSwitcherActionMenu
     * @return a long click listener of the long press action of tab switcher button
     */
    public static OnLongClickListener createOnLongClickListener(Callback<Integer> onItemClicked) {
        return (view) -> {
            Context context = view.getContext();
            TabSwitcherActionMenuCoordinator menu = new TabSwitcherActionMenuCoordinator();
            menu.displayMenu(context, view, menu.buildMenuItems(context), (id) -> {
                recordUserActions(id);
                onItemClicked.onResult(id);
            });
            return true;
        };
    }

    private static void recordUserActions(int id) {
        if (id == R.id.close_tab) {
            RecordUserAction.record("MobileMenuCloseTab.LongTapMenu");
        } else if (id == R.id.new_tab_menu_id) {
            RecordUserAction.record("MobileMenuNewTab.LongTapMenu");
        } else if (id == R.id.new_incognito_tab_menu_id) {
            RecordUserAction.record("MobileMenuNewIncognitoTab.LongTapMenu");
        }
    }

    /**
     * Created and display the tab switcher action menu anchored to the specified view
     *
     * @param context        The context of the TabSwitcherActionMenu.
     * @param anchorView     The anchor {@link View} of the {@link PopupWindow}.
     * @param listItems      The menu item models
     * @param onItemClicked  The clicked listener handling clicks on TabSwitcherActionMenu
     */
    @VisibleForTesting
    public void displayMenu(final Context context, View anchorView, ModelList listItems,
            Callback<Integer> onItemClicked) {
        mContentView = LayoutInflater.from(context).inflate(
                R.layout.tab_switcher_action_menu_layout, null);
        mListView = (ListView) mContentView.findViewById(R.id.tab_switcher_action_menu_list);

        ModelListAdapter adapter = new ModelListAdapter(listItems) {
            @Override
            public boolean areAllItemsEnabled() {
                return false;
            }

            @Override
            public boolean isEnabled(int position) {
                // make divider not clickable
                return getItemViewType(position) != ListItemType.DIVIDER;
            }

            @Override
            public long getItemId(int position) {
                return ((ListItem) getItem(position))
                        .model.get(TabSwitcherActionMenuItemProperties.MENU_ID);
            }
        };

        mListView.setAdapter(adapter);

        // Note: clang-format does a bad job formatting lambdas so we turn it off here.
        // clang-format off
        adapter.registerType(ListItemType.DIVIDER,
                () -> LayoutInflater.from(mListView.getContext())
                        .inflate(R.layout.context_menu_divider, mListView, false),
                (m, v, p) -> {});

        adapter.registerType(ListItemType.MENU_ITEM,
                () -> LayoutInflater.from(mListView.getContext())
                        .inflate(R.layout.tab_switcher_action_menu_item, mListView, false),
                TabSwitcherActionMenuItemBinder::binder);
        // clang-format on

        mListView.setOnItemClickListener((p, v, pos, id) -> {
            if (onItemClicked != null) onItemClicked.onResult((int) id);
            mPopup.dismiss();
        });

        ViewRectProvider rectProvider = new ViewRectProvider(anchorView);
        rectProvider.setIncludePadding(true);

        int toolbarHeight = anchorView.getHeight();
        int iconHeight = context.getResources().getDimensionPixelSize(R.dimen.toolbar_icon_height);
        int paddingBottom = (toolbarHeight - iconHeight) / 2;
        rectProvider.setInsetPx(0, 0, 0, paddingBottom);
        mPopup = new AnchoredPopupWindow(context, anchorView,
                ApiCompatibilityUtils.getDrawable(
                        context.getResources(), R.drawable.popup_bg_tinted),
                mContentView, rectProvider);
        mPopup.setFocusable(true);
        mPopup.setAnimationStyle(R.style.OverflowMenuAnim);

        int popupWidth = context.getResources().getDimensionPixelSize(R.dimen.menu_width);
        mPopup.setMaxWidth(popupWidth);
        mPopup.setHorizontalOverlapAnchor(true);
        mPopup.show();
    }

    @VisibleForTesting
    public View getContentView() {
        return mContentView;
    }

    @VisibleForTesting
    public ModelList buildMenuItems(Context context) {
        ModelList itemList = new ModelList();
        itemList.add(new ListItem(ListItemType.MENU_ITEM,
                buildPropertyModel(
                        context, R.string.close_tab, R.id.close_tab, R.drawable.btn_close)));
        itemList.add(new ListItem(ListItemType.DIVIDER,
                new PropertyModel.Builder(TabSwitcherActionMenuItemProperties.ALL_KEYS).build()));
        itemList.add(new ListItem(ListItemType.MENU_ITEM,
                buildPropertyModel(context, R.string.menu_new_tab, R.id.new_tab_menu_id,
                        R.drawable.new_tab_icon)));
        itemList.add(new ListItem(ListItemType.MENU_ITEM,
                buildPropertyModel(context, R.string.menu_new_incognito_tab,
                        R.id.new_incognito_tab_menu_id, R.drawable.incognito_simple)));
        return itemList;
    }

    private PropertyModel buildPropertyModel(
            Context context, @StringRes int titleId, @IdRes int menuId, @DrawableRes int iconId) {
        return new PropertyModel.Builder(TabSwitcherActionMenuItemProperties.ALL_KEYS)
                .with(TabSwitcherActionMenuItemProperties.TITLE, context.getString(titleId))
                .with(TabSwitcherActionMenuItemProperties.MENU_ID, menuId)
                .with(TabSwitcherActionMenuItemProperties.ICON_ID, iconId)
                .build();
    }
}
