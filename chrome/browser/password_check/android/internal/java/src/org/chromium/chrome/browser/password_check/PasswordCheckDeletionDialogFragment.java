// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.chromium.chrome.browser.password_check.internal.R;

/**
 * Shows the dialog that confirms the user really wants to delete a credential.
 */
public class PasswordCheckDeletionDialogFragment extends DialogFragment {
    /**
     * This interface combines handling the clicks on the buttons and the general dismissal of the
     * dialog.
     */
    interface Handler extends DialogInterface.OnClickListener {
        /** Handle the dismissal of the dialog.*/
        void onDismiss();
    }

    // This handler is used to answer the user actions on the dialog.
    private final Handler mHandler;
    private final String mOrigin;

    PasswordCheckDeletionDialogFragment(Handler handler, String origin) {
        mHandler = handler;
        mOrigin = origin;
    }

    /**
     * Opens the dialog with the confirmation and sets the button listener to a fragment identified
     * by ID passed in arguments.
     */
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        return new AlertDialog
                .Builder(getActivity(), R.style.Theme_Chromium_AlertDialog_NoActionBar)
                .setTitle(R.string.password_check_delete_credential_dialog_title)
                .setPositiveButton(
                        R.string.password_check_delete_credential_dialog_confirm, mHandler)
                .setNegativeButton(R.string.password_check_credential_dialog_cancel, mHandler)
                .setMessage(
                        getString(R.string.password_check_delete_credential_dialog_body, mOrigin))
                .create();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (savedInstanceState != null) {
            dismiss();
            return;
        }
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        super.onDismiss(dialog);
        if (mHandler != null) mHandler.onDismiss();
    }
}
