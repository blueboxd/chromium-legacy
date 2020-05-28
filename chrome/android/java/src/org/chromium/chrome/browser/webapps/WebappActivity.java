// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler.IntentIgnoringCriterion;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.dependency_injection.BaseCustomTabActivityModule;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityCommonsModule;
import org.chromium.chrome.browser.webapps.dependency_injection.WebappActivityComponent;
import org.chromium.webapk.lib.common.WebApkConstants;

/**
 * Displays a webapp in a nearly UI-less Chrome (InfoBars still appear).
 */
public class WebappActivity extends BaseCustomTabActivity<WebappActivityComponent> {
    public static final String WEBAPP_SCHEME = "webapp";

    private static final String TAG = "WebappActivity";

    private static BrowserServicesIntentDataProvider sIntentDataProviderOverride;

    private TabObserverRegistrar mTabObserverRegistrar;

    @Override
    protected BrowserServicesIntentDataProvider buildIntentDataProvider(
            Intent intent, @CustomTabsIntent.ColorScheme int colorScheme) {
        if (intent == null) return null;

        if (sIntentDataProviderOverride != null) {
            return sIntentDataProviderOverride;
        }

        return TextUtils.isEmpty(WebappIntentUtils.getWebApkPackageName(intent))
                ? WebappIntentDataProviderFactory.create(intent)
                : WebApkIntentDataProviderFactory.create(intent);
    }

    @VisibleForTesting
    public static void setIntentDataProviderForTesting(
            BrowserServicesIntentDataProvider intentDataProvider) {
        sIntentDataProviderOverride = intentDataProvider;
    }

    @Override
    public boolean shouldPreferLightweightFre(Intent intent) {
        // We cannot get WebAPK package name from BrowserServicesIntentDataProvider because
        // {@link WebappActivity#performPreInflationStartup()} may not have been called yet.
        String webApkPackageName =
                IntentUtils.safeGetStringExtra(intent, WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME);

        // Use the lightweight FRE for unbound WebAPKs.
        return webApkPackageName != null
                && !webApkPackageName.startsWith(WebApkConstants.WEBAPK_PACKAGE_PREFIX);
    }

    @Override
    protected WebappActivityComponent createComponent(ChromeActivityCommonsModule commonsModule) {
        IntentIgnoringCriterion intentIgnoringCriterion =
                (intent) -> mIntentHandler.shouldIgnoreIntent(intent);

        BaseCustomTabActivityModule baseCustomTabModule =
                new BaseCustomTabActivityModule(mIntentDataProvider, getStartupTabPreloader(),
                        mNightModeStateController, intentIgnoringCriterion);
        WebappActivityComponent component =
                ChromeApplication.getComponent().createWebappActivityComponent(
                        commonsModule, baseCustomTabModule);
        onComponentCreated(component);

        mTabObserverRegistrar = component.resolveTabObserverRegistrar();

        mNavigationController.setFinishHandler((reason) -> { handleFinishAndClose(); });

        return component;
    }

    @Override
    public void onStopWithNative() {
        super.onStopWithNative();
        getFullscreenManager().exitPersistentFullscreenMode();
    }

    @Override
    public boolean onMenuOrKeyboardAction(int id, boolean fromMenu) {
        // Disable creating bookmark.
        if (id == R.id.bookmark_this_page_id) {
            return true;
        }
        if (id == R.id.open_in_browser_id) {
            mNavigationController.openCurrentUrlInBrowser(false);
            if (fromMenu) {
                RecordUserAction.record("WebappMenuOpenInChrome");
            } else {
                RecordUserAction.record("Webapp.NotificationOpenInChrome");
            }
            return true;
        }
        return super.onMenuOrKeyboardAction(id, fromMenu);
    }

    @Override
    protected Drawable getBackgroundDrawable() {
        return null;
    }
}
