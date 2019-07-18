// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;

import org.chromium.base.Log;
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
    private boolean mDestroyed;
    private ProgressDialog mDialog;

    private SmsReceiverDialog(long nativeSmsDialogAndroid) {
        mNativeSmsDialogAndroid = nativeSmsDialogAndroid;
    }

    @CalledByNative
    private static SmsReceiverDialog create(long nativeSmsDialogAndroid) {
        if (DEBUG) Log.d(TAG, "SmsReceiverDialog.create()");
        return new SmsReceiverDialog(nativeSmsDialogAndroid);
    }

    @CalledByNative
    private void destroy() {
        if (DEBUG) Log.d(TAG, "SmsReceiverDialog.destroy()");
        assert !mDestroyed;
        mDestroyed = true;
        mNativeSmsDialogAndroid = 0;
    }

    @CalledByNative
    private void open(WindowAndroid window) {
        if (DEBUG) Log.d(TAG, "SmsReceiverDialog.open()");

        assert !mDestroyed : "open() called after object was destroyed";

        Activity activity = window.getActivity().get();

        mDialog = new ProgressDialog(activity);
        mDialog.setIndeterminate(true);
        mDialog.setCancelable(false);
        mDialog.setCanceledOnTouchOutside(false);
        mDialog.setTitle(activity.getText(R.string.sms_dialog_title));
        mDialog.setMessage(activity.getText(R.string.sms_dialog_status_waiting));
        mDialog.setButton(DialogInterface.BUTTON_NEGATIVE,
                activity.getText(android.R.string.cancel), new OnClickListener() {
                    @Override
                    public void onClick(DialogInterface prompt, int which) {
                        assert !mDestroyed;
                        prompt.dismiss();
                        SmsReceiverDialogJni.get().onCancel(mNativeSmsDialogAndroid);
                    }
                });
        mDialog.show();
    }

    @CalledByNative
    private void close() {
        if (DEBUG) Log.d(TAG, "SmsReceiverDialog.close()");

        assert !mDestroyed : "close() called after object was destroyed";

        mDialog.dismiss();
    }

    @NativeMethods
    interface Natives {
        void onCancel(long nativeSmsDialogAndroid);
    }
}
