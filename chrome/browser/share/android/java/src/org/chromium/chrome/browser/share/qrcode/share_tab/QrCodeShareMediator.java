// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.share_tab;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.text.DynamicLayout;
import android.text.Layout.Alignment;
import android.text.TextPaint;
import android.text.TextUtils.TruncateAt;
import android.view.View;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.share.BitmapDownloadRequest;
import org.chromium.chrome.browser.share.qrcode.QRCodeGenerationRequest;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * QrCodeShareMediator is in charge of calculating and setting values for QrCodeShareViewProperties.
 */
class QrCodeShareMediator {
    private final Context mContext;
    private final PropertyModel mPropertyModel;

    // The number of times the user has attempted to download the QR code in this dialog.
    private int mNumDownloads;

    private long mDownloadStartTime;
    private boolean mIsDownloadInProgress;
    private String mUrl;
    private Runnable mCloseDialog;

    /**
     * The QrCodeScanMediator constructor.
     * @param context The context to use.
     * @param propertyModel The property modelto use to communicate with views.
     * @param closeDialog The {@link Runnable} to close the dialog.
     * @param url The url to create the QRCode.
     */
    QrCodeShareMediator(
            Context context, PropertyModel propertyModel, Runnable closeDialog, String url) {
        mContext = context;
        mPropertyModel = propertyModel;
        mCloseDialog = closeDialog;
        mUrl = url;
        ChromeBrowserInitializer.getInstance().runNowOrAfterFullBrowserStarted(
                () -> refreshQrCode(mUrl));
    }

    /**
     * Refreshes the QR Code bitmap for given data.
     * @param data The data to encode.
     */
    protected void refreshQrCode(String data) {
        QRCodeGenerationRequest.QRCodeServiceCallback callback =
                new QRCodeGenerationRequest.QRCodeServiceCallback() {
                    @Override
                    public void onQRCodeAvailable(Bitmap bitmap) {
                        // TODO(skare): If bitmap is null, surface an error.
                        if (bitmap != null) {
                            mPropertyModel.set(QrCodeShareViewProperties.QRCODE_BITMAP, bitmap);
                        }
                    }
                };
        new QRCodeGenerationRequest(data, callback);
    }

    /** Triggers download for the generated QR code bitmap if available. */
    protected void downloadQrCode(View view) {
        Bitmap qrcodeBitmap = mPropertyModel.get(QrCodeShareViewProperties.QRCODE_BITMAP);
        if (qrcodeBitmap != null && !mIsDownloadInProgress) {
            String fileName = mContext.getString(
                    R.string.qr_code_filename_prefix, String.valueOf(System.currentTimeMillis()));
            mIsDownloadInProgress = true;
            BitmapDownloadRequest.downloadBitmap(fileName, addUrlToBitmap(qrcodeBitmap, mUrl));
        }
        logDownload();
        mCloseDialog.run();
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

    private Bitmap addUrlToBitmap(Bitmap bitmap, String url) {
        int qrCodeSize = mContext.getResources().getDimensionPixelSize(
                org.chromium.chrome.browser.share.R.dimen.qrcode_size);
        int fontSize = mContext.getResources().getDimensionPixelSize(R.dimen.text_size_large);
        int sidePadding = mContext.getResources().getDimensionPixelSize(
                org.chromium.chrome.browser.share.R.dimen.side_padding);
        int textTopPadding = mContext.getResources().getDimensionPixelSize(
                org.chromium.chrome.browser.share.R.dimen.url_box_top_padding);
        int textBottomPadding = mContext.getResources().getDimensionPixelSize(
                org.chromium.chrome.browser.share.R.dimen.url_box_bottom_padding);

        TextPaint mTextPaint = new TextPaint();
        mTextPaint.setAntiAlias(true);
        mTextPaint.setColor(android.graphics.Color.BLACK);
        mTextPaint.setTextSize(fontSize);

        // Text is as wide as the QR code.
        FixedLineCountLayout mTextLayout = new FixedLineCountLayout(
                url, mTextPaint, qrCodeSize, Alignment.ALIGN_CENTER, 1.0f, 0.0f, true, 2);

        // New bitmap should be long enough to fit the url with its margins, the QR code bitmap and
        // equal padding from the bottom.
        int height =
                (textTopPadding + mTextLayout.getHeight() + textBottomPadding) * 2 + qrCodeSize;
        int width = qrCodeSize + 2 * sidePadding;
        Bitmap newBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(newBitmap);
        canvas.drawColor(android.graphics.Color.WHITE);
        canvas.translate(sidePadding, textTopPadding);
        mTextLayout.draw(canvas);
        canvas.drawBitmap(Bitmap.createScaledBitmap(bitmap, qrCodeSize, qrCodeSize, false), 0,
                mTextLayout.getHeight() + textBottomPadding, mTextPaint);
        return newBitmap;
    }

    // Helps to limit number of text lines and shows ellipsis for only the last line.
    class FixedLineCountLayout extends DynamicLayout {
        int mMaxLines;

        FixedLineCountLayout(CharSequence base, TextPaint paint, int width, Alignment align,
                float spacingmult, float spacingadd, boolean includepad, int maxLines) {
            super(base, base, paint, width, align, spacingmult, spacingadd, includepad,
                    TruncateAt.END, width);
            mMaxLines = maxLines;
        }
        @Override
        public int getLineCount() {
            if (super.getLineCount() - 1 > mMaxLines) {
                return mMaxLines;
            }
            return super.getLineCount() - 1;
        }

        @Override
        public int getEllipsisCount(int line) {
            if (line == mMaxLines - 1 && super.getLineCount() - 2 > line) {
                return 3;
            }
            return 0;
        }

        @Override
        public int getEllipsisStart(int line) {
            if (line == mMaxLines - 1 && super.getLineCount() - 2 > line) {
                return getLineEnd(line) - getLineStart(line) - 1;
            }
            return 0;
        }
    }
}
