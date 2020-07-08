// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.content.Context;
import android.view.LayoutInflater;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.download.dialogs.DownloadDateTimePickerDialogProperties.State;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.prefs.PrefService;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator to construct the download later dialog.
 */
public class DownloadLaterDialogCoordinator
        implements ModalDialogProperties.Controller, DownloadLaterDialogView.Controller,
                   DownloadDateTimePickerDialogCoordinator.Controller {
    private static final long INVALID_START_TIME = -1;
    private PropertyModel mDownloadLaterDialogModel;
    private DownloadLaterDialogView mCustomView;

    private Context mContext;
    private ModalDialogManager mModalDialogManager;
    private PrefService mPrefService;

    private PropertyModel mDialogModel;
    private PropertyModelChangeProcessor<PropertyModel, DownloadLaterDialogView, PropertyKey>
            mPropertyModelChangeProcessor;

    private DownloadLaterDialogController mController;
    private final DownloadDateTimePickerDialogCoordinator mDateTimePickerDialog;

    @DownloadLaterDialogChoice
    private int mDownloadLaterChoice = DownloadLaterDialogChoice.DOWNLOAD_NOW;

    /**
     * Creates the {@link DownloadLaterDialogCoordinator}.
     * @param dateTimePickerDialog The date time selection widget.
     */
    public DownloadLaterDialogCoordinator(
            @NonNull DownloadDateTimePickerDialogCoordinator dateTimePickerDialog) {
        mDateTimePickerDialog = dateTimePickerDialog;
    }

    /**
     * Initializes the download location dialog.
     * @param controller Receives events from download location dialog.
     */
    public void initialize(@NonNull DownloadLaterDialogController controller) {
        mController = controller;
    }

    /**
     * Shows the download later dialog.
     * @param context The {@link Context} for the dialog.
     * @param modalDialogManager {@link ModalDialogManager} to control the dialog.
     * @param prefService {@link PrefService} to write download later prompt status preference.
     * @param model The data model that defines the UI details.
     */
    public void showDialog(Context context, ModalDialogManager modalDialogManager,
            PrefService prefService, PropertyModel model) {
        if (context == null || modalDialogManager == null) {
            onDismiss(null, DialogDismissalCause.ACTIVITY_DESTROYED);
            return;
        }

        mContext = context;
        mModalDialogManager = modalDialogManager;
        mPrefService = prefService;

        // Set up the download later UI MVC.
        mDownloadLaterDialogModel = model;
        mCustomView = (DownloadLaterDialogView) LayoutInflater.from(context).inflate(
                R.layout.download_later_dialog, null);
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(mDownloadLaterDialogModel, mCustomView,
                        DownloadLaterDialogView.Binder::bind, true /*performInitialBind*/);
        mDownloadLaterChoice =
                model.get(DownloadLaterDialogProperties.DOWNLOAD_TIME_INITIAL_SELECTION);

        // Set up the modal dialog.
        mDialogModel = getModalDialogModel(context, this);
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    /**
     * Dismisses the download later dialog.
     * @param dismissalCause The reason to dismiss the dialog, used in metrics tracking.
     */
    public void dismissDialog(@DialogDismissalCause int dismissalCause) {
        mModalDialogManager.dismissDialog(mDialogModel, dismissalCause);
    }

    public @DownloadLaterDialogChoice int getChoice() {
        return mDownloadLaterChoice;
    }

    /**
     * Destroy the download later dialog.
     */
    public void destroy() {
        if (mPropertyModelChangeProcessor != null) {
            mPropertyModelChangeProcessor.destroy();
        }

        if (mModalDialogManager != null) {
            mModalDialogManager.dismissDialog(
                    mDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
        }

        mDateTimePickerDialog.destroy();
    }

    private PropertyModel getModalDialogModel(
            Context context, ModalDialogProperties.Controller modalDialogController) {
        assert mCustomView != null;
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, modalDialogController)
                .with(ModalDialogProperties.CUSTOM_VIEW, mCustomView)
                .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, context.getResources(),
                        R.string.duplicate_download_infobar_download_button)
                .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, context.getResources(),
                        R.string.cancel)
                .build();
    }

    private void onPositiveButtonClicked(@DownloadLaterDialogChoice int choice) {
        mDownloadLaterChoice = choice;

        // Immediately show the date time picker when selecting the "Download later".
        if (choice == DownloadLaterDialogChoice.DOWNLOAD_LATER) {
            dismissDialog(DialogDismissalCause.ACTION_ON_CONTENT);
            showDateTimePicker();
            return;
        }

        // The user select "Download now" or "On wifi", no time is selected.
        notifyComplete(INVALID_START_TIME);
    }

    private void showDateTimePicker() {
        long now = System.currentTimeMillis();
        // TODO(xingliu): Round up default time to next hour from now.
        PropertyModel model =
                new PropertyModel.Builder(DownloadDateTimePickerDialogProperties.ALL_KEYS)
                        .with(DownloadDateTimePickerDialogProperties.STATE, State.DATE)
                        .with(DownloadDateTimePickerDialogProperties.INITIAL_TIME, now)
                        .with(DownloadDateTimePickerDialogProperties.MIN_TIME, now)
                        .build();

        mDateTimePickerDialog.showDialog(mContext, mModalDialogManager, model);
    }

    private void notifyComplete(long time) {
        assert mController != null;
        updatePromptStatus();
        mController.onDownloadLaterDialogComplete(mDownloadLaterChoice, time);
    }

    private void notifyCancel() {
        assert mController != null;
        updatePromptStatus();
        mController.onDownloadLaterDialogCanceled();
    }

    private void updatePromptStatus() {
        assert mCustomView != null;
        assert mPrefService != null;
        Integer promptStatus = mCustomView.getPromptStatus();
        if (promptStatus != null) {
            mPrefService.setInteger(
                    Pref.DOWNLOAD_LATER_PROMPT_STATUS, mCustomView.getPromptStatus());
        }
    }

    // ModalDialogProperties.Controller implementation.
    @Override
    public void onClick(PropertyModel model, int buttonType) {
        switch (buttonType) {
            case ModalDialogProperties.ButtonType.POSITIVE:
                mModalDialogManager.dismissDialog(
                        model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                break;
            case ModalDialogProperties.ButtonType.NEGATIVE:
                mModalDialogManager.dismissDialog(
                        model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                break;
            default:
        }
    }

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
            onPositiveButtonClicked(mDownloadLaterChoice);
            return;
        }

        // Temporary dismiss due to the user clicking the "Edit" to open download location dialog.
        if (dismissalCause == DialogDismissalCause.ACTION_ON_CONTENT) return;

        notifyCancel();
    }

    // DownloadDateTimePickerDialogCoordinator.Controller implementation.
    @Override
    public void onDateTimePicked(long time) {
        notifyComplete(time);
    }

    @Override
    public void onDateTimePickerCanceled() {
        notifyCancel();
    }

    // DownloadLaterDialogView.Controller.
    @Override
    public void onEditLocationClicked() {
        // Ask the controller to decide what to do, even though we can dismiss ourselves here.
        assert mController != null;
        mController.onEditLocationClicked();
    }

    @Override
    public void onCheckedChanged(int choice) {
        mDownloadLaterChoice = choice;
    }
}
