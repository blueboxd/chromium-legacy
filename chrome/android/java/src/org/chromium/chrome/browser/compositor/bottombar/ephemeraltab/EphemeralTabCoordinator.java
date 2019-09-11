// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.ephemeraltab;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentDelegate;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentProgressObserver;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.embedder_support.view.ContentView;

/**
 * Central class for ephemeral tab, responsible for spinning off other classes necessary to display
 * the preview tab UI.
 */
public class EphemeralTabCoordinator {
    // TODO(crbug/1001256): Use Context after removing dependency on OverlayPanelContent.
    private final ChromeActivity mActivity;
    private final BottomSheetController mBottomSheetController;
    private OverlayPanelContent mPanelContent;
    private EphemeralTabSheetContent mSheetContent;
    private boolean mIsIncognito;

    /**
     * Constructor.
     * @param activity The associated {@link ChromeActivity}.
     * @param bottomSheetController The associated {@link BottomSheetController}.
     */
    public EphemeralTabCoordinator(
            ChromeActivity activity, BottomSheetController bottomSheetController) {
        mActivity = activity;
        mBottomSheetController = bottomSheetController;
        mBottomSheetController.getBottomSheet().addObserver(new EmptyBottomSheetObserver() {
            @Override
            public void onSheetStateChanged(int newState) {
                if (newState == BottomSheet.SheetState.HIDDEN) destroyContent();
            }
        });
    }

    /**
     * Entry point for ephemeral tab flow. This will create an ephemeral tab and show it in the
     * bottom sheet.
     * @param url The URL to be shown.
     * @param title The title to be shown.
     * @param isIncognito Whether we are currently in incognito mode.
     */
    public void requestOpenSheet(String url, String title, boolean isIncognito) {
        mIsIncognito = isIncognito;
        if (mSheetContent == null) mSheetContent = new EphemeralTabSheetContent(mActivity);

        // TODO(shaktisahu): If the sheet is already open, maybe close the sheet.

        getContent().loadUrl(url, true);
        mSheetContent.attachWebContents(
                getContent().getWebContents(), (ContentView) getContent().getContainerView());
        mSheetContent.setTitleText(title);
        mBottomSheetController.requestShowContent(mSheetContent, true);

        // TODO(donnd): Collect UMA with OverlayPanel.StateChangeReason.CLICK.
    }

    private OverlayPanelContent getContent() {
        if (mPanelContent == null) {
            // TODO(shaktisahu): Provide implementations for OverlayContentDelegate and
            // OverlayContentProgressObserver, to drive progress bar and favicon.
            mPanelContent = new OverlayPanelContent(new OverlayContentDelegate(),
                    new OverlayContentProgressObserver(), mActivity, mIsIncognito,
                    mActivity.getResources().getDimensionPixelSize(
                            R.dimen.overlay_panel_bar_height));
        }
        return mPanelContent;
    }

    private void destroyContent() {
        if (mSheetContent != null) {
            mSheetContent.destroy();
            mSheetContent = null;
        }

        if (mPanelContent != null) {
            mPanelContent.destroy();
            mPanelContent = null;
        }
    }
}
