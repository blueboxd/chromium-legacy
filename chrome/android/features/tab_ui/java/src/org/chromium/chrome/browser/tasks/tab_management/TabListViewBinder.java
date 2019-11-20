// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.os.Build;
import android.support.v4.content.res.ResourcesCompat;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;

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
}
