// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.http_auth;

import android.content.Context;
import android.content.DialogInterface;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;

import org.chromium.ui.UiUtils;

/**
 * HTTP Authentication Dialog
 *
 * This borrows liberally from android.browser.HttpAuthenticationDialog.
 */
public class LoginPrompt {
    private final Context mContext;
    private final String mMessageBody;
    private final Observer mObserver;

    private AlertDialog mDialog;
    private EditText mUsernameView;
    private EditText mPasswordView;

    /**
     * This is a public interface that provides the result of the prompt.
     */
    public static interface Observer {
        /**
         * Cancel the authorization request.
         */
        public void cancel();

        /**
         * Proceed with the authorization with the given credentials.
         */
        public void proceed(String username, String password);
    }

    public LoginPrompt(Context context, String messageBody, Observer observer) {
        mContext = context;
        mMessageBody = messageBody;
        mObserver = observer;
        createDialog();
    }

    private void createDialog() {
        View v = LayoutInflater.from(mContext).inflate(R.layout.http_auth_dialog, null);
        mUsernameView = (EditText) v.findViewById(R.id.username);
        mPasswordView = (EditText) v.findViewById(R.id.password);
        mPasswordView.setOnEditorActionListener((v1, actionId, event) -> {
            if (actionId == EditorInfo.IME_ACTION_DONE) {
                mDialog.getButton(AlertDialog.BUTTON_POSITIVE).performClick();
                return true;
            }
            return false;
        });

        TextView explanationView = (TextView) v.findViewById(R.id.explanation);
        explanationView.setText(mMessageBody);

        mDialog =
                new UiUtils
                        .CompatibleAlertDialogBuilder(mContext, R.style.Theme_Chromium_AlertDialog)
                        .setTitle(R.string.login_dialog_title)
                        .setView(v)
                        .setPositiveButton(R.string.login_dialog_ok_button_label,
                                (DialogInterface.OnClickListener) (dialog, whichButton)
                                        -> mObserver.proceed(getUsername(), getPassword()))
                        .setNegativeButton(R.string.cancel,
                                (DialogInterface.OnClickListener) (dialog,
                                        whichButton) -> mObserver.cancel())
                        .setOnCancelListener(dialog -> mObserver.cancel())
                        .create();
        mDialog.getDelegate().setHandleNativeActionModesEnabled(false);

        // Make the IME appear when the dialog is displayed if applicable.
        mDialog.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE);
    }

    /**
     * Shows the dialog.
     */
    public void show() {
        mDialog.show();
        mUsernameView.requestFocus();
    }

    /**
     * Dismisses the dialog.
     */
    public void dismiss() {
        mDialog.dismiss();
    }

    /** Return whether the dialog is being shown. */
    public boolean isShowing() {
        return mDialog != null && mDialog.isShowing();
    }

    private String getUsername() {
        return mUsernameView.getText().toString();
    }

    private String getPassword() {
        return mPasswordView.getText().toString();
    }

    public void onAutofillDataAvailable(String username, String password) {
        mUsernameView.setText(username);
        mPasswordView.setText(password);
        mUsernameView.selectAll();
    }
}
