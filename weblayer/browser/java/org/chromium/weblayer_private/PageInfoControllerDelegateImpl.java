// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Intent;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.Consumer;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.omnibox.AutocompleteSchemeClassifier;
import org.chromium.components.page_info.PageInfoControllerDelegate;
import org.chromium.components.page_info.PageInfoView;
import org.chromium.components.page_info.PageInfoView.PageInfoViewParams;
import org.chromium.components.page_info.SystemSettingsActivityRequiredListener;
import org.chromium.components.page_info.VrHandler;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * WebLayer's implementation of PageInfoControllerDelegate.
 */
public class PageInfoControllerDelegateImpl implements PageInfoControllerDelegate {
    private final BrowserImpl mBrowser;
    private final VrHandler mVrHandler;
    public PageInfoControllerDelegateImpl(BrowserImpl browser) {
        mBrowser = browser;
        mVrHandler = new VrHandler() {
            @Override
            public boolean isInVr() {
                return false;
            }
            @Override
            public void exitVrAndRun(Runnable r, @VrHandler.UiType int uiType) {
                assert false;
            }
        };
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public AutocompleteSchemeClassifier createAutocompleteSchemeClassifier() {
        return new AutocompleteSchemeClassifierImpl();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean cookieControlsShown() {
        return false;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public ModalDialogManager getModalDialogManager() {
        return ((FragmentWindowAndroid) mBrowser.getWindowAndroid()).getModalDialogManager();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean useDarkColors() {
        return !mBrowser.getDarkThemeEnabled();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void initPreviewUiParams(
            PageInfoViewParams viewParams, Consumer<Runnable> runAfterDismiss) {
        // Preview UI is not supported for WebLayer.
        viewParams.previewUIShown = false;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean isShowingPreview() {
        // Preview UI is not supported for WebLayer.
        return false;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean isPreviewPageInsecure() {
        // Preview UI is not supported for WebLayer.
        return false;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean isInstantAppAvailable(String url) {
        // Instant apps aren't yet supported for WebLayer.
        return false;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public Intent getInstantAppIntentForUrl(String url) {
        // Instant apps aren't yet supported for WebLayer.
        return null;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public VrHandler getVrHandler() {
        return mVrHandler;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public String getOfflinePageUrl() {
        // Offline Pages are not supported for WebLayer.
        return null;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean isShowingOfflinePage() {
        // Offline Pages are not supported for WebLayer.
        return false;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void initOfflinePageUiParams(
            PageInfoViewParams viewParams, Consumer<Runnable> runAfterDismiss) {
        // Offline Pages are not supported for WebLayer.
        viewParams.openOnlineButtonShown = false;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    @Nullable
    public String getOfflinePageConnectionMessage() {
        // Offline Pages are not supported for WebLayer.
        return null;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean isSiteSettingsAvailable() {
        // TODO(crbug.com/1058595): Once SiteSettingsHelper is componentized, add logic here.
        return false;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void showSiteSettings(String url) {
        // TODO(crbug.com/1058595): Once SiteSettingsHelper is componentized, add logic here.
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void createCookieControlsBridge(CookieControlsObserver observer) {
        // TODO(crbug.com/1058597): Once CookieControlsBridge is componentized, add logic here.
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void onUiClosing() {
        // TODO(crbug.com/1058597): Once CookieControlsBridge is componentized, add logic here.
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void setThirdPartyCookieBlockingEnabledForSite(boolean blockCookies) {
        // TODO(crbug.com/1058597): Once CookieControlsBridge is componentized, add logic here.
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void createPermissionParamsListBuilder(AndroidPermissionDelegate permissionDelegate,
            String fullUrl, boolean shouldShowTitle,
            SystemSettingsActivityRequiredListener systemSettingsActivityRequiredListener,
            Callback<PageInfoView.PermissionParams> displayPermissionsCallback) {
        // TODO(crbug.com/1058597): Once PermissionParamsListBuilder is componentized, add logic
        // here.
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void addPermissionEntry(
            String name, int type, @ContentSettingValues int currentSettingValue) {
        // TODO(crbug.com/1058597): Once PermissionParamsListBuilder is componentized, add logic
        // here.
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void updatePermissionDisplay(PageInfoView view) {
        // TODO(crbug.com/1058597): Once PermissionParamsListBuilder is componentized, add logic
        // here.
    }
}
