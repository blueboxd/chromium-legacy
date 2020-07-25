// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.content.Context;
import android.view.LayoutInflater;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.download.dialogs.DownloadDateTimePickerDialogProperties.State;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * A {@link DownloadDateTimePickerDialog} that uses Clank modal dialog and embeds the date picker
 * and time picker view.
 */
// TODO(xingliu): Remove this.
public class DownloadDateTimePickerDialogCoordinator
        implements DownloadDateTimePickerDialog, ModalDialogProperties.Controller {
    private DownloadDateTimePickerDialog.Controller mController;

    private ModalDialogManager mModalDialogManager;

    private PropertyModel mModel;

    private DownloadDateTimePickerView mView;
    private PropertyModelChangeProcessor mProcessor;

    // DownloadDateTimePickerDialog implementation.
    @Override
    public void initialize(@NonNull DownloadDateTimePickerDialog.Controller controller) {
        mController = controller;
    }

    @Override
    public void showDialog(
            Context context, ModalDialogManager modalDialogManager, PropertyModel model) {
        if (context == null || modalDialogManager == null) {
            onDismiss(null, DialogDismissalCause.ACTIVITY_DESTROYED);
            return;
        }

        mModalDialogManager = modalDialogManager;
        mModel = model;
        mView = (DownloadDateTimePickerView) LayoutInflater.from(context).inflate(
                R.layout.download_later_date_time_picker_dialog, null);
        mProcessor = PropertyModelChangeProcessor.create(mModel, mView,
                DownloadDateTimePickerView.Binder::bind, true /*performInitialBind*/);
        mModalDialogManager.showDialog(
                getModalDialogModel(context), ModalDialogManager.ModalDialogType.APP);
    }

    @Override
    public void destroy() {
        if (mProcessor != null) mProcessor.destroy();
    }

    // ModalDialogProperties.Controller implementation.
    @Override
    public void onClick(PropertyModel model, int buttonType) {
        switch (buttonType) {
            case ModalDialogProperties.ButtonType.POSITIVE:
                @State
                int state = mModel.get(DownloadDateTimePickerDialogProperties.STATE);
                if (state == State.DATE) {
                    mModel.set(DownloadDateTimePickerDialogProperties.STATE, State.TIME);
                } else if (state == State.TIME) {
                    mModalDialogManager.dismissDialog(
                            model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                }

                break;
            case ModalDialogProperties.ButtonType.NEGATIVE:
                mModalDialogManager.dismissDialog(
                        model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                break;
            default:
        }
    }

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {
        assert mController != null;
        if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
            // TODO(xingliu): Handle edge cases that the time is before the current time.
            mController.onDateTimePicked(mView.getTime());
            return;
        }

        mController.onDateTimePickerCanceled();
    }

    private PropertyModel getModalDialogModel(Context context) {
        assert mView != null;
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, this)
                .with(ModalDialogProperties.CUSTOM_VIEW, mView)
                .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, context.getResources(),
                        R.string.download_date_time_picker_next_text)
                .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, context.getResources(),
                        R.string.cancel)
                .build();
    }
}
