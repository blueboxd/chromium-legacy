// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.download.dialogs.DownloadLocationDialogController;
import org.chromium.chrome.browser.download.dialogs.DownloadLocationDialogCoordinator;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.ui.base.WindowAndroid;

/**
 * Glues download dialogs UI code and handles the communication to download native backend.
 */
public class DownloadDialogBridge implements DownloadLocationDialogController {
    private static final long INVALID_START_TIME = -1;
    private long mNativeDownloadDialogBridge;

    private final DownloadLocationDialogCoordinator mLocationDialog;

    public DownloadDialogBridge(
            long nativeDownloadDialogBridge, DownloadLocationDialogCoordinator locationDialog) {
        mNativeDownloadDialogBridge = nativeDownloadDialogBridge;
        mLocationDialog = locationDialog;
    }

    @CalledByNative
    private static DownloadDialogBridge create(long nativeDownloadDialogBridge) {
        DownloadLocationDialogCoordinator locationDialog = new DownloadLocationDialogCoordinator();
        DownloadDialogBridge bridge =
                new DownloadDialogBridge(nativeDownloadDialogBridge, locationDialog);
        locationDialog.initialize(bridge);
        return bridge;
    }

    @CalledByNative
    void destroy() {
        mNativeDownloadDialogBridge = 0;
        mLocationDialog.destroy();
    }

    @CalledByNative
    void showDialog(WindowAndroid windowAndroid, long totalBytes,
            @DownloadLocationDialogType int dialogType, String suggestedPath) {
        mLocationDialog.showDialog(windowAndroid, totalBytes, dialogType, suggestedPath);
    }

    // DownloadLocationDialogController implementation.
    @Override
    public void onComplete(String returnedPath) {
        if (mNativeDownloadDialogBridge == 0) return;

        DownloadDialogBridgeJni.get().onComplete(mNativeDownloadDialogBridge,
                DownloadDialogBridge.this, returnedPath, false /*onWifi*/, INVALID_START_TIME);
    }

    @Override
    public void onCancel() {
        if (mNativeDownloadDialogBridge == 0) return;

        if (mNativeDownloadDialogBridge != 0) {
            DownloadDialogBridgeJni.get().onCanceled(
                    mNativeDownloadDialogBridge, DownloadDialogBridge.this);
        }
    }

    /**
     * @return The stored download default directory.
     */
    public static String getDownloadDefaultDirectory() {
        return DownloadDialogBridgeJni.get().getDownloadDefaultDirectory();
    }

    /**
     * @param directory New directory to set as the download default directory.
     */
    public static void setDownloadAndSaveFileDefaultDirectory(String directory) {
        DownloadDialogBridgeJni.get().setDownloadAndSaveFileDefaultDirectory(directory);
    }

    /**
     * @return The status of prompt for download pref, defined by {@link DownloadPromptStatus}.
     */
    @DownloadPromptStatus
    public static int getPromptForDownloadAndroid() {
        return PrefServiceBridge.getInstance().getInteger(Pref.PROMPT_FOR_DOWNLOAD_ANDROID);
    }

    /**
     * @param status New status to update the prompt for download preference.
     */
    public static void setPromptForDownloadAndroid(@DownloadPromptStatus int status) {
        PrefServiceBridge.getInstance().setInteger(Pref.PROMPT_FOR_DOWNLOAD_ANDROID, status);
    }

    /**
     * @return The prompt status for download later dialog.
     */
    @DownloadLaterPromptStatus
    public static int getDownloadLaterPromptStatus() {
        return PrefServiceBridge.getInstance().getInteger(Pref.DOWNLOAD_LATER_PROMPT_STATUS);
    }

    /**
     * Sets the prompt status for download later dialog.
     * @param status New status to update the download later prmopt status.
     */
    public static void setDownloadLaterPromptStatus(@DownloadLaterPromptStatus int status) {
        PrefServiceBridge.getInstance().setInteger(Pref.DOWNLOAD_LATER_PROMPT_STATUS, status);
    }

    @NativeMethods
    interface Natives {
        void onComplete(long nativeDownloadDialogBridge, DownloadDialogBridge caller,
                String returnedPath, boolean onWifi, long startTime);
        void onCanceled(long nativeDownloadDialogBridge, DownloadDialogBridge caller);
        String getDownloadDefaultDirectory();
        void setDownloadAndSaveFileDefaultDirectory(String directory);
    }
}
