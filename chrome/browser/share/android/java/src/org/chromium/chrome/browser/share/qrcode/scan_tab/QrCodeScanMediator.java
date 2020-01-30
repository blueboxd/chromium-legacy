// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.scan_tab;

import android.Manifest.permission;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.ImageFormat;
import android.hardware.Camera;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.util.SparseArray;
import android.webkit.URLUtil;

import com.google.android.gms.vision.Frame;
import com.google.android.gms.vision.barcode.Barcode;
import com.google.android.gms.vision.barcode.BarcodeDetector;

import org.chromium.ui.base.ActivityAndroidPermissionDelegate;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.base.PermissionCallback;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.Toast;

import java.lang.ref.WeakReference;
import java.nio.ByteBuffer;

/**
 * QrCodeScanMediator is in charge of calculating and setting values for QrCodeScanViewProperties.
 */
public class QrCodeScanMediator implements Camera.PreviewCallback {
    /** Interface used for notifying in the event of navigation to a URL. */
    public interface NavigationObserver { void onNavigation(); }

    /** Interface for creating and navigation to a new tab for a given URL. */
    public interface TabCreator { void createNewTab(String url); }

    private final Context mContext;
    private final PropertyModel mPropertyModel;
    private final BarcodeDetector mDetector;
    private final NavigationObserver mNavigationObserver;
    private final TabCreator mTabCreator;
    private final Handler mMainThreadHandler;
    private final AndroidPermissionDelegate mPermissionDelegate;

    /**
     * The QrCodeScanMediator constructor.
     *
     * @param context The context to use for user permissions.
     * @param propertyModel The property modelto use to communicate with views.
     * @param observer The observer for navigation event.
     */
    QrCodeScanMediator(Context context, PropertyModel propertyModel, NavigationObserver observer,
            TabCreator tabCreator) {
        mContext = context;
        mPropertyModel = propertyModel;
        mPermissionDelegate = new ActivityAndroidPermissionDelegate(
                new WeakReference<Activity>((Activity) mContext));
        mPropertyModel.set(QrCodeScanViewProperties.HAS_CAMERA_PERMISSION, hasCameraPermission());
        mPropertyModel.set(
                QrCodeScanViewProperties.CAN_PROMPT_FOR_PERMISSION, canPromptForPermission());
        mDetector = new BarcodeDetector.Builder(context).build();
        mNavigationObserver = observer;
        mTabCreator = tabCreator;
        mMainThreadHandler = new Handler(Looper.getMainLooper());
    }

    /** Returns whether the user has granted camera permissions. */
    private Boolean hasCameraPermission() {
        return mContext.checkPermission(permission.CAMERA, Process.myPid(), Process.myUid())
                == PackageManager.PERMISSION_GRANTED;
    }

    /** Returns whether the user has granted camera permissions. */
    private Boolean canPromptForPermission() {
        return mPermissionDelegate.canRequestPermission(permission.CAMERA);
    }

    /**
     * Sets whether QrCode UI is on foreground.
     *
     * @param isOnForeground Indicates whether this component UI is current on foreground.
     */
    public void setIsOnForeground(boolean isOnForeground) {
        mPropertyModel.set(QrCodeScanViewProperties.IS_ON_FOREGROUND, isOnForeground);
    }

    /**
     * Prompts the user for camera permission and processes the results.
     */
    public void promptForCameraPermission() {
        final PermissionCallback callback = new PermissionCallback() {
            // Handle the results from prompting the user for camera permission.
            @Override
            public void onRequestPermissionsResult(String[] permissions, int[] grantResults) {
                // No results were produced (Does this ever happen?)
                if (grantResults.length == 0) {
                    return;
                }
                if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    mPropertyModel.set(QrCodeScanViewProperties.HAS_CAMERA_PERMISSION, true);
                } else {
                    mPropertyModel.set(QrCodeScanViewProperties.HAS_CAMERA_PERMISSION, false);
                    if (!mPermissionDelegate.canRequestPermission(permission.CAMERA)) {
                        mPropertyModel.set(
                                QrCodeScanViewProperties.CAN_PROMPT_FOR_PERMISSION, false);
                    }
                }
            }
        };

        mPermissionDelegate.requestPermissions(new String[] {permission.CAMERA}, callback);
    }

    /**
     * Processes data received from the camera preview to detect QR/barcode containing URLs. If
     * found, navigates to it by creating a new tab. If not found registers for camera preview
     * callback again. Runs on the same tread that was used to open the camera.
     */
    @Override
    public void onPreviewFrame(byte[] data, Camera camera) {
        ByteBuffer buffer = ByteBuffer.allocate(data.length);
        buffer.put(data);
        Frame frame =
                new Frame.Builder()
                        .setImageData(buffer, camera.getParameters().getPreviewSize().width,
                                camera.getParameters().getPreviewSize().height, ImageFormat.NV21)
                        .build();
        SparseArray<Barcode> barcodes = mDetector.detect(frame);
        if (barcodes.size() == 0) {
            camera.setOneShotPreviewCallback(this);
            return;
        }

        Barcode firstCode = barcodes.valueAt(0);
        Toast.makeText(mContext, firstCode.rawValue, Toast.LENGTH_LONG).show();
        if (!URLUtil.isValidUrl(firstCode.rawValue)) {
            camera.setOneShotPreviewCallback(this);
            return;
        }

        /** Tab creation should happen on the main thread. */
        mMainThreadHandler.post(() -> {
            mTabCreator.createNewTab(firstCode.rawValue);
            mNavigationObserver.onNavigation();
        });
    }
}
