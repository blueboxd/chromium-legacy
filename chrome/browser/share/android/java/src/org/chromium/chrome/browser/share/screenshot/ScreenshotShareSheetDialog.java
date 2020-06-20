// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.app.Dialog;
import android.app.DialogFragment;
import android.content.Context;
import android.graphics.Bitmap;
import android.os.Bundle;

import androidx.appcompat.app.AlertDialog;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;

/**
 * ScreenshotShareSheetDialog is the main view for sharing non edited screenshots.
 */
public class ScreenshotShareSheetDialog extends DialogFragment {
    private Context mContext;
    private ScreenshotShareSheetView mDialogView;
    private Bitmap mScreenshot;
    private Runnable mDeleteRunnable;
    private Tab mTab;
    private ChromeOptionShareCallback mShareCallback;

    /**
     * The ScreenshotShareSheetDialog constructor.
     */
    public ScreenshotShareSheetDialog() {}

    /**
     * Initialize the dialog outside of the constructor as fragments require default constructor.
     * @param screenshot The screenshot image to show.
     * @param deleteRunnable The function to call on delete.
     * @param tab The shared tab.
     * @param shareSheetCoordnator the base share sheet coordinator
     */
    public void init(Bitmap screenshot, Runnable deleteRunnable, Tab tab,
            ChromeOptionShareCallback shareSheetCallback) {
        mScreenshot = screenshot;
        mDeleteRunnable = deleteRunnable;
        mTab = tab;
        mShareCallback = shareSheetCallback;
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mContext = context;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        AlertDialog.Builder builder =
                new AlertDialog.Builder(getActivity(), R.style.Theme_Chromium_Fullscreen);
        ScreenshotShareSheetView screenshotShareSheetView =
                (ScreenshotShareSheetView) getActivity().getLayoutInflater().inflate(
                        org.chromium.chrome.browser.share.R.layout.screenshot_share_sheet, null);
        builder.setView(screenshotShareSheetView);

        ScreenshotShareSheetCoordinator shareCoordinator =
                new ScreenshotShareSheetCoordinator(mContext, mScreenshot, mDeleteRunnable,
                        screenshotShareSheetView, mTab, mShareCallback);
        return builder.create();
    }
}
