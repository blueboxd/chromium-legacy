// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.chromium.chrome.R;

/**
 * A class to display the dialogs the user may encounter when switching to/from or signing into/out
 * of a managed account.
 */
public class ConfirmManagedSyncDataDialog extends DialogFragment
        implements DialogInterface.OnClickListener {
    /**
     * A listener to allow the Dialog to report on the action taken. Either
     * {@link Listener#onConfirm} or {@link Listener#onCancel} will be called once.
     */
    public interface Listener {
        /**
         * The user has accepted the dialog.
         */
        void onConfirm();

        /**
         * The user has cancelled the dialog either through a negative response or by dismissing it.
         */
        void onCancel();
    }

    private static final String KEY_DOMAIN = "domain";

    private Listener mListener;
    private boolean mListenerCalled;

    /**
     * Creates {@link ConfirmManagedSyncDataDialog} when signing in to a managed account
     * (either through sign in or when switching accounts).
     * @param listener Callback for result.
     * @param domain The domain of the managed account.
     */
    static ConfirmManagedSyncDataDialog create(Listener listener, String domain) {
        ConfirmManagedSyncDataDialog dialog = new ConfirmManagedSyncDataDialog();
        Bundle args = new Bundle();
        args.putString(KEY_DOMAIN, domain);
        dialog.setArguments(args);
        dialog.setListener(listener);
        return dialog;
    }

    private void setListener(Listener listener) {
        assert listener != null;
        mListener = listener;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        if (mListener == null) {
            dismiss();
        }
        String title = getString(R.string.sign_in_managed_account);
        String description = getString(
                R.string.sign_in_managed_account_description, getArguments().getString(KEY_DOMAIN));
        String positiveButton = getString(R.string.policy_dialog_proceed);
        String negativeButton = getString(R.string.cancel);

        return new AlertDialog.Builder(getActivity(), R.style.Theme_Chromium_AlertDialog)
                .setTitle(title)
                .setMessage(description)
                .setPositiveButton(positiveButton, this)
                .setNegativeButton(negativeButton, this)
                .create();
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        if (which == AlertDialog.BUTTON_POSITIVE) {
            mListener.onConfirm();
        } else {
            assert which == AlertDialog.BUTTON_NEGATIVE;
            mListener.onCancel();
        }
        mListenerCalled = true;
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        super.onDismiss(dialog);
        if (mListener != null && !mListenerCalled) {
            mListener.onCancel();
        }
    }
}

