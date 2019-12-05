// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.graphics.Color;
import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.ui.widget.TintedDrawable;

/**
 * Stores info about a web app.
 */
public class WebappIntentDataProvider extends BrowserServicesIntentDataProvider {
    private int mToolbarColor;
    private boolean mHasCustomToolbarColor;
    private Drawable mCloseButtonIcon;
    private WebappExtras mWebappExtras;
    private WebApkExtras mWebApkExtras;

    /**
     * Returns the toolbar color to use if a custom color is not specified by the webapp.
     */
    public static int getDefaultToolbarColor() {
        return Color.WHITE;
    }

    WebappIntentDataProvider(int toolbarColor, boolean hasCustomToolbarColor,
            WebappExtras webappExtras, WebApkExtras webApkExtras) {
        mToolbarColor = toolbarColor;
        mHasCustomToolbarColor = hasCustomToolbarColor;
        mCloseButtonIcon = TintedDrawable.constructTintedDrawable(
                ContextUtils.getApplicationContext(), R.drawable.btn_close);
        mWebappExtras = webappExtras;
        mWebApkExtras = webApkExtras;
    }

    @Override
    public int getToolbarColor() {
        return mToolbarColor;
    }

    @Override
    public boolean hasCustomToolbarColor() {
        return mHasCustomToolbarColor;
    }

    @Override
    public Drawable getCloseButtonDrawable() {
        return mCloseButtonIcon;
    }

    @Override
    public int getTitleVisibilityState() {
        return CustomTabsIntent.SHOW_PAGE_TITLE;
    }

    @Override
    @Nullable
    public WebappExtras getWebappExtras() {
        return mWebappExtras;
    }

    @Override
    @Nullable
    public WebApkExtras getWebApkExtras() {
        return mWebApkExtras;
    }
}
