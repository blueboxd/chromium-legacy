// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sms;

import android.app.Activity;
import android.app.Dialog;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.content_public.browser.sms.Event;
import org.chromium.ui.base.WindowAndroid;

/**
 * Modal dialog that enables users to abort/opt-out the operation of receiving
 * SMS messages.
 */
public class SmsReceiverDialog {
    private static final String TAG = "SmsReceiverDialog";
    private static final boolean DEBUG = false;
    private long mNativeSmsDialogAndroid;
    private Activity mActivity;
    // The dialog this class encapsulates.
    private Dialog mDialog;
    private ProgressBar mProgressBar;
    private ImageView mDoneIcon;
    private ImageView mErrorIcon;
    private TextView mStatus;
    private Button mCancelButton;
    private Button mConfirmOrTryAgainButton;
    private String mOrigin;

    @VisibleForTesting
    @CalledByNative
    static SmsReceiverDialog create(long nativeSmsDialogAndroid, String origin) {
        if (DEBUG) Log.d(TAG, "SmsReceiverDialog.create()");
        return new SmsReceiverDialog(nativeSmsDialogAndroid, origin);
    }

    private SmsReceiverDialog(long nativeSmsDialogAndroid, String origin) {
        mNativeSmsDialogAndroid = nativeSmsDialogAndroid;
        mOrigin = origin;
    }

    private void createAndShowDialog(WindowAndroid windowAndroid) {
        mActivity = windowAndroid.getActivity().get();

        LinearLayout dialogContainer = (LinearLayout) LayoutInflater.from(mActivity).inflate(
                R.layout.sms_receiver_dialog, null);

        TextView title = (TextView) dialogContainer.findViewById(R.id.dialog_title);
        title.setText(mActivity.getResources().getString(R.string.sms_dialog_title, mOrigin));

        mProgressBar = (ProgressBar) dialogContainer.findViewById(R.id.progress);

        mDoneIcon = (ImageView) dialogContainer.findViewById(R.id.done_icon);

        mErrorIcon = (ImageView) dialogContainer.findViewById(R.id.error_icon);

        mStatus = (TextView) dialogContainer.findViewById(R.id.status);

        mCancelButton = (Button) dialogContainer.findViewById(R.id.cancel_button);
        mCancelButton.setOnClickListener(v -> {
            assert mNativeSmsDialogAndroid != 0;
            SmsReceiverDialogJni.get().onEvent(mNativeSmsDialogAndroid, Event.CANCEL);
        });

        mConfirmOrTryAgainButton =
                (Button) dialogContainer.findViewById(R.id.confirm_or_try_again_button);
        mConfirmOrTryAgainButton.setOnClickListener(v -> {
            assert mNativeSmsDialogAndroid != 0;
            SmsReceiverDialogJni.get().onEvent(mNativeSmsDialogAndroid, Event.CONFIRM);
        });

        mDialog = new Dialog(mActivity);
        mDialog.setCanceledOnTouchOutside(false);
        mDialog.addContentView(dialogContainer,
                new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
                        LinearLayout.LayoutParams.MATCH_PARENT));

        Window window = mDialog.getWindow();
        window.setLayout(Math.round(mActivity.getWindow().getDecorView().getWidth() * 0.9f),
                LayoutParams.WRAP_CONTENT);
        window.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));

        mDialog.show();
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

        createAndShowDialog(window);
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

        mProgressBar.setVisibility(View.GONE);
        mDoneIcon.setVisibility(View.VISIBLE);
        mStatus.setText(mActivity.getText(R.string.sms_dialog_status_sms_received));
        mConfirmOrTryAgainButton.setEnabled(true);
    }

    @CalledByNative
    void smsTimeout() {
        if (DEBUG) Log.d(TAG, "SmsReceiverDialog.smsTimeout()");

        mProgressBar.setVisibility(View.GONE);
        mErrorIcon.setVisibility(View.VISIBLE);
        mStatus.setText(mActivity.getText(R.string.sms_dialog_status_timeout));
        mCancelButton.setVisibility(View.GONE);
        mConfirmOrTryAgainButton.setText(mActivity.getText(R.string.try_again));
        mConfirmOrTryAgainButton.setEnabled(true);
        mConfirmOrTryAgainButton.setOnClickListener(v -> {
            assert mNativeSmsDialogAndroid != 0;
            SmsReceiverDialogJni.get().onEvent(mNativeSmsDialogAndroid, Event.TIMEOUT);
        });
    }

    /**
     * Returns the dialog associated with this class. For use with tests only.
     */
    @VisibleForTesting
    Dialog getDialogForTesting() {
        return mDialog;
    }

    @VisibleForTesting
    boolean isDialogDestroyedForTesting() {
        return mNativeSmsDialogAndroid == 0;
    }

    @NativeMethods
    interface Natives {
        void onEvent(long nativeSmsDialogAndroid, int eventType);
    }
}
