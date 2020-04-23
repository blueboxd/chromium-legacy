// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.share_tab;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import org.chromium.chrome.browser.share.R;

/**
 * Manages the Android View representing the QrCode share panel.
 */
class QrCodeShareView {
    private final Context mContext;
    private final View mView;

    public QrCodeShareView(Context context, View.OnClickListener listener) {
        mContext = context;

        mView = (View) LayoutInflater.from(context).inflate(
                R.layout.qrcode_share_layout, null, false);

        Button downloadButton = (Button) mView.findViewById(R.id.download);
        downloadButton.setOnClickListener(listener);
    }

    public View getView() {
        return mView;
    }

    /**
     * Updates QR code image on share panel.
     *
     * @param bitmap The {@link Bitmap} to display on share panel.
     */
    public void updateQrCodeBitmap(Bitmap bitmap) {
        TextView qrcodeText = mView.findViewById(R.id.qrcode_text_image);
        Drawable drawable = new BitmapDrawable(bitmap);
        qrcodeText.setCompoundDrawablesWithIntrinsicBounds(null, null, null, drawable);
    }
}
