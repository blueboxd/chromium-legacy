// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.download.DownloadLaterPromptStatus;
import org.chromium.chrome.browser.download.R;
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
        implements ModalDialogProperties.Controller, DownloadLaterDialogView.Controller {
    private PropertyModel mDownloadLaterDialogModel;
    private DownloadLaterDialogView mCustomView;
    private ModalDialogManager mModalDialogManager;
    private PropertyModel mDialogModel;
    private PropertyModelChangeProcessor<PropertyModel, DownloadLaterDialogView, PropertyKey>
            mPropertyModelChangeProcessor;

    private DownloadLaterDialogController mController;

    /**
     * Initializes the download location dialog.
     * @param controller Receives events from download location dialog.
     */
    public void initialize(@NonNull DownloadLaterDialogController controller) {
        mController = controller;
    }

    /**
     * Shows the download later dialog.
     * @param activity The activity that provides android {@link Context} to the dialog.
     * @param modalDialogManager {@link ModalDialogManager} to control the dialog.
     * @param model The data model that defines the UI details.
     */
    public void showDialog(
            Activity activity, ModalDialogManager modalDialogManager, PropertyModel model) {
        if (activity == null || modalDialogManager == null) {
            onDismiss(null, DialogDismissalCause.ACTIVITY_DESTROYED);
            return;
        }

        // Set up the download later UI MVC.
        mDownloadLaterDialogModel = model;
        mCustomView = (DownloadLaterDialogView) LayoutInflater.from(activity).inflate(
                R.layout.download_later_dialog, null);
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(mDownloadLaterDialogModel, mCustomView,
                        DownloadLaterDialogView.Binder::bind, true /*performInitialBind*/);

        // Set up the modal dialog.
        mModalDialogManager = modalDialogManager;
        mDialogModel = getModalDialogModel(activity, this);

        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    /**
     * Dismisses the download later dialog.
     * @param dismissalCause The reason to dismiss the dialog, used in metrics tracking.
     */
    public void dismissDialog(@DialogDismissalCause int dismissalCause) {
        mModalDialogManager.dismissDialog(mDialogModel, dismissalCause);
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
            @DownloadLaterDialogChoice
            int choice = (mCustomView == null) ? DownloadLaterDialogChoice.DOWNLOAD_NOW
                                               : mCustomView.getChoice();
            @DownloadLaterPromptStatus
            int promptStatus = (mCustomView == null) ? DownloadLaterPromptStatus.SHOW_INITIAL
                                                     : mCustomView.getPromptStatus();
            assert mController != null;
            mController.onDownloadLaterDialogComplete(choice, promptStatus);
            return;
        }

        assert mController != null;
        mController.onDownloadLaterDialogCanceled();
    }

    @Override
    public void onEditLocationClicked() {
        mController.onEditLocationClicked();
    }
}
