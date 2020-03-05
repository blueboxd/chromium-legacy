// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogPasswordChangeProperties.DETAILS;
import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogPasswordChangeProperties.HELP_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogPasswordChangeProperties.ILLUSTRATION;
import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogPasswordChangeProperties.TITLE;

import android.content.Context;
import android.graphics.Typeface;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.StyleSpan;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.password_manager.PasswordManagerDialogPasswordChangeContents.BoldRange;
import org.chromium.chrome.password_change.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The coordinator for the password manager illustration modal dialog. Manages
 * the sub-component objects.
 */

// TODO (crbug/1058764) Unfork credential leak dialog password change when prototype is done.

public class PasswordManagerDialogPasswordChangeCoordinator {
    private final PasswordManagerDialogPasswordChangeMediator mMediator;
    private PropertyModel mModel;

    public PasswordManagerDialogPasswordChangeCoordinator(ModalDialogManager modalDialogManager,
            View androidContentView, ChromeFullscreenManager fullscreenManager,
            int containerHeightResource) {
        mMediator = new PasswordManagerDialogPasswordChangeMediator(
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS), modalDialogManager,
                androidContentView, fullscreenManager, containerHeightResource);
    }

    public void initialize(Context context, PasswordManagerDialogPasswordChangeContents contents) {
        View customView = LayoutInflater.from(context).inflate(
                R.layout.password_manager_dialog_with_help_button_password_change, null);
        mModel = buildModel(contents);
        mMediator.initialize(mModel, customView, contents);
        PropertyModelChangeProcessor.create(
                mModel, customView, PasswordManagerDialogPasswordChangeViewBinder::bind);
    }

    public void showDialog() {
        mMediator.showDialog();
    }

    public void dismissDialog(@DialogDismissalCause int dismissalCause) {
        mMediator.dismissDialog(dismissalCause);
    }

    private PropertyModel buildModel(PasswordManagerDialogPasswordChangeContents contents) {
        return PasswordManagerDialogPasswordChangeProperties.defaultModelBuilder()
                .with(TITLE, contents.getTitle())
                .with(DETAILS,
                        addBoldSpanToDetails(contents.getDetails(), contents.getBoldRanges()))
                .with(ILLUSTRATION, contents.getIllustrationId())
                .with(HELP_BUTTON_CALLBACK, contents.getHelpButtonCallback())
                .build();
    }

    private SpannableString addBoldSpanToDetails(String details, BoldRange[] boldRanges) {
        SpannableString spannableDetails = new SpannableString(details);
        for (BoldRange boldRange : boldRanges) {
            StyleSpan boldSpan = new StyleSpan(Typeface.BOLD);
            spannableDetails.setSpan(
                    boldSpan, boldRange.start, boldRange.end, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        }
        return spannableDetails;
    }

    @VisibleForTesting
    public PasswordManagerDialogPasswordChangeMediator getMediatorForTesting() {
        return mMediator;
    }
}
