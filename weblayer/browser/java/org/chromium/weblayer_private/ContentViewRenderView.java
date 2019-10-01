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
import android.webkit.ValueCallback;
import android.widget.FrameLayout;

import androidx.annotation.IntDef;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.resources.ResourceManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;

/**
 * This class manages the chromium compositor and the Surface that is used by
 * the chromium compositor. Note it can be used to display only one WebContents.
 * This allows switching between SurfaceView and TextureView as the source of
 * the Surface used by chromium compositor, and attempts to make the switch
 * visually seamless. The rough steps for a switch are:
 * 1) Allocate new view, and insert it into view hierarchy below the existing
 *    view, so it is not yet showing.
 * 2) When Surface is allocated by new View, swap chromium compositor to the
 *    new Surface. Note at this point the existing view should is still visible.
 * 3) After chromium compositor swaps a frame in the new surface, detach the
 *    existing view.
 * Here are some more details.
 * * New view is added at index 0, ie below all other children.
 * * SurfaceView that uses SurfaceControl will need to retain the underlying
 *   EGLSurface to avoid desotrying the Surface.
 * * Use postOnAnimation to manipulate view tree as it is not safe to modify
 *   the view tree inside layout or draw.
 */
@JNINamespace("weblayer")
public class ContentViewRenderView extends FrameLayout {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({MODE_SURFACE_VIEW, MODE_SURFACE_VIEW})
    public @interface Mode {}
    public static final int MODE_SURFACE_VIEW = 0;
    public static final int MODE_TEXTURE_VIEW = 1;

    // This is mode that is requested by client.
    private SurfaceData mRequested;
    // This is the mode that last supplied the Surface to the compositor.
    // This should generally be equal to |mRequested| except during transitions.
    private SurfaceData mCurrent;
    private final ArrayList<SurfaceData> mMarkedForDestroySurfaces = new ArrayList<>();

    // The native side of this object.
    private long mNativeContentViewRenderView;

    private WindowAndroid mWindowAndroid;
    private WebContents mWebContents;

    private int mBackgroundColor;
    private int mWidth;
    private int mHeight;

    // Common interface to listen to surface related events.
    private interface SurfaceEventListener {
        void surfaceCreated();
        void surfaceChanged(
                Surface surface, boolean canBeUsedWithSurfaceControl, int width, int height);
        // |cacheBackBuffer| will delay destroying the EGLSurface until after the next swap.
        void surfaceDestroyed(boolean cacheBackBuffer);
    }

    // Non-static implementation of SurfaceEventListener that forward calls to native Compositor.
    // It is also responsible for updating |mRequested| and |mCurrent|.
    private class SurfaceEventListenerImpl implements SurfaceEventListener {
        private SurfaceData mSurfaceData;

        public void setRequestData(SurfaceData surfaceData) {
            assert mSurfaceData == null;
            mSurfaceData = surfaceData;
        }

        @Override
        public void surfaceCreated() {
            assert mNativeContentViewRenderView != 0;
            assert mSurfaceData == ContentViewRenderView.this.mRequested
                    || mSurfaceData == ContentViewRenderView.this.mCurrent;
            if (ContentViewRenderView.this.mCurrent != null
                    && ContentViewRenderView.this.mCurrent != mSurfaceData) {
                ContentViewRenderView.this.mCurrent.markForDestroy(
                        mMarkedForDestroySurfaces, true /* hasNextSurface */);
            }
            ContentViewRenderView.this.mCurrent = mSurfaceData;
            ContentViewRenderViewJni.get().surfaceCreated(mNativeContentViewRenderView);
        }

        @Override
        public void surfaceChanged(
                Surface surface, boolean canBeUsedWithSurfaceControl, int width, int height) {
            assert mNativeContentViewRenderView != 0;
            assert mSurfaceData == ContentViewRenderView.this.mCurrent;
            ContentViewRenderViewJni.get().surfaceChanged(mNativeContentViewRenderView,
                    canBeUsedWithSurfaceControl, width, height, surface);
            if (mWebContents != null) {
                ContentViewRenderViewJni.get().onPhysicalBackingSizeChanged(
                        mNativeContentViewRenderView, mWebContents, width, height);
            }
        }

