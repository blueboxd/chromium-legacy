// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.scan_tab;

import android.content.Context;
import android.hardware.Camera.PreviewCallback;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import org.chromium.ui.widget.ButtonCompat;

/**
 * Manages the Android View representing the QrCode scan panel.
 */
class QrCodeScanView {
    public interface PermissionPrompter { void promptForCameraPermission(); }

    private final Context mContext;
    private final FrameLayout mView;
    private final PreviewCallback mCameraCallback;

    private boolean mHasCameraPermission;
    private boolean mCanPromptForPermission;
    private boolean mIsOnForeground;
    private CameraPreview mCameraPreview;
    private View mPermissionsView;

    /**
     * The QrCodeScanView constructor.
     *
     * @param context The context to use for user permissions.
     * @param cameraCallback The callback to processing camera preview.
     */
    public QrCodeScanView(Context context, PreviewCallback cameraCallback,
            PermissionPrompter permissionPrompter) {
        mContext = context;
        mCameraCallback = cameraCallback;
        mView = new FrameLayout(context);
        mView.setLayoutParams(
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mPermissionsView = (View) LayoutInflater.from(context).inflate(
                org.chromium.chrome.browser.share.qrcode.R.layout.qrcode_permission_layout, null,
                false);

        ButtonCompat cameraPermissionPrompt = mPermissionsView.findViewById(
                org.chromium.chrome.browser.share.qrcode.R.id.ask_for_permission);
        cameraPermissionPrompt.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                permissionPrompter.promptForCameraPermission();
            }
        });
    }

    public View getView() {
        return mView;
    }

    /**
     * Sets camera if possible.
     *
     * @param hasCameraPermission Indicates whether camera permissions were granted.
     */
    public void cameraPermissionsChanged(Boolean hasCameraPermission) {
        // No change, nothing to do here (This really shouldn't happen)
        if (mHasCameraPermission == hasCameraPermission) {
            return;
        }
        mHasCameraPermission = hasCameraPermission;
        // Check that the camera permission has changed and that it is now set to true.
        if (hasCameraPermission) {
            setCameraPreview();
        } else {
            // TODO(tgupta): Check that the user can be prompted. If not, show the error
            // screen instead.
            displayPermissionDialog();
        }
    }

    /**
     * Checks whether Chrome can prompt the user for Camera permission. Updates the view accordingly
     * to let the user know if the permission has been permanently denied.
     *
     * @param canPromptForPermission
     */
    public void canPromptForPermissionChanged(Boolean canPromptForPermission) {
        if (mCanPromptForPermission != canPromptForPermission && !canPromptForPermission) {
            // User chose the "don't ask again option, display the appropriate message
            // TODO (tgupta): Update this message
        }
    }

    /**
     * Applies changes necessary to camera preview.
     *
     * @param isOnForeground Indicates whether this component UI is current on foreground.
     */
    public void onForegroundChanged(Boolean isOnForeground) {
        mIsOnForeground = isOnForeground;
        updateCameraPreviewState();
    }

    /** Creates and sets the camera preview. */
    private void setCameraPreview() {
        mView.removeAllViews();
        if (mCameraPreview != null) {
            mCameraPreview.stopCamera();
            mCameraPreview = null;
        }

        if (mHasCameraPermission) {
            mCameraPreview = new CameraPreview(mContext, mCameraCallback);
            mView.addView(mCameraPreview);
            mView.addView(new CameraPreviewOverlay(mContext));

            updateCameraPreviewState();
        }
    }

    /** Starts or stops camera if necessary. */
    private void updateCameraPreviewState() {
        if (mCameraPreview == null) {
            return;
        }

        if (mIsOnForeground) {
            mCameraPreview.startCamera();
        } else {
            mCameraPreview.stopCamera();
        }
    }

    /**
     * Displays the permission dialog. Caller should check that the user can be prompted and hasn't
     * permanently denied permission.
     */
    private void displayPermissionDialog() {
        mView.removeAllViews();
        mView.addView(mPermissionsView);
    }
}
