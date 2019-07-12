// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Activity;
import android.content.Context;
import android.view.Surface;

import dalvik.system.BaseDexClassLoader;

import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.infobar.SimpleConfirmInfoBarBuilder;
import org.chromium.chrome.browser.modules.ModuleInstallUi;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.module_installer.ModuleInstaller;

/**
 * Provides ARCore classes access to java-related app functionality.
 */
@JNINamespace("vr")
public class ArCoreJavaUtils implements ModuleInstallUi.FailureUiListener {
    private static final String TAG = "ArCoreJavaUtils";
    private static final boolean DEBUG_LOGS = false;

    private long mNativeArCoreJavaUtils;
    private boolean mAppInfoInitialized;
    private Tab mTab;

    private ArImmersiveOverlay mArImmersiveOverlay;

    // Instance that requested installation of ARCore.
    // Should be non-null only if there is a pending request to install ARCore.
    private static ArCoreJavaUtils sRequestInstallInstance;

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
            // shouldn't happen - we should only call this method once AR module is installed.
            throw new RuntimeException(e);
        } catch (InstantiationException e) {
            throw new RuntimeException(e);
        } catch (IllegalAccessException e) {
            throw new RuntimeException(e);
        }

        return sArCoreInstance;
    }

    public static void installArCoreDeviceProviderFactory() {
        nativeInstallArCoreDeviceProviderFactory();
    }

    /**
     * Gets the current application context.
     *
     * @return Context The application context.
     */
    @CalledByNative
    private static Context getApplicationContext() {
        return ContextUtils.getApplicationContext();
    }

    @CalledByNative
    private static ArCoreJavaUtils create(long nativeArCoreJavaUtils) {
        ThreadUtils.assertOnUiThread();
        return new ArCoreJavaUtils(nativeArCoreJavaUtils);
    }

    @CalledByNative
    private static String getArCoreShimLibraryPath() {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return ((BaseDexClassLoader) ContextUtils.getApplicationContext().getClassLoader())
                    .findLibrary("arcore_sdk_c");
        }
    }

    @Override
    public void onRetry() {
        if (mNativeArCoreJavaUtils == 0) return;
        requestInstallArModule(mTab);
    }

    @Override
    public void onCancel() {
        if (mNativeArCoreJavaUtils == 0) return;
        nativeOnRequestInstallArModuleResult(mNativeArCoreJavaUtils, /* success = */ false);
    }

    private ArCoreJavaUtils(long nativeArCoreJavaUtils) {
        if (DEBUG_LOGS) Log.i(TAG, "constructor, nativeArCoreJavaUtils=" + nativeArCoreJavaUtils);
        mNativeArCoreJavaUtils = nativeArCoreJavaUtils;
        initializeAppInfo();
    }

    @CalledByNative
    private void startSession(final Tab tab) {
        if (DEBUG_LOGS) Log.i(TAG, "startSession");
        mArImmersiveOverlay = new ArImmersiveOverlay();
        mArImmersiveOverlay.show(tab.getActivity(), this);
    }

    @CalledByNative
    private void destroyArImmersiveOverlay() {
        if (DEBUG_LOGS) Log.i(TAG, "destroyArImmersiveOverlay");
        if (mArImmersiveOverlay != null) {
            mArImmersiveOverlay.destroyDialog();
            mArImmersiveOverlay = null;
        }
    }

    public void onDrawingSurfaceReady(Surface surface, int rotation, int width, int height) {
        if (DEBUG_LOGS) Log.i(TAG, "onDrawingSurfaceReady");
        if (mNativeArCoreJavaUtils == 0) return;
        nativeOnDrawingSurfaceReady(mNativeArCoreJavaUtils, surface, rotation, width, height);
    }

    public void onDrawingSurfaceTouch(boolean isTouching, float x, float y) {
        if (DEBUG_LOGS) Log.i(TAG, "onDrawingSurfaceTouch");
        if (mNativeArCoreJavaUtils == 0) return;
        nativeOnDrawingSurfaceTouch(mNativeArCoreJavaUtils, isTouching, x, y);
    }

    public void onDrawingSurfaceDestroyed() {
        if (DEBUG_LOGS) Log.i(TAG, "onDrawingSurfaceDestroyed");
        if (mNativeArCoreJavaUtils == 0) return;
        mArImmersiveOverlay = null;
        nativeOnDrawingSurfaceDestroyed(mNativeArCoreJavaUtils);
    }

    @CalledByNative
    private void onNativeDestroy() {
        mNativeArCoreJavaUtils = 0;
    }

    private void initializeAppInfo() {
        mAppInfoInitialized = true;

        // Need to be called before trying to access the AR module.
        ModuleInstaller.getInstance().init();
    }

    private @ArCoreShim.Availability int getArCoreInstallStatus() {
        return getArCoreShimInstance().checkAvailability(getApplicationContext());
    }

    @CalledByNative
    private boolean shouldRequestInstallSupportedArCore() {
        @ArCoreShim.Availability
        int availability = getArCoreInstallStatus();
        // Skip ARCore installation if we are certain that it is already installed.
        // In all other cases, we might as well try to install it and handle installation failures.
        return availability != ArCoreShim.Availability.SUPPORTED_INSTALLED;
    }

    @CalledByNative
    private void requestInstallArModule(Tab tab) {
        mTab = tab;
        ModuleInstallUi ui = new ModuleInstallUi(mTab, R.string.ar_module_title, this);
        ui.showInstallStartUi();
        ModuleInstaller.getInstance().install("ar", success -> {
            assert shouldRequestInstallArModule() != success;

            if (success) {
                // As per documentation, it's recommended to issue a call to
                // ArCoreApk.checkAvailability() early in application lifecycle & ignore the result
                // so that subsequent calls can return cached result:
                // https://developers.google.com/ar/develop/java/enable-arcore
                // This is as early in the app lifecycle as it gets for us - just after installing
                // AR module.
                getArCoreInstallStatus();
            }

            if (mNativeArCoreJavaUtils != 0) {
                if (success) {
                    ui.showInstallSuccessUi();
                    nativeOnRequestInstallArModuleResult(mNativeArCoreJavaUtils, success);
                } else {
                    ui.showInstallFailureUi();
                    // early exit - user will be offered a choice to retry & install flow will
                    // continue from onRetry / onCancel
                    return;
                }
            }
        });
    }

    @CalledByNative
    private void requestInstallSupportedArCore(final Tab tab) {
        assert shouldRequestInstallSupportedArCore();

        @ArCoreShim.Availability
        int arCoreAvailability = getArCoreInstallStatus();
        final Activity activity = tab.getActivity();
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
                        sRequestInstallInstance = ArCoreJavaUtils.this;
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
        SimpleConfirmInfoBarBuilder.create(tab, listener, InfoBarIdentifier.AR_CORE_UPGRADE_ANDROID,
                R.drawable.ic_error_outline_googblue_24dp, infobarText, buttonText, null, null,
                true);
    }

    @CalledByNative
    private boolean canRequestInstallArModule() {
        // We can only try to install the AR module if we are in a bundle mode.
        return BundleUtils.isBundle();
    }

    @CalledByNative
    private boolean shouldRequestInstallArModule() {
        try {
            // Try to find class in AR module that has not been obfuscated.
            Class.forName("com.google.ar.core.ArCoreApk");
            return false;
        } catch (ClassNotFoundException e) {
            return true;
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

    /**
     * Helper used to notify native code about the result of the request to install ARCore.
     */
    private void maybeNotifyNativeOnRequestInstallSupportedArCoreResult(boolean success) {
        if (mNativeArCoreJavaUtils != 0) {
            nativeOnRequestInstallSupportedArCoreResult(mNativeArCoreJavaUtils, success);
        }
    }

    private static native void nativeInstallArCoreDeviceProviderFactory();
    private native void nativeOnRequestInstallArModuleResult(
            long nativeArCoreJavaUtils, boolean success);
    private native void nativeOnRequestInstallSupportedArCoreResult(
            long nativeArCoreJavaUtils, boolean success);

    private native void nativeOnDrawingSurfaceReady(
            long nativeArCoreJavaUtils, Surface surface, int rotation, int width, int height);
    private native void nativeOnDrawingSurfaceTouch(
            long nativeArCoreJavaUtils, boolean touching, float x, float y);
    private native void nativeOnDrawingSurfaceDestroyed(long nativeArCoreJavaUtils);
}
