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

/***
 * This view is used by a ContentView to render its content.
 * Call {@link #setCurrentWebContents(WebContents)} with the webContents that should be
 * managing the content.
 * Note that only one WebContents can be shown at a time.
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
    private SurfaceData mCurrent;

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
        void surfaceDestroyed();
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
                ContentViewRenderView.this.mCurrent.destroy();
            }
            ContentViewRenderView.this.mCurrent = mSurfaceData;
            ContentViewRenderViewJni.get().surfaceCreated(
                    mNativeContentViewRenderView, ContentViewRenderView.this);
        }

        @Override
        public void surfaceChanged(
                Surface surface, boolean canBeUsedWithSurfaceControl, int width, int height) {
            assert mNativeContentViewRenderView != 0;
            assert mSurfaceData == ContentViewRenderView.this.mCurrent;
            ContentViewRenderViewJni.get().surfaceChanged(mNativeContentViewRenderView,
                    ContentViewRenderView.this, canBeUsedWithSurfaceControl, width, height,
                    surface);
            if (mWebContents != null) {
                ContentViewRenderViewJni.get().onPhysicalBackingSizeChanged(
                        mNativeContentViewRenderView, ContentViewRenderView.this, mWebContents,
                        width, height);
            }
        }

        @Override
        public void surfaceDestroyed() {
            assert mNativeContentViewRenderView != 0;
            assert mSurfaceData == ContentViewRenderView.this.mCurrent;
            ContentViewRenderViewJni.get().surfaceDestroyed(
                    mNativeContentViewRenderView, ContentViewRenderView.this);
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
        private boolean mDestroyed;

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

                parent.addView(mSurfaceView,
                        new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
                                FrameLayout.LayoutParams.MATCH_PARENT));
                mSurfaceView.setVisibility(View.VISIBLE);

                mTextureView = null;
                mSurfaceTextureListener = null;
            } else if (mode == MODE_TEXTURE_VIEW) {
                mTextureView = new TextureView(parent.getContext());
                mSurfaceTextureListener = new TextureViewSurfaceTextureListener(this);
                mTextureView.setSurfaceTextureListener(mSurfaceTextureListener);

                mTextureView.setVisibility(VISIBLE);
                parent.addView(mTextureView,
                        new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
                                FrameLayout.LayoutParams.MATCH_PARENT));

                mSurfaceView = null;
                mSurfaceCallback = null;
            } else {
                throw new RuntimeException("Illegal mode: " + mode);
            }
            parent.postInvalidateOnAnimation();
        }

        public @Mode int getMode() {
            return mMode;
        }

        public void addCallback(ValueCallback<Boolean> callback) {
            assert !mDestroyed;
            mModeCallbacks.add(callback);
            if (mCreated) runCallbacks();
        }

        public void destroy() {
            if (mDestroyed) return;
            mDestroyed = true;
            if (mMode == MODE_SURFACE_VIEW) {
                mParent.removeView(mSurfaceView);
                mSurfaceView.getHolder().removeCallback(mSurfaceCallback);
            } else if (mMode == MODE_TEXTURE_VIEW) {
                mParent.removeView(mTextureView);
                mTextureView.setSurfaceTextureListener(null);
            } else {
                assert false;
            }

            runCallbacks();
        }

        public void setBackgroundColor(int color) {
            assert !mDestroyed;
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
            if (mDestroyed) return;

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
        }

        @Override
        public void surfaceChanged(
                Surface surface, boolean canBeUsedWithSurfaceControl, int width, int height) {
            if (mDestroyed) return;
            mListener.surfaceChanged(surface, canBeUsedWithSurfaceControl, width, height);
        }

        @Override
        public void surfaceDestroyed() {
            if (mDestroyed) return;
            mListener.surfaceDestroyed();
        }

        private void runCallbacks() {
            assert mCreated;
            // PostTask to avoid possible reentrancy problems with embedder code.
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
                ArrayList<ValueCallback<Boolean>> clone =
                        (ArrayList<ValueCallback<Boolean>>) mModeCallbacks.clone();
                mModeCallbacks.clear();
                for (ValueCallback<Boolean> run : clone) {
                    run.onReceiveValue(!mDestroyed);
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
            mListener.surfaceDestroyed();
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
            mListener.surfaceDestroyed();
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
            if (mRequested != mCurrent) mRequested.destroy();
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
            mRequested.destroy();
            if (mCurrent != null && mCurrent != mRequested) {
                mCurrent.destroy();
            }
        }
        mRequested = null;
        mCurrent = null;
        mWindowAndroid = null;
        ContentViewRenderViewJni.get().destroy(
                mNativeContentViewRenderView, ContentViewRenderView.this);
        mNativeContentViewRenderView = 0;
    }

    public void setCurrentWebContents(WebContents webContents) {
        assert mNativeContentViewRenderView != 0;
        mWebContents = webContents;

        if (webContents != null) {
            webContents.setSize(mWidth, mHeight);
            ContentViewRenderViewJni.get().onPhysicalBackingSizeChanged(
                    mNativeContentViewRenderView, ContentViewRenderView.this, webContents, mWidth,
                    mHeight);
        }
        ContentViewRenderViewJni.get().setCurrentWebContents(
                mNativeContentViewRenderView, ContentViewRenderView.this, webContents);
    }

    public ResourceManager getResourceManager() {
        return ContentViewRenderViewJni.get().getResourceManager(
                mNativeContentViewRenderView, ContentViewRenderView.this);
    }

    @CalledByNative
    private void didSwapFrame() {
        assert mCurrent != null;
        mCurrent.didSwapFrame();
    }

    public long getNativeHandle() {
        return mNativeContentViewRenderView;
    }

    @NativeMethods
    interface Natives {
        long init(ContentViewRenderView caller, WindowAndroid rootWindow);
        void destroy(long nativeContentViewRenderView, ContentViewRenderView caller);
        void setCurrentWebContents(long nativeContentViewRenderView, ContentViewRenderView caller,
                WebContents webContents);
        void onPhysicalBackingSizeChanged(long nativeContentViewRenderView,
                ContentViewRenderView caller, WebContents webContents, int width, int height);
        void surfaceCreated(long nativeContentViewRenderView, ContentViewRenderView caller);
        void surfaceDestroyed(long nativeContentViewRenderView, ContentViewRenderView caller);
        void surfaceChanged(long nativeContentViewRenderView, ContentViewRenderView caller,
                boolean canBeUsedWithSurfaceControl, int width, int height, Surface surface);
        ResourceManager getResourceManager(
                long nativeContentViewRenderView, ContentViewRenderView caller);
    }
}
