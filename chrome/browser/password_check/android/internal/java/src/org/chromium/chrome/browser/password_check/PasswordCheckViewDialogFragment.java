// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.app.Dialog;
import android.os.Bundle;
import android.text.InputType;
import android.view.View;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;

import org.chromium.chrome.browser.password_check.internal.R;

/**
 * Shows the dialog that allows the user to see the compromised credential.
 */
public class PasswordCheckViewDialogFragment extends PasswordCheckDialogFragment {
    private CompromisedCredential mCredential;

    PasswordCheckViewDialogFragment(Handler handler, CompromisedCredential credential) {
        super(handler);
        mCredential = credential;
    }

    /**
     * Opens the dialog with the compromised credential
     */
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        View dialogContent = getActivity().getLayoutInflater().inflate(
                R.layout.password_check_view_credential_dialog, null);
        TextView passwordView = dialogContent.findViewById(R.id.view_dialog_compromised_password);
        passwordView.setText(mCredential.getPassword());
        passwordView.setInputType(InputType.TYPE_CLASS_TEXT
                | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
                | InputType.TYPE_TEXT_FLAG_MULTI_LINE);
        AlertDialog viewDialog = new AlertDialog.Builder(getActivity())
                                         .setTitle(mCredential.getDisplayOrigin())
                                         .setNegativeButton(R.string.close, mHandler)
                                         .setView(dialogContent)
                                         .create();
        return viewDialog;
    }
}
