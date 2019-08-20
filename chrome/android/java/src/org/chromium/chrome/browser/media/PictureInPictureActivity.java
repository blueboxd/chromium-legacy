// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;

/**
 * A picture in picture activity which get created when requesting
 * PiP from web API. The activity will connect to web API through
 * OverlayWindowAndroid.
 */
public class PictureInPictureActivity extends AsyncInitializationActivity {
    private static long sNativeOverlayWindowAndroid;
    private static Tab sInitiatorTab;
    private static int sInitiatorTabTaskID;
    private static InitiatorTabObserver sTabObserver;

    private static class InitiatorTabObserver extends EmptyTabObserver {
        private enum Status { OK, DESTROYED }
        private PictureInPictureActivity mActivity;
        private Status mStatus;

        InitiatorTabObserver() {
            mStatus = Status.OK;
        }

        public void setActivity(PictureInPictureActivity activity) {
            mActivity = activity;
        }

        public Status getStatus() {
            return mStatus;
        }

        @Override
        public void onDestroyed(Tab tab) {
            if (tab.isClosing() || !isInitiatorTabAlive()) {
                mStatus = Status.DESTROYED;
                if (mActivity != null) mActivity.finish();
            }
        }

        @Override
        public void onCrash(Tab tab) {
            mStatus = Status.DESTROYED;
            if (mActivity != null) mActivity.finish();
        }
    }

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

        // Finish the activity if OverlayWindowAndroid has already been destroyed
        // or InitiatorTab has been destroyed by user or crashed.
        if (sNativeOverlayWindowAndroid == 0
                || sTabObserver.getStatus() == InitiatorTabObserver.Status.DESTROYED) {
            this.finish();
            return;
        }

        sTabObserver.setActivity(this);

        PictureInPictureActivityJni.get().onActivityStart(
                sNativeOverlayWindowAndroid, this, getWindowAndroid());
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        sNativeOverlayWindowAndroid = 0;
        sInitiatorTab.removeObserver(sTabObserver);
        sInitiatorTab = null;
        sTabObserver = null;
    }

    @Override
    public void onPictureInPictureModeChanged(
            boolean isInPictureInPictureMode, Configuration newConfig) {
        if (!isInPictureInPictureMode) this.finish();
    }

    @Override
    protected ActivityWindowAndroid createWindowAndroid() {
        return new ActivityWindowAndroid(this);
    }

    @SuppressLint("NewApi")
    private static boolean isInitiatorTabAlive() {
        ActivityManager activityManager =
                (ActivityManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.ACTIVITY_SERVICE);
        for (ActivityManager.AppTask appTask : activityManager.getAppTasks()) {
            if (appTask.getTaskInfo().id == sInitiatorTabTaskID) return true;
        }

        return false;
    }

    @CalledByNative
    private void close() {
        this.finish();
    }

    @CalledByNative
    private static void createActivity(long nativeOverlayWindowAndroid, Object initiatorTab) {
        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent(context, PictureInPictureActivity.class);

        // Dissociate OverlayWindowAndroid if there is one already.
        if (sNativeOverlayWindowAndroid != 0)
            PictureInPictureActivityJni.get().destroy(sNativeOverlayWindowAndroid);

        sNativeOverlayWindowAndroid = nativeOverlayWindowAndroid;
        sInitiatorTab = (Tab) initiatorTab;
        sInitiatorTabTaskID = sInitiatorTab.getActivity().getTaskId();

        sTabObserver = new InitiatorTabObserver();
        sInitiatorTab.addObserver(sTabObserver);

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
        void onActivityStart(long nativeOverlayWindowAndroid, PictureInPictureActivity self,
                WindowAndroid window);

        void destroy(long nativeOverlayWindowAndroid);
    }
}
