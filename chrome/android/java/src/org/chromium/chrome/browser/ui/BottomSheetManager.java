// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.util.TokenHolder;

/**
 * A class that manages activity-specific interactions with the BottomSheet component that it
 * otherwise shouldn't know about.
 */
class BottomSheetManager extends EmptyBottomSheetObserver
        implements Destroyable, ChromeFullscreenManager.FullscreenListener {
    /** A token for suppressing app modal dialogs. */
    private int mAppModalToken = TokenHolder.INVALID_TOKEN;

    /** A token for suppressing tab modal dialogs. */
    private int mTabModalToken = TokenHolder.INVALID_TOKEN;

    /** A handle to the {@link BottomSheetController} this class manages interactions with. */
    private BottomSheetController mSheetController;

    /** A mechanism for accessing the currently active tab. */
    private Supplier<Tab> mTabSupplier;

    /** A supplier of the {@link ChromeFullscreenManager}. */
    private Supplier<ChromeFullscreenManager> mFullscreenManager;

    /** A supplier of the activity's dialog manager. */
    private Supplier<ModalDialogManager> mDialogManager;

    /** A supplier of a snackbar manager for the bottom sheet. */
    private Supplier<SnackbarManager> mSnackbarManager;

    /** A delegate that provides the functionality of obscuring all tabs. */
    private TabObscuringHandler mTabObscuringHandler;

    /**
     * Used to track whether the active content has a custom scrim lifecycle. This is kept here
     * because there are some instances where the active content is changed prior to the close event
     * being called.
     */
    private boolean mContentHasCustomScrimLifecycle;

    public BottomSheetManager(BottomSheetController controller, Supplier<Tab> tabSupplier,
            Supplier<ChromeFullscreenManager> fullscreenManager,
            Supplier<ModalDialogManager> dialogManager,
            Supplier<SnackbarManager> snackbarManagerSupplier,
            TabObscuringHandler obscuringDelegate) {
        mSheetController = controller;
        mTabSupplier = tabSupplier;
        mFullscreenManager = fullscreenManager;
        mDialogManager = dialogManager;
        mSnackbarManager = snackbarManagerSupplier;
        mTabObscuringHandler = obscuringDelegate;

        mSheetController.addObserver(this);
        mFullscreenManager.get().addListener(BottomSheetManager.this);
    }

    @Override
    public void onSheetOpened(int reason) {
        Tab activeTab = mTabSupplier.get();
        if (activeTab != null) {
            WebContents webContents = activeTab.getWebContents();
            if (webContents != null) {
                SelectionPopupController.fromWebContents(webContents).clearSelection();
            }
        }

        BottomSheetContent content = mSheetController.getCurrentSheetContent();
        // Content with a custom scrim lifecycle should not obscure the tab. The feature
        // is responsible for adding itself to the list of obscuring views when applicable.
        if (content != null && content.hasCustomScrimLifecycle()) {
            mContentHasCustomScrimLifecycle = true;
            return;
        }

        mSheetController.setIsObscuringAllTabs(mTabObscuringHandler, true);

        assert mAppModalToken == TokenHolder.INVALID_TOKEN;
        assert mTabModalToken == TokenHolder.INVALID_TOKEN;
        if (mDialogManager.get() != null) {
            mAppModalToken =
                    mDialogManager.get().suspendType(ModalDialogManager.ModalDialogType.APP);
            mTabModalToken =
                    mDialogManager.get().suspendType(ModalDialogManager.ModalDialogType.TAB);
        }
    }

    @Override
    public void onSheetClosed(int reason) {
        BottomSheetContent content = mSheetController.getCurrentSheetContent();
        // If the content has a custom scrim, it wasn't obscuring tabs.
        if (mContentHasCustomScrimLifecycle) {
            mContentHasCustomScrimLifecycle = false;
            return;
        }

        mSheetController.setIsObscuringAllTabs(mTabObscuringHandler, false);

        // Tokens can be invalid if the sheet has a custom lifecycle.
        if (mDialogManager.get() != null
                && (mAppModalToken != TokenHolder.INVALID_TOKEN
                        || mTabModalToken != TokenHolder.INVALID_TOKEN)) {
            // If one modal dialog token is set, the other should be as well.
            assert mAppModalToken != TokenHolder.INVALID_TOKEN
                    && mTabModalToken != TokenHolder.INVALID_TOKEN;
            mDialogManager.get().resumeType(ModalDialogManager.ModalDialogType.APP, mAppModalToken);
            mDialogManager.get().resumeType(ModalDialogManager.ModalDialogType.TAB, mTabModalToken);
        }
        mAppModalToken = TokenHolder.INVALID_TOKEN;
        mTabModalToken = TokenHolder.INVALID_TOKEN;
    }

    @Override
    public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
        if (mSnackbarManager.get() == null) return;
        mSnackbarManager.get().dismissAllSnackbars();
    }

    @Override
    public void onContentOffsetChanged(int offset) {}

    @Override
    public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
            int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {}

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {}

    @Override
    public void destroy() {
        mSheetController.removeObserver(this);
        mFullscreenManager.get().removeListener(this);
    }
}
