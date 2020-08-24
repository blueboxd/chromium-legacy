// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.status_indicator;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
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
    private PropertyModelChangeProcessor mMCP;
    private StatusIndicatorSceneLayer mSceneLayer;
    private boolean mIsShowing;
    private Runnable mRemoveOnLayoutChangeListener;

    /**
     * Constructs the status indicator.
     * @param activity The {@link Activity} to find and inflate the status indicator view.
     * @param resourceManager The {@link ResourceManager} for the status indicator's cc layer.
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} to listen to
     *                                     for the changes in controls offsets.
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
            BrowserControlsStateProvider browserControlsStateProvider,
            Supplier<Integer> statusBarColorWithoutStatusIndicatorSupplier,
            Supplier<Boolean> canAnimateNativeBrowserControls, Callback<Runnable> requestRender) {
        mSceneLayer = new StatusIndicatorSceneLayer(browserControlsStateProvider);

        // This will create the view before showing it.
        Runnable inflateView = () -> {
            ViewStub stub = activity.findViewById(R.id.status_indicator_stub);
            ViewResourceFrameLayout root = (ViewResourceFrameLayout) stub.inflate();
            resourceManager.getDynamicResourceLoader().registerResource(
                    root.getId(), root.getResourceAdapter());
            mSceneLayer.setResourceId(root.getId());
            PropertyModel model =
                    new PropertyModel.Builder(StatusIndicatorProperties.ALL_KEYS)
                            .with(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY, View.GONE)
                            .with(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE, false)
                            .build();
            mMCP = PropertyModelChangeProcessor.create(model,
                    new StatusIndicatorViewBinder.ViewHolder(root, mSceneLayer),
                    StatusIndicatorViewBinder::bind);
            mMediator.setModel(model);
            root.addOnLayoutChangeListener(mMediator);
            mRemoveOnLayoutChangeListener = () -> root.removeOnLayoutChangeListener(mMediator);
        };

        // This will run to destroy the view once it's hidden.
        Runnable destroyView = () -> {
            mRemoveOnLayoutChangeListener.run();
            mRemoveOnLayoutChangeListener = null;
            ViewResourceFrameLayout root = activity.findViewById(R.id.status_indicator);
            mSceneLayer.setResourceId(0);
            resourceManager.getDynamicResourceLoader().unregisterResource(root.getId());
            mMediator.setModel(null);
            mMCP.destroy();
            mMCP = null;

            // Remove the view and add a ViewStub in its place. This is basically returning the
            // view tree to its initial condition and will make it possible to inflate the view
            // easily the next time it's needed, i.e. next #show() call.
            final ViewGroup parent = ((ViewGroup) root.getParent());
            final int index = parent.indexOfChild(root);
            parent.removeViewAt(index);
            final ViewStub stub = new ViewStub(activity);
            stub.setLayoutParams(new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            stub.setId(R.id.status_indicator_stub);
            stub.setInflatedId(R.id.status_indicator);
            stub.setLayoutResource(R.layout.status_indicator_container);
            parent.addView(stub, index);
        };
        Callback<Runnable> invalidateCompositorView = callback -> {
            ViewResourceFrameLayout root = activity.findViewById(R.id.status_indicator);
            root.getResourceAdapter().invalidate(null);
            requestRender.onResult(callback);
        };
        Runnable requestLayout = () -> {
            ViewResourceFrameLayout root = activity.findViewById(R.id.status_indicator);
            root.requestLayout();
        };
        mMediator = new StatusIndicatorMediator(browserControlsStateProvider,
                statusBarColorWithoutStatusIndicatorSupplier, inflateView, destroyView,
                canAnimateNativeBrowserControls, invalidateCompositorView, requestLayout);
    }

    public void destroy() {
        if (mRemoveOnLayoutChangeListener != null) {
            mRemoveOnLayoutChangeListener.run();
        }
        mMediator.destroy();
    }

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
        // TODO(crbug.com/1081471): We should make sure #show, #hide, and #updateContent can't be
        // called at the wrong time, or the call is ignored with a way to communicate this to the
        // caller, e.g. returning a boolean.
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