        @Override
        public void surfaceDestroyed(boolean cacheBackBuffer) {
            assert mNativeContentViewRenderView != 0;
            assert mSurfaceData == ContentViewRenderView.this.mCurrent;
            ContentViewRenderViewJni.get().surfaceDestroyed(
                    mNativeContentViewRenderView, cacheBackBuffer);
        }
    }

    // Abstract differences between SurfaceView and TextureView behind this class.
    // Also responsible for holding and calling callbacks.
    private static class SurfaceData implements SurfaceEventListener {
        @Mode
        private final int mMode;
        private final SurfaceEventListener mListener;
        private final FrameLayout mParent;

        private boolean mCreated;
        private boolean mMarkedForDestroy;

        private boolean mNeedsOnSurfaceDestroyed;

        private final SurfaceHolderCallback mSurfaceCallback;
        private final SurfaceView mSurfaceView;

        private final TextureView mTextureView;
        private final TextureViewSurfaceTextureListener mSurfaceTextureListener;

        private final ArrayList<ValueCallback<Boolean>> mModeCallbacks = new ArrayList<>();

        public SurfaceData(@Mode int mode, FrameLayout parent, SurfaceEventListener listener,
                int backgroundColor) {
            mMode = mode;
            mListener = listener;
            mParent = parent;
            if (mode == MODE_SURFACE_VIEW) {
                mSurfaceView = new SurfaceView(parent.getContext());
                mSurfaceView.setZOrderMediaOverlay(true);
                mSurfaceView.setBackgroundColor(backgroundColor);

                mSurfaceCallback = new SurfaceHolderCallback(this);
                mSurfaceView.getHolder().addCallback(mSurfaceCallback);
                mSurfaceView.setVisibility(View.VISIBLE);

                mTextureView = null;
                mSurfaceTextureListener = null;
            } else if (mode == MODE_TEXTURE_VIEW) {
                mTextureView = new TextureView(parent.getContext());
                mSurfaceTextureListener = new TextureViewSurfaceTextureListener(this);
                mTextureView.setSurfaceTextureListener(mSurfaceTextureListener);
                mTextureView.setVisibility(VISIBLE);

                mSurfaceView = null;
                mSurfaceCallback = null;
            } else {
                throw new RuntimeException("Illegal mode: " + mode);
            }

            parent.postOnAnimation(() -> {
                if (mMarkedForDestroy) return;
                View view = (mMode == MODE_SURFACE_VIEW) ? mSurfaceView : mTextureView;
                assert view != null;
                // Always insert view for new surface below the existing view to avoid artifacts
                // during surface swaps. Index 0 is the lowest child.
                mParent.addView(view, 0,
                        new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
                                FrameLayout.LayoutParams.MATCH_PARENT));
                mParent.invalidate();
            });
        }

        public @Mode int getMode() {
            return mMode;
        }

        public void addCallback(ValueCallback<Boolean> callback) {
            assert !mMarkedForDestroy;
            mModeCallbacks.add(callback);
            if (mCreated) runCallbacks();
        }

        // Tearing down is separated into markForDestroy and destroy. After markForDestroy
        // this class will is guaranteed to not issue any calls to its SurfaceEventListener.
        public void markForDestroy(ArrayList<SurfaceData> pendingDestroy, boolean hasNextSurface) {
            if (mMarkedForDestroy) return;
            mMarkedForDestroy = true;

            if (mNeedsOnSurfaceDestroyed) {
                mListener.surfaceDestroyed(hasNextSurface && mMode == MODE_SURFACE_VIEW);
                mNeedsOnSurfaceDestroyed = false;
            }

            if (mMode == MODE_SURFACE_VIEW) {
                mSurfaceView.getHolder().removeCallback(mSurfaceCallback);
            } else if (mMode == MODE_TEXTURE_VIEW) {
                mTextureView.setSurfaceTextureListener(null);
            } else {
                assert false;
            }

            pendingDestroy.add(this);
        }

        // Remove view from parent hierarchy.
        public void destroy() {
            assert mMarkedForDestroy;
            mParent.postOnAnimation(() -> {
                if (mMode == MODE_SURFACE_VIEW) {
                    mParent.removeView(mSurfaceView);
                } else if (mMode == MODE_TEXTURE_VIEW) {
                    mParent.removeView(mTextureView);
                } else {
                    assert false;
                }
                runCallbacks();
            });
        }

        public void setBackgroundColor(int color) {
            assert !mMarkedForDestroy;
            if (mMode == MODE_SURFACE_VIEW) {
                mSurfaceView.setBackgroundColor(color);
            }
        }

