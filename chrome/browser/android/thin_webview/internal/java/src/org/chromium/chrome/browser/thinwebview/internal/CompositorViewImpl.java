// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.thinwebview.internal;

import android.content.Context;
import android.graphics.PixelFormat;
import android.graphics.SurfaceTexture;
import android.os.Build;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.TextureView;
import android.view.View;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.thinwebview.CompositorView;
import org.chromium.ui.base.WindowAndroid;

/**
 * An android view backed by a {@link Surface} that is able to display a cc::Layer. Either, a {@link
 * TextureView} or {@link SurfaceView} can be used to provide the surface. The cc::Layer should be
 * provided in the native.
 */
@JNINamespace("thin_webview::android")
public class CompositorViewImpl implements CompositorView {
    private final Context mContext;
    private final View mView;
    private long mNativeCompositorViewImpl;

    /**
     * Creates a {@link CompositorView} backed by a {@link Surface}. The surface is provided by
     * a either a {@link TextureView} or {@link SurfaceView}.
     * @param context The context to create this view.
     * @param windowAndroid The associated {@code WindowAndroid} on which the view is to be
     *         displayed.
     */
    public CompositorViewImpl(Context context, WindowAndroid windowAndroid) {
        mContext = context;
        mView = useSurfaceView() ? createSurfaceView() : createTextureView();
        mNativeCompositorViewImpl = nativeInit(windowAndroid);
    }

    @Override
    public View getView() {
        return mView;
    }

    @Override
    public void destroy() {
        if (mNativeCompositorViewImpl != 0) nativeDestroy(mNativeCompositorViewImpl);
    }

    @Override
    public void requestRender() {
        if (mNativeCompositorViewImpl != 0) nativeSetNeedsComposite(mNativeCompositorViewImpl);
    }

    private SurfaceView createSurfaceView() {
        SurfaceView surfaceView = new SurfaceView(mContext);
        surfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder surfaceHolder) {
                if (mNativeCompositorViewImpl == 0) return;
                nativeSurfaceCreated(mNativeCompositorViewImpl);
            }

            @Override
            public void surfaceChanged(
                    SurfaceHolder surfaceHolder, int format, int width, int height) {
                if (mNativeCompositorViewImpl == 0) return;
                nativeSurfaceChanged(mNativeCompositorViewImpl, format, width, height,
                        surfaceHolder.getSurface());
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder surfaceHolder) {
                if (mNativeCompositorViewImpl == 0) return;
                nativeSurfaceDestroyed(mNativeCompositorViewImpl);
            }
        });

        return surfaceView;
    }

    private TextureView createTextureView() {
        TextureView textureView = new TextureView(mContext);
        textureView.setSurfaceTextureListener(new TextureView.SurfaceTextureListener() {
            @Override
            public void onSurfaceTextureUpdated(SurfaceTexture surfaceTexture) {}

            @Override
            public void onSurfaceTextureSizeChanged(
                    SurfaceTexture surfaceTexture, int width, int height) {
                if (mNativeCompositorViewImpl == 0) return;
                nativeSurfaceChanged(mNativeCompositorViewImpl, PixelFormat.OPAQUE, width, height,
                        new Surface(surfaceTexture));
            }

            @Override
            public boolean onSurfaceTextureDestroyed(SurfaceTexture surfaceTexture) {
                if (mNativeCompositorViewImpl == 0) return false;
                nativeSurfaceDestroyed(mNativeCompositorViewImpl);
                return false;
            }

            @Override
            public void onSurfaceTextureAvailable(
                    SurfaceTexture surfaceTexture, int width, int height) {
                if (mNativeCompositorViewImpl == 0) return;
                nativeSurfaceCreated(mNativeCompositorViewImpl);
                nativeSurfaceChanged(mNativeCompositorViewImpl, PixelFormat.OPAQUE, width, height,
                        new Surface(surfaceTexture));
            }
        });
        return textureView;
    }

    @CalledByNative
    private long getNativePtr() {
        assert mNativeCompositorViewImpl != 0;
        return mNativeCompositorViewImpl;
    }

    @CalledByNative
    private void onCompositorLayout() {}

    @CalledByNative
    private void recreateSurface() {
        // TODO(shaktisahu): May be detach and reattach the surface view from the hierarchy.
    }

    private static boolean useSurfaceView() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.N;
    }

    private native long nativeInit(WindowAndroid windowAndroid);
    private native void nativeDestroy(long nativeCompositorViewImpl);
    private native void nativeSurfaceCreated(long nativeCompositorViewImpl);
    private native void nativeSurfaceDestroyed(long nativeCompositorViewImpl);
    private native void nativeSurfaceChanged(
            long nativeCompositorViewImpl, int format, int width, int height, Surface surface);
    private native void nativeSetNeedsComposite(long nativeCompositorViewImpl);
}
