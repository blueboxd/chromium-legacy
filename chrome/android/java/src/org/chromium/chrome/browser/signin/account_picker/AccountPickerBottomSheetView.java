// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/**
 * This class is the AccountPickerBottomsheet view for the web sign-in flow.
 */
class AccountPickerBottomSheetView implements BottomSheetContent {
    private final Context mContext;
    private final View mContentView;
    private final RecyclerView mAccountPickerItemView;

    AccountPickerBottomSheetView(Context context) {
        mContext = context;
        mContentView = LayoutInflater.from(mContext).inflate(
                R.layout.account_picker_bottom_sheet_view, null);
        mAccountPickerItemView = mContentView.findViewById(R.id.account_picker_item);
        mAccountPickerItemView.setLayoutManager(new LinearLayoutManager(
                mAccountPickerItemView.getContext(), LinearLayoutManager.VERTICAL, false));
    }

    RecyclerView getAccountPickerItemView() {
        return mAccountPickerItemView;
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
        return mAccountPickerItemView.computeVerticalScrollOffset();
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        // TODO(https://crbug.com/1081253): The description will
        // be adapter once the UI mock will be finalized
        return R.string.signin_account_picker_dialog_title;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // TODO(https://crbug.com/1081253): The description will
        // be adapter once the UI mock will be finalized
        return R.string.signin_account_picker_dialog_title;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        // TODO(https://crbug.com/1081253): The description will
        // be adapter once the UI mock will be finalized
        return R.string.signin_account_picker_dialog_title;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        // TODO(https://crbug.com/1081253): The description will
        // be adapter once the UI mock will be finalized
        return R.string.signin_account_picker_dialog_title;
    }
}
