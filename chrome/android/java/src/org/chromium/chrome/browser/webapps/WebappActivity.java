// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabAppMenuPropertiesDelegate;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler.IntentIgnoringCriterion;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.dependency_injection.BaseCustomTabActivityModule;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityCommonsModule;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.webapps.dependency_injection.WebappActivityComponent;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.util.ArrayList;

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
    public AppMenuPropertiesDelegate createAppMenuPropertiesDelegate() {
        return new CustomTabAppMenuPropertiesDelegate(this, getActivityTabProvider(),
                getMultiWindowModeStateDispatcher(), getTabModelSelector(), getToolbarManager(),
                getWindow().getDecorView(), mBookmarkBridgeSupplier,
                CustomTabsUiType.MINIMAL_UI_WEBAPP, new ArrayList<String>(),
                true /* is opened by Chrome */, true /* should show share */,
                false /* should show star (bookmarking) */, false /* should show download */,
                false /* is incognito */);
    }

    @Override
    public boolean onMenuOrKeyboardAction(int id, boolean fromMenu) {
        // Disable creating bookmark.
        if (id == R.id.bookmark_this_page_id) {
            return true;
        }
        if (id == R.id.open_in_browser_id) {
            openCurrentUrlInChrome();
            if (fromMenu) {
                RecordUserAction.record("WebappMenuOpenInChrome");
            } else {
                RecordUserAction.record("Webapp.NotificationOpenInChrome");
            }
            return true;
        }
        return super.onMenuOrKeyboardAction(id, fromMenu);
    }

    /**
     * Opens the URL currently being displayed in the browser by reparenting the tab.
     */
    private boolean openCurrentUrlInChrome() {
        Tab tab = getActivityTab();
        if (tab == null) return false;

        String url = tab.getOriginalUrl();
        if (TextUtils.isEmpty(url)) {
            url = IntentHandler.getUrlFromIntent(getIntent());
        }

        // TODO(piotrs): Bring reparenting back once CCT animation is fixed. See crbug/774326
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setClass(this, ChromeLauncherActivity.class);
        IntentHandler.startActivityForTrustedIntent(intent);

        return true;
    }

    @Override
    protected Drawable getBackgroundDrawable() {
        return null;
    }
}
