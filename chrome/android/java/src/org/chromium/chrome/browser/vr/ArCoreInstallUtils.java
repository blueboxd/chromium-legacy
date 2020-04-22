// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Activity;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.ui.messages.infobar.SimpleConfirmInfoBarBuilder;

/**
 * Installs AR DFM and ArCore runtimes.
 */
@JNINamespace("vr")
public class ArCoreInstallUtils {
    private static final String TAG = "ArCoreInstallUtils";

    private long mNativeArCoreInstallUtils;

    // Instance that requested installation of ARCore.
    // Should be non-null only if there is a pending request to install ARCore.
    private static ArCoreInstallUtils sRequestInstallInstance;

    // Cached ArCoreShim instance - valid only after AR module was installed and
    // getArCoreShimInstance() was called.
    private static ArCoreShim sArCoreInstance;

    private static ArCoreShim getArCoreShimInstance() {
        if (sArCoreInstance != null) return sArCoreInstance;

        try {
            sArCoreInstance =
                    (ArCoreShim) Class.forName("org.chromium.chrome.browser.vr.ArCoreShimImpl")
                            .newInstance();
        } catch (ClassNotFoundException e) {
            throw new RuntimeException(e);
        } catch (InstantiationException e) {
            throw new RuntimeException(e);
        } catch (IllegalAccessException e) {
            throw new RuntimeException(e);
        }

        return sArCoreInstance;
    }

    @CalledByNative
    private static ArCoreInstallUtils create(long nativeArCoreInstallUtils) {
        return new ArCoreInstallUtils(nativeArCoreInstallUtils);
    }

    @CalledByNative
    private void onNativeDestroy() {
        mNativeArCoreInstallUtils = 0;
    }

    private ArCoreInstallUtils(long nativeArCoreInstallUtils) {
        mNativeArCoreInstallUtils = nativeArCoreInstallUtils;
    }

    private static @ArCoreShim.Availability int getArCoreInstallStatus() {
        try {
            return getArCoreShimInstance().checkAvailability(ContextUtils.getApplicationContext());
        } catch (RuntimeException e) {
            return ArCoreShim.Availability.UNSUPPORTED_DEVICE_NOT_CAPABLE;
        }
    }

    @CalledByNative
    private static boolean shouldRequestInstallSupportedArCore() {
        @ArCoreShim.Availability
        int availability = getArCoreInstallStatus();
        // Skip ARCore installation if we are certain that it is already installed.
        // In all other cases, we might as well try to install it and handle installation failures.
        return availability != ArCoreShim.Availability.SUPPORTED_INSTALLED;
    }

