// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.status_indicator;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewStub;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.components.browser_ui.widget.ViewResourceFrameLayout;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.resources.ResourceManager;

/**
 * The coordinator for a status indicator that is positioned below the status bar and is persistent.
 * Typically used to relay status, e.g. indicate user is offline.
 */
public class StatusIndicatorCoordinator {
    /** An observer that will be notified of the changes to the status indicator, e.g. height. */
    public interface StatusIndicatorObserver {
        /**
         * Called when the height of the status indicator changes.
         * @param newHeight The new height in pixels.
         */
        default void onStatusIndicatorHeightChanged(int newHeight) {}

        /**
         * Called when the background color of the status indicator changes.
         * @param newColor The new color as {@link ColorInt}.
         */
        default void onStatusIndicatorColorChanged(@ColorInt int newColor) {}
    }

    private StatusIndicatorMediator mMediator;
    private StatusIndicatorSceneLayer mSceneLayer;
    private boolean mIsShowing;
    private Runnable mRemoveOnLayoutChangeListener;

    /**
     * Constructs the status indicator.
     * @param activity The {@link Activity} to find and inflate the status indicator view.
     * @param resourceManager The {@link ResourceManager} for the status indicator's cc layer.
     * @param fullscreenManager The {@link ChromeFullscreenManager} to listen to for the changes in
     *                          controls offsets.
     * @param statusBarColorWithoutStatusIndicatorSupplier A supplier that will get the status bar
     *                                                     color without taking the status indicator
     *                                                     into account.
     * @param canAnimateNativeBrowserControls Will supply a boolean meaning whether the native
     *                                        browser controls can be animated. This will be false
     *                                        where we can't have a reliable cc::BCOM instance, e.g.
     *                                        tab switcher.
     * @param requestRender Runnable to request a render when the cc-layer needs to be updated.
     */
    public StatusIndicatorCoordinator(Activity activity, ResourceManager resourceManager,
            ChromeFullscreenManager fullscreenManager,
            Supplier<Integer> statusBarColorWithoutStatusIndicatorSupplier,
            Supplier<Boolean> canAnimateNativeBrowserControls, Callback<Runnable> requestRender) {
        // TODO(crbug.com/1005843): Create this view lazily if/when we need it. This is a task for
        // when we have the public API figured out. First, we should avoid inflating the view here
        // in case it's never used.
        final ViewStub stub = activity.findViewById(R.id.status_indicator_stub);
        ViewResourceFrameLayout root = (ViewResourceFrameLayout) stub.inflate();
        mSceneLayer = new StatusIndicatorSceneLayer(root, () -> fullscreenManager);
        PropertyModel model =
                new PropertyModel.Builder(StatusIndicatorProperties.ALL_KEYS)
                        .with(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY, View.GONE)
                        .with(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE, false)
                        .build();
        PropertyModelChangeProcessor.create(model,
                new StatusIndicatorViewBinder.ViewHolder(root, mSceneLayer),
                StatusIndicatorViewBinder::bind);
        Callback<Runnable> invalidateCompositorView = callback -> {
            root.getResourceAdapter().invalidate(null);
            requestRender.onResult(callback);
        };

        mMediator = new StatusIndicatorMediator(model, fullscreenManager,
                statusBarColorWithoutStatusIndicatorSupplier, canAnimateNativeBrowserControls,
                invalidateCompositorView);
        resourceManager.getDynamicResourceLoader().registerResource(
                root.getId(), root.getResourceAdapter());
        root.addOnLayoutChangeListener(mMediator);
        mRemoveOnLayoutChangeListener = () -> root.removeOnLayoutChangeListener(mMediator);
    }

    public void destroy() {
        mRemoveOnLayoutChangeListener.run();
    }

    // TODO(sinansahin): Destroy the view when not needed.

    /**
     * Show the status indicator with the initial properties with animations.
     *
     * @param statusText The status string that will be visible on the status indicator.
     * @param statusIcon The icon {@link Drawable} that will appear next to the status text.
     * @param backgroundColor The background color for the status indicator and the status bar.
     * @param textColor Status text color.
     * @param iconTint Status icon tint.
     */
    public void show(@NonNull String statusText, Drawable statusIcon, @ColorInt int backgroundColor,
            @ColorInt int textColor, @ColorInt int iconTint) {
        // TODO(sinansahin): Once we've moved the connectivity detection code to a separate class,
        // we should make sure #show(), #updateContent(), and #hide() are called correctly there,
        // e.g. show shouldn't be called if we're already showing. Then, we can turn these if checks
        // into asserts.
        if (mIsShowing) return;
        mIsShowing = true;

        mMediator.animateShow(statusText, statusIcon, backgroundColor, textColor, iconTint);
    }

    /**
     * Update the status indicator text, icon and colors with animations. All of the properties will
     * be animated even if only one property changes. Support to animate a single property may be
     * added in the future if needed.
     *
     * @param statusText The string that will replace the current text.
     * @param statusIcon The icon that will replace the current icon.
     * @param backgroundColor The color that will replace the status indicator background color.
     * @param textColor The new text color to fit the new background.
     * @param iconTint The new icon tint to fit the background.
     * @param animationCompleteCallback The callback that will be run once the animations end.
     */
    public void updateContent(@NonNull String statusText, Drawable statusIcon,
            @ColorInt int backgroundColor, @ColorInt int textColor, @ColorInt int iconTint,
            Runnable animationCompleteCallback) {
        if (!mIsShowing) return;

        mMediator.animateUpdate(statusText, statusIcon, backgroundColor, textColor, iconTint,
                animationCompleteCallback);
    }

    /**
     * Hide the status indicator with animations.
     */
    public void hide() {
        if (!mIsShowing) return;
        mIsShowing = false;

        mMediator.animateHide();
    }

    public void addObserver(StatusIndicatorObserver observer) {
        mMediator.addObserver(observer);
    }

    public void removeObserver(StatusIndicatorObserver observer) {
        mMediator.removeObserver(observer);
    }

    /**
     * @return The {@link StatusIndicatorSceneLayer}.
     */
    public StatusIndicatorSceneLayer getSceneLayer() {
        return mSceneLayer;
    }
}
