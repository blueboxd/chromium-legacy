// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.content.Context;
import android.content.Intent;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JCaller;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;

/**
 * A picture in picture activity which get created when requesting
 * PiP from web API. The activity will connect to web API through
 * OverlayWindowAndroid.
 */
public class PictureInPictureActivity extends AsyncInitializationActivity {
    private static long sNativeOverlayWindowAndroid;

    @Override
    protected void triggerLayoutInflation() {
        onInitialLayoutInflationComplete();
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    @Override
    public void onStart() {
        super.onStart();
        enterPictureInPictureMode();

        // Finish the activity if OverlayWindowAndroid has already been destroyed.
        if (sNativeOverlayWindowAndroid == 0) {
            this.finish();
            return;
        }

        PictureInPictureActivityJni.get().onActivityStart(this, sNativeOverlayWindowAndroid);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        if (sNativeOverlayWindowAndroid == 0) return;

        PictureInPictureActivityJni.get().onActivityDestroy(sNativeOverlayWindowAndroid);
        sNativeOverlayWindowAndroid = 0;
    }

    @CalledByNative
    private void close() {
        sNativeOverlayWindowAndroid = 0;
        this.finish();
    }

    @CalledByNative
    private static void createActivity(long nativeOverlayWindowAndroid) {
        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent(context, PictureInPictureActivity.class);

        // Dissociate OverlayWindowAndroid if there is one already.
        if (sNativeOverlayWindowAndroid != 0)
            PictureInPictureActivityJni.get().onActivityDestroy(sNativeOverlayWindowAndroid);

        sNativeOverlayWindowAndroid = nativeOverlayWindowAndroid;

        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
    }

    @CalledByNative
    private static void onWindowDestroyed(long nativeOverlayWindowAndroid) {
        if (sNativeOverlayWindowAndroid != nativeOverlayWindowAndroid) return;

        sNativeOverlayWindowAndroid = 0;
    }

    @NativeMethods
    interface Natives {
        void onActivityStart(
                @JCaller PictureInPictureActivity self, long nativeOverlayWindowAndroid);

        void onActivityDestroy(long nativeOverlayWindowAndroid);
    }
}
