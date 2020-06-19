// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.scene_layer;

import android.content.Context;
import android.graphics.RectF;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.Layout.ViewportMode;
import org.chromium.chrome.browser.compositor.layouts.LayoutProvider;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.components.VirtualView;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EventFilter;
import org.chromium.chrome.browser.compositor.overlays.SceneOverlay;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.toolbar.ToolbarColors;
import org.chromium.components.browser_ui.widget.ClipDrawableProgressBar.DrawingInfo;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.resources.ResourceManager;

import java.util.List;

/**
 * A SceneLayer to render layers for the toolbar.
 */
@JNINamespace("android")
public class ToolbarSceneLayer extends SceneOverlayLayer implements SceneOverlay {
    /** Pointer to native ToolbarSceneLayer. */
    private long mNativePtr;

    /** Information used to draw the progress bar. */
    private DrawingInfo mProgressBarDrawingInfo;

    /** An Android Context. */
    private Context mContext;

    /** A LayoutProvider for accessing the current layout. */
    private LayoutProvider mLayoutProvider;

    /** A LayoutRenderHost for accessing drawing information about the toolbar. */
    private LayoutRenderHost mRenderHost;

    /** A Layout for browser controls. */
    private final ControlContainer mToolbarContainer;

    /** Provides current tab. */
    private final Supplier<Tab> mCurrentTab;

    /**
     * @param context An Android context to use.
     * @param provider A LayoutProvider for accessing the current layout.
     * @param renderHost A LayoutRenderHost for accessing drawing information about the toolbar.
     * @param toolbarContainer A Layout for browser controls.
     * @param currentTab A supplier for the current tab.
     */
    public ToolbarSceneLayer(Context context, LayoutProvider provider, LayoutRenderHost renderHost,
            ControlContainer toolbarContainer, Supplier<Tab> currentTab) {
        mContext = context;
        mLayoutProvider = provider;
        mRenderHost = renderHost;
        mToolbarContainer = toolbarContainer;
        mCurrentTab = currentTab;
    }

