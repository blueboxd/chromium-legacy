// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import android.content.Context;
import android.support.annotation.DimenRes;
import android.support.annotation.Nullable;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetObserver;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;

/**
 * This class is responsible for rendering the bottom sheet which displays the touch to fill
 * credentials. It is a View in this Model-View-Controller component and doesn't inherit but holds
 * Android Views.
 */
class TouchToFillView implements BottomSheet.BottomSheetContent {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final RecyclerView mSheetItemListView;
    private final LinearLayout mContentView;
    private Callback<Integer> mDismissHandler;

    private final BottomSheetObserver mBottomSheetObserver = new EmptyBottomSheetObserver() {
        @Override
        public void onSheetClosed(@BottomSheet.StateChangeReason int reason) {
            super.onSheetClosed(reason);
            assert mDismissHandler != null;
            mDismissHandler.onResult(reason);
            mBottomSheetController.getBottomSheet().removeObserver(mBottomSheetObserver);
        }
    };

    /**
     * Constructs a TouchToFillView which creates, modifies, and shows the bottom sheet.
     * @param context A {@link Context} used to load resources and inflate the sheet.
     * @param bottomSheetController The {@link BottomSheetController} used to show/hide the sheet.
     */
    TouchToFillView(Context context, BottomSheetController bottomSheetController) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mContentView = (LinearLayout) LayoutInflater.from(mContext).inflate(
                R.layout.touch_to_fill_sheet, null);
        mSheetItemListView = mContentView.findViewById(R.id.sheet_item_list);
        mSheetItemListView.setLayoutManager(new LinearLayoutManager(
                mSheetItemListView.getContext(), LinearLayoutManager.VERTICAL, false));
        mSheetItemListView.setItemAnimator(null);
    }

    /**
     * Sets a new listener that reacts to events like item selection or dismissal.
     * @param dismissHandler A {@link Callback<Integer>}.
     */
    void setDismissHandler(Callback<Integer> dismissHandler) {
        mDismissHandler = dismissHandler;
    }

    /**
     * If set to true, requests to show the bottom sheet. Otherwise, requests to hide the sheet.
     * @param isVisible A boolean describing whether to show or hide the sheet.
     */
    void setVisible(boolean isVisible) {
        if (isVisible) {
            mBottomSheetController.getBottomSheet().addObserver(mBottomSheetObserver);
            mBottomSheetController.requestShowContent(this, false);
        } else {
            mBottomSheetController.hideContent(this, false);
        }
    }

    void setSheetItemListAdapter(RecyclerView.Adapter adapter) {
        mSheetItemListView.setAdapter(adapter);
    }

    void setOnManagePasswordClick(Runnable runnable) {
        mContentView.findViewById(R.id.touch_to_fill_sheet_manage_passwords)
                .setOnClickListener((v) -> runnable.run());
    }

    Context getContext() {
        return mContext;
    }

    @Override
    public void destroy() {
        mBottomSheetController.getBottomSheet().removeObserver(mBottomSheetObserver);
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mSheetItemListView.computeVerticalScrollOffset();
    }

    @Override
    public int getPriority() {
        return BottomSheet.ContentPriority.HIGH;
    }

    @Override
    public boolean hasCustomScrimLifecycle() {
        return false;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public int getPeekHeight() {
        return Math.min(mContext.getResources().getDimensionPixelSize(getDesiredSheetHeight()),
                (int) mBottomSheetController.getBottomSheet().getSheetContainerHeight());
    }

    @Override
    public boolean wrapContentEnabled() {
        return false;
    }

    @Override
    public boolean hideOnScroll() {
        return true;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.touch_to_fill_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.touch_to_fill_sheet_half_height;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.touch_to_fill_sheet_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.touch_to_fill_sheet_closed;
    }

    // TODO(crbug.com/1009331): This should add up the height of all items up to the 2nd credential.
    private @DimenRes int getDesiredSheetHeight() {
        if (mSheetItemListView.getAdapter() != null
                && mSheetItemListView.getAdapter().getItemCount() > 2) {
            return R.dimen.touch_to_fill_sheet_height_multiple_credentials;
        }
        return R.dimen.touch_to_fill_sheet_height_single_credential;
    }
}
