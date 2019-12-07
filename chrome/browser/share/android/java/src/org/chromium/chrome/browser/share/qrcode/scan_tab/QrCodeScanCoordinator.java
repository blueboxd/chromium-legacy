// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.scan_tab;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.share.qrcode.QrCodeDialogTab;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Creates and represents the QrCode scan panel UI.
 */
public class QrCodeScanCoordinator implements QrCodeDialogTab {
    private final QrCodeScanView mScanView;
    private final QrCodeScanMediator mMediator;

    /**
     * The QrCodeScanCoordinator constructor.
     * @param context The context to use for user permissions.
     */
    public QrCodeScanCoordinator(Context context) {
        PropertyModel scanViewModel = new PropertyModel(QrCodeScanViewProperties.ALL_KEYS);
        mMediator = new QrCodeScanMediator(context, scanViewModel);

        mScanView = new QrCodeScanView(context);
        PropertyModelChangeProcessor.create(scanViewModel, mScanView, new QrCodeScanViewBinder());
    }

    /** QrCodeDialogTab implementation. */
    @Override
    public View getView() {
        return mScanView.getView();
    }

    @Override
    public void onResume() {
        mMediator.setIsOnForeground(true);
    }

    @Override
    public void onPause() {
        mMediator.setIsOnForeground(false);
    }
}