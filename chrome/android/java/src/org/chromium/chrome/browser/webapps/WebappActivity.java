// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;

import android.content.Intent;
import android.graphics.PixelFormat;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.StrictMode;
import android.text.TextUtils;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabAppMenuPropertiesDelegate;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler.IntentIgnoringCriterion;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.customtabs.dependency_injection.BaseCustomTabActivityModule;
import org.chromium.chrome.browser.customtabs.features.ImmersiveModeController;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityCommonsModule;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.chrome.browser.webapps.dependency_injection.WebappActivityComponent;
import org.chromium.chrome.browser.webapps.dependency_injection.WebappActivityModule;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.util.ArrayList;

/**
 * Displays a webapp in a nearly UI-less Chrome (InfoBars still appear).
 */
public class WebappActivity extends BaseCustomTabActivity<WebappActivityComponent> {
    public static final String WEBAPP_SCHEME = "webapp";

    private static final String TAG = "WebappActivity";
    private static final long MS_BEFORE_NAVIGATING_BACK_FROM_INTERSTITIAL = 1000;

    protected static final String BUNDLE_TAB_ID = "tabId";

    private static BrowserServicesIntentDataProvider sIntentDataProviderOverride;

    private BrowserServicesIntentDataProvider mIntentDataProvider;
    private WebappActivityTabController mTabController;
    private SplashController mSplashController;
    private TabObserverRegistrar mTabObserverRegistrar;

    private Integer mBrandColor;

    private static Integer sOverrideCoreCountForTesting;

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
    public BrowserServicesIntentDataProvider getIntentDataProvider() {
        return mIntentDataProvider;
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
    public void initializeState() {
        super.initializeState();

        mTabController.initializeState();
        mTabObserverRegistrar.registerActivityTabObserver(createTabObserver());
    }

    @VisibleForTesting
    public static void setOverrideCoreCount(int coreCount) {
        sOverrideCoreCountForTesting = coreCount;
    }

    private static int getCoreCount() {
        if (sOverrideCoreCountForTesting != null) return sOverrideCoreCountForTesting;
        return Runtime.getRuntime().availableProcessors();
    }

    @Override
    protected void doLayoutInflation() {
        // Because we delay the layout inflation, the CompositorSurfaceManager and its
        // SurfaceView(s) are created and attached late (ie after the first draw). At the time of
        // the first attach of a SurfaceView to the view hierarchy (regardless of the SurfaceView's
        // actual opacity), the window transparency hint changes (because the window creates a
        // transparent hole and attaches the SurfaceView to that hole). This may cause older android
        // versions to destroy the window and redraw it causing a flicker. This line sets the window
        // transparency hint early so that when the SurfaceView gets attached later, the
        // transparency hint need not change and no flickering occurs.
        getWindow().setFormat(PixelFormat.TRANSLUCENT);
        // No need to inflate layout synchronously since splash screen is displayed.
        Runnable inflateTask = () -> {
            ViewGroup mainView = WarmupManager.inflateViewHierarchy(
                    WebappActivity.this, getControlContainerLayoutId(), getToolbarLayoutId());
            if (isActivityFinishingOrDestroyed()) return;
            if (mainView != null) {
                PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
                    if (isActivityFinishingOrDestroyed()) return;
                    onLayoutInflated(mainView);
                });
            } else {
                if (isActivityFinishingOrDestroyed()) return;
                PostTask.postTask(
                        UiThreadTaskTraits.DEFAULT, () -> WebappActivity.super.doLayoutInflation());
            }
        };

