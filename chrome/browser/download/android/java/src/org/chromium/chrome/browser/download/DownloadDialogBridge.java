// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.download.dialogs.DownloadDateTimePickerDialogCoordinator;
import org.chromium.chrome.browser.download.dialogs.DownloadDateTimePickerDialogProperties;
import org.chromium.chrome.browser.download.dialogs.DownloadDateTimePickerDialogProperties.State;
import org.chromium.chrome.browser.download.dialogs.DownloadLaterDialogChoice;
import org.chromium.chrome.browser.download.dialogs.DownloadLaterDialogController;
import org.chromium.chrome.browser.download.dialogs.DownloadLaterDialogCoordinator;
import org.chromium.chrome.browser.download.dialogs.DownloadLaterDialogProperties;
import org.chromium.chrome.browser.download.dialogs.DownloadLocationDialogController;
import org.chromium.chrome.browser.download.dialogs.DownloadLocationDialogCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Glues download dialogs UI code and handles the communication to download native backend.
 * When {@link ChromeFeatureList#DOWNLOAD_LATER} is enabled, the following dialogs will be shown in
 * a sequence.
 * Download later dialog ==> (optional) Download date time picker ==> Download location dialog
 * When {@link ChromeFeatureList#DOWNLOAD_LATER} is disabled, only the download location dialog will
 * be shown.
 */
public class DownloadDialogBridge implements DownloadLocationDialogController,
                                             DownloadLaterDialogController,
                                             DownloadDateTimePickerDialogCoordinator.Controller {
    private static final long INVALID_START_TIME = -1;
    private long mNativeDownloadDialogBridge;

    private final DownloadLocationDialogCoordinator mLocationDialog;
    private final DownloadLaterDialogCoordinator mDownloadLaterDialog;
    private final DownloadDateTimePickerDialogCoordinator mDateTimePickerDialog;

    private Activity mActivity;
    private ModalDialogManager mModalDialogManager;
    private long mTotalBytes;
    private @DownloadLocationDialogType int mLocationDialogType;
    private String mSuggestedPath;
    private PrefService mPrefService;

    @DownloadLaterDialogChoice
    private int mDownloadLaterChoice = DownloadLaterDialogChoice.DOWNLOAD_NOW;
    @DownloadLaterPromptStatus
    private int mDownloadLaterPromptStatus = DownloadLaterPromptStatus.SHOW_INITIAL;
    private long mDownloadLaterTime = INVALID_START_TIME;

    public DownloadDialogBridge(long nativeDownloadDialogBridge,
            DownloadLaterDialogCoordinator downloadLaterDialog,
            DownloadDateTimePickerDialogCoordinator dateTimePickerDialog,
            DownloadLocationDialogCoordinator locationDialog) {
        mNativeDownloadDialogBridge = nativeDownloadDialogBridge;
        mDownloadLaterDialog = downloadLaterDialog;
        mDateTimePickerDialog = dateTimePickerDialog;
        mLocationDialog = locationDialog;
    }

    @CalledByNative
    private static DownloadDialogBridge create(long nativeDownloadDialogBridge) {
        DownloadLocationDialogCoordinator locationDialog = new DownloadLocationDialogCoordinator();
        DownloadLaterDialogCoordinator downloadLaterDialog = new DownloadLaterDialogCoordinator();
        DownloadDateTimePickerDialogCoordinator dateTimePickerDialog =
                new DownloadDateTimePickerDialogCoordinator();
        DownloadDialogBridge bridge = new DownloadDialogBridge(nativeDownloadDialogBridge,
                downloadLaterDialog, dateTimePickerDialog, locationDialog);
        downloadLaterDialog.initialize(bridge);
        dateTimePickerDialog.initialize(bridge);
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
            @DownloadLocationDialogType int dialogType, String suggestedPath,
            boolean supportsLaterDialog) {
        if (mPrefService == null) mPrefService = getPrefService();
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            onCancel();
            return;
        }

        ModalDialogManager modalDialogManager =
                ((ModalDialogManagerHolder) activity).getModalDialogManager();
        showDialog(activity, modalDialogManager, totalBytes, dialogType, suggestedPath,
                supportsLaterDialog);
    }

    void showDialog(Activity activity, ModalDialogManager modalDialogManager, long totalBytes,
            @DownloadLocationDialogType int dialogType, String suggestedPath,
            boolean supportsLaterDialog) {
        mActivity = activity;
        mModalDialogManager = modalDialogManager;
        mTotalBytes = totalBytes;
        mLocationDialogType = dialogType;
        mSuggestedPath = suggestedPath;

        mDownloadLaterChoice = DownloadLaterDialogChoice.DOWNLOAD_NOW;
        mDownloadLaterTime = INVALID_START_TIME;

        // Download later dialogs flow.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOAD_LATER) && supportsLaterDialog) {
            assert mPrefService != null;
            @DownloadLaterPromptStatus
            int promptStatus = mPrefService.getInteger(Pref.DOWNLOAD_LATER_PROMPT_STATUS);

            PropertyModel model =
                    new PropertyModel.Builder(DownloadLaterDialogProperties.ALL_KEYS)
                            .with(DownloadLaterDialogProperties.CONTROLLER, mDownloadLaterDialog)
                            .with(DownloadLaterDialogProperties.DOWNLOAD_TIME_INITIAL_SELECTION,
                                    DownloadLaterDialogChoice.DOWNLOAD_NOW)
                            .with(DownloadLaterDialogProperties.DONT_SHOW_AGAIN_SELECTION,
                                    promptStatus)
                            .with(DownloadLaterDialogProperties.LOCATION_TEXT,
                                    activity.getResources().getString(R.string.menu_downloads))
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

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOAD_LATER)
                && mDownloadLaterPromptStatus != DownloadLaterPromptStatus.SHOW_INITIAL) {
            assert mPrefService != null;
            mPrefService.setInteger(Pref.DOWNLOAD_LATER_PROMPT_STATUS, mDownloadLaterPromptStatus);
        }

        DownloadDialogBridgeJni.get().onComplete(mNativeDownloadDialogBridge,
                DownloadDialogBridge.this, returnedPath, onlyOnWifi, startTime);
    }

    private void onCancel() {
        if (mNativeDownloadDialogBridge == 0) return;
        DownloadDialogBridgeJni.get().onCanceled(
                mNativeDownloadDialogBridge, DownloadDialogBridge.this);
    }

    private void showLocationDialog() {
        mLocationDialog.showDialog(
                mActivity, mModalDialogManager, mTotalBytes, mLocationDialogType, mSuggestedPath);
    }

    // DownloadLaterDialogController implementation.
    @Override
    public void onDownloadLaterDialogComplete(
            @DownloadLaterDialogChoice int choice, @DownloadPromptStatus int promptStatus) {
        // Cache the result from download later dialog.
        mDownloadLaterChoice = choice;
        mDownloadLaterPromptStatus = promptStatus;

        mDownloadLaterDialog.dismissDialog(DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

        // Let the user to pick date and time if they choose to download later.
        if (mDownloadLaterChoice == DownloadLaterDialogChoice.DOWNLOAD_LATER) {
            long now = System.currentTimeMillis();
            // TODO(xingliu): Round up default time to next hour from now.
            PropertyModel model =
                    new PropertyModel.Builder(DownloadDateTimePickerDialogProperties.ALL_KEYS)
                            .with(DownloadDateTimePickerDialogProperties.STATE, State.DATE)
                            .with(DownloadDateTimePickerDialogProperties.INITIAL_TIME, now)
                            .with(DownloadDateTimePickerDialogProperties.MIN_TIME, now)
                            .build();
            mDateTimePickerDialog.showDialog(mActivity, mModalDialogManager, model);
            return;
        }

        // For non download later options, show the next dialog.
        showLocationDialog();
    }

    @Override
    public void onDownloadLaterDialogCanceled() {
        onCancel();
    }

    @Override
    public void onEditLocationClicked() {
        // TODO(xingliu): Trigger download location dialog. After it's done, come back to this flow.
    }

    // DownloadDateTimePickerDialogCoordinator.Controller.
    @Override
    public void onDateTimePicked(long time) {
        mDownloadLaterTime = time;

        // Show the next dialog.
        showLocationDialog();
    }

    @Override
    public void onDateTimePickerCanceled() {
        onCancel();
    }

    // DownloadLocationDialogController implementation.
    @Override
    public void onDownloadLocationDialogComplete(String returnedPath) {
        boolean onlyOnWifi = false;
        if (mDownloadLaterChoice == DownloadLaterDialogChoice.ON_WIFI) onlyOnWifi = true;
        onComplete(returnedPath, onlyOnWifi, mDownloadLaterTime);
    }

    @Override
    public void onDownloadLocationDialogCanceled() {
        onCancel();
    }

    void setPrefServiceForTesting(PrefService prefService) {
        mPrefService = prefService;
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
        return getPrefService().getInteger(Pref.PROMPT_FOR_DOWNLOAD_ANDROID);
    }

    /**
     * @param status New status to update the prompt for download preference.
     */
    public static void setPromptForDownloadAndroid(@DownloadPromptStatus int status) {
        getPrefService().setInteger(Pref.PROMPT_FOR_DOWNLOAD_ANDROID, status);
    }

    private static PrefService getPrefService() {
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
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
