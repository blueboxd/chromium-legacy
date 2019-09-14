// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.graphics.Color;
import android.graphics.SurfaceTexture;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.TextureView;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.IntDef;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.resources.ResourceManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/***
 * This view is used by a ContentView to render its content.
 * Call {@link #setCurrentWebContents(WebContents)} with the webContents that should be
 * managing the content.
 * Note that only one WebContents can be shown at a time.
 */
@JNINamespace("weblayer")
public class ContentViewRenderView extends FrameLayout {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({MODE_UNINITIALIZED, MODE_SURFACE_VIEW, MODE_SURFACE_VIEW})
    public @interface Mode {}
    private static final int MODE_UNINITIALIZED = -1;
    public static final int MODE_SURFACE_VIEW = 0;
    public static final int MODE_TEXTURE_VIEW = 1;

    // This is mode that is requested by client.
    @Mode
    private int mRequestedMode;
    // This is the mode that last supplied the Surface to the compositor.
    @Mode
    private int mCurrentMode;

    // The native side of this object.
    private long mNativeContentViewRenderView;

    private WindowAndroid mWindowAndroid;
    private WebContents mWebContents;

    private SurfaceHolder.Callback mSurfaceCallback;
    private SurfaceView mSurfaceView;

    private TextureView mTextureView;
    private TextureView.SurfaceTextureListener mSurfaceTextureListener;

    private int mBackgroundColor;
    private int mWidth;
    private int mHeight;

    /**
     * Constructs a new ContentViewRenderView.
     * This should be called and the {@link ContentViewRenderView} should be added to the view
     * hierarchy before the first draw to avoid a black flash that is seen every time a
     * {@link SurfaceView} is added.
     * @param context The context used to create this.
     */
    public ContentViewRenderView(Context context) {
        super(context);

        mCurrentMode = MODE_UNINITIALIZED;
        mRequestedMode = MODE_UNINITIALIZED;

        setBackgroundColor(Color.WHITE);
    }

    /**
     * Initialization that requires native libraries should be done here.
     * Native code should add/remove the layers to be rendered through the ContentViewLayerRenderer.
     * @param rootWindow The {@link WindowAndroid} this render view should be linked to.
     */
    public void onNativeLibraryLoaded(WindowAndroid rootWindow, @Mode int mode) {
        assert rootWindow != null;
        mNativeContentViewRenderView = nativeInit(rootWindow);
        assert mNativeContentViewRenderView != 0;
        mWindowAndroid = rootWindow;
        requestMode(mode);
    }

    public void requestMode(@Mode int mode) {
        assert mode == MODE_SURFACE_VIEW || mode == MODE_TEXTURE_VIEW;
        if (mode == MODE_SURFACE_VIEW) {
            mRequestedMode = mode;
            initializeSurfaceView();
        } else if (mode == MODE_TEXTURE_VIEW) {
            mRequestedMode = mode;
            initializeTextureView();
        } else {
            throw new RuntimeException("Unexpected mode: " + mode);
        }
    }

    private void setCurrentMode(@Mode int mode) {
        if (mCurrentMode == mode) return;

        if (mCurrentMode == MODE_SURFACE_VIEW) {
            uninitializeSurfaceView();
        } else if (mCurrentMode == MODE_TEXTURE_VIEW) {
            uninitializeTextureView();
        }
        mCurrentMode = mode;
    }

