// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.content.Intent;

import androidx.annotation.Nullable;

import org.chromium.base.Consumer;
import org.chromium.components.omnibox.AutocompleteSchemeClassifier;
import org.chromium.components.page_info.PageInfoView.PageInfoViewParams;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Interface that provides embedder-level information to PageInfoController.
 */
public interface PageInfoControllerDelegate {
    /**
     * Creates an AutoCompleteClassifier.
     */
    AutocompleteSchemeClassifier createAutocompleteSchemeClassifier();

    /**
     * Whether cookie controls should be shown in Page Info UI.
     */
    boolean cookieControlsShown();

    /**
     * Return the ModalDialogManager to be used.
     */
    ModalDialogManager getModalDialogManager();

    /**
     * Whether Page Info UI should use dark colors.
     */
    boolean useDarkColors();

    /**
     * Initialize viewParams with Preview UI info, if any.
     * @param viewParams The params to be initialized with Preview UI info.
     * @param runAfterDismiss Used to set "show original" callback on Previews UI.
     */
    void initPreviewUiParams(PageInfoViewParams viewParams, Consumer<Runnable> runAfterDismiss);

    /**
     * Whether website dialog is displayed for a preview.
     */
    boolean isShowingPreview();

    /**
     * Whether Preview page state is INSECURE.
     */
    boolean isPreviewPageInsecure();

    /**
     * Returns whether or not an instant app is available for |url|.
     */
    boolean isInstantAppAvailable(String url);

    /**
     * Gets the instant app intent for the given URL if one exists.
     */
    Intent getInstantAppIntentForUrl(String url);

    /**
     * Returns a VrHandler for Page Info UI.
     */
    public VrHandler getVrHandler();

    /**
     * Gets the Url of the offline page being shown if any. Returns null otherwise.
     */
    @Nullable
    String getOfflinePageUrl();

    /**
     * Whether the page being shown is an offline page.
     */
    boolean isShowingOfflinePage();

    /**
     * Initialize viewParams with Offline Page UI info, if any.
     * @param viewParams The PageInfoViewParams to set state on.
     * @param consumer Used to set "open Online" button callback for offline page.
     */
    void initOfflinePageUiParams(PageInfoViewParams viewParams, Consumer<Runnable> runAfterDismiss);

    /**
     * Return the connection message shown for an offline page, if appropriate.
     * Returns null if there's no offline page.
     */
    @Nullable
    String getOfflinePageConnectionMessage();
}
