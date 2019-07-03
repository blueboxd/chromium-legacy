// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.ContextMenuManager.ContextMenuItemId;
import org.chromium.chrome.browser.ntp.cards.ActionItem;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.chrome.touchless.R;
import org.chromium.ui.widget.ChromeImageView;
import org.chromium.ui.widget.Toast;

/** ViewHolder associated to {@link ItemViewType#ACTION} for touchless devices. */
class TouchlessActionItemViewHolder extends ActionItem.ViewHolder {
    private TextView mTextView;
    private ChromeImageView mImageView;

    TouchlessActionItemViewHolder(SuggestionsRecyclerView recyclerView,
            ContextMenuManager contextMenuManager, final SuggestionsUiDelegate uiDelegate,
            UiConfig uiConfig) {
        super(R.layout.touchless_action_card, recyclerView, contextMenuManager, uiDelegate,
                uiConfig);
        mTextView = itemView.findViewById(R.id.dialog_item_text);
        mImageView = itemView.findViewById(R.id.dialog_item_icon);
    }

    @Override
    public void onBindViewHolder(ActionItem item) {
        super.onBindViewHolder(item);

        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(itemView.getLayoutParams());
        params.bottomMargin = itemView.getResources().getDimensionPixelSize(
                R.dimen.touchless_new_tab_recycler_view_over_scroll);
        itemView.setLayoutParams(params);

        mButton.setBackground(ApiCompatibilityUtils.getDrawable(
                itemView.getResources(), R.drawable.hairline_border_card_background));
        mTextView.setText(itemView.getResources().getString(R.string.more_articles));
        mImageView.setImageDrawable(
                ApiCompatibilityUtils.getDrawable(itemView.getResources(), R.drawable.ic_note_add));
        // Image view defaults to GONE.
        mImageView.setVisibility(View.VISIBLE);
        ContextMenuManager.registerViewForTouchlessContextMenu(
                mButton, new ContextMenuManager.EmptyDelegate() {
                    @Override
                    public String getContextMenuTitle() {
                        return mButton.getResources().getString(R.string.more_articles);
                    }

                    @Override
                    public boolean isItemSupported(@ContextMenuItemId int menuItemId) {
                        return menuItemId == ContextMenuItemId.SEARCH
                                || menuItemId == ContextMenuItemId.LEARN_MORE;
                    }
                });
    }

    @Override
    protected void showFetchFailureMessage() {
        Toast.makeText(itemView.getContext(), R.string.ntp_suggestions_fetch_failed,
                     Toast.LENGTH_SHORT)
                .show();
    }

    @Override
    protected void showNoNewSuggestionsMessage() {
        Toast.makeText(itemView.getContext(), R.string.ntp_suggestions_fetch_no_new_suggestions,
                     Toast.LENGTH_SHORT)
                .show();
    }

    @VisibleForTesting
    View getButtonForTesting() {
        return mButton;
    }
}
