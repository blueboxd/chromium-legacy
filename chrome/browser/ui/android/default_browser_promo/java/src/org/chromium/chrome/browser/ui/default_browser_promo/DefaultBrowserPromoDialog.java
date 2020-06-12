// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import android.app.Activity;
import android.content.DialogInterface;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.components.browser_ui.widget.PromoDialog;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The promo dialog guiding how to set Chrome as the default browser.
 */
public class DefaultBrowserPromoDialog extends PromoDialog {
    @IntDef({DialogStyle.ROLE_MANAGER, DialogStyle.DISAMBIGUATION_SHEET,
            DialogStyle.SYSTEM_SETTINGS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DialogStyle {
        int ROLE_MANAGER = 0;
        int DISAMBIGUATION_SHEET = 1;
        int SYSTEM_SETTINGS = 2;
    }

    private final int mDialogStyle;
    private final Runnable mOnOK;
    private Runnable mOnCancel;

    /**
     * Building a {@link DefaultBrowserPromoDialog}.
     * @param activity The activity to display dialog.
     * @param dialogStyle The type of dialog.
     * @param onOK The {@link Runnable} on user's agreeing to change default.
     * @param onCancel The {@link Runnable} on user's refusing or dismissing the dialog.
     * @return
     */
    public static DefaultBrowserPromoDialog createDialog(
            Activity activity, @DialogStyle int dialogStyle, Runnable onOK, Runnable onCancel) {
        return new DefaultBrowserPromoDialog(activity, dialogStyle, onOK, onCancel);
    }

    private DefaultBrowserPromoDialog(
            Activity activity, @DialogStyle int style, Runnable onOK, Runnable onCancel) {
        super(activity);
        mDialogStyle = style;
        mOnOK = onOK;
        mOnCancel = onCancel;
        setOnDismissListener(this);
    }

    @Override
    @VisibleForTesting
    public DialogParams getDialogParams() {
        DialogParams params = new DialogParams();

        Activity activity = getOwnerActivity();
        assert activity != null;

        String appName = BuildInfo.getInstance().hostPackageLabel;
        params.vectorDrawableResource = R.drawable.default_browser_promo_illustration;
        params.headerCharSequence =
                activity.getString(R.string.default_browser_promo_dialog_title, appName);
        String desc =
                activity.getString(R.string.default_browser_promo_dialog_desc, appName) + "\n\n";
        String steps;
        String primaryButtonText;
        if (mDialogStyle == DialogStyle.ROLE_MANAGER) {
            steps = activity.getString(
                    R.string.default_browser_promo_dialog_role_manager_steps, appName);
            primaryButtonText = activity.getString(
                    R.string.default_browser_promo_dialog_choose_chrome_button, appName);
        } else if (mDialogStyle == DialogStyle.DISAMBIGUATION_SHEET) {
            steps = activity.getString(
                    R.string.default_browser_promo_dialog_disambiguation_sheet_steps, appName);
            primaryButtonText = activity.getString(
                    R.string.default_browser_promo_dialog_choose_chrome_button, appName);
        } else {
            assert mDialogStyle == DialogStyle.SYSTEM_SETTINGS;
            steps = activity.getString(
                    R.string.default_browser_promo_dialog_system_settings_steps, appName);
            primaryButtonText =
                    activity.getString(R.string.default_browser_promo_dialog_go_to_settings_button);
        }
        params.subheaderCharSequence = desc + steps;
        params.primaryButtonCharSequence = primaryButtonText;
        params.secondaryButtonStringResource = R.string.no_thanks;
        return params;
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        // Can be dismissed by pressing the back button.
        if (mOnCancel != null) mOnCancel.run();
    }

    @Override
    public void onClick(View view) {
        super.onClick(view);
        int id = view.getId();
        if (id == R.id.button_primary) {
            mOnCancel = null;
            mOnOK.run();
            dismiss();
        } else if (id == R.id.button_secondary) {
            if (mOnCancel != null) mOnCancel.run();
            mOnCancel = null;
            dismiss();
        }
    }

    @VisibleForTesting
    public int getDialogStyleForTesting() {
        return mDialogStyle;
    }
}