        // Conditionally do layout inflation synchronously if device has low core count.
        // When layout inflation is done asynchronously, it blocks UI thread startup. While
        // blocked, the UI thread will draw unnecessary frames - causing the lower priority
        // layout inflation thread to be de-scheduled significantly more often, especially on
        // devices with low core count. Thus for low core count devices, there is a startup
        // performance improvement incurred by doing layout inflation synchronously.
        if (getCoreCount() > 2) {
            new Thread(inflateTask).start();
        } else {
            inflateTask.run();
        }
    }

    private void onLayoutInflated(ViewGroup mainView) {
        ViewGroup contentView = (ViewGroup) findViewById(android.R.id.content);
        WarmupManager.transferViewHeirarchy(mainView, contentView);
        mSplashController.bringSplashBackToFront();
        onInitialLayoutInflationComplete();
    }

    @Override
    public void performPreInflationStartup() {
        mIntentDataProvider =
                buildIntentDataProvider(getIntent(), CustomTabsIntent.COLOR_SCHEME_LIGHT);

        if (mIntentDataProvider == null) {
            // If {@link info} is null, there isn't much we can do, abort.
            ApiCompatibilityUtils.finishAndRemoveTask(this);
            return;
        }

        WebappExtras webappExtras = mIntentDataProvider.getWebappExtras();

        // Initialize the WebappRegistry and warm up the shared preferences for this web app. No-ops
        // if the registry and this web app are already initialized. Must override Strict Mode to
        // avoid a violation.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            WebappRegistry.getInstance();
            WebappRegistry.warmUpSharedPrefsForId(webappExtras.id);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }

        super.performPreInflationStartup();

        if (webappExtras.displayMode == WebDisplayMode.FULLSCREEN) {
            new ImmersiveModeController(getLifecycleDispatcher(), this)
                    .enterImmersiveMode(LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT, false /*sticky*/);
        }

        initSplash();
    }

    @Override
    protected WebappActivityComponent createComponent(ChromeActivityCommonsModule commonsModule) {
        IntentIgnoringCriterion intentIgnoringCriterion =
                (intent) -> mIntentHandler.shouldIgnoreIntent(intent);

        BaseCustomTabActivityModule baseCustomTabModule = new BaseCustomTabActivityModule(
                mIntentDataProvider, mNightModeStateController, intentIgnoringCriterion);
        WebappActivityModule webappModule = new WebappActivityModule();
        WebappActivityComponent component =
                ChromeApplication.getComponent().createWebappActivityComponent(
                        commonsModule, baseCustomTabModule, webappModule);
        onComponentCreated(component);

        mTabController = component.resolveTabController();
        mSplashController = component.resolveSplashController();
        mTabObserverRegistrar = component.resolveTabObserverRegistrar();

        mToolbarColorController.setUseTabThemeColor(true /* useTabThemeColor */);
        mStatusBarColorProvider.setUseTabThemeColor(true /* useTabThemeColor */);

        mNavigationController.setFinishHandler((reason) -> { handleFinishAndClose(); });

        return component;
    }

    @Override
    public void finishNativeInitialization() {
        getFullscreenManager().setTab(getActivityTab());
        super.finishNativeInitialization();
    }

    @Override
    public void onStartWithNative() {
        super.onStartWithNative();
        WebappDirectoryManager.cleanUpDirectories();
    }

    @Override
    public void onStopWithNative() {
        super.onStopWithNative();
        getFullscreenManager().exitPersistentFullscreenMode();
    }

    @Override
    public void onResume() {
        if (!isFinishing()) {
            if (getIntent() != null) {
                // Avoid situations where Android starts two Activities with the same data.
                AndroidTaskUtils.finishOtherTasksWithData(getIntent().getData(), getTaskId());
            }
        }
        super.onResume();
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

    protected CustomTabTabObserver createTabObserver() {
        return new CustomTabTabObserver() {
            @Override
            public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
                if (navigation.hasCommitted() && navigation.isInMainFrame()
                        && !navigation.isSameDocument()) {
                    // Notify the renderer to permanently hide the top controls since they do
                    // not apply to fullscreen content views.
                    TabBrowserControlsConstraintsHelper.update(
                            tab, TabBrowserControlsConstraintsHelper.getConstraints(tab), true);
                }
            }
        };
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

    @VisibleForTesting
    SplashController getSplashControllerForTests() {
        return mSplashController;
    }

    @Override
    protected Drawable getBackgroundDrawable() {
        return null;
    }

    // We're temporarily disable CS on webapp since there are some issues. (http://crbug.com/471950)
    // TODO(changwan): re-enable it once the issues are resolved.
    @Override
    protected boolean isContextualSearchAllowed() {
        return false;
    }

    /** Inits the splash screen */
    private void initSplash() {
        // Splash screen is shown after preInflationStartup() is run and the delegate is set.
        boolean isWindowInitiallyTranslucent =
                BaseCustomTabActivity.isWindowInitiallyTranslucent(this);
        mSplashController.setConfig(new WebappSplashDelegate(this, mTabObserverRegistrar,
                                            WebappInfo.create(mIntentDataProvider)),
                isWindowInitiallyTranslucent, WebappSplashDelegate.HIDE_ANIMATION_DURATION_MS);
    }

    @Override
    public void onUpdateStateChanged() {}
}
