// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.signin.DisplayableProfileData;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.ui.widget.ButtonCompat;

/**
 * This class is the AccountPickerBottomsheet view for the web sign-in flow.
 *
 * The bottom sheet shows a single account with a |Continue as ...| button by default, clicking
 * on the account will expand the bottom sheet to an account list together with other sign-in
 * options like "Add account" and "Go incognito mode".
 */
class AccountPickerBottomSheetView implements BottomSheetContent {
    private final Context mContext;
    private final View mContentView;
    private final RecyclerView mAccountListView;
    private final View mSelectedAccountView;
    private final ButtonCompat mContinueAsButton;

    AccountPickerBottomSheetView(Context context) {
        mContext = context;
        mContentView = LayoutInflater.from(mContext).inflate(
                R.layout.account_picker_bottom_sheet_view, null);
        mAccountListView = mContentView.findViewById(R.id.account_picker_account_list);
        mAccountListView.setLayoutManager(new LinearLayoutManager(
                mAccountListView.getContext(), LinearLayoutManager.VERTICAL, false));
        mSelectedAccountView = mContentView.findViewById(R.id.account_picker_selected_account);
        mContinueAsButton = mContentView.findViewById(R.id.account_picker_continue_as_button);
    }

    /**
     * The account list view is visible when the account list is expanded.
     */
    RecyclerView getAccountListView() {
        return mAccountListView;
    }

    /**
     * The selected account is visible when the account list is collapsed.
     */
    View getSelectedAccountView() {
        return mSelectedAccountView;
    }

    /**
     * The |Continue As| button on the bottom sheet.
     */
    ButtonCompat getContinueAsButton() {
        return mContinueAsButton;
    }

    /**
     * Expands the account list.
     */
    void expandAccountList() {
        mSelectedAccountView.setVisibility(View.GONE);
        mContinueAsButton.setVisibility(View.GONE);
        mAccountListView.setVisibility(View.VISIBLE);
    }

    /**
     * Collapses the account list.
     * If there is a non null selected account, the account list will collapse to that account,
     * otherwise, the account list will just collapse the remaining.
     *
     * @param isSelectedAccountNonNull Flag indicates if the selected profile data exists
     *                                 in model.
     */
    void collapseAccountList(boolean isSelectedAccountNonNull) {
        mAccountListView.setVisibility(View.GONE);
        mSelectedAccountView.setVisibility(isSelectedAccountNonNull ? View.VISIBLE : View.GONE);
        mContinueAsButton.setVisibility(View.VISIBLE);
    }

    /**
     * Updates the view of the collapsed account list.
     */
    void updateCollapsedAccountList(DisplayableProfileData accountProfileData) {
        if (accountProfileData == null) {
            mContinueAsButton.setText(R.string.signin_add_account_to_device);
        } else {
            ExistingAccountRowViewBinder.bindAccountView(accountProfileData, mSelectedAccountView);

            ImageView rowEndImage = mSelectedAccountView.findViewById(R.id.account_selection_mark);
            rowEndImage.setImageResource(R.drawable.ic_expand_more_black_24dp);
            rowEndImage.setColorFilter(R.color.default_icon_color);

            String continueAsButtonText = mContext.getString(R.string.signin_promo_continue_as,
                    accountProfileData.getGivenNameOrFullNameOrEmail());
            mContinueAsButton.setText(continueAsButtonText);
        }
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
        return mAccountListView.computeVerticalScrollOffset();
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