    @CalledByNative
    private void requestInstallSupportedArCore(final Tab tab) {
        assert shouldRequestInstallSupportedArCore();

        @ArCoreShim.Availability
        int arCoreAvailability = getArCoreInstallStatus();
        final Activity activity = TabUtils.getActivity(tab);
        String infobarText = null;
        String buttonText = null;
        switch (arCoreAvailability) {
            case ArCoreShim.Availability.UNSUPPORTED_DEVICE_NOT_CAPABLE:
                maybeNotifyNativeOnRequestInstallSupportedArCoreResult(false);
                break;
            case ArCoreShim.Availability.UNKNOWN_CHECKING:
            case ArCoreShim.Availability.UNKNOWN_ERROR:
            case ArCoreShim.Availability.UNKNOWN_TIMED_OUT:
            case ArCoreShim.Availability.SUPPORTED_NOT_INSTALLED:
                infobarText = activity.getString(R.string.ar_core_check_infobar_install_text);
                buttonText = activity.getString(R.string.app_banner_install);
                break;
            case ArCoreShim.Availability.SUPPORTED_APK_TOO_OLD:
                infobarText = activity.getString(R.string.ar_core_check_infobar_update_text);
                buttonText = activity.getString(R.string.update_from_market);
                break;
            case ArCoreShim.Availability.SUPPORTED_INSTALLED:
                assert false;
                break;
        }

        SimpleConfirmInfoBarBuilder.Listener listener = new SimpleConfirmInfoBarBuilder.Listener() {
            @Override
            public void onInfoBarDismissed() {
                maybeNotifyNativeOnRequestInstallSupportedArCoreResult(
                        !shouldRequestInstallSupportedArCore());
            }

            @Override
            public boolean onInfoBarButtonClicked(boolean isPrimary) {
                try {
                    assert sRequestInstallInstance == null;
                    @ArCoreShim.InstallStatus
                    int installStatus = getArCoreShimInstance().requestInstall(activity, true);

                    if (installStatus == ArCoreShim.InstallStatus.INSTALL_REQUESTED) {
                        // Install flow will resume in onArCoreRequestInstallReturned, mark that
                        // there is active request. Native code notification will be deferred until
                        // our activity gets resumed.
                        sRequestInstallInstance = ArCoreInstallUtils.this;
                    } else if (installStatus == ArCoreShim.InstallStatus.INSTALLED) {
                        // No need to install - notify native code.
                        maybeNotifyNativeOnRequestInstallSupportedArCoreResult(true);
                    }

                } catch (ArCoreShim.UnavailableDeviceNotCompatibleException e) {
                    sRequestInstallInstance = null;
                    Log.w(TAG, "ARCore installation request failed with exception: %s",
                            e.toString());

                    maybeNotifyNativeOnRequestInstallSupportedArCoreResult(false);
                } catch (ArCoreShim.UnavailableUserDeclinedInstallationException e) {
                    maybeNotifyNativeOnRequestInstallSupportedArCoreResult(false);
                }

                return false;
            }

            @Override
            public boolean onInfoBarLinkClicked() {
                return false;
            }
        };
        // TODO(ijamardo, https://crbug.com/838833): Add icon for AR info bar.
        SimpleConfirmInfoBarBuilder.create(tab.getWebContents(), listener,
                InfoBarIdentifier.AR_CORE_UPGRADE_ANDROID, tab.getContext(),
                R.drawable.ic_error_outline_googblue_24dp, infobarText, buttonText, null, null,
                true);
    }

    /**
     * Helper used to notify native code about the result of the request to install ARCore.
     */
    private void maybeNotifyNativeOnRequestInstallSupportedArCoreResult(boolean success) {
        if (mNativeArCoreInstallUtils != 0) {
            ArCoreInstallUtilsJni.get().onRequestInstallSupportedArCoreResult(
                    mNativeArCoreInstallUtils, success);
        }
    }

    private void onArCoreRequestInstallReturned(Activity activity) {
        try {
            // Since |userRequestedInstall| parameter is false, the below call should
            // throw if ARCore is still not installed - no need to check the result.
            getArCoreShimInstance().requestInstall(activity, false);
            maybeNotifyNativeOnRequestInstallSupportedArCoreResult(true);
        } catch (ArCoreShim.UnavailableDeviceNotCompatibleException e) {
            Log.w(TAG, "Exception thrown when trying to validate install state of ARCore: %s",
                    e.toString());
            maybeNotifyNativeOnRequestInstallSupportedArCoreResult(false);
        } catch (ArCoreShim.UnavailableUserDeclinedInstallationException e) {
            maybeNotifyNativeOnRequestInstallSupportedArCoreResult(false);
        }
    }

    /**
     * This method should be called by the Activity that gets resumed.
     * We are only interested in the cases where our current Activity got paused
     * as a result of a call to ArCoreApk.requestInstall() method.
     */
    public static void onResumeActivityWithNative(Activity activity) {
        if (sRequestInstallInstance != null) {
            sRequestInstallInstance.onArCoreRequestInstallReturned(activity);
            sRequestInstallInstance = null;
        }
    }

    public static void installArCoreDeviceProviderFactory() {
        ArCoreInstallUtilsJni.get().installArCoreDeviceProviderFactory();
    }

    @NativeMethods
    /* package */ interface ArInstallHelperNative {
        void onRequestInstallSupportedArCoreResult(long nativeArCoreInstallHelper, boolean success);
        void installArCoreDeviceProviderFactory();
    }
}
