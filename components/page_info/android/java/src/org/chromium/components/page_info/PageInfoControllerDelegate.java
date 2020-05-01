// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.content.Intent;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.Consumer;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.omnibox.AutocompleteSchemeClassifier;
import org.chromium.components.page_info.PageInfoView.PageInfoViewParams;
import org.chromium.ui.base.AndroidPermissionDelegate;
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

    /**
     * Returns whether or not the performance badge should be shown for |url|.
     */
    boolean shouldShowPerformanceBadge(String url);

    /**
     * Whether Site settings are available.
     */
    boolean isSiteSettingsAvailable();

    /**
     * Show site settings for the URL passed in.
     * @param url The URL to show site settings for.
     */
    void showSiteSettings(String url);

    // TODO(crbug.com/1052375): Remove the next three methods when cookie controls UI
    // has been componentized.
    /**
     * Creates Cookie Controls Bridge.
     * @param The CookieControlsObserver to create the bridge with.
     */
    void createCookieControlsBridge(CookieControlsObserver observer);

    /**
     * Called when cookie controls UI is closed.
     */
    void onUiClosing();

    /**
     * Notes whether third party cookies should be blocked for the site.
     */
    void setThirdPartyCookieBlockingEnabledForSite(boolean blockCookies);

    // TODO(crbug.com/1058595): Componentize PermissionParamsBuilder once site settings code has
    // been componentized. These methods can be removed at that point.
    /**
     * Creates a Permission Params List builder and holds on to it.
     * @param permissionDelegate Delegate for checking system permissions.
     * @param fullUrl Full URL of the site whose permissions are being displayed.
     * @param shouldShowTitle Should show section title for permissions in Page Info UI.
     * @param systemSettingsActivityRequiredListener Listener for when we need the user to enable
     *                                               a system setting to proceed.
     * @param displayPermissionsCallback Callback to run to display fresh permissions in response to
     *                                   user interaction with a permission entry.
     */
    void createPermissionParamsListBuilder(AndroidPermissionDelegate permissionDelegate,
            String fullUrl, boolean shouldShowTitle,
            SystemSettingsActivityRequiredListener systemSettingsActivityRequiredListener,
            Callback<PageInfoView.PermissionParams> displayPermissionsCallback);

    /**
     * Adds a permission entry corresponding to the input params.
     * @param name Name of the permission to add.
     * @param type Type of the permission to add.
     * @param currentSettingValue The value of the permission to add.
     */
    void addPermissionEntry(String name, int type, @ContentSettingValues int currentSettingValue);

    /**
     * Updates the Page Info View passed in with up to date permissions info.
     * @param view The Page Info view to update.
     */
    void updatePermissionDisplay(PageInfoView view);
}
