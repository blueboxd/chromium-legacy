// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.scan_tab;

import android.content.Context;
import android.hardware.Camera;
import android.hardware.Camera.CameraInfo;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import org.chromium.ui.display.DisplayAndroid;

/** CameraPreview class controls camera and camera previews. */
public class CameraPreview extends SurfaceView implements SurfaceHolder.Callback {
    private static final String THREAD_NAME = "CameraHandlerThread";

    private final Context mContext;
    private final Camera.PreviewCallback mCameraCallback;

    private int mCameraId;
    private Camera mCamera;
    private HandlerThread mCameraThread;

    /**
     * The CameraPreview constructor.
     * @param context The context to use for user permissions.
     * @param cameraCallback The callback to processing camera preview.
     */
    public CameraPreview(Context context, Camera.PreviewCallback cameraCallback) {
        super(context);
        mContext = context;
        mCameraCallback = cameraCallback;
    }

    /** Obtains a camera and starts the preview. */
    public void startCamera() {
        startCameraAsync();
    }

    /** Starts the default camera on a separate thread. */
    private void startCameraAsync() {
        if (mCameraThread == null) {
            mCameraThread = new HandlerThread(THREAD_NAME);
            mCameraThread.start();
        }
        mCameraId = getDefaultCameraId();
        Handler handler = new Handler(mCameraThread.getLooper());
        handler.post(() -> {
            Camera camera = getCameraInstance(mCameraId);
            Handler mainHandler = new Handler(Looper.getMainLooper());
            mainHandler.post(() -> { setupCamera(camera); });
        });
    }

    /** Sets camera information. Set camera to null if camera is used or doesn't exist. */
    private void setupCamera(Camera camera) {
        mCamera = camera;
        startCameraPreview();
    }

    /** Stops the camera and releases it for other apps. */
    public void stopCamera() {
        if (mCamera == null) {
            return;
        }

        stopCameraPreview();
        mCamera.release();

        if (mCameraThread != null) {
            mCameraThread.quit();
            mCameraThread = null;
        }
    }

    /** Sets up and starts camera preview. */
    private void startCameraPreview() {
        getHolder().addCallback(this);

        if (mCamera == null) {
            return;
        }

        try {
            mCamera.setPreviewDisplay(getHolder());
            mCamera.setDisplayOrientation(getCameraOrientation());
            mCamera.setOneShotPreviewCallback(mCameraCallback);

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
        getHolder().removeCallback(this);

        if (mCamera == null) {
            return;
        }

        mCamera.setOneShotPreviewCallback(null);
        try {
            mCamera.stopPreview();
        } catch (RuntimeException e) {
            // Ignore, error is not important after stopPreview is called.
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
        DisplayAndroid display = DisplayAndroid.getNonMultiDisplay(mContext);
        return display.getRotationDegrees();
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

    /**
     * Returns an instance of the Camera for the give id. Returns null if camera is used or doesn't
     * exist.
     */
    private static Camera getCameraInstance(int cameraId) {
        Camera camera = null;
        try {
            camera = Camera.open(cameraId);
        } catch (RuntimeException e) {
            // TODO(gayane): Should show error message to users, when error strings are approved.
        }
        return camera;
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
