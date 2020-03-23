// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.share_tab;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.View;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.share.ShareImageFileUtils;
import org.chromium.ui.modelutil.PropertyModel;

import java.io.File;

/**
 * QrCodeShareMediator is in charge of calculating and setting values for QrCodeShareViewProperties.
 */
class QrCodeShareMediator implements ShareImageFileUtils.OnImageSaveListener {
    private final Context mContext;
    private final PropertyModel mPropertyModel;
    // The number of times the user has attempted to download the QR code in this dialog.
    private int mNumDownloads;

    /**
     * The QrCodeScanMediator constructor.
     * @param context The context to use.
     * @param propertyModel The property modelto use to communicate with views.
     */
    QrCodeShareMediator(Context context, PropertyModel propertyModel) {
        mContext = context;
        mPropertyModel = propertyModel;

        // TODO(gayane): Request generated QR code bitmap with a callback that sets QRCODE_BITMAP
        // property.
    }

    /** Triggers download for the generated QR code bitmap if available. */
    protected void downloadQrCode(View view) {
        Bitmap qrcodeBitmap = mPropertyModel.get(QrCodeShareViewProperties.QRCODE_BITMAP);
        if (qrcodeBitmap != null) {
            String fileName = mContext.getString(
                    R.string.qr_code_filename_prefix, String.valueOf(System.currentTimeMillis()));
            ShareImageFileUtils.saveBitmapToExternalStorage(mContext, fileName, qrcodeBitmap, this);
        }
        logDownload();
    }

    /** Logs user actions when attempting to download a QR code. */
    private void logDownload() {
        // Always log the singular metric; otherwise it's easy to miss during analysis.
        RecordUserAction.record("SharingQRCode.DownloadQRCode");
        if (mNumDownloads > 0) {
            RecordUserAction.record("SharingQRCode.DownloadQRCodeMultipleAttempts");
        }
        mNumDownloads++;
    }

    // ShareImageFileUtils.OnImageSaveListener implementation.
    @Override
    public void onImageSaved(File imageFile) {
        // TODO(gayane): Maybe need to show confirmation message.
        mPropertyModel.set(QrCodeShareViewProperties.DOWNLOAD_SUCCESSFUL, true);
        RecordUserAction.record("SharingQRCode.DownloadQRCode.Succeeded");
    }

    @Override
    public void onImageSaveError() {
        // TODO(gayane): Maybe need to show error message.
        mPropertyModel.set(QrCodeShareViewProperties.DOWNLOAD_SUCCESSFUL, false);
        RecordUserAction.record("SharingQRCode.DownloadQRCode.Failed");
    }
}