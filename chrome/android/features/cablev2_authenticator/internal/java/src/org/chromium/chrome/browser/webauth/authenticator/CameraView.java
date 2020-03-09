// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

import android.content.Context;
import android.hardware.Camera;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import org.chromium.base.Log;

import java.io.IOException;

/**
 * Provides a SurfaceView and adapts it for use as a camera preview target
 * so that the current camera image can be displayed.
 */
class CameraView extends SurfaceView implements SurfaceHolder.Callback {
    private static final String TAG = "CameraView";
    private final Camera.PreviewCallback mCallback;
    private Camera mCamera;

    public CameraView(Context context, Camera.PreviewCallback callback) {
        super(context);
        mCallback = callback;
    }

    public void rearmCallback() {
        if (mCamera != null) {
            mCamera.setOneShotPreviewCallback(mCallback);
        }
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        getHolder().addCallback(this);
        // TODO: Camera.open is slow and shouldn't be called on the UI
        // thread.
        mCamera = Camera.open();
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        getHolder().removeCallback(this);
        if (mCamera != null) {
            mCamera.release();
            mCamera = null;
        }
    }

    private void startCamera() {
        try {
            mCamera.setPreviewDisplay(getHolder());
            // Use a one-shot callback so that callbacks don't happen faster
            // they're processed.
            mCamera.setOneShotPreviewCallback(mCallback);
            Camera.Parameters parameters = mCamera.getParameters();
            parameters.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE);
            mCamera.setParameters(parameters);
            // TODO: the display orientation should be configured as
            // described in
            // https://developer.android.com/reference/android/hardware/Camera.html#setDisplayOrientation(int)
            mCamera.startPreview();
        } catch (IOException e) {
            Log.w(TAG, "Exception while starting camera", e);
        }
    }

    private void stopCamera() {
        if (mCamera == null) {
            return;
        }

        mCamera.setOneShotPreviewCallback(null);
        mCamera.stopPreview();
    }

    /** SurfaceHolder.Callback implementation. */
    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        startCamera();
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        stopCamera();
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
        stopCamera();
        startCamera();
    }
}
