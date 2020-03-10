// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.share_tab;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

class QrCodeShareViewBinder implements ViewBinder<PropertyModel, QrCodeShareView, PropertyKey> {
    @Override
    public void bind(PropertyModel model, QrCodeShareView view, PropertyKey propertyKey) {
        if (QrCodeShareViewProperties.QRCODE_BITMAP == propertyKey) {
            view.updateQrCodeBitmap(model.get(QrCodeShareViewProperties.QRCODE_BITMAP));
        }
    }
}
