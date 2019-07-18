// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.RenderWidgetHostView;

/**
 * The Android implementation of RenderWidgetHostView.  This is a Java wrapper to allow
 * communicating with the native RenderWidgetHostViewAndroid object (note the different class
 * names). This object allows the browser to access and control the renderer's top level View.
 */
@JNINamespace("content")
public class RenderWidgetHostViewImpl implements RenderWidgetHostView {
    private long mNativeRenderWidgetHostView;

    // Remember the stack for clearing native the native stack for debugging use after destroy.
    private Throwable mNativeDestroyThrowable;

    @CalledByNative
    private static RenderWidgetHostViewImpl create(long renderWidgetHostViewLong) {
        return new RenderWidgetHostViewImpl(renderWidgetHostViewLong);
    }

    /** Do not call this constructor from Java, use native WebContents->GetRenderWidgetHostView. */
    private RenderWidgetHostViewImpl(long renderWidgetHostViewLong) {
        mNativeRenderWidgetHostView = renderWidgetHostViewLong;
    }

    @Override
    public boolean isReady() {
        checkNotDestroyed();
        return nativeIsReady(getNativePtr());
    }

    @Override
    public int getBackgroundColor() {
        return nativeGetBackgroundColor(getNativePtr());
    }

    /**
     * Removes handles used in text selection.
     */
    public void dismissTextHandles() {
        if (isDestroyed()) return;
        nativeDismissTextHandles(getNativePtr());
    }

    /**
     * Shows the paste popup menu and the touch handles at the specified location.
     * @param x The horizontal location of the touch in dps.
     * @param y The vertical location of the touch in dps.
     */
    public void showContextMenuAtTouchHandle(int x, int y) {
        checkNotDestroyed();
        nativeShowContextMenuAtTouchHandle(getNativePtr(), x, y);
    }

    @Override
    public void insetViewportBottom(int bottomAdjustPx) {
        checkNotDestroyed();
        nativeInsetViewportBottom(getNativePtr(), bottomAdjustPx);
    }

    @Override
    public void writeContentBitmapToDiskAsync(
            int width, int height, String path, Callback<String> callback) {
        if (isDestroyed()) callback.onResult("RWHVA already destroyed!");
        nativeWriteContentBitmapToDiskAsync(getNativePtr(), width, height, path, callback);
    }

    //====================
    // Support for native.
    //====================

    public boolean isDestroyed() {
        return getNativePtr() == 0;
    }

    private long getNativePtr() {
        return mNativeRenderWidgetHostView;
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeRenderWidgetHostView = 0;
        mNativeDestroyThrowable = new RuntimeException("clearNativePtr");
    }

    private void checkNotDestroyed() {
        if (getNativePtr() != 0) return;
        throw new IllegalStateException(
                "Native RenderWidgetHostViewAndroid already destroyed", mNativeDestroyThrowable);
    }

    private native boolean nativeIsReady(long nativeRenderWidgetHostViewAndroid);
    private native int nativeGetBackgroundColor(long nativeRenderWidgetHostViewAndroid);
    private native void nativeDismissTextHandles(long nativeRenderWidgetHostViewAndroid);
    private native void nativeShowContextMenuAtTouchHandle(
            long nativeRenderWidgetHostViewAndroid, int x, int y);
    private native void nativeInsetViewportBottom(
            long nativeRenderWidgetHostViewAndroid, int bottomAdjustPx);
    private native void nativeWriteContentBitmapToDiskAsync(long nativeRenderWidgetHostViewAndroid,
            int width, int height, String path, Callback<String> callback);
}
