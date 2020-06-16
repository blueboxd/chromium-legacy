// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.download.dialogs.DownloadLaterDialogChoice;
import org.chromium.chrome.browser.download.dialogs.DownloadLaterDialogController;
import org.chromium.chrome.browser.download.dialogs.DownloadLaterDialogCoordinator;
import org.chromium.chrome.browser.download.dialogs.DownloadLaterDialogProperties;
import org.chromium.chrome.browser.download.dialogs.DownloadLocationDialogController;
import org.chromium.chrome.browser.download.dialogs.DownloadLocationDialogCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Glues download dialogs UI code and handles the communication to download native backend.
 */
public class DownloadDialogBridge
        implements DownloadLocationDialogController, DownloadLaterDialogController {
    private static final long INVALID_START_TIME = -1;
    private long mNativeDownloadDialogBridge;

    private final DownloadLocationDialogCoordinator mLocationDialog;
    private final DownloadLaterDialogCoordinator mDownloadLaterDialog;

    private Activity mActivity;
    private ModalDialogManager mModalDialogManager;
    private long mTotalBytes;
    private @DownloadLocationDialogType int mLocationDialogType;
    private String mSuggestedPath;

    @DownloadLaterDialogChoice
    int mDownloadLaterChoice = DownloadLaterDialogChoice.DOWNLOAD_NOW;

    public DownloadDialogBridge(long nativeDownloadDialogBridge,
            DownloadLaterDialogCoordinator downloadLaterDialog,
            DownloadLocationDialogCoordinator locationDialog) {
        mNativeDownloadDialogBridge = nativeDownloadDialogBridge;
        mDownloadLaterDialog = downloadLaterDialog;
        mLocationDialog = locationDialog;
    }

    @CalledByNative
    private static DownloadDialogBridge create(long nativeDownloadDialogBridge) {
        DownloadLocationDialogCoordinator locationDialog = new DownloadLocationDialogCoordinator();
        DownloadLaterDialogCoordinator downloadLaterDialog = new DownloadLaterDialogCoordinator();
        DownloadDialogBridge bridge = new DownloadDialogBridge(
                nativeDownloadDialogBridge, downloadLaterDialog, locationDialog);
        downloadLaterDialog.initialize(bridge);
        locationDialog.initialize(bridge);
        return bridge;
    }

    @CalledByNative
    void destroy() {
        mNativeDownloadDialogBridge = 0;
        mDownloadLaterDialog.destroy();
        mLocationDialog.destroy();
    }

    @CalledByNative
    private void showDialog(WindowAndroid windowAndroid, long totalBytes,
            @DownloadLocationDialogType int dialogType, String suggestedPath) {
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            onCancel();
            return;
        }

        ModalDialogManager modalDialogManager =
                ((ModalDialogManagerHolder) activity).getModalDialogManager();
        showDialog(activity, modalDialogManager, totalBytes, dialogType, suggestedPath);
    }

    void showDialog(Activity activity, ModalDialogManager modalDialogManager, long totalBytes,
            @DownloadLocationDialogType int dialogType, String suggestedPath) {
        mActivity = activity;
        mModalDialogManager = modalDialogManager;
        mTotalBytes = totalBytes;
        mLocationDialogType = dialogType;
        mSuggestedPath = suggestedPath;

        // Download later dialogs flow.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOAD_LATER)) {
            PropertyModel model =
                    new PropertyModel.Builder(DownloadLaterDialogProperties.ALL_KEYS)
                            .with(DownloadLaterDialogProperties.DOWNLOAD_TIME_INITIAL_SELECTION,
                                    DownloadLaterDialogChoice.DOWNLOAD_NOW)
                            .build();
            mDownloadLaterDialog.showDialog(mActivity, mModalDialogManager, model);
            return;
        }

        // Download location dialog flow.
        mLocationDialog.showDialog(
                mActivity, mModalDialogManager, totalBytes, dialogType, suggestedPath);
    }

    private void onComplete(String returnedPath, boolean onlyOnWifi, long startTime) {
        if (mNativeDownloadDialogBridge == 0) return;
        DownloadDialogBridgeJni.get().onComplete(mNativeDownloadDialogBridge,
                DownloadDialogBridge.this, returnedPath, onlyOnWifi, startTime);
    }

    private void onCancel() {
        if (mNativeDownloadDialogBridge == 0) return;
        DownloadDialogBridgeJni.get().onCanceled(
                mNativeDownloadDialogBridge, DownloadDialogBridge.this);
    }

    // DownloadLaterDialogController implementation.
    @Override
    public void onDownloadLaterDialogComplete(@DownloadLaterDialogChoice int choice) {
        // Cache the result from download later dialog.
        mDownloadLaterChoice = choice;
        mDownloadLaterDialog.dismissDialog(DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

        // Show the next dialog.
        mLocationDialog.showDialog(
                mActivity, mModalDialogManager, mTotalBytes, mLocationDialogType, mSuggestedPath);
    }

    @Override
    public void onDownloadLaterDialogCanceled() {
        onCancel();
    }

    // DownloadLocationDialogController implementation.
    @Override
    public void onDownloadLocationDialogComplete(String returnedPath) {
        boolean onlyOnWifi = false;
        if (mDownloadLaterChoice == DownloadLaterDialogChoice.ON_WIFI) onlyOnWifi = true;
        onComplete(returnedPath, onlyOnWifi, INVALID_START_TIME);
    }

    @Override
    public void onDownloadLocationDialogCanceled() {
        onCancel();
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
