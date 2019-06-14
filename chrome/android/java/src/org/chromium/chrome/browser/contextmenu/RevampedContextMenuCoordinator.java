// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.app.Activity;
import android.graphics.Bitmap;
import android.support.annotation.IntDef;
import android.support.v7.app.AlertDialog;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.ContextMenuDialog;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * The main coordinator for the Revamped Context Menu, responsible for creating the context menu in
 * general and the header component.
 */
public class RevampedContextMenuCoordinator implements ContextMenuUi {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ListItemType.DIVIDER, ListItemType.HEADER, ListItemType.CONTEXT_MENU_ITEM})
    public @interface ListItemType {
        int DIVIDER = 0;
        int HEADER = 1;
        int CONTEXT_MENU_ITEM = 2;
    }

    private static final int INVALID_ITEM_ID = -1;

    private RevampedContextMenuHeaderCoordinator mHeaderCoordinator;

    private float mTopContentOffsetPx;

    /**
     * Constructor that also sets the content offset.
     *
     * @param topContentOffsetPx content offset from the top.
     */
    RevampedContextMenuCoordinator(float topContentOffsetPx) {
        mTopContentOffsetPx = topContentOffsetPx;
    }

    @Override
    public void displayMenu(final Activity activity, ContextMenuParams params,
            List<Pair<Integer, List<ContextMenuItem>>> items, Callback<Integer> onItemClicked,
            final Runnable onMenuShown, final Runnable onMenuClosed) {
        final float density = activity.getResources().getDisplayMetrics().density;
        final float touchPointXPx = params.getTriggeringTouchXDp() * density;
        final float touchPointYPx = params.getTriggeringTouchYDp() * density;

        final View view =
                LayoutInflater.from(activity).inflate(R.layout.revamped_context_menu, null);
        ContextMenuDialog dialog =
                createContextMenuDialog(activity, view, touchPointXPx, touchPointYPx);
        dialog.setOnShowListener(dialogInterface -> onMenuShown.run());
        dialog.setOnDismissListener(dialogInterface -> onMenuClosed.run());

        mHeaderCoordinator = new RevampedContextMenuHeaderCoordinator(
                activity, params, params.getUrl(), params.isImage());

        ModelListAdapter adapter = new ModelListAdapter() {
            @Override
            public boolean areAllItemsEnabled() {
                return false;
            }

            @Override
            public boolean isEnabled(int position) {
                return getItemViewType(position) == ListItemType.CONTEXT_MENU_ITEM;
            }

            @Override
            public long getItemId(int position) {
                if (getItemViewType(position) == ListItemType.CONTEXT_MENU_ITEM) {
                    return ((Pair<Integer, PropertyModel>) getItem(position))
                            .second.get(RevampedContextMenuItemProperties.MENU_ID);
                }
                return INVALID_ITEM_ID;
            }
        };

        RevampedContextMenuListView listView = view.findViewById(R.id.context_menu_list_view);
        listView.setAdapter(adapter);

        // Note: clang-format does a bad job formatting lambdas so we turn it off here.
        // clang-format off
        adapter.registerType(
                ListItemType.HEADER,
                mHeaderCoordinator::getView,
                RevampedContextMenuHeaderViewBinder::bind);
        adapter.registerType(
                ListItemType.DIVIDER,
                () -> LayoutInflater.from(listView.getContext())
                        .inflate(R.layout.context_menu_divider, null),
                (m, v, p) -> {});
        adapter.registerType(
                ListItemType.CONTEXT_MENU_ITEM,
                () -> LayoutInflater.from(listView.getContext())
                        .inflate(R.layout.revamped_context_menu_row, null),
                RevampedContextMenuItemViewBinder::bind);
        // clang-format on

        // The Integer here specifies the {@link ListItemType}.
        List<Pair<Integer, PropertyModel>> itemList = getItemList(activity, items);

        adapter.updateModels(itemList);
        listView.setOnItemClickListener((p, v, pos, id) -> {
            assert id != INVALID_ITEM_ID;

            dialog.dismiss();
            onItemClicked.onResult((int) id);
        });

        dialog.show();
    }

    /**
     * Returns the fully complete dialog based off the params and the itemGroups.
     *
     * @param activity Used to inflate the dialog.
     * @param view The inflated view that contains the list view.
     * @param touchPointYPx The x-coordinate of the touch that triggered the context menu.
     * @param touchPointXPx The y-coordinate of the touch that triggered the context menu.
     * @return Returns a final dialog that does not have a background can be displayed using
     *         {@link AlertDialog#show()}.
     */
    private ContextMenuDialog createContextMenuDialog(
            Activity activity, View view, float touchPointXPx, float touchPointYPx) {
        View listView = view.findViewById(R.id.context_menu_list_view);
        // TODO(sinansahin): Refactor ContextMenuDialog as well.
        final ContextMenuDialog dialog =
                new ContextMenuDialog(activity, R.style.Theme_Chromium_DialogWhenLarge,
                        touchPointXPx, touchPointYPx, mTopContentOffsetPx, listView);
        dialog.setContentView(view);

        return dialog;
    }

    private List<Pair<Integer, PropertyModel>> getItemList(
            Activity activity, List<Pair<Integer, List<ContextMenuItem>>> items) {
        List<Pair<Integer, PropertyModel>> itemList = new ArrayList<>();

        // TODO(sinansahin): We should be able to remove this conversion once we can get the items
        // in the desired format.
        itemList.add(new Pair<>(ListItemType.HEADER, mHeaderCoordinator.getModel()));

        for (Pair<Integer, List<ContextMenuItem>> group : items) {
            // Add a divider
            itemList.add(new Pair<>(ListItemType.DIVIDER, new PropertyModel()));

            for (ContextMenuItem item : group.second) {
                PropertyModel itemModel =
                        new PropertyModel.Builder(RevampedContextMenuItemProperties.ALL_KEYS)
                                .with(RevampedContextMenuItemProperties.MENU_ID, item.getMenuId())
                                .with(RevampedContextMenuItemProperties.TEXT,
                                        item.getTitle(activity))
                                .build();

                itemList.add(new Pair<>(ListItemType.CONTEXT_MENU_ITEM, itemModel));
            }
        }

        return itemList;
    }

    Callback<Bitmap> getOnImageThumbnailRetrievedReference() {
        return mHeaderCoordinator.getOnImageThumbnailRetrievedReference();
    }
}
