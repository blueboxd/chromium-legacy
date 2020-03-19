// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.os.Build;
import android.support.graphics.drawable.AnimatedVectorDrawableCompat;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * {@link org.chromium.ui.modelutil.SimpleRecyclerViewMcp.ViewBinder} for tab List.
 */
class TabListViewBinder {
    // TODO(1023557): Merge with TabGridViewBinder for shared properties.
    public static void bindListTab(
            PropertyModel model, ViewGroup view, @Nullable PropertyKey propertyKey) {
        View fastView = view;

        if (TabProperties.TITLE == propertyKey) {
            String title = model.get(TabProperties.TITLE);
            ((TextView) fastView.findViewById(R.id.title)).setText(title);
        } else if (TabProperties.FAVICON == propertyKey) {
            Drawable favicon = model.get(TabProperties.FAVICON);
            ImageView faviconView = (ImageView) fastView.findViewById(R.id.icon_view);
            faviconView.setImageDrawable(favicon);
            int padding = favicon == null
                    ? 0
                    : (int) view.getResources().getDimension(R.dimen.tab_list_card_padding);
            faviconView.setPadding(padding, padding, padding, padding);
        } else if (TabProperties.TAB_CLOSED_LISTENER == propertyKey) {
            fastView.findViewById(R.id.action_button).setOnClickListener(v -> {
                int tabId = model.get(TabProperties.TAB_ID);
                model.get(TabProperties.TAB_CLOSED_LISTENER).run(tabId);
            });
        } else if (TabProperties.IS_SELECTED == propertyKey) {
            int selectedTabBackground =
                    model.get(TabProperties.SELECTED_TAB_BACKGROUND_DRAWABLE_ID);
            if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1) {
                if (model.get(TabProperties.IS_SELECTED)) {
                    fastView.findViewById(R.id.selected_view_below_lollipop)
                            .setBackgroundResource(selectedTabBackground);
                    fastView.findViewById(R.id.selected_view_below_lollipop)
                            .setVisibility(View.VISIBLE);
                } else {
                    fastView.findViewById(R.id.selected_view_below_lollipop)
                            .setVisibility(View.GONE);
                }
            } else {
                Resources res = view.getResources();
                Resources.Theme theme = view.getContext().getTheme();
                Drawable drawable = new InsetDrawable(
                        ResourcesCompat.getDrawable(res, selectedTabBackground, theme),
                        (int) res.getDimension(R.dimen.tab_list_selected_inset));
                view.setForeground(model.get(TabProperties.IS_SELECTED) ? drawable : null);
            }
        } else if (TabProperties.TAB_SELECTED_LISTENER == propertyKey) {
            view.setOnClickListener(v -> {
                int tabId = model.get(TabProperties.TAB_ID);
                model.get(TabProperties.TAB_SELECTED_LISTENER).run(tabId);
            });
        } else if (TabProperties.URL == propertyKey) {
            String title = model.get(TabProperties.URL);
            ((TextView) fastView.findViewById(R.id.description)).setText(title);
        }
    }

    /**
     * Bind a selectable tab to view.
     * @param model The model to bind.
     * @param view The view to bind to.
     * @param propertyKey The property that changed.
     */
    public static void bindSelectableListTab(
            PropertyModel model, ViewGroup view, @Nullable PropertyKey propertyKey) {
        bindListTab(model, view, propertyKey);

        final int tabId = model.get(TabProperties.TAB_ID);
        final int defaultLevel = view.getResources().getInteger(R.integer.list_item_level_default);
        final int selectedLevel =
                view.getResources().getInteger(R.integer.list_item_level_selected);

        if (TabProperties.SELECTABLE_TAB_CLICKED_LISTENER == propertyKey) {
            view.setOnClickListener(v -> {
                model.get(TabProperties.SELECTABLE_TAB_CLICKED_LISTENER).run(tabId);
                ((SelectableTabGridView) view).onClick();
            });
            view.setOnLongClickListener(v -> {
                model.get(TabProperties.SELECTABLE_TAB_CLICKED_LISTENER).run(tabId);
                return ((SelectableTabGridView) view).onLongClick(view);
            });
        } else if (TabProperties.TAB_SELECTION_DELEGATE == propertyKey) {
            assert model.get(TabProperties.TAB_SELECTION_DELEGATE) != null;

            ((SelectableTabGridView) view)
                    .setSelectionDelegate(model.get(TabProperties.TAB_SELECTION_DELEGATE));
            ((SelectableTabGridView) view).setItem(tabId);
        } else if (TabProperties.IS_SELECTED == propertyKey) {
            boolean isSelected = model.get(TabProperties.IS_SELECTED);
            ImageView actionButton = (ImageView) view.findViewById(R.id.action_button);
            actionButton.getBackground().setLevel(isSelected ? selectedLevel : defaultLevel);
            DrawableCompat.setTintList(actionButton.getBackground().mutate(),
                    isSelected ? model.get(
                            TabProperties.SELECTABLE_TAB_ACTION_BUTTON_SELECTED_BACKGROUND)
                               : model.get(TabProperties.SELECTABLE_TAB_ACTION_BUTTON_BACKGROUND));

            // The check should be invisible if not selected.
            actionButton.getDrawable().setAlpha(isSelected ? 255 : 0);
            ApiCompatibilityUtils.setImageTintList(actionButton,
                    isSelected ? model.get(TabProperties.CHECKED_DRAWABLE_STATE_LIST) : null);
            if (isSelected) ((AnimatedVectorDrawableCompat) actionButton.getDrawable()).start();
        }
    }
}
