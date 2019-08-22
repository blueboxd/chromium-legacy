// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.support.annotation.DrawableRes;
import android.support.annotation.NonNull;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The coordinator for the password manager illustration modal dialog. Manages the sub-component
 * objects.
 */
public class PasswordManagerDialogCoordinator {
    private final PasswordManagerDialogMediator mMediator;

    PasswordManagerDialogCoordinator(@NonNull ChromeActivity activity) {
        PropertyModel mModel = PasswordManagerDialogProperties.defaultModelBuilder().build();
        View customView =
                LayoutInflater.from(activity).inflate(R.layout.password_manager_dialog, null);
        mMediator = new PasswordManagerDialogMediator(
                mModel, activity.getModalDialogManager(), createDialogModelBuilder(customView));
        PropertyModelChangeProcessor.create(
                mModel, customView, PasswordManagerDialogViewBinder::bind);
    }

    public void showDialog(String title, String details, @DrawableRes int drawableId,
            String positiveButtonText, String negativeButtonText, Callback<Boolean> onClick) {
        mMediator.setContents(title, details, drawableId);
        mMediator.setButtons(positiveButtonText, negativeButtonText, onClick);
        mMediator.showDialog();
    }

    public void dismissDialog(@DialogDismissalCause int dismissalClause) {
        mMediator.dismissDialog(dismissalClause);
    }

    private static PropertyModel.Builder createDialogModelBuilder(View customView) {
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CUSTOM_VIEW, customView);
    }
}
