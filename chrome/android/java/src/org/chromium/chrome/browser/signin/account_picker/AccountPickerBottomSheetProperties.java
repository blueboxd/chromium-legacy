// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.signin.DisplayableProfileData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Properties of account picker bottom sheet.
 */
class AccountPickerBottomSheetProperties {
    /**
     * States of account picker.
     * Different account picker state correspond to different account picker bottom sheet
     * configuration.
     */
    @IntDef({
            AccountPickerBottomSheetState.NO_ACCOUNTS,
            AccountPickerBottomSheetState.COLLAPSED_ACCOUNT_LIST,
            AccountPickerBottomSheetState.EXPANDED_ACCOUNT_LIST,
            AccountPickerBottomSheetState.SIGNIN_IN_PROGRESS,
            AccountPickerBottomSheetState.INCOGNITO_INTERSTITIAL,
            AccountPickerBottomSheetState.SIGNIN_GENERAL_ERROR,
            AccountPickerBottomSheetState.SIGNIN_AUTH_ERROR,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface AccountPickerBottomSheetState {
        /**
         * When there is no account on device, the user sees only one blue button
         * |Add account to device|.
         *
         * The bottom sheet starts with this state when there is zero account on device.
         */
        int NO_ACCOUNTS = 0;

        /**
         * When the account list is collapsed with exactly one account shown as the selected
         * account and a blue |Continue as| button to sign in with the selected account.
         *
         * The bottom sheet starts with this state when there is at least one account on device.
         *
         * This state can also be reached from EXPANDED_ACCOUNT_LIST by clicking on one of
         * the accounts in the expanded account list.
         */
        int COLLAPSED_ACCOUNT_LIST = 1;

        /**
         * When the account list is expanded, the user sees the account list of all the accounts
         * on device and some additional rows like |Add account to device| and |Go incognito mode|.
         *
         * This state is reached from COLLAPSED_ACCOUNT_LIST by clicking the selected account of
         * the collapsed account list.
         */
        int EXPANDED_ACCOUNT_LIST = 2;

        /**
         * When the user is in the sign-in process, no account or button will be visible, the user
         * sees mainly a spinner in the bottom sheet.
         *
         * This state can only be reached from COLLAPSED_ACCOUNT_LIST, when the button
         * |Continue as| is clicked. This state does not lead to any other state.
         */
        int SIGNIN_IN_PROGRESS = 3;

        /**
         * When the account list is expanded, the user sees the account list of all the accounts
         * on device and some additional rows like |Add account to device| and |Go incognito mode|.
         *
         * This state can only be reached from EXPANDED_ACCOUNT_LIST and would represent that the
         * user has clicked the "Go incognito mode" option.
         */
        int INCOGNITO_INTERSTITIAL = 4;

        /**
         * When user cannot complete sign-in due to connectivity issues for example, the
         * general sign-in error screen will be shown.
         *
         * The state can be reached when an error appears during the sign-in process.
         */
        int SIGNIN_GENERAL_ERROR = 5;

        /**
         * When user cannot complete sign-in due to invalidate credential, the
         * sign-in auth error screen will be shown.
         *
         * The state can be reached when an auth error appears during the sign-in process.
         */
        int SIGNIN_AUTH_ERROR = 6;
    }

    // PropertyKeys for the selected account view when the account list is collapsed.
    // The selected account view is replaced by account list view when the
    // account list is expanded.
    static final ReadableObjectPropertyKey<Runnable> ON_SELECTED_ACCOUNT_CLICKED =
            new ReadableObjectPropertyKey<>("on_selected_account_clicked");
    static final WritableObjectPropertyKey<DisplayableProfileData> SELECTED_ACCOUNT_DATA =
            new WritableObjectPropertyKey<>("selected_account_data");

    // PropertyKey for the button |Continue as ...|
    // The button is visible during all the lifecycle of the bottom sheet
    static final ReadableObjectPropertyKey<Runnable> ON_CONTINUE_AS_CLICKED =
            new ReadableObjectPropertyKey<>("on_continue_as_clicked");

    // PropertyKey indicates the state of the account picker bottom sheet
    static final WritableIntPropertyKey ACCOUNT_PICKER_BOTTOM_SHEET_STATE =
            new WritableIntPropertyKey("account_picker_bottom_sheet_state");

    static final PropertyKey[] ALL_KEYS = new PropertyKey[] {ON_SELECTED_ACCOUNT_CLICKED,
            SELECTED_ACCOUNT_DATA, ON_CONTINUE_AS_CLICKED, ACCOUNT_PICKER_BOTTOM_SHEET_STATE};

    /**
     * Creates a default model for the AccountPickerBottomSheet.
     *
     * In the default model, as the selected account data is null, the bottom sheet is in the
     * state {@link AccountPickerBottomSheetState#NO_ACCOUNTS}.
     */
    static PropertyModel createModel(
            Runnable onSelectedAccountClicked, Runnable onContinueAsClicked) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(ON_SELECTED_ACCOUNT_CLICKED, onSelectedAccountClicked)
                .with(SELECTED_ACCOUNT_DATA, null)
                .with(ON_CONTINUE_AS_CLICKED, onContinueAsClicked)
                .with(ACCOUNT_PICKER_BOTTOM_SHEET_STATE, AccountPickerBottomSheetState.NO_ACCOUNTS)
                .build();
    }

    private AccountPickerBottomSheetProperties() {}
}
