// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.os.Build;
import android.view.DisplayCutout;
import android.view.View;
import android.view.WindowInsets;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.annotations.VerifiesOnP;

/**
 * Display cutout controller for WebView.
 *
 * This object should be constructed in WebView's constructor to support set listener logic for
 * Android P and above.
 */
@TargetApi(Build.VERSION_CODES.P)
@VerifiesOnP
public class AwDisplayCutoutController {
    private static final boolean DEBUG = false;
    private static final String TAG = "DisplayCutout";

    /**
     * This is a delegate that the embedder needs to implement.
     */
    public interface Delegate {
        /** @return The DIP scale. */
        float getDipScale();
        /** @return The display width. */
        int getDisplayWidth();
        /** @return The display height. */
        int getDisplayHeight();
        /**
         * Set display cutout safe area such that webpage can read safe-area-insets CSS properties.
         * Note that this can be called with the same parameter repeatedly, and the embedder needs
         * to check / throttle as necessary.
         *
         * @param insets A placeholder to store left, top, right, and bottom insets in regards to
         *               WebView. Note that DIP scale has been applied.
         */
        void setDisplayCutoutSafeArea(Insets insets);
    }

    /**
     * A placeholder for insets.
     *
     * android.graphics.Insets is available from Q, while we support display cutout from P and
     * above, so adding a new class.
     */
    public static final class Insets {
        public int left;
        public int top;
        public int right;
        public int bottom;

        public Insets() {}

        public Insets(int left, int top, int right, int bottom) {
            set(left, top, right, bottom);
        }

        public Rect toRect(Rect rect) {
            rect.set(left, top, right, bottom);
            return rect;
        }

        public void set(Insets insets) {
            left = insets.left;
            top = insets.top;
            right = insets.right;
            bottom = insets.bottom;
        }

        public void set(int left, int top, int right, int bottom) {
            this.left = left;
            this.top = top;
            this.right = right;
            this.bottom = bottom;
        }

        @Override
        public final boolean equals(Object o) {
            if (!(o instanceof Insets)) return false;
            Insets i = (Insets) o;
            return left == i.left && top == i.top && right == i.right && bottom == i.bottom;
        }

        @Override
        public final String toString() {
            return "Insets: (" + left + ", " + top + ")-(" + right + ", " + bottom + ")";
        }

        @Override
        public final int hashCode() {
            return 3 * left + 5 * top + 7 * right + 11 * bottom;
        }
    }

    private Delegate mDelegate;
    private View mContainerView;

    // Reuse these structures to minimize memory impact.
    private static int[] sCachedLocationOnScreen = {0, 0};
    private Insets mCachedViewInsets = new Insets();
    private Rect mCachedViewRect = new Rect();
    private Rect mCachedWindowRect = new Rect();
    private Rect mCachedDisplayRect = new Rect();
    private Matrix mCachedMatrix = new Matrix();

    /**
     * Constructor for AwDisplayCutoutController.
     *
     * @param delegate The delegate.
     * @param containerView The container view (WebView).
     */
    public AwDisplayCutoutController(Delegate delegate, View containerView) {
        mDelegate = delegate;
        mContainerView = containerView;
        registerContainerView(containerView);
    }

