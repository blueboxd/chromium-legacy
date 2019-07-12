// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.content.Context;
import android.view.Surface;

import dalvik.system.BaseDexClassLoader;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Provides ARCore classes access to java-related app functionality.
 */
@JNINamespace("vr")
public class ArCoreJavaUtils {
    private static final String TAG = "ArCoreJavaUtils";
    private static final boolean DEBUG_LOGS = false;

    private long mNativeArCoreJavaUtils;

    private ArImmersiveOverlay mArImmersiveOverlay;

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

    /**
     * Gets the current application context.
     *
     * @return Context The application context.
     */
    @CalledByNative
    private static Context getApplicationContext() {
        return ContextUtils.getApplicationContext();
    }

    private ArCoreJavaUtils(long nativeArCoreJavaUtils) {
        if (DEBUG_LOGS) Log.i(TAG, "constructor, nativeArCoreJavaUtils=" + nativeArCoreJavaUtils);
        mNativeArCoreJavaUtils = nativeArCoreJavaUtils;
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

    private native void nativeOnDrawingSurfaceReady(
            long nativeArCoreJavaUtils, Surface surface, int rotation, int width, int height);
    private native void nativeOnDrawingSurfaceTouch(
            long nativeArCoreJavaUtils, boolean touching, float x, float y);
    private native void nativeOnDrawingSurfaceDestroyed(long nativeArCoreJavaUtils);
}
