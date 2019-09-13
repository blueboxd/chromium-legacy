// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.status_indicator;

import android.graphics.RectF;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.components.VirtualView;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EventFilter;
import org.chromium.chrome.browser.compositor.overlays.SceneOverlay;
import org.chromium.chrome.browser.compositor.scene_layer.SceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.SceneOverlayLayer;
import org.chromium.chrome.browser.widget.ViewResourceFrameLayout;
import org.chromium.ui.resources.ResourceManager;

import java.util.List;

/**
 * A composited view that is positioned below the status bar and is persistent. Typically used to
 * relay status, e.g. indicate user is offline.
 */
@JNINamespace("android")
class StatusIndicatorSceneLayer extends SceneOverlayLayer implements SceneOverlay {
    /** Handle to the native side of this class. */
    private long mNativePtr;

    /** The resource ID used to reference the view bitmap in native. */
    private int mResourceId;

    private boolean mIsVisible;

    /** Build a composited status view layer. */
    public StatusIndicatorSceneLayer() {}

    /**
     * Build a composited status view layer.
     * @param resourceManager A resource manager for dynamic resource creation.
     * @param statusIndicator The view used to generate the composited version.
     */
    public StatusIndicatorSceneLayer(
            ResourceManager resourceManager, ViewResourceFrameLayout statusIndicator) {
        mResourceId = statusIndicator.getId();
        resourceManager.getDynamicResourceLoader().registerResource(
                mResourceId, statusIndicator.getResourceAdapter());
    }

    /**
     * Change the visibility of the scene layer.
     * @param visible True if visible.
     */
    public void setIsVisible(boolean visible) {
        mIsVisible = visible;
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = nativeInit();
        }
        assert mNativePtr != 0;
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        nativeSetContentTree(mNativePtr, contentTree);
    }

    @Override
    public SceneOverlayLayer getUpdatedSceneOverlayTree(RectF viewport, RectF visibleViewport,
            LayerTitleCache layerTitleCache, ResourceManager resourceManager, float yOffset) {
        nativeUpdateStatusIndicatorLayer(mNativePtr, resourceManager, mResourceId);
        return this;
    }

    @Override
    public boolean isSceneOverlayTreeShowing() {
        return mIsVisible;
    }

    @Override
    public EventFilter getEventFilter() {
        return null;
    }

    @Override
    public boolean shouldHideAndroidBrowserControls() {
        return false;
    }

    @Override
    public boolean updateOverlay(long time, long dt) {
        return false;
    }

    @Override
    public boolean onBackPressed() {
        return false;
    }

    @Override
    public boolean handlesTabCreating() {
        return false;
    }

    @Override
    public void onSizeChanged(
            float width, float height, float visibleViewportOffsetY, int orientation) {}

    @Override
    public void getVirtualViews(List<VirtualView> views) {}

    @Override
    public void onHideLayout() {}

    @Override
    public void tabTitleChanged(int tabId, String title) {}

    @Override
    public void tabStateInitialized() {}

    @Override
    public void tabModelSwitched(boolean incognito) {}

    @Override
    public void tabCreated(long time, boolean incognito, int id, int prevId, boolean selected) {}

    private native long nativeInit();
    private native void nativeSetContentTree(
            long nativeStatusIndicatorSceneLayer, SceneLayer contentTree);
    private native void nativeUpdateStatusIndicatorLayer(long nativeStatusIndicatorSceneLayer,
            ResourceManager resourceManager, int viewResourceId);
}
