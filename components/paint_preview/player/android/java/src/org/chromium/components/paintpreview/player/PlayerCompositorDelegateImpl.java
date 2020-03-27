// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import android.graphics.Bitmap;
import android.graphics.Rect;

import org.chromium.base.Callback;
import org.chromium.base.UnguessableToken;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.paintpreview.browser.NativePaintPreviewServiceProvider;
import org.chromium.url.GURL;

import javax.annotation.Nonnull;

/**
 * This class and its native counterpart (player_compositor_delegate.cc) communicate with the Paint
 * Preview compositor.
 */
@JNINamespace("paint_preview")
class PlayerCompositorDelegateImpl implements PlayerCompositorDelegate {
    interface CompositorListener {
        void onCompositorReady(boolean safeToShow, UnguessableToken rootFrameGuid,
                UnguessableToken[] frameGuids, int[] frameContentSize, int[] subFramesCount,
                UnguessableToken[] subFrameGuids, int[] subFrameClipRects);
    }

    private CompositorListener mCompositorListener;
    private LinkClickHandler mLinkClickHandler;
    private long mNativePlayerCompositorDelegate;

    PlayerCompositorDelegateImpl(NativePaintPreviewServiceProvider service, GURL url,
            String directoryKey, @Nonnull CompositorListener compositorListener,
            @Nonnull LinkClickHandler linkClickHandler) {
        mCompositorListener = compositorListener;
        mLinkClickHandler = linkClickHandler;
        if (service != null && service.getNativeService() != 0) {
            mNativePlayerCompositorDelegate = PlayerCompositorDelegateImplJni.get().initialize(
                    this, service.getNativeService(), url.getSpec(), directoryKey);
        }
        // TODO(crbug.com/1021590): Handle initialization errors when
        // mNativePlayerCompositorDelegate == 0.
    }

    /**
     * Called by native when the Paint Preview compositor is ready.
     *
     * @param safeToShow true if the native initialization of the Paint Preview player succeeded and
     * the captured result matched the expected URL.
     * @param rootFrameGuid The GUID for the root frame.
     * @param frameGuids Contains all frame GUIDs that are in this hierarchy.
     * @param frameContentSize Contains the content size for each frame. In native, this is called
     * scroll extent. The order corresponds to {@code frameGuids}. The content width and height for
     * the ith frame in {@code frameGuids} are respectively in the {@code 2*i} and {@code 2*i+1}
     * indices of {@code frameContentSize}.
     * @param subFramesCount Contains the number of sub-frames for each frame. The order corresponds
     * to {@code frameGuids}. The number of sub-frames for the {@code i}th frame in {@code
     * frameGuids} is {@code subFramesCount[i]}.
     * @param subFrameGuids Contains the GUIDs of all sub-frames. The GUID for the {@code j}th
     * sub-frame of {@code frameGuids[i]} will be at {@code subFrameGuids[k]}, where {@code k} is:
     * <pre>
     *     int k = j;
     *     for (int s = 0; s < i; s++) k += subFramesCount[s];
     * </pre>
     * @param subFrameClipRects Contains clip rect values for each sub-frame. Each clip rect value
     * comes in a series of four consecutive integers that represent x, y, width, and height. The
     * clip rect values for the {@code j}th sub-frame of {@code frameGuids[i]} will be at {@code
     * subFrameGuids[4*k]}, {@code subFrameGuids[4*k+1]} , {@code subFrameGuids[4*k+2]}, and {@code
     * subFrameGuids[4*k+3]}, where {@code k} has the same value as above.
     */
    @CalledByNative
    void onCompositorReady(boolean safeToShow, UnguessableToken rootFrameGuid,
            UnguessableToken[] frameGuids, int[] frameContentSize, int[] subFramesCount,
            UnguessableToken[] subFrameGuids, int[] subFrameClipRects) {
        mCompositorListener.onCompositorReady(safeToShow, rootFrameGuid, frameGuids,
                frameContentSize, subFramesCount, subFrameGuids, subFrameClipRects);
    }

    @Override
    public void requestBitmap(UnguessableToken frameGuid, Rect clipRect, float scaleFactor,
            Callback<Bitmap> bitmapCallback, Runnable errorCallback) {
        if (mNativePlayerCompositorDelegate == 0) {
            return;
        }

        PlayerCompositorDelegateImplJni.get().requestBitmap(mNativePlayerCompositorDelegate,
                frameGuid, bitmapCallback, errorCallback, scaleFactor, clipRect.left, clipRect.top,
                clipRect.width(), clipRect.height());
    }

    @Override
    public void onClick(UnguessableToken frameGuid, int x, int y) {
        if (mNativePlayerCompositorDelegate == 0) {
            return;
        }

        PlayerCompositorDelegateImplJni.get().onClick(
                mNativePlayerCompositorDelegate, frameGuid, x, y);
    }

    @CalledByNative
    public void onLinkClicked(String url) {
        mLinkClickHandler.onLinkClicked(new GURL(url));
    }

    void destroy() {
        if (mNativePlayerCompositorDelegate == 0) {
            return;
        }

        PlayerCompositorDelegateImplJni.get().destroy(mNativePlayerCompositorDelegate);
        mNativePlayerCompositorDelegate = 0;
    }

    @NativeMethods
    interface Natives {
        long initialize(PlayerCompositorDelegateImpl caller, long nativePaintPreviewBaseService,
                String urlSpec, String directoryKey);
        void destroy(long nativePlayerCompositorDelegateAndroid);
        void requestBitmap(long nativePlayerCompositorDelegateAndroid, UnguessableToken frameGuid,
                Callback<Bitmap> bitmapCallback, Runnable errorCallback, float scaleFactor,
                int clipX, int clipY, int clipWidth, int clipHeight);
        void onClick(long nativePlayerCompositorDelegateAndroid, UnguessableToken frameGuid, int x,
                int y);
    }
}
