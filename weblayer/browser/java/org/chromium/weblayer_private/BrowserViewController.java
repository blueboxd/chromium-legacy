// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.res.Resources;
import android.os.RemoteException;
import android.util.AndroidRuntimeException;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.webkit.ValueCallback;
import android.widget.FrameLayout;
import android.widget.RelativeLayout;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * BrowserViewController controls the set of Views needed to show the WebContents.
 */
@JNINamespace("weblayer")
public final class BrowserViewController
        implements TopControlsContainerView.Listener,
                   WebContentsGestureStateTracker.OnGestureStateChangedListener,
                   ModalDialogManager.ModalDialogManagerObserver {
    private final ContentViewRenderView mContentViewRenderView;
    private final ContentView mContentView;
    // Child of mContentView, holds top-view from client.
    private final TopControlsContainerView mTopControlsContainerView;
    // Other child of mContentView, which holds views that sit on top of the web contents, such as
    // tab modal dialogs.
    private final FrameLayout mWebContentsOverlayView;

    private final FragmentWindowAndroid mWindowAndroid;
    private final ModalDialogManager mModalDialogManager;

    private TabImpl mTab;

    private WebContentsGestureStateTracker mGestureStateTracker;

    /**
     * The value of mCachedDoBrowserControlsShrinkRendererSize is set when
     * WebContentsGestureStateTracker begins a gesture. This is necessary as the values should only
     * change once a gesture is no longer under way.
     */
    private boolean mCachedDoBrowserControlsShrinkRendererSize;

    public BrowserViewController(FragmentWindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
        Context context = mWindowAndroid.getContext().get();
        mContentViewRenderView = new ContentViewRenderView(context);

        mContentViewRenderView.onNativeLibraryLoaded(
                mWindowAndroid, ContentViewRenderView.MODE_SURFACE_VIEW);
        mTopControlsContainerView =
                new TopControlsContainerView(context, mContentViewRenderView, this);
        mTopControlsContainerView.setId(View.generateViewId());
        mContentView = ContentView.createContentView(
                context, mTopControlsContainerView.getEventOffsetHandler());
        mContentViewRenderView.addView(mContentView,
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.UNSPECIFIED_GRAVITY));
        mContentView.addView(mTopControlsContainerView,
                new RelativeLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        mWebContentsOverlayView = new FrameLayout(context);
        RelativeLayout.LayoutParams overlayParams =
                new RelativeLayout.LayoutParams(LayoutParams.MATCH_PARENT, 0);
        overlayParams.addRule(RelativeLayout.BELOW, mTopControlsContainerView.getId());
        overlayParams.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
        mContentView.addView(mWebContentsOverlayView, overlayParams);
        mWindowAndroid.setAnimationPlaceholderView(mWebContentsOverlayView);

        mModalDialogManager = new ModalDialogManager(
                new AppModalPresenter(context), ModalDialogManager.ModalDialogType.APP);
        mModalDialogManager.addObserver(this);
        mModalDialogManager.registerPresenter(
                new WebLayerTabModalPresenter(this, context), ModalDialogType.TAB);
        mWindowAndroid.setModalDialogManager(mModalDialogManager);
    }

    public void destroy() {
        mWindowAndroid.setModalDialogManager(null);
        setActiveTab(null);
        mTopControlsContainerView.destroy();
        mContentViewRenderView.destroy();
    }

    /** Returns top-level View this Controller works with */
    public View getView() {
        return mContentViewRenderView;
    }

    public ViewGroup getContentView() {
        return mContentView;
    }

    public FrameLayout getWebContentsOverlayView() {
        return mWebContentsOverlayView;
    }

    public void setActiveTab(TabImpl tab) {
        if (tab == mTab) return;

        if (mTab != null) {
            mTab.onDidLoseActive();
            // WebContentsGestureStateTracker is relatively cheap, easier to destroy rather than
            // update WebContents.
            mGestureStateTracker.destroy();
            mGestureStateTracker = null;
        }

        mModalDialogManager.dismissDialogsOfType(
                ModalDialogType.TAB, DialogDismissalCause.TAB_SWITCHED);

        mTab = tab;
        WebContents webContents = mTab != null ? mTab.getWebContents() : null;
        // Create the WebContentsGestureStateTracker before setting the WebContents on
        // the views as they may call back to this class.
        if (mTab != null) {
            mGestureStateTracker =
                    new WebContentsGestureStateTracker(mContentView, webContents, this);
        }
        mContentView.setTab(mTab);

        mContentViewRenderView.setWebContents(webContents);
        mTopControlsContainerView.setWebContents(webContents);
        if (mTab != null) {
            mTab.onDidGainActive(mTopControlsContainerView.getNativeHandle());
            mContentView.requestFocus();
        }
    }

    public TabImpl getTab() {
        return mTab;
    }

    public void setTopView(View view) {
        mTopControlsContainerView.setView(view);
    }

    @Override
    public void onTopControlsCompletelyShownOrHidden() {
        adjustWebContentsHeightIfNecessary();
    }

    @Override
    public void onGestureStateChanged() {
        if (mGestureStateTracker.isInGestureOrScroll()) {
            mCachedDoBrowserControlsShrinkRendererSize =
                    mTopControlsContainerView.isTopControlVisible();
        }
        adjustWebContentsHeightIfNecessary();
    }

    @Override
    public void onDialogShown(PropertyModel model) {
        onDialogVisibilityChanged(true);
    }

    @Override
    public void onDialogHidden(PropertyModel model) {
        onDialogVisibilityChanged(false);
    }

    private void onDialogVisibilityChanged(boolean showing) {
        if (WebLayerFactoryImpl.getClientMajorVersion() < 82) return;

        if (mModalDialogManager.getCurrentType() == ModalDialogType.TAB) {
            try {
                mTab.getClient().onTabModalStateChanged(showing);
            } catch (RemoteException e) {
                throw new AndroidRuntimeException(e);
            }
        }
    }

    private void adjustWebContentsHeightIfNecessary() {
        if (mGestureStateTracker.isInGestureOrScroll()
                || !mTopControlsContainerView.isTopControlsCompletelyShownOrHidden()) {
            return;
        }
        mContentViewRenderView.setWebContentsHeightDelta(
                mTopControlsContainerView.getTopContentOffset());
    }

    public void setSupportsEmbedding(boolean enable, ValueCallback<Boolean> callback) {
        mContentViewRenderView.requestMode(enable ? ContentViewRenderView.MODE_TEXTURE_VIEW
                                                  : ContentViewRenderView.MODE_SURFACE_VIEW,
                callback);
    }

    public void onTopControlsChanged(int topControlsOffsetY, int topContentOffsetY) {
        mTopControlsContainerView.onTopControlsChanged(topControlsOffsetY, topContentOffsetY);
    }

    public boolean doBrowserControlsShrinkRendererSize() {
        return (mGestureStateTracker.isInGestureOrScroll())
                ? mCachedDoBrowserControlsShrinkRendererSize
                : mTopControlsContainerView.isTopControlVisible();
    }

    /**
     * @return true if a tab modal was showing and has been dismissed.
     */
    public boolean dismissTabModalOverlay() {
        return mModalDialogManager.dismissActiveDialogOfType(
                ModalDialogType.TAB, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
    }

    /**
     * Asks the user to confirm a page reload on a POSTed page.
     */
    public void showRepostFormWarningDialog() {
        ModalDialogProperties.Controller dialogController =
                new SimpleModalDialogController(mModalDialogManager, (Integer dismissalCause) -> {
                    WebContents webContents = mTab == null ? null : mTab.getWebContents();
                    if (webContents == null) return;
                    switch (dismissalCause) {
                        case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                            webContents.getNavigationController().continuePendingReload();
                            break;
                        default:
                            webContents.getNavigationController().cancelPendingReload();
                            break;
                    }
                });

        Resources resources = mWindowAndroid.getContext().get().getResources();
        PropertyModel dialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, dialogController)
                        .with(ModalDialogProperties.TITLE, resources,
                                R.string.http_post_warning_title)
                        .with(ModalDialogProperties.MESSAGE, resources, R.string.http_post_warning)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources,
                                R.string.http_post_warning_resend)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                R.string.cancel)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .build();

        mModalDialogManager.showDialog(dialogModel, ModalDialogManager.ModalDialogType.TAB, true);
    }
}
