// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.app.Dialog;
import android.app.DialogFragment;
import android.content.Context;
import android.os.Bundle;
import android.view.View;

import androidx.appcompat.app.AlertDialog;

import org.chromium.chrome.R;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * ScreenshotShareSheetDialog is the main view for sharing non edited screenshots.
 */
public class ScreenshotShareSheetDialog extends DialogFragment {
    private Context mContext;

    /**
     * The ScreenshotShareSheetDialog constructor.
     */
    public ScreenshotShareSheetDialog() {}

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mContext = context;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        AlertDialog.Builder builder =
                new AlertDialog.Builder(getActivity(), R.style.Theme_Chromium_Fullscreen);
        builder.setView(getDialogView());
        return builder.create();
    }

    private View getDialogView() {
        ScreenshotShareSheetView dialogView =
                (ScreenshotShareSheetView) getActivity().getLayoutInflater().inflate(
                        org.chromium.chrome.browser.share.R.layout.screenshot_share_sheet, null);
        ChromeImageButton closeButton =
                (ChromeImageButton) dialogView.findViewById(R.id.close_button);
        closeButton.setOnClickListener(v -> dismiss());

        return dialogView;
    }
}
