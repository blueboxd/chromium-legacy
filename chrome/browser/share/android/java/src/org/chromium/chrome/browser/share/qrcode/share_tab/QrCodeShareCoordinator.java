// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.share_tab;

import android.content.Context;
import android.view.View;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.share.qrcode.QrCodeDialogTab;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Creates and represents the QrCode share panel UI.
 */
public class QrCodeShareCoordinator implements QrCodeDialogTab {
    private final QrCodeShareView mShareView;

    public QrCodeShareCoordinator(Context context) {
        PropertyModel shareViewModel = new PropertyModel(QrCodeShareViewProperties.ALL_KEYS);
        QrCodeShareMediator shareViewMediator = new QrCodeShareMediator(context, shareViewModel);

        mShareView = new QrCodeShareView(context, shareViewMediator::downloadQrCode);
        PropertyModelChangeProcessor.create(
                shareViewModel, mShareView, new QrCodeShareViewBinder());
    }

    /** QrCodeDialogTab implementation. */
    @Override
    public View getView() {
        return mShareView.getView();
    }

    @Override
    public void onResume() {
        RecordUserAction.record("SharingQRCode.TabVisible.Share");
    }

    @Override
    public void onPause() {}

    @Override
    public void onDestroy() {}
}