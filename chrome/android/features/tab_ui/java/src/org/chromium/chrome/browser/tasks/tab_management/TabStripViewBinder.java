// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.drawable.Drawable;
import android.support.annotation.Nullable;
import android.support.v4.content.res.ResourcesCompat;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.widget.FrameLayout;

import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * {@link org.chromium.ui.modelutil.SimpleRecyclerViewMcp.ViewBinder} for tab strip.
 * This class supports both full and partial updates to the {@link TabStripViewHolder}.
 */
class TabStripViewBinder {
    /**
     * Partially or fully update the given ViewHolder based on the given model over propertyKey.
     * @param holder The {@link ViewHolder} to use.
     * @param item The model to use.
     * @param propertyKey If present, to be used as the key to partially update. If null, a full
     *                    bind is done.
     */
    public static void onBindViewHolder(
            TabStripViewHolder holder, PropertyModel item, @Nullable PropertyKey propertyKey) {
        if (propertyKey == null) {
            onBindViewHolder(holder, item);
            return;
        }
        if (TabProperties.IS_SELECTED == propertyKey) {
            ((FrameLayout) holder.itemView)
                    .setForeground(item.get(TabProperties.IS_SELECTED)
                                    ? ResourcesCompat.getDrawable(holder.itemView.getResources(),
                                            R.drawable.tabstrip_selected,
                                            holder.itemView.getContext().getTheme())
                                    : null);
            String title = item.get(TabProperties.TITLE);
            if (item.get(TabProperties.IS_SELECTED)) {
                holder.button.setOnClickListener(view -> {
                    item.get(TabProperties.TAB_CLOSED_LISTENER).run(holder.getTabId());
                });
                holder.button.setContentDescription(holder.itemView.getContext().getString(
                        R.string.accessibility_tabstrip_btn_close_tab, title));
            } else {
                holder.button.setOnClickListener(view -> {
                    item.get(TabProperties.TAB_SELECTED_LISTENER).run(holder.getTabId());
                });
                holder.button.setContentDescription(holder.itemView.getContext().getString(
                        R.string.accessibility_tabstrip_tab, title));
            }
        } else if (TabProperties.FAVICON == propertyKey) {
            Drawable faviconDrawable = item.get(TabProperties.FAVICON);
            holder.button.setBackgroundResource(R.drawable.tabstrip_favicon_background);
            if (faviconDrawable != null) {
                holder.button.setImageDrawable(faviconDrawable);
            }
        } else if (TabProperties.TAB_ID == propertyKey) {
            holder.setTabId(item.get(TabProperties.TAB_ID));
        }
    }

    private static void onBindViewHolder(TabStripViewHolder holder, PropertyModel item) {
        for (PropertyKey propertyKey : TabProperties.ALL_KEYS_TAB_STRIP) {
            onBindViewHolder(holder, item, propertyKey);
        }
    }
}
