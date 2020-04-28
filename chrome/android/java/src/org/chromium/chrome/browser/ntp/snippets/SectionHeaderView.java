// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * View for the header of the personalized feed that has a context menu to
 * manage the feed.
 */
public class SectionHeaderView extends LinearLayout implements View.OnClickListener {
    // Views in the header layout that are set during inflate.
    private TextView mTitleView;
    private TextView mStatusView;
    private ListMenuButton mMenuView;

    // Properties that are set after construction & inflate using setters.
    @Nullable
    private SectionHeader mHeader;

    private boolean mHasMenu;

    public SectionHeaderView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTitleView = findViewById(R.id.header_title);
        mStatusView = findViewById(R.id.header_status);
        mMenuView = findViewById(R.id.header_menu);

        // Use the menu instead of the status text when the menu is available from the inflated
        // layout.
        mHasMenu = mMenuView != null;

        if (mHasMenu) {
            mMenuView.setOnClickListener((View v) -> { displayMenu(); });
        }
    }

    @Override
    public void onClick(View view) {
        assert mHeader.isExpandable() : "onClick() is called on a non-expandable section header.";
        mHeader.toggleHeader();
        SuggestionsMetrics.recordExpandableHeaderTapped(mHeader.isExpanded());
        SuggestionsMetrics.recordArticlesListVisible();
    }

    /** @param header The {@link SectionHeader} that holds the data for this class. */
    public void setHeader(SectionHeader header) {
        mHeader = header;
        if (mHeader == null) return;

        // Set visuals with the menu view when present.
        if (mHasMenu) {
            updateVisuals();
            return;
        }

        // Set visuals with the status view when no menu.
        mStatusView.setVisibility(mHeader.isExpandable() ? View.VISIBLE : View.GONE);
        updateVisuals();
        setOnClickListener(mHeader.isExpandable() ? this : null);
    }

    /** Update the header view based on whether the header is expanded and its text contents. */
    public void updateVisuals() {
        if (mHeader == null) return;

        mTitleView.setText(mHeader.getHeaderText());

        if (mHeader.isExpandable()) {
            if (!mHasMenu) {
                mStatusView.setText(mHeader.isExpanded() ? R.string.hide : R.string.show);
            }
            setBackgroundResource(
                    mHeader.isExpanded() ? 0 : R.drawable.hairline_border_card_background);
        }
    }

    private void displayMenu() {
        if (mMenuView == null) {
            assert false : "No menu view to display the menu";
            return;
        }

        ModelList listItems = mHeader.getMenuModelList();
        if (listItems == null) {
            assert false : "No list items model to display the menu";
            return;
        }

        ListMenu.Delegate listMenuDelegate = mHeader.getListMenuDelegate();
        if (listMenuDelegate == null) {
            assert false : "No list menu delegate for the menu";
            return;
        }

        BasicListMenu listMenu =
                new BasicListMenu(mMenuView.getContext(), listItems, listMenuDelegate);

        ListMenuButtonDelegate delegate = new ListMenuButtonDelegate() {
            @Override
            public ListMenu getListMenu() {
                return listMenu;
            }

            @Override
            public RectProvider getRectProvider(View listMenuButton) {
                ViewRectProvider rectProvider = new ViewRectProvider(listMenuButton);
                rectProvider.setIncludePadding(true);
                rectProvider.setInsetPx(0, 0, 0, 0);
                return rectProvider;
            }
        };

        mMenuView.setDelegate(delegate);
        mMenuView.showMenu();
    }
}
