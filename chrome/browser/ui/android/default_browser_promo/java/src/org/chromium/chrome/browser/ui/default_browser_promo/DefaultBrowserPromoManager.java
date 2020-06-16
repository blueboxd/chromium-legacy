// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.role.RoleManager;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.provider.Settings;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Manage all types of default browser promo dialogs and listen to the activity state change to
 * trigger dialogs.
 */
public class DefaultBrowserPromoManager {
    private static final String SKIP_PRIMER_PARAM = "skip_primer";

    private final Activity mActivity;
    private DefaultBrowserPromoDialog mDialog;

    /**
     * @param activity Activity to show promo dialogs.
     * @return A {@link DefaultBrowserPromoManager} displaying dialogs based on android version and
     *         current default browser state in system.
     */
    public static DefaultBrowserPromoManager create(Activity activity) {
        return new DefaultBrowserPromoManager(activity);
    }

    private DefaultBrowserPromoManager(Activity activity) {
        mActivity = activity;
    }

    /**
     * @param state The current {@link DefaultBrowserPromoUtils.DefaultBrowserState} in system.
     */
    public void promo(@DefaultBrowserPromoUtils.DefaultBrowserState int state) {
        promoInternal(state, Build.VERSION.SDK_INT);
    }

    private void promoInternal(
            @DefaultBrowserPromoUtils.DefaultBrowserState int state, int sdkInt) {
        if (sdkInt >= Build.VERSION_CODES.Q) {
            promoByRoleManager();
        } else if (state == DefaultBrowserPromoUtils.DefaultBrowserState.NO_DEFAULT) {
            promoByDisambiguationSheet();
        } else if (sdkInt >= Build.VERSION_CODES.M) {
            promoBySystemSettings();
        }
    }

    @SuppressLint({"WrongConstant", "NewApi"})
    private void promoByRoleManager() {
        Runnable onOK = () -> {
            RoleManager roleManager =
                    (RoleManager) mActivity.getSystemService(Context.ROLE_SERVICE);
            boolean isRoleAvailable = roleManager.isRoleAvailable(RoleManager.ROLE_BROWSER);
            boolean isRoleHeld = roleManager.isRoleHeld(RoleManager.ROLE_BROWSER);

            // TODO(crbug.com/1090103): check the condition before deciding
            // to show promo and remove the assertion.
            assert isRoleAvailable && !isRoleHeld;

            Intent intent = roleManager.createRequestRoleIntent(RoleManager.ROLE_BROWSER);
            mActivity.startActivityForResult(intent, 0);
        };
        boolean shouldSkipPrimer = ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.ANDROID_DEFAULT_BROWSER_PROMO, SKIP_PRIMER_PARAM, false);
        if (shouldSkipPrimer) {
            onOK.run();
        } else {
            showDialog(DefaultBrowserPromoDialog.DialogStyle.ROLE_MANAGER, onOK);
        }
    }

    private void promoBySystemSettings() {
        showDialog(DefaultBrowserPromoDialog.DialogStyle.SYSTEM_SETTINGS, () -> {
            Intent intent = new Intent(Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS);
            mActivity.startActivity(intent);
        });
    }

    private void promoByDisambiguationSheet() {
        showDialog(DefaultBrowserPromoDialog.DialogStyle.DISAMBIGUATION_SHEET, () -> {
            Intent intent = new Intent(Intent.ACTION_MAIN);
            intent.addCategory(Intent.CATEGORY_BROWSABLE);
            mActivity.startActivity(intent);
        });
    }

    private void showDialog(@DefaultBrowserPromoDialog.DialogStyle int style, Runnable okCallback) {
        mDialog = DefaultBrowserPromoDialog.createDialog(mActivity, style, okCallback, null);
        mDialog.show();
    }

    @VisibleForTesting
    public DefaultBrowserPromoDialog getDialogForTesting() {
        return mDialog;
    }

    @VisibleForTesting
    public void promoForTesting(
            @DefaultBrowserPromoUtils.DefaultBrowserState int state, int sdkInt) {
        promoInternal(state, sdkInt);
    }
}