    /**
     * Update the toolbar and progress bar layers.
     *
     * @param browserControlsBackgroundColor The background color of the browser controls.
     * @param browserControlsUrlBarAlpha The alpha of the URL bar.
     * @param browserControlsStateProvider A BrowserControlsStateProvider instance.
     * @param resourceManager A ResourceManager for loading static resources.
     * @param forceHideAndroidBrowserControls True if the Android browser controls are being hidden.
     * @param viewportMode The sizing mode of the viewport being drawn in.
     * @param isTablet If the device is a tablet.
     * @param windowHeight The height of the window.
     */
    private void update(int browserControlsBackgroundColor, float browserControlsUrlBarAlpha,
            BrowserControlsStateProvider browserControlsStateProvider,
            ResourceManager resourceManager, boolean forceHideAndroidBrowserControls,
            @ViewportMode int viewportMode, boolean isTablet, float windowHeight) {
        if (!DeviceClassManager.enableFullscreen()) return;

        if (browserControlsStateProvider == null) return;
        if (!isTablet && mToolbarContainer != null) {
            if (mProgressBarDrawingInfo == null) mProgressBarDrawingInfo = new DrawingInfo();
            mToolbarContainer.getProgressBarDrawingInfo(mProgressBarDrawingInfo);
        } else {
            assert mProgressBarDrawingInfo == null;
        }

        // Texture is always used unless it is completely off-screen.
        boolean useTexture =
                !BrowserControlsUtils.areBrowserControlsOffScreen(browserControlsStateProvider)
                && viewportMode != ViewportMode.ALWAYS_FULLSCREEN;
        boolean drawControlsAsTexture =
                BrowserControlsUtils.drawControlsAsTexture(browserControlsStateProvider);
        boolean showShadow = drawControlsAsTexture || forceHideAndroidBrowserControls;

        int textBoxColor = ToolbarColors.getTextBoxColorForToolbarBackground(
                mContext.getResources(), mCurrentTab.get(), browserControlsBackgroundColor);

        int textBoxResourceId = R.drawable.modern_location_bar;
        // The content offset is passed to the toolbar layer so that it can position itself at the
        // bottom of the space available for top controls. The main reason for using content offset
        // instead of top controls offset is that top controls can have a greater height than that
        // of the toolbar, e.g. when status indicator is visible, and the toolbar needs to be
        // positioned at the bottom of the top controls regardless of the total height.
        ToolbarSceneLayerJni.get().updateToolbarLayer(mNativePtr, ToolbarSceneLayer.this,
                resourceManager, R.id.control_container, browserControlsBackgroundColor,
                textBoxResourceId, browserControlsUrlBarAlpha, textBoxColor,
                browserControlsStateProvider.getContentOffset(), windowHeight, useTexture,
                showShadow);

        if (mProgressBarDrawingInfo == null) return;
        ToolbarSceneLayerJni.get().updateProgressBar(mNativePtr, ToolbarSceneLayer.this,
                mProgressBarDrawingInfo.progressBarRect.left,
                mProgressBarDrawingInfo.progressBarRect.top,
                mProgressBarDrawingInfo.progressBarRect.width(),
                mProgressBarDrawingInfo.progressBarRect.height(),
                mProgressBarDrawingInfo.progressBarColor,
                mProgressBarDrawingInfo.progressBarBackgroundRect.left,
                mProgressBarDrawingInfo.progressBarBackgroundRect.top,
                mProgressBarDrawingInfo.progressBarBackgroundRect.width(),
                mProgressBarDrawingInfo.progressBarBackgroundRect.height(),
                mProgressBarDrawingInfo.progressBarBackgroundColor);
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        ToolbarSceneLayerJni.get().setContentTree(mNativePtr, ToolbarSceneLayer.this, contentTree);
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = ToolbarSceneLayerJni.get().init(ToolbarSceneLayer.this);
        }
        assert mNativePtr != 0;
    }

    /**
     * Destroys this object and the corresponding native component.
     */
    @Override
    public void destroy() {
        super.destroy();
        mNativePtr = 0;
    }

    // SceneOverlay implementation.

    @Override
    public SceneOverlayLayer getUpdatedSceneOverlayTree(RectF viewport, RectF visibleViewport,
            LayerTitleCache layerTitleCache, ResourceManager resourceManager, float yOffset) {
        boolean forceHideBrowserControlsAndroidView =
                mLayoutProvider.getActiveLayout().forceHideBrowserControlsAndroidView();
        @ViewportMode
        int viewportMode = mLayoutProvider.getActiveLayout().getViewportMode();

        // In Chrome modern design, the url bar is always opaque since it is drawn in the
        // compositor.
        float alpha = 1;

        update(mRenderHost.getBrowserControlsBackgroundColor(mContext.getResources()), alpha,
                mLayoutProvider.getFullscreenManager(), resourceManager,
                forceHideBrowserControlsAndroidView, viewportMode,
                DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext), viewport.height());

        return this;
    }

    @Override
    public boolean isSceneOverlayTreeShowing() {
        return true;
    }

    @Override
    public EventFilter getEventFilter() {
        return null;
    }

    @Override
    public void onSizeChanged(
            float width, float height, float visibleViewportOffsetY, int orientation) {}

    @Override
    public void getVirtualViews(List<VirtualView> views) {}

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
    public void onHideLayout() {}

    @Override
    public boolean handlesTabCreating() {
        return false;
    }

    @Override
    public void tabTitleChanged(int tabId, String title) {}

    @Override
    public void tabStateInitialized() {}

    @Override
    public void tabModelSwitched(boolean incognito) {}

    @Override
    public void tabCreated(long time, boolean incognito, int id, int prevId, boolean selected) {}

    @NativeMethods
    interface Natives {
        long init(ToolbarSceneLayer caller);
        void setContentTree(
                long nativeToolbarSceneLayer, ToolbarSceneLayer caller, SceneLayer contentTree);
        void updateToolbarLayer(long nativeToolbarSceneLayer, ToolbarSceneLayer caller,
                ResourceManager resourceManager, int resourceId, int toolbarBackgroundColor,
                int urlBarResourceId, float urlBarAlpha, int urlBarColor, float contentOffset,
                float viewHeight, boolean visible, boolean showShadow);
        void updateProgressBar(long nativeToolbarSceneLayer, ToolbarSceneLayer caller,
                int progressBarX, int progressBarY, int progressBarWidth, int progressBarHeight,
                int progressBarColor, int progressBarBackgroundX, int progressBarBackgroundY,
                int progressBarBackgroundWidth, int progressBarBackgroundHeight,
                int progressBarBackgroundColor);
    }
}
