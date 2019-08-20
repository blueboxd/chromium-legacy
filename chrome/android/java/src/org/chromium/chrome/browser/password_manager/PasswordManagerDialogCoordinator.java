// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.content.res.Resources;
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

    public void showDialog(
            String title, String details, int drawableId, Callback<Boolean> onClick) {
        mMediator.setContents(title, details, drawableId);
        mMediator.showDialog(onClick);
    }

    public void dismissDialog(@DialogDismissalCause int dismissalClause) {
        mMediator.dismissDialog(dismissalClause);
    }

    private static PropertyModel.Builder createDialogModelBuilder(View customView) {
        Resources resources = customView.getResources();
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources,
                        R.string.continue_button)
                .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources, R.string.cancel);
    }
}
