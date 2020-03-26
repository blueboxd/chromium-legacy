// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;

import java.lang.ref.WeakReference;

// TODO (crbug/1058764) Unfork credential leak dialog password change when prototype is done.
/**
 * JNI call glue between the native password manager CredentialLeak class and
 * Java objects.
 */
public class CredentialLeakDialogPasswordChangeBridge {
    private long mNativeCredentialLeakDialogPasswordChangeViewAndroid;
    private final PasswordManagerDialogPasswordChangeCoordinator mCredentialLeakDialog;
    private final WeakReference<ChromeActivity> mActivity;

    private CredentialLeakDialogPasswordChangeBridge(
            WindowAndroid windowAndroid, long nativeCredentialLeakDialogPasswordChangeViewAndroid) {
        mNativeCredentialLeakDialogPasswordChangeViewAndroid =
                nativeCredentialLeakDialogPasswordChangeViewAndroid;
        ChromeActivity activity = (ChromeActivity) windowAndroid.getActivity().get();
        mActivity = new WeakReference<>(activity);
        mCredentialLeakDialog = new PasswordManagerDialogPasswordChangeCoordinator(
                activity.getModalDialogManager(), activity.findViewById(android.R.id.content),
                activity.getFullscreenManager(), activity.getControlContainerHeightResource());
    }

    @CalledByNative
    public static CredentialLeakDialogPasswordChangeBridge create(
            WindowAndroid windowAndroid, long nativeDialog) {
        return new CredentialLeakDialogPasswordChangeBridge(windowAndroid, nativeDialog);
    }

    @CalledByNative
    public void showDialog(String credentialLeakTitle, String credentialLeakDetails,
            String positiveButton, String negativeButton) {
        if (mActivity.get() == null) return;

        PasswordManagerDialogPasswordChangeContents contents =
                new PasswordManagerDialogPasswordChangeContents(credentialLeakTitle,
                        credentialLeakDetails, R.drawable.password_check_warning, positiveButton,
                        negativeButton, this::onClick);
        contents.setPrimaryButtonFilled(negativeButton != null);
        contents.setHelpButtonCallback(this::showHelpArticle);

        mCredentialLeakDialog.initialize(mActivity.get(), contents);
        mCredentialLeakDialog.showDialog();
    }

    @CalledByNative
    private void destroy() {
        mNativeCredentialLeakDialogPasswordChangeViewAndroid = 0;
        mCredentialLeakDialog.dismissDialog(DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    private void onClick(@DialogDismissalCause int dismissalCause) {
        if (mNativeCredentialLeakDialogPasswordChangeViewAndroid == 0) return;
        switch (dismissalCause) {
            case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                CredentialLeakDialogPasswordChangeBridgeJni.get().accepted(
                        mNativeCredentialLeakDialogPasswordChangeViewAndroid,
                        CredentialLeakDialogPasswordChangeBridge.this);
                return;
            case DialogDismissalCause.NEGATIVE_BUTTON_CLICKED:
                CredentialLeakDialogPasswordChangeBridgeJni.get().cancelled(
                        mNativeCredentialLeakDialogPasswordChangeViewAndroid,
                        CredentialLeakDialogPasswordChangeBridge.this);
                return;
            default:
                CredentialLeakDialogPasswordChangeBridgeJni.get().closed(
                        mNativeCredentialLeakDialogPasswordChangeViewAndroid,
                        CredentialLeakDialogPasswordChangeBridge.this);
        }
    }

    private void showHelpArticle() {
        if (mActivity.get() == null) return;

        Profile profile = Profile.fromWebContents(
                mActivity.get().getActivityTabProvider().get().getWebContents());
        HelpAndFeedback.getInstance().show(mActivity.get(),
                mActivity.get().getString(R.string.help_context_password_leak_detection), profile,
                null);
    }

    @NativeMethods
    interface Natives {
        void accepted(long nativeCredentialLeakDialogPasswordChangeViewAndroid,
                CredentialLeakDialogPasswordChangeBridge caller);

        void cancelled(long nativeCredentialLeakDialogPasswordChangeViewAndroid,
                CredentialLeakDialogPasswordChangeBridge caller);

        void closed(long nativeCredentialLeakDialogPasswordChangeViewAndroid,
                CredentialLeakDialogPasswordChangeBridge caller);
    }
}
