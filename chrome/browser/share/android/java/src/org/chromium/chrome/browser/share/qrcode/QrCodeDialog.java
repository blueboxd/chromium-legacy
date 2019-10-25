// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode;

import android.content.Context;
import android.support.v7.app.AlertDialog;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * QrCodeDialog is the main view for QR code sharing.
 */
public class QrCodeDialog extends AlertDialog {
    /**
     * The QrCodeDialog constructor.
     * @param context The context to use.
     */
    public QrCodeDialog(Context context) {
        super(context, R.style.Theme_Chromium_Fullscreen);

        View dialogView = (View) LayoutInflater.from(context).inflate(
                org.chromium.chrome.browser.share.qrcode.R.layout.qrcode_dialog, null);
        setView(dialogView);
        ChromeImageButton close_button =
                (ChromeImageButton) dialogView.findViewById(R.id.close_button);
        close_button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                cancel();
            }
        });
    }
}
