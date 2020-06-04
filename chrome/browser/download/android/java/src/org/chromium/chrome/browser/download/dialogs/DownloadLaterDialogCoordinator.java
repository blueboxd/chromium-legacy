// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;

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
public class DownloadLaterDialogCoordinator {
    private final PropertyModel mDownloadLaterDialogModel;
    private final DownloadLaterDialogView mCustomView;
    private final ModalDialogManager mModalDialogManager;
    private final PropertyModel mDialogModel;
    private final ModalDialogProperties.Controller mController;
    private final PropertyModelChangeProcessor<PropertyModel, DownloadLaterDialogView, PropertyKey>
            mPropertyModelChangeProcessor;

    /**
     * The coordinator that bridges the download later diaog view logic to multiple data models.
     * @param activity The activity mainly to provide the {@link ModalDialogManager}.
     */
    public DownloadLaterDialogCoordinator(Activity activity, ModalDialogManager modalDialogManager,
            ModalDialogProperties.Controller controller) {
        // Set up the download later UI MVC.
        mDownloadLaterDialogModel = new PropertyModel(DownloadLaterDialogProperties.ALL_KEYS);
        mCustomView = (DownloadLaterDialogView) LayoutInflater.from(activity).inflate(
                R.layout.download_later_dialog, null);
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(mDownloadLaterDialogModel, mCustomView,
                        DownloadLaterDialogView.Binder::bind, true /*performInitialBind*/);

        // Set up the modal dialog.
        mModalDialogManager = modalDialogManager;
        mController = controller;
        mDialogModel = getModalDialogModel(activity);
    }

    /**
     * Shows the download later dialog.
     */
    public void showDialog() {
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
        mPropertyModelChangeProcessor.destroy();
    }

    private PropertyModel getModalDialogModel(Context context) {
        assert mController != null;
        assert mCustomView != null;
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, mController)
                .with(ModalDialogProperties.CUSTOM_VIEW, mCustomView)
                .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, context.getResources(),
                        R.string.duplicate_download_infobar_download_button)
                .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, context.getResources(),
                        R.string.cancel)
                .build();
    }
}
