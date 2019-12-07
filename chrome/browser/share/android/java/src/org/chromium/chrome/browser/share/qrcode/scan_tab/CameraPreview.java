// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.scan_tab;

import android.content.Context;
import android.hardware.Camera;
import android.hardware.Camera.CameraInfo;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager;

/** CameraPreview class controls camera and camera previews. */
public class CameraPreview extends SurfaceView implements SurfaceHolder.Callback {
    private SurfaceHolder mHolder;
    private Camera mCamera;
    private int mCameraId;
    private Context mContext;

    /**
     * The CameraPreview constructor.
     * @param context The context to use for user permissions.
     */
    public CameraPreview(Context context) {
        super(context);
        mContext = context;
        mHolder = getHolder();
    }

    /** Obtains a camera and starts the preview. */
    public void startCamera() {
        setCameraInstance();
        startCameraPreview();
    }

    /** Stops the camera and releases it for other apps. */
    public void stopCamera() {
        if (mCamera == null) {
            return;
        }

        stopCameraPreview();
        mCamera.release();
    }

    /** Sets up and starts camera preview. */
    private void startCameraPreview() {
        if (mCamera == null) {
            return;
        }

        getHolder().addCallback(this);

        try {
            mCamera.setPreviewDisplay(getHolder());
            mCamera.setDisplayOrientation(getCameraOrientation());

            Camera.Parameters parameters = mCamera.getParameters();
            parameters.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE);
            mCamera.setParameters(parameters);

            mCamera.startPreview();
        } catch (Exception e) {
            // TODO(gayane): Should show error message to users, when error strings are approved.
        }
    }

    /** Stops camera preview. */
    private void stopCameraPreview() {
        if (mCamera == null) {
            return;
        }

        getHolder().removeCallback(this);

        try {
            mCamera.stopPreview();
        } catch (RuntimeException e) {
            // Ignore, error is not important after stopPreview is called.
        }
    }

    /** Sets camera information. Set camera to null if camera is used or doesn't exist. */
    private void setCameraInstance() {
        mCamera = null;
        mCameraId = getDefaultCameraId();

        if (mCameraId == -1) {
            return;
        }

        try {
            mCamera = Camera.open(mCameraId);
        } catch (RuntimeException e) {
            // TODO(gayane): Should show error message to users, when error strings are approved.
        }
    }

    /** Calculates camera's orientation based on displaye's orientation and camera. */
    private int getCameraOrientation() {
        Camera.CameraInfo info = new Camera.CameraInfo();
        Camera.getCameraInfo(mCameraId, info);

        int displayOrientation = getDisplayOrientation();
        if (info == null) {
            return displayOrientation;
        }

        int result;
        if (isBackCamera(info)) {
            result = (info.orientation - displayOrientation + 360) % 360;
        } else {
            // front-facing
            result = (info.orientation + displayOrientation) % 360;
            result = (360 - result) % 360; // compensate the mirror
        }
        return result;
    }

    /** Gets the display orientation degree as integer. */
    private int getDisplayOrientation() {
        final int orientation;
        WindowManager wm = (WindowManager) mContext.getSystemService(Context.WINDOW_SERVICE);
        switch (wm.getDefaultDisplay().getRotation()) {
            case Surface.ROTATION_90:
                orientation = 90;
                break;
            case Surface.ROTATION_180:
                orientation = 180;
                break;
            case Surface.ROTATION_270:
                orientation = 270;
                break;
            case Surface.ROTATION_0:
            default:
                orientation = 0;
                break;
        }
        return orientation;
    }

    /** Returns whether given camera info corresponds to back camera. */
    private static boolean isBackCamera(CameraInfo info) {
        return info.facing == android.hardware.Camera.CameraInfo.CAMERA_FACING_BACK;
    }

    /** Returns default camera id. Prefers back camera if available. */
    private static int getDefaultCameraId() {
        int numberOfCameras = Camera.getNumberOfCameras();
        Camera.CameraInfo cameraInfo = new Camera.CameraInfo();
        int defaultCameraId = -1;
        for (int i = 0; i < numberOfCameras; i++) {
            defaultCameraId = i;
            Camera.getCameraInfo(i, cameraInfo);
            if (isBackCamera(cameraInfo)) {
                return i;
            }
        }
        return defaultCameraId;
    }

    /** SurfaceHolder.Callback implementation. */
    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        startCameraPreview();
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        stopCameraPreview();
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
        stopCameraPreview();
        startCameraPreview();
    }
}