        public void didSwapFrame() {
            if (mSurfaceView != null && mSurfaceView.getBackground() != null) {
                mSurfaceView.post(new Runnable() {
                    @Override
                    public void run() {
                        if (mSurfaceView != null) mSurfaceView.setBackgroundResource(0);
                    }
                });
            }
        }

        @Override
        public void surfaceCreated() {
            if (mMarkedForDestroy) return;

            if (!mCreated) {
                mCreated = true;
                runCallbacks();
            }

            // On pre-M Android, layers start in the hidden state until a relayout happens.
            // There is a bug that manifests itself when entering overlay mode on pre-M devices,
            // where a relayout never happens. This bug is out of Chromium's control, but can be
            // worked around by forcibly re-setting the visibility of the surface view.
            // Otherwise, the screen stays black, and some tests fail.
            if (mSurfaceView != null) {
                mSurfaceView.setVisibility(mSurfaceView.getVisibility());
            }
            mListener.surfaceCreated();
            mNeedsOnSurfaceDestroyed = true;
        }

        @Override
        public void surfaceChanged(
                Surface surface, boolean canBeUsedWithSurfaceControl, int width, int height) {
            if (mMarkedForDestroy) return;
            mListener.surfaceChanged(surface, canBeUsedWithSurfaceControl, width, height);
        }

        @Override
        public void surfaceDestroyed(boolean cacheBackBuffer) {
            if (mMarkedForDestroy) return;
            assert mNeedsOnSurfaceDestroyed;
            mListener.surfaceDestroyed(cacheBackBuffer);
            mNeedsOnSurfaceDestroyed = false;
        }

