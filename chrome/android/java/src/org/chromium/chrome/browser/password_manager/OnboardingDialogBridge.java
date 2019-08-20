// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;

/** JNI call glue between the native password manager onboarding class and Java objects. */
public class OnboardingDialogBridge {
    private long mNativeOnboardingDialogView;
    private final PasswordManagerDialogCoordinator mOnboardingDialog;

    private OnboardingDialogBridge(WindowAndroid windowAndroid, long nativeOnboardingDialogView) {
        mNativeOnboardingDialogView = nativeOnboardingDialogView;
        ChromeActivity activity = (ChromeActivity) windowAndroid.getActivity().get();
        mOnboardingDialog = new PasswordManagerDialogCoordinator(activity);
    }

    @CalledByNative
    public static OnboardingDialogBridge create(WindowAndroid windowAndroid, long nativeDialog) {
        return new OnboardingDialogBridge(windowAndroid, nativeDialog);
    }

    @CalledByNative
    public void showDialog(String onboardingTitle, String onboardingDetails) {
        // TODO(crbug.com/983445): Replace the drawable once the real image is available.
        mOnboardingDialog.showDialog(onboardingTitle, onboardingDetails,
                R.drawable.data_reduction_illustration, this::onClick);
    }

    @CalledByNative
    private void destroy() {
        mNativeOnboardingDialogView = 0;
        mOnboardingDialog.dismissDialog(DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    private void onClick(boolean ok) {
        if (mNativeOnboardingDialogView == 0) return;
        if (ok) {
            nativeOnboardingAccepted(mNativeOnboardingDialogView);
        } else {
            nativeOnboardingRejected(mNativeOnboardingDialogView);
        }
    }

    private native void nativeOnboardingAccepted(long nativeOnboardingDialogView);
    private native void nativeOnboardingRejected(long nativeOnboardingDialogView);
}
