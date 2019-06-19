// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.PopupWindow;

import org.chromium.chrome.browser.widget.selection.SelectableListLayout;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.chrome.tab_ui.R;

/**
 * This class is used to show the {@link SelectableListLayout} in a {@link PopupWindow}.
 */
class TabSelectionEditorLayout extends SelectableListLayout<Integer> {
    private TabSelectionEditorToolbar mToolbar;
    private final PopupWindow mWindow;
    private View mParentView;
    private boolean mIsInitialized;

    // TODO(meiliang): inflates R.layout.tab_selection_editor_layout in
    // TabSelectionEditorCoordinator.
    public TabSelectionEditorLayout(Context context, AttributeSet attrs) {
        super(context, attrs);

        DisplayMetrics displayMetrics = new DisplayMetrics();
        ((WindowManager) getContext().getSystemService(Context.WINDOW_SERVICE))
                .getDefaultDisplay()
                .getMetrics(displayMetrics);
        mWindow = new PopupWindow(this, displayMetrics.widthPixels, displayMetrics.heightPixels);
    }

    /**
     * Initializes the RecyclerView and the toolbar for the layout. This must be called before
     * calling show/hide.
     *
     * @param parentView The parent view for the {@link PopupWindow}.
     * @param recyclerView The recycler view to be shown.
     * @param adapter The adapter that provides views that represent items in the recycler view.
     * @param selectionDelegate The {@link SelectionDelegate} that will inform the toolbar of
     *                            selection changes.
     */
    void initialize(View parentView, RecyclerView recyclerView, RecyclerView.Adapter adapter,
            SelectionDelegate<Integer> selectionDelegate) {
        mIsInitialized = true;
        initializeRecyclerView(adapter, recyclerView);
        mToolbar =
                (TabSelectionEditorToolbar) initializeToolbar(R.layout.tab_selection_editor_toolbar,
                        selectionDelegate, 0, 0, 0, null, false, true);
        mParentView = parentView;
    }

    /**
     * Shows the layout in a {@link PopupWindow}.
     */
    public void show() {
        assert mIsInitialized;
        mWindow.showAtLocation(mParentView, Gravity.CENTER, 0, 0);
    }

    /**
     * Hides the {@link PopupWindow}.
     */
    public void hide() {
        assert mIsInitialized;
        if (mWindow.isShowing()) mWindow.dismiss();
    }

    /**
     * @return The toolbar of the layout.
     */
    public TabSelectionEditorToolbar getToolbar() {
        return mToolbar;
    }
}
