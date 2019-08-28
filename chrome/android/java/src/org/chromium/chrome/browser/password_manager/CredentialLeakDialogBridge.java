// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;

/** JNI call glue between the native password manager CredentialLeak class and Java objects. */
public class CredentialLeakDialogBridge {
    private long mNativeCredentialLeakDialogViewAndroid;
    private final PasswordManagerDialogCoordinator mCredentialLeakDialog;

    private CredentialLeakDialogBridge(
            WindowAndroid windowAndroid, long nativeCredentialLeakDialogViewAndroid) {
        mNativeCredentialLeakDialogViewAndroid = nativeCredentialLeakDialogViewAndroid;
        ChromeActivity activity = (ChromeActivity) windowAndroid.getActivity().get();
        mCredentialLeakDialog = new PasswordManagerDialogCoordinator(
                windowAndroid.getContext().get(), activity.getModalDialogManager(),
                activity.findViewById(android.R.id.content), activity.getFullscreenManager(),
                activity.getControlContainerHeightResource());
    }

    @CalledByNative
    public static CredentialLeakDialogBridge create(
            WindowAndroid windowAndroid, long nativeDialog) {
        return new CredentialLeakDialogBridge(windowAndroid, nativeDialog);
    }

    @CalledByNative
    public void showDialog(String credentialLeakTitle, String credentialLeakDetails,
            String positiveButton, String negativeButton) {
        mCredentialLeakDialog.showDialog(credentialLeakTitle, credentialLeakDetails,
                R.drawable.password_check_warning, positiveButton, negativeButton, this::onClick);
    }

    @CalledByNative
    private void destroy() {
        mNativeCredentialLeakDialogViewAndroid = 0;
        mCredentialLeakDialog.dismissDialog(DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    private void onClick(boolean ok) {
        if (mNativeCredentialLeakDialogViewAndroid == 0) return;
        if (ok) {
            nativeAccepted(mNativeCredentialLeakDialogViewAndroid);
        } else {
            nativeClosed(mNativeCredentialLeakDialogViewAndroid);
        }
    }

    private native void nativeAccepted(long nativeCredentialLeakDialogViewAndroid);
    private native void nativeClosed(long nativeCredentialLeakDialogViewAndroid);
}