    private void initializeSurfaceView() {
        if (mCurrentMode == MODE_SURFACE_VIEW) {
            assert mSurfaceView != null;
            assert mSurfaceCallback != null;
            return;
        }
        if (mSurfaceView != null) return;

        mSurfaceView = new SurfaceView(getContext());
        mSurfaceView.setZOrderMediaOverlay(true);
        mSurfaceView.setBackgroundColor(mBackgroundColor);
        mWindowAndroid.setAnimationPlaceholderView(mSurfaceView);

        mSurfaceCallback = new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                if (mRequestedMode != MODE_SURFACE_VIEW) {
                    uninitializeSurfaceView();
                    return;
                }
                setCurrentMode(MODE_SURFACE_VIEW);
                ContentViewRenderView.this.surfaceCreated();

                // On pre-M Android, layers start in the hidden state until a relayout happens.
                // There is a bug that manifests itself when entering overlay mode on pre-M devices,
                // where a relayout never happens. This bug is out of Chromium's control, but can be
                // worked around by forcibly re-setting the visibility of the surface view.
                // Otherwise, the screen stays black, and some tests fail.
                mSurfaceView.setVisibility(mSurfaceView.getVisibility());
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                if (mCurrentMode != MODE_SURFACE_VIEW) return;

                ContentViewRenderView.this.surfaceChanged(holder.getSurface(), true, width, height);
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                if (mCurrentMode != MODE_SURFACE_VIEW) return;
                ContentViewRenderView.this.surfaceDestroyed();
            }
        };
        mSurfaceView.getHolder().addCallback(mSurfaceCallback);

        addView(mSurfaceView,
                new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
        mSurfaceView.setVisibility(VISIBLE);
    }

    private void uninitializeSurfaceView() {
        if (mSurfaceView == null) return;
        removeView(mSurfaceView);
        mWindowAndroid.setAnimationPlaceholderView(null);
        mSurfaceView.getHolder().removeCallback(mSurfaceCallback);
        mSurfaceCallback = null;
        mSurfaceView = null;
    }

    private void initializeTextureView() {
        if (mCurrentMode == MODE_TEXTURE_VIEW) {
            assert mTextureView != null;
            return;
        }
        if (mTextureView != null) return;

        mTextureView = new TextureView(getContext());

        mSurfaceTextureListener = new TextureView.SurfaceTextureListener() {
            private SurfaceTexture mCurrentSurfaceTexture;
            private Surface mCurrentSurface;

            @Override
            public void onSurfaceTextureAvailable(
                    SurfaceTexture surfaceTexture, int width, int height) {
                if (mRequestedMode != MODE_TEXTURE_VIEW) {
                    uninitializeTextureView();
                    return;
                }
                setCurrentMode(MODE_TEXTURE_VIEW);

                ContentViewRenderView.this.surfaceCreated();
                onSurfaceTextureSizeChanged(surfaceTexture, width, height);
            }

            @Override
            public boolean onSurfaceTextureDestroyed(SurfaceTexture surfaceTexture) {
                if (mCurrentMode != MODE_TEXTURE_VIEW) return true;

                ContentViewRenderView.this.surfaceDestroyed();
                return true;
            }

            @Override
            public void onSurfaceTextureSizeChanged(
                    SurfaceTexture surfaceTexture, int width, int height) {
                if (mCurrentMode != MODE_TEXTURE_VIEW) return;

                if (mCurrentSurfaceTexture != surfaceTexture) {
                    mCurrentSurfaceTexture = surfaceTexture;
                    mCurrentSurface = new Surface(mCurrentSurfaceTexture);
                }
                ContentViewRenderView.this.surfaceChanged(mCurrentSurface, false, width, height);
            }

            @Override
            public void onSurfaceTextureUpdated(SurfaceTexture surfaceTexture) {}
        };
        mTextureView.setSurfaceTextureListener(mSurfaceTextureListener);
        mTextureView.setVisibility(VISIBLE);
        addView(mTextureView,
                new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
    }

    private void uninitializeTextureView() {
        if (mTextureView == null) return;
        removeView(mTextureView);
        mTextureView.setSurfaceTextureListener(null);
        mSurfaceTextureListener = null;
        mTextureView = null;
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        mWidth = w;
        mHeight = h;
        if (mWebContents != null) mWebContents.setSize(w, h);
    }

    /**
     * View's method override to notify WindowAndroid about changes in its visibility.
     */
    @Override
    protected void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);

        if (mWindowAndroid == null) return;

        if (visibility == View.GONE) {
            mWindowAndroid.onVisibilityChanged(false);
        } else if (visibility == View.VISIBLE) {
            mWindowAndroid.onVisibilityChanged(true);
        }
    }

    /**
     * Sets the background color of the surface / texture view.  This method is necessary because
     * the background color of ContentViewRenderView itself is covered by the background of
     * SurfaceView.
     * @param color The color of the background.
     */
    @Override
    public void setBackgroundColor(int color) {
        super.setBackgroundColor(color);
        mBackgroundColor = color;
        if (mSurfaceView != null) {
            mSurfaceView.setBackgroundColor(color);
        }
    }

    /**
     * Should be called when the ContentViewRenderView is not needed anymore so its associated
     * native resource can be freed.
     */
    public void destroy() {
        setCurrentMode(MODE_UNINITIALIZED);
        uninitializeSurfaceView();
        uninitializeTextureView();
        mWindowAndroid = null;
        nativeDestroy(mNativeContentViewRenderView);
        mNativeContentViewRenderView = 0;
    }

    public void setCurrentWebContents(WebContents webContents) {
        assert mNativeContentViewRenderView != 0;
        mWebContents = webContents;

        if (webContents != null) {
            webContents.setSize(mWidth, mHeight);
            nativeOnPhysicalBackingSizeChanged(
                    mNativeContentViewRenderView, webContents, mWidth, mHeight);
        }
        nativeSetCurrentWebContents(mNativeContentViewRenderView, webContents);
    }

    public ResourceManager getResourceManager() {
        return nativeGetResourceManager(mNativeContentViewRenderView);
    }

    private void surfaceCreated() {
        assert mNativeContentViewRenderView != 0;
        nativeSurfaceCreated(mNativeContentViewRenderView);
    }

    private void surfaceChanged(
            Surface surface, boolean canBeUsedWithSurfaceControl, int width, int height) {
        assert mNativeContentViewRenderView != 0;
        nativeSurfaceChanged(
                mNativeContentViewRenderView, canBeUsedWithSurfaceControl, width, height, surface);
        if (mWebContents != null) {
            nativeOnPhysicalBackingSizeChanged(
                    mNativeContentViewRenderView, mWebContents, width, height);
        }
    }

    private void surfaceDestroyed() {
        assert mNativeContentViewRenderView != 0;
        nativeSurfaceDestroyed(mNativeContentViewRenderView);
    }

    @CalledByNative
    private void didSwapFrame() {
        if (mSurfaceView != null && mSurfaceView.getBackground() != null) {
            post(new Runnable() {
                @Override
                public void run() {
                    if (mSurfaceView != null) mSurfaceView.setBackgroundResource(0);
                }
            });
        }
    }

    public long getNativeHandle() {
        return mNativeContentViewRenderView;
    }

    private native long nativeInit(WindowAndroid rootWindow);
    private native void nativeDestroy(long nativeContentViewRenderView);
    private native void nativeSetCurrentWebContents(
            long nativeContentViewRenderView, WebContents webContents);
    private native void nativeOnPhysicalBackingSizeChanged(
            long nativeContentViewRenderView, WebContents webContents, int width, int height);
    private native void nativeSurfaceCreated(long nativeContentViewRenderView);
    private native void nativeSurfaceDestroyed(long nativeContentViewRenderView);
    private native void nativeSurfaceChanged(long nativeContentViewRenderView,
            boolean canBeUsedWithSurfaceControl, int width, int height, Surface surface);
    private native ResourceManager nativeGetResourceManager(long nativeContentViewRenderView);
}
