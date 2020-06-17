// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.app.Activity;
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

import java.util.concurrent.TimeUnit;

/**
 * The coordinator for download date time picker. The user can pick an exact time to start the
 * download later. The dialog has two stage:
 * 1. A calendar to let the user to pick the date.
 * 2. A clock to let the user to pick a time.
 */
public class DownloadDateTimePickerDialogCoordinator implements ModalDialogProperties.Controller {
    /** The maximum time that the user can select in the dialog.*/
    public static final long MAX_TIME = TimeUnit.DAYS.toMillis(7); /* 7 days */
    /**
     * The controller that receives events from the date time picker.
     */
    public interface Controller {
        /**
         * Called when the user picked the time from date picker and time picker.
         * @param time The time the user picked as a unix timestamp.
         */
        void onDateTimePicked(long time);

        /**
         * The user canceled date time picking flow.
         */
        void onDateTimePickerCanceled();
    }

    private Controller mController;

    private ModalDialogManager mModalDialogManager;

    private PropertyModel mModel;

    private DownloadDateTimePickerView mView;
    private PropertyModelChangeProcessor mProcessor;

    /**
     * Initializes the download date time picker dialog.
     * @param controller The controller that receives events from the date time picker.
     */
    public void initialize(@NonNull Controller controller) {
        mController = controller;
    }

    /**
     * Shows the date time picker.
     * @param windowAndroid The window android handle that provides contexts.
     * @param model The model that defines the application data used to update the UI view.
     */
    public void showDialog(
            Activity activity, ModalDialogManager modalDialogManager, PropertyModel model) {
        if (activity == null || modalDialogManager == null) {
            onDismiss(null, DialogDismissalCause.ACTIVITY_DESTROYED);
            return;
        }

        mModalDialogManager = modalDialogManager;
        mModel = model;
        mView = (DownloadDateTimePickerView) LayoutInflater.from(activity).inflate(
                R.layout.download_later_date_time_picker_dialog, null);
        mProcessor = PropertyModelChangeProcessor.create(mModel, mView,
                DownloadDateTimePickerView.Binder::bind, true /*performInitialBind*/);
        mModalDialogManager.showDialog(
                getModalDialogModel(activity), ModalDialogManager.ModalDialogType.APP);
    }

    /**
     * Destroys the download date time picker dialog.
     */
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
