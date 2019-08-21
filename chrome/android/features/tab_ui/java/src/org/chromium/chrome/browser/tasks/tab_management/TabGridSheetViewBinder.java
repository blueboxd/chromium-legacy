// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabGridSheetProperties.ADD_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridSheetProperties.ANIMATION_PARAMS;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridSheetProperties.COLLAPSE_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridSheetProperties.CONTENT_TOP_MARGIN;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridSheetProperties.HEADER_TITLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridSheetProperties.IS_DIALOG_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridSheetProperties.PRIMARY_COLOR;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridSheetProperties.SCRIMVIEW_OBSERVER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridSheetProperties.TINT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridSheetProperties.UNGROUP_BAR_STATUS;

import android.support.annotation.Nullable;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * ViewBinder for TabGridSheet.
 */
class TabGridSheetViewBinder {
    /**
     * ViewHolder class to get access to all {@link View}s inside the TabGridSheet.
     */
    public static class ViewHolder {
        public final TabGroupUiToolbarView toolbarView;
        public final View contentView;
        @Nullable
        public TabGridDialogParent dialogView;

        ViewHolder(TabGroupUiToolbarView toolbarView, View contentView,
                @Nullable TabGridDialogParent dialogView) {
            this.toolbarView = toolbarView;
            this.contentView = contentView;
            this.dialogView = dialogView;
        }
    }

    /**
     * Binds the given model to the given view, updating the payload in propertyKey.
     * @param model The model to use.
     * @param viewHolder The ViewHolder to use.
     * @param propertyKey The key for the property to update for.
     */
    public static void bind(PropertyModel model, ViewHolder viewHolder, PropertyKey propertyKey) {
        if (COLLAPSE_CLICK_LISTENER == propertyKey) {
            viewHolder.toolbarView.setLeftButtonOnClickListener(model.get(COLLAPSE_CLICK_LISTENER));
        } else if (ADD_CLICK_LISTENER == propertyKey) {
            viewHolder.toolbarView.setRightButtonOnClickListener(model.get(ADD_CLICK_LISTENER));
        } else if (HEADER_TITLE == propertyKey) {
            viewHolder.toolbarView.setTitle(model.get(HEADER_TITLE));
        } else if (CONTENT_TOP_MARGIN == propertyKey) {
            ((FrameLayout.LayoutParams) viewHolder.contentView.getLayoutParams()).topMargin =
                    model.get(CONTENT_TOP_MARGIN);
            viewHolder.contentView.requestLayout();
        } else if (PRIMARY_COLOR == propertyKey) {
            viewHolder.toolbarView.setPrimaryColor(model.get(PRIMARY_COLOR));
            viewHolder.contentView.setBackgroundColor(model.get(PRIMARY_COLOR));
        } else if (TINT == propertyKey) {
            viewHolder.toolbarView.setTint(model.get(TINT));
        } else if (SCRIMVIEW_OBSERVER == propertyKey) {
            viewHolder.dialogView.setScrimViewObserver(model.get(SCRIMVIEW_OBSERVER));
        } else if (IS_DIALOG_VISIBLE == propertyKey) {
            if (model.get(IS_DIALOG_VISIBLE)) {
                viewHolder.dialogView.resetDialog(viewHolder.toolbarView, viewHolder.contentView);
                viewHolder.dialogView.showDialog();
            } else {
                viewHolder.dialogView.hideDialog();
            }
        } else if (ANIMATION_PARAMS == propertyKey) {
            viewHolder.dialogView.setupDialogAnimation(model.get(ANIMATION_PARAMS));
        } else if (UNGROUP_BAR_STATUS == propertyKey) {
            viewHolder.dialogView.updateUngroupBar(model.get(UNGROUP_BAR_STATUS));
        }
    }
}