    /**
     * Register a container view to listen to window insets.
     *
     * Note that you do not need to register the containerView.
     *
     * @param containerView A container View, such as fullscreen view.
     */
    public void registerContainerView(View containerView) {
        if (DEBUG) Log.i(TAG, "registerContainerView");
        // For Android P~R, we set the listener in WebView's constructor.
        // Once we set the listener, we will no longer get View#onApplyWindowInsets(WindowInsets).
        // If the app sets its own listener after WebView's constructor, then the app can override
        // our logic, which seems like a natural behavior.
        // For Android S, WebViewChromium can get onApplyWindowInsets(WindowInsets) call, so we do
        // not need to set the listener.
        // TODO(https://crbug.com/1094366): do not set listener and plumb WebViewChromium to handle
        // onApplyWindowInsets in S and above.
        containerView.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
            @Override
            public WindowInsets onApplyWindowInsets(View view, WindowInsets insets) {
                // Ignore if this is not the current container view.
                if (view == mContainerView) {
                    return AwDisplayCutoutController.this.onApplyWindowInsets(insets);
                } else {
                    if (DEBUG) Log.i(TAG, "Ignoring onApplyWindowInsets on View: " + view);
                    return insets;
                }
            }
        });
    }

    /**
     * Set the current container view.
     *
     * @param containerView The current container view.
     */
    public void setCurrentContainerView(View containerView) {
        if (DEBUG) Log.i(TAG, "setCurrentContainerView: " + containerView);
        mContainerView = containerView;
        // Ensure that we get new insets for the new container view.
        mContainerView.requestApplyInsets();
    }

    /**
     * Call this when window insets are first applied or changed.
     *
     * @see View#onApplyWindowInsets(WindowInsets)
     * @param insets The window (display) insets.
     */
    @VisibleForTesting
    public WindowInsets onApplyWindowInsets(final WindowInsets insets) {
        if (DEBUG) Log.i(TAG, "onApplyWindowInsets: " + insets.toString());
        // TODO(https://crbug.com/1094366): add a throttling logic.
        DisplayCutout cutout = insets.getDisplayCutout();
        // DisplayCutout can be null if there is no notch, or layoutInDisplayCutoutMode is DEFAULT
        // (before R) or consumed in the parent view.
        if (cutout != null) {
            Insets displayCutoutInsets =
                    new Insets(cutout.getSafeInsetLeft(), cutout.getSafeInsetTop(),
                            cutout.getSafeInsetRight(), cutout.getSafeInsetBottom());
            onApplyWindowInsetsInternal(displayCutoutInsets);
        }
        return insets;
    }

    /**
     * Call this when window insets are first applied or changed.
     *
     * Similar to {@link onApplyWindowInsets(WindowInsets)}, but accepts
     * Rect as input.
     *
     * @param displayCutoutInsets Insets to store left, top, right, bottom insets.
     */
    @VisibleForTesting
    public void onApplyWindowInsetsInternal(final Insets displayCutoutInsets) {
        // Copy such that we can log the original value.
        mCachedViewInsets.set(displayCutoutInsets);

        getViewRectOnScreen(mContainerView, mCachedViewRect);
        getViewRectOnScreen(mContainerView.getRootView(), mCachedWindowRect);

        // Get display coordinates.
        int displayWidth = mDelegate.getDisplayWidth();
        int displayHeight = mDelegate.getDisplayHeight();
        mCachedDisplayRect.set(0, 0, displayWidth, displayHeight);

        float dipScale = mDelegate.getDipScale();

        if (!mCachedViewRect.equals(mCachedDisplayRect)) {
            // We apply window insets only when webview is occupying the entire window and display.
            // Checking the window rect is more complicated and therefore not doing it for now, but
            // there can still be cases where the window is a bit off.
            if (DEBUG) {
                Log.i(TAG, "WebView is not occupying the entire screen, so no insets applied.");
            }
            mCachedViewInsets.set(0, 0, 0, 0);
        } else if (!mCachedViewRect.equals(mCachedWindowRect)) {
            if (DEBUG) {
                Log.i(TAG, "WebView is not occupying the entire window, so no insets applied.");
            }
            mCachedViewInsets.set(0, 0, 0, 0);
        } else if (hasTransform()) {
            if (DEBUG) {
                Log.i(TAG, "WebView is rotated or scaled, so no insets applied.");
            }
            mCachedViewInsets.set(0, 0, 0, 0);
        } else {
            // We only apply this logic when webview is occupying the entire screen.
            adjustInsetsForScale(mCachedViewInsets, dipScale);
        }

        if (DEBUG) {
            Log.i(TAG,
                    "onApplyWindowInsetsInternal. displayCutoutInsets: " + displayCutoutInsets
                            + ", view rect: " + mCachedViewRect + ", display rect: "
                            + mCachedDisplayRect + ", window rect: " + mCachedWindowRect
                            + ", dip scale: " + dipScale + ", viewInsets: " + mCachedViewInsets);
        }
        mDelegate.setDisplayCutoutSafeArea(mCachedViewInsets);
    }

    private static void getViewRectOnScreen(View view, Rect rect) {
        if (view == null) {
            rect.set(0, 0, 0, 0);
            return;
        }
        view.getLocationOnScreen(sCachedLocationOnScreen);
        int width = view.getMeasuredWidth();
        int height = view.getMeasuredHeight();

        rect.set(sCachedLocationOnScreen[0], sCachedLocationOnScreen[1],
                sCachedLocationOnScreen[0] + width, sCachedLocationOnScreen[1] + height);
    }

    @SuppressLint("NewApi") // need this exception since we will try using Q API in P
    private boolean hasTransform() {
        mCachedMatrix.reset(); // set to identity
        // Check if a view coordinates transforms to screen coordinates that is not an identity
        // matrix, which means that view is rotated or scaled in regards to the screen.
        // This API got hidden from L, and readded in API 29 (Q). It seems that we can call this
        // on P most of the time, but adding try-catch just in case.
        try {
            mContainerView.transformMatrixToGlobal(mCachedMatrix);
        } catch (Throwable e) {
            return true;
        }
        return !mCachedMatrix.isIdentity();
    }

    private void onUpdateWindowInsets() {
        mContainerView.requestApplyInsets();
    }

    /** @see View#onSizeChanged(int, int, int, int) */
    public void onSizeChanged() {
        if (DEBUG) Log.i(TAG, "onSizeChanged");
        onUpdateWindowInsets();
    }

    /** @see View#onAttachedToWindow() */
    public void onAttachedToWindow() {
        if (DEBUG) Log.i(TAG, "onAttachedToWindow");
        onUpdateWindowInsets();
    }

    private static void adjustInsetsForScale(Insets insets, float dipScale) {
        insets.left = adjustOneInsetForScale(insets.left, dipScale);
        insets.top = adjustOneInsetForScale(insets.top, dipScale);
        insets.right = adjustOneInsetForScale(insets.right, dipScale);
        insets.bottom = adjustOneInsetForScale(insets.bottom, dipScale);
    }

    /**
     * Adjusts a WindowInset inset to a CSS pixel value.
     *
     * @param inset The inset as an integer.
     * @param dipScale The devices dip scale as an integer.
     * @return The CSS pixel value adjusted for scale.
     */
    private static int adjustOneInsetForScale(int inset, float dipScale) {
        return (int) Math.ceil(inset / dipScale);
    }
}