        private void runCallbacks() {
            assert mCreated || mMarkedForDestroy;
            // PostTask to avoid possible reentrancy problems with embedder code.
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
                ArrayList<ValueCallback<Boolean>> clone =
                        (ArrayList<ValueCallback<Boolean>>) mModeCallbacks.clone();
                mModeCallbacks.clear();
                for (ValueCallback<Boolean> run : clone) {
                    run.onReceiveValue(!mMarkedForDestroy);
                }
            });
        }
    }

    // Adapter for SurfaceHoolder.Callback.
    private static class SurfaceHolderCallback implements SurfaceHolder.Callback {
        private final SurfaceEventListener mListener;

        public SurfaceHolderCallback(SurfaceEventListener listener) {
            mListener = listener;
        }

        @Override
        public void surfaceCreated(SurfaceHolder holder) {
            mListener.surfaceCreated();
        }

        @Override
        public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            mListener.surfaceChanged(holder.getSurface(), true, width, height);
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder holder) {
            mListener.surfaceDestroyed(false /* cacheBackBuffer */);
        }
    }

    // Adapter for TextureView.SurfaceTextureListener.
    private static class TextureViewSurfaceTextureListener
            implements TextureView.SurfaceTextureListener {
        private final SurfaceEventListener mListener;

        private SurfaceTexture mCurrentSurfaceTexture;
        private Surface mCurrentSurface;

        public TextureViewSurfaceTextureListener(SurfaceEventListener listener) {
            mListener = listener;
        }

        @Override
        public void onSurfaceTextureAvailable(
                SurfaceTexture surfaceTexture, int width, int height) {
            mListener.surfaceCreated();
            onSurfaceTextureSizeChanged(surfaceTexture, width, height);
        }

        @Override
        public boolean onSurfaceTextureDestroyed(SurfaceTexture surfaceTexture) {
            mListener.surfaceDestroyed(false /* cacheBackBuffer */);
            return true;
        }

        @Override
        public void onSurfaceTextureSizeChanged(
                SurfaceTexture surfaceTexture, int width, int height) {
            if (mCurrentSurfaceTexture != surfaceTexture) {
                mCurrentSurfaceTexture = surfaceTexture;
                mCurrentSurface = new Surface(mCurrentSurfaceTexture);
            }
            mListener.surfaceChanged(mCurrentSurface, false, width, height);
        }

        @Override
        public void onSurfaceTextureUpdated(SurfaceTexture surfaceTexture) {}
    }

    /**
     * Constructs a new ContentViewRenderView.
     * This should be called and the {@link ContentViewRenderView} should be added to the view
     * hierarchy before the first draw to avoid a black flash that is seen every time a
     * {@link SurfaceView} is added.
     * @param context The context used to create this.
     */
    public ContentViewRenderView(Context context) {
        super(context);
        setBackgroundColor(Color.WHITE);
    }

    /**
     * Initialization that requires native libraries should be done here.
     * Native code should add/remove the layers to be rendered through the ContentViewLayerRenderer.
     * @param rootWindow The {@link WindowAndroid} this render view should be linked to.
     */
    public void onNativeLibraryLoaded(WindowAndroid rootWindow, @Mode int mode) {
        assert rootWindow != null;
        mNativeContentViewRenderView =
                ContentViewRenderViewJni.get().init(ContentViewRenderView.this, rootWindow);
        assert mNativeContentViewRenderView != 0;
        mWindowAndroid = rootWindow;
        requestMode(mode, (Boolean result) -> {});
    }

    public void requestMode(@Mode int mode, ValueCallback<Boolean> callback) {
        assert mode == MODE_SURFACE_VIEW || mode == MODE_TEXTURE_VIEW;
        assert callback != null;
        if (mRequested != null && mRequested.getMode() != mode) {
            if (mRequested != mCurrent) {
                mRequested.markForDestroy(mMarkedForDestroySurfaces, false /* hasNextSurface */);
                mRequested.destroy();
            }
            mRequested = null;
        }

        if (mRequested == null) {
            SurfaceEventListenerImpl listener = new SurfaceEventListenerImpl();
            mRequested = new SurfaceData(mode, this, listener, mBackgroundColor);
            listener.setRequestData(mRequested);
        }
        assert mRequested.getMode() == mode;
        mRequested.addCallback(callback);
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
        if (mRequested != null) {
            mRequested.setBackgroundColor(color);
        }
        if (mCurrent != null) {
            mCurrent.setBackgroundColor(color);
        }
    }

    /**
     * Should be called when the ContentViewRenderView is not needed anymore so its associated
     * native resource can be freed.
     */
    public void destroy() {
        if (mRequested != null) {
            mRequested.markForDestroy(mMarkedForDestroySurfaces, false /* hasNextSurface */);
            if (mCurrent != null && mCurrent != mRequested) {
                mCurrent.markForDestroy(mMarkedForDestroySurfaces, false /* hasNextSurface */);
            }
        }
        mRequested = null;
        mCurrent = null;
        runPendingSurfaceDestroy();

        mWindowAndroid = null;
        ContentViewRenderViewJni.get().destroy(mNativeContentViewRenderView);
        mNativeContentViewRenderView = 0;
    }

    public void setCurrentWebContents(WebContents webContents) {
        assert mNativeContentViewRenderView != 0;
        mWebContents = webContents;

        if (webContents != null) {
            webContents.setSize(mWidth, mHeight);
            ContentViewRenderViewJni.get().onPhysicalBackingSizeChanged(
                    mNativeContentViewRenderView, webContents, mWidth, mHeight);
        }
        ContentViewRenderViewJni.get().setCurrentWebContents(
                mNativeContentViewRenderView, webContents);
    }

    public ResourceManager getResourceManager() {
        return ContentViewRenderViewJni.get().getResourceManager(mNativeContentViewRenderView);
    }

    @CalledByNative
    private void didSwapFrame() {
        assert mCurrent != null;
        mCurrent.didSwapFrame();
        runPendingSurfaceDestroy();
    }

    private void runPendingSurfaceDestroy() {
        for (SurfaceData surface : mMarkedForDestroySurfaces) {
            surface.destroy();
        }
        mMarkedForDestroySurfaces.clear();
    }

    public long getNativeHandle() {
        return mNativeContentViewRenderView;
    }

    @NativeMethods
    interface Natives {
        long init(ContentViewRenderView caller, WindowAndroid rootWindow);
        void destroy(long nativeContentViewRenderView);
        void setCurrentWebContents(long nativeContentViewRenderView, WebContents webContents);
        void onPhysicalBackingSizeChanged(
                long nativeContentViewRenderView, WebContents webContents, int width, int height);
        void surfaceCreated(long nativeContentViewRenderView);
        void surfaceDestroyed(long nativeContentViewRenderView, boolean cacheBackBuffer);
        void surfaceChanged(long nativeContentViewRenderView, boolean canBeUsedWithSurfaceControl,
                int width, int height, Surface surface);
        ResourceManager getResourceManager(long nativeContentViewRenderView);
    }
}
