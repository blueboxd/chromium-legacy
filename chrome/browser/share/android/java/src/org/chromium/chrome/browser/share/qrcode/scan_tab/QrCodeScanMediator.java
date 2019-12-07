// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.share.qrcode.scan_tab;

import android.Manifest.permission;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Process;

import org.chromium.ui.modelutil.PropertyModel;

/**
 * QrCodeScanMediator is in charge of calculating and setting values for QrCodeScanViewProperties.
 */
public class QrCodeScanMediator {
    private final Context mContext;
    private final PropertyModel mPropertyModel;

    /**
     * The QrCodeScanMediator constructor.
     * @param context The context to use for user permissions.
     * @param propertyModel The property modelto use to communicate with views.
     */
    QrCodeScanMediator(Context context, PropertyModel propertyModel) {
        mContext = context;
        mPropertyModel = propertyModel;
        mPropertyModel.set(QrCodeScanViewProperties.HAS_CAMERA_PERMISSION, hasCameraPermission());
    }

    /** Returns whether uers has granted camera permissions. */
    private Boolean hasCameraPermission() {
        return mContext.checkPermission(permission.CAMERA, Process.myPid(), Process.myUid())
                == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * Sets whether QrCode UI is on foreground.
     * @param isOnForeground Indicates whether this component UI is current on foreground.
     */
    public void setIsOnForeground(boolean isOnForeground) {
        mPropertyModel.set(QrCodeScanViewProperties.IS_ON_FOREGROUND, isOnForeground);
    }
}
