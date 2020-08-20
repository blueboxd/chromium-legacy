// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import org.chromium.chrome.browser.signin.DisplayableProfileData;
import org.chromium.chrome.browser.signin.account_picker.AccountPickerBottomSheetProperties.AccountPickerBottomSheetState;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Stateless AccountPickerBottomSheet view binder.
 */
class AccountPickerBottomSheetViewBinder {
    static void bind(
            PropertyModel model, AccountPickerBottomSheetView view, PropertyKey propertyKey) {
        if (propertyKey == AccountPickerBottomSheetProperties.ON_SELECTED_ACCOUNT_CLICKED) {
            view.getSelectedAccountView().setOnClickListener(v -> {
                model.get(AccountPickerBottomSheetProperties.ON_SELECTED_ACCOUNT_CLICKED).run();
            });
        } else if (propertyKey
                == AccountPickerBottomSheetProperties.ACCOUNT_PICKER_BOTTOM_SHEET_STATE) {
            @AccountPickerBottomSheetState
            int state =
                    model.get(AccountPickerBottomSheetProperties.ACCOUNT_PICKER_BOTTOM_SHEET_STATE);
            switchToState(view, state);
        } else if (propertyKey == AccountPickerBottomSheetProperties.SELECTED_ACCOUNT_DATA) {
            DisplayableProfileData profileData =
                    model.get(AccountPickerBottomSheetProperties.SELECTED_ACCOUNT_DATA);
            if (profileData != null) {
                view.updateSelectedAccount(profileData);
            }
        } else if (propertyKey == AccountPickerBottomSheetProperties.ON_CONTINUE_AS_CLICKED) {
            view.getContinueAsButton().setOnClickListener(v -> {
                model.get(AccountPickerBottomSheetProperties.ON_CONTINUE_AS_CLICKED).run();
            });
        }
    }

    /**
     * Sets up the configuration of account picker bottom sheet according to the given state.
     */
    private static void switchToState(AccountPickerBottomSheetView view,
            @AccountPickerBottomSheetState int accountPickerBottomSheetState) {
        switch (accountPickerBottomSheetState) {
            case AccountPickerBottomSheetState.NO_ACCOUNTS:
                view.collapseToNoAccountView();
                break;
            case AccountPickerBottomSheetState.COLLAPSED_ACCOUNT_LIST:
                view.collapseAccountList();
                break;
            case AccountPickerBottomSheetState.EXPANDED_ACCOUNT_LIST:
                view.expandAccountList();
                break;
            case AccountPickerBottomSheetState.SIGNIN_IN_PROGRESS:
                view.setUpSignInInProgressView();
                break;
            case AccountPickerBottomSheetState.INCOGNITO_INTERSTITIAL:
                view.setUpIncognitoInterstitialView();
                break;
            case AccountPickerBottomSheetState.SIGNIN_GENERAL_ERROR:
                view.setUpSignInGeneralErrorView();
                break;
            case AccountPickerBottomSheetState.SIGNIN_AUTH_ERROR:
                view.setUpSignInAuthErrorView();
                break;
            default:
                throw new IllegalArgumentException(
                        "Cannot bind AccountPickerBottomSheetView for the state:"
                        + accountPickerBottomSheetState);
        }
    }

    private AccountPickerBottomSheetViewBinder() {}
}
