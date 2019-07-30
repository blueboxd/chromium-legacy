// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.graphics.drawable.Drawable;
import android.support.v7.content.res.AppCompatResources;
import android.widget.Button;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.ui.base.WindowAndroid;

/**
 * Modal dialog that enables users to abort/opt-out the operation of receiving
 * SMS messages.
 */
public class SmsReceiverDialog {
    private static final String TAG = "SmsReceiverDialog";
    private static final boolean DEBUG = false;
    private long mNativeSmsDialogAndroid;
    private ProgressDialog mDialog;
    private Activity mActivity;

    @VisibleForTesting
    @CalledByNative
    static SmsReceiverDialog create(long nativeSmsDialogAndroid) {
        if (DEBUG) Log.d(TAG, "SmsReceiverDialog.create()");
        return new SmsReceiverDialog(nativeSmsDialogAndroid);
    }

    private static ProgressDialog createDialog(Activity activity, long nativeSmsDialogAndroid) {
        ProgressDialog dialog = new ProgressDialog(activity);
        dialog.setIndeterminate(true);
        dialog.setCancelable(false);
        dialog.setCanceledOnTouchOutside(false);
        dialog.setTitle(activity.getText(R.string.sms_dialog_title));
        dialog.setMessage(activity.getText(R.string.sms_dialog_status_waiting));

        dialog.setButton(DialogInterface.BUTTON_NEGATIVE, activity.getText(R.string.cancel),
                new OnClickListener() {
                    @Override
                    public void onClick(DialogInterface prompt, int which) {
                        assert nativeSmsDialogAndroid != 0;
                        prompt.dismiss();
                        SmsReceiverDialogJni.get().onCancel(nativeSmsDialogAndroid);
                    }
                });
        dialog.setButton(DialogInterface.BUTTON_POSITIVE,
                activity.getText(R.string.sms_dialog_continue_button_text), new OnClickListener() {
                    @Override
                    public void onClick(DialogInterface prompt, int which) {
                        assert nativeSmsDialogAndroid != 0;
                        prompt.dismiss();
                        SmsReceiverDialogJni.get().onContinue(nativeSmsDialogAndroid);
                    }
                });

        return dialog;
    }

    private SmsReceiverDialog(long nativeSmsDialogAndroid) {
        mNativeSmsDialogAndroid = nativeSmsDialogAndroid;
    }

    @VisibleForTesting
    @CalledByNative
    void destroy() {
        if (DEBUG) Log.d(TAG, "SmsReceiverDialog.destroy()");
        assert mNativeSmsDialogAndroid != 0;
        mNativeSmsDialogAndroid = 0;
    }

    @VisibleForTesting
    @CalledByNative
    void open(WindowAndroid window) {
        if (DEBUG) Log.d(TAG, "SmsReceiverDialog.open()");

        assert mNativeSmsDialogAndroid != 0 : "open() called after object was destroyed";

        mActivity = window.getActivity().get();
        mDialog = createDialog(mActivity, mNativeSmsDialogAndroid);
        mDialog.show();
        Button continueButton = mDialog.getButton(DialogInterface.BUTTON_POSITIVE);
        continueButton.setEnabled(false);
    }

    @VisibleForTesting
    @CalledByNative
    void close() {
        if (DEBUG) Log.d(TAG, "SmsReceiverDialog.close()");

        assert mNativeSmsDialogAndroid != 0 : "close() called after object was destroyed";

        mDialog.dismiss();
    }

    @VisibleForTesting
    @CalledByNative
    void smsReceived() {
        if (DEBUG) Log.d(TAG, "SmsReceiverDialog.smsReceived()");

        mDialog.dismiss();
        mDialog = createDialog(mActivity, mNativeSmsDialogAndroid);
        Drawable smsReceivedDrawable =
                AppCompatResources.getDrawable(mActivity, R.drawable.ic_check_circle_blue_18dp);
        mDialog.setIndeterminateDrawable(smsReceivedDrawable);
        mDialog.setMessage(mActivity.getText(R.string.sms_dialog_status_sms_received));
        mDialog.show();

        Button continueButton = mDialog.getButton(DialogInterface.BUTTON_POSITIVE);
        continueButton.setEnabled(true);
    }

    /**
     * Returns the dialog associated with this class. For use with tests only.
     */
    @VisibleForTesting
    ProgressDialog getDialogForTesting() {
        return mDialog;
    }

    @VisibleForTesting
    boolean isDialogDestroyedForTesting() {
        return mNativeSmsDialogAndroid == 0;
    }

    @NativeMethods
    interface Natives {
        void onCancel(long nativeSmsDialogAndroid);
        void onContinue(long nativeSmsDialogAndroid);
    }
}
