// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ntp.cards.ItemViewType;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;
import org.chromium.chrome.browser.ntp.cards.OptionalLeaf;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/**
 * Represents the data for a header of a group of snippets.
 */
public class SectionHeader extends OptionalLeaf {
    /** The header text to be shown. */
    private String mHeaderText;

    /** The model of the menu items to show in the overflow menu to manage the feed. */
    @Nullable
    private ModelList mMenuModelList;

    @Nullable
    private ListMenu.Delegate mListMenuDelegate;

    private Runnable mToggleCallback;
    private boolean mIsExpanded;

    /**
     * Constructor for non-expandable header.
     * @param headerText The title of the header.
     */
    public SectionHeader(String headerText) {
        this.mHeaderText = headerText;
        setVisibilityInternal(true);
    }

    /**
     * Constructor for expandable header.
     * @param headerText The title of the header.
     * @param isExpanded Whether the header is expanded initially.
     * @param toggleCallback The callback to run when the header is toggled.
     */
    public SectionHeader(String headerText, boolean isExpanded, @NonNull Runnable toggleCallback) {
        this(headerText);
        mToggleCallback = toggleCallback;
        mIsExpanded = isExpanded;
    }

    @Override
    @ItemViewType
    public int getItemViewType() {
        return ItemViewType.HEADER;
    }

    public String getHeaderText() {
        return mHeaderText;
    }

    public void setHeaderText(String headerText) {
        if (TextUtils.equals(mHeaderText, headerText)) return;

        mHeaderText = headerText;
        notifyItemChanged(0, null);
    }

    public ModelList getMenuModelList() {
        return mMenuModelList;
    }

    public void setMenuModelList(ModelList modelList) {
        mMenuModelList = modelList;
    }

    public ListMenu.Delegate getListMenuDelegate() {
        return mListMenuDelegate;
    }

    public void setListMenuDelegate(ListMenu.Delegate delegate) {
        mListMenuDelegate = delegate;
    }

    /**
     * @return Whether or not the header is expandable.
     */
    public boolean isExpandable() {
        return mToggleCallback != null;
    }

    /**
     * @return Whether or not the header is currently at the expanded state.
     */
    public boolean isExpanded() {
        return mIsExpanded;
    }

    /**
     * Toggle the expanded state of the header.
     */
    public void toggleHeader() {
        mIsExpanded = !mIsExpanded;
        notifyItemChanged(0, null);
        mToggleCallback.run();
    }

    @Override
    protected void onBindViewHolder(NewTabPageViewHolder holder) {
        // TODO(https://crbug.com/1069183): Dead code, refactor to remove.
    }

    @Override
    public String describeForTesting() {
        return "HEADER";
    }

    public void setVisible(boolean visible) {
        setVisibilityInternal(visible);
    }
}
