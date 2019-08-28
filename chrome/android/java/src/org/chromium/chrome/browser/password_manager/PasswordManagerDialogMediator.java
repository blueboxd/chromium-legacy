// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogProperties.DETAILS;
import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogProperties.ILLUSTRATION;
import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogProperties.ILLUSTRATION_VISIBLE;
import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogProperties.TITLE;

import android.content.res.Resources;
import android.support.annotation.DrawableRes;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.modaldialog.TabModalPresenter;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator class responsible for the logic of showing the password manager dialog (e.g. onboarding
 * dialog).
 */
class PasswordManagerDialogMediator implements View.OnLayoutChangeListener {
    private final PropertyModel mModel;
    private final ModalDialogManager mDialogManager;
    private PropertyModel.Builder mModalDialogBuilder;
    private PropertyModel mDialogModel;
    private final View mAndroidContentView;
    private final Resources mResources;
    private final ChromeFullscreenManager mFullscreenManager;
    private final int mContainerHeightResource;

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

    PasswordManagerDialogMediator(PropertyModel model, PropertyModel.Builder dialogBuilder,
            ModalDialogManager manager, View androidContentView, Resources resources,
            ChromeFullscreenManager fullscreenManager, int containerHeightResource) {
        mModel = model;
        mDialogManager = manager;
        mModalDialogBuilder = dialogBuilder;
        mAndroidContentView = androidContentView;
        mResources = resources;
        mFullscreenManager = fullscreenManager;
        mContainerHeightResource = containerHeightResource;
        mAndroidContentView.addOnLayoutChangeListener(this);
    }

    void setContents(String title, String details, @DrawableRes int drawableId) {
        mModel.set(ILLUSTRATION, drawableId);
        mModel.set(TITLE, title);
        mModel.set(DETAILS, details);
    }

    void setButtons(
            String positiveButtonText, String negativeButtonText, Callback<Boolean> onClick) {
        mModalDialogBuilder.with(ModalDialogProperties.CONTROLLER, new DialogClickHandler(onClick))
                .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, positiveButtonText)
                .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, negativeButtonText);
    }

    private boolean hasSufficientSpaceForIllustration(int heightPx) {
        heightPx -= TabModalPresenter.getContainerTopMargin(mResources, mContainerHeightResource);
        heightPx -= TabModalPresenter.getContainerBottomMargin(mFullscreenManager);
        return heightPx >= mResources.getDimensionPixelSize(
                       R.dimen.password_manager_dialog_min_vertical_space_to_show_illustration);
    }

    @Override
    public void onLayoutChange(View view, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        int oldHeight = oldBottom - oldTop;
        int newHeight = bottom - top;
        if (newHeight == oldHeight) return;
        mModel.set(ILLUSTRATION_VISIBLE, hasSufficientSpaceForIllustration(newHeight));
    }

    void showDialog() {
        mModel.set(ILLUSTRATION_VISIBLE,
                hasSufficientSpaceForIllustration(mAndroidContentView.getHeight()));
        mDialogModel = mModalDialogBuilder.build();
        mDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.TAB);
    }

    void dismissDialog(int dismissalClause) {
        mDialogManager.dismissDialog(mDialogModel, dismissalClause);
        mAndroidContentView.removeOnLayoutChangeListener(this);
    }

    @VisibleForTesting
    public PropertyModel getModelForTesting() {
        return mModel;
    }
}
