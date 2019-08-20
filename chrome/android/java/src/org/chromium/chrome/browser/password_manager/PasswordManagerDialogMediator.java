// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogProperties.DETAILS;
import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogProperties.ILLUSTRATION;
import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogProperties.TITLE;

import org.chromium.base.Callback;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator class responsible for the logic of showing the password manager dialog (e.g. onboarding
 * dialog).
 */
class PasswordManagerDialogMediator {
    private final PropertyModel mModel;
    private final ModalDialogManager mDialogManager;
    private PropertyModel.Builder mModalDialogBuilder;
    private PropertyModel mDialogModel;

    private static class DialogClickHandler implements ModalDialogProperties.Controller {
        private Callback<Boolean> mCallback;

        DialogClickHandler(Callback<Boolean> onClick) {
            mCallback = onClick;
        }

        @Override
        public void onClick(PropertyModel model, int buttonType) {
            switch (buttonType) {
                case ModalDialogProperties.ButtonType.POSITIVE:
                    mCallback.onResult(true);
                    break;
                case ModalDialogProperties.ButtonType.NEGATIVE:
                    mCallback.onResult(false);
                    break;
                default:
                    assert false : "Unexpected button pressed in dialog: " + buttonType;
            }
        }

        @Override
        public void onDismiss(PropertyModel model, int dismissalCause) {
            mCallback.onResult(false);
        }
    }

    PasswordManagerDialogMediator(
            PropertyModel model, ModalDialogManager manager, PropertyModel.Builder dialogBuilder) {
        mModel = model;
        mDialogManager = manager;
        mModalDialogBuilder = dialogBuilder;
    }

    void setContents(String title, String details, int drawableId) {
        mModel.set(ILLUSTRATION, drawableId);
        mModel.set(TITLE, title);
        mModel.set(DETAILS, details);
    }

    void showDialog(Callback<Boolean> onClick) {
        mDialogModel =
                mModalDialogBuilder
                        .with(ModalDialogProperties.CONTROLLER, new DialogClickHandler(onClick))
                        .build();
        mDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.TAB);
    }

    void dismissDialog(int dismissalClause) {
        mDialogManager.dismissDialog(mDialogModel, dismissalClause);
    }
}
