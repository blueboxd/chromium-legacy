// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;

import android.app.Activity;
import android.content.Intent;
import android.graphics.PixelFormat;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.StrictMode;
import android.text.TextUtils;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
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
import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler.IntentIgnoringCriterion;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.customtabs.dependency_injection.BaseCustomTabActivityModule;
import org.chromium.chrome.browser.customtabs.features.ImmersiveModeController;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityCommonsModule;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.usage_stats.UsageStatsService;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.chrome.browser.webapps.dependency_injection.WebappActivityComponent;
import org.chromium.chrome.browser.webapps.dependency_injection.WebappActivityModule;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.HashMap;

/**
 * Displays a webapp in a nearly UI-less Chrome (InfoBars still appear).
 */
public class WebappActivity extends BaseCustomTabActivity<WebappActivityComponent> {
    public static final String WEBAPP_SCHEME = "webapp";

    private static final String TAG = "WebappActivity";
    private static final long MS_BEFORE_NAVIGATING_BACK_FROM_INTERSTITIAL = 1000;

    protected static final String BUNDLE_TAB_ID = "tabId";

    private WebappInfo mWebappInfo;

    private BrowserServicesIntentDataProvider mIntentDataProvider;
    private WebappActivityTabController mTabController;
    private SplashController mSplashController;
    private TabObserverRegistrar mTabObserverRegistrar;
    private CustomTabDelegateFactory mDelegateFactory;

    private boolean mIsInitialized;
    private Integer mBrandColor;

    private static Integer sOverrideCoreCountForTesting;

    /** Initialization-on-demand holder. This exists for thread-safe lazy initialization. */
    private static class Holder {
        // This static map is used to cache WebappInfo objects between their initial creation in
        // WebappLauncherActivity and final use in WebappActivity.
        private static final HashMap<String, WebappInfo> sWebappInfoMap =
                new HashMap<String, WebappInfo>();
    }

    /** Returns the running WebappActivity with the given tab id. Returns null if there is none. */
    public static WeakReference<WebappActivity> findWebappActivityWithTabId(int tabId) {
        if (tabId == Tab.INVALID_TAB_ID) return null;

        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (!(activity instanceof WebappActivity)) continue;

            WebappActivity webappActivity = (WebappActivity) activity;
            Tab tab = webappActivity.getActivityTab();
            if (tab != null && tab.getId() == tabId) {
                return new WeakReference<>(webappActivity);
            }
        }
        return null;
    }

    /** Returns the WebappActivity with the given {@link webappId}. */
    public static WeakReference<WebappActivity> findRunningWebappActivityWithId(String webappId) {
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (!(activity instanceof WebappActivity)) {
                continue;
            }
            WebappActivity webappActivity = (WebappActivity) activity;
            if (webappActivity != null
                    && TextUtils.equals(webappId, webappActivity.getWebappInfo().id())) {
                return new WeakReference<>(webappActivity);
            }
        }
        return null;
    }

    /**
     * Construct all the variables that shouldn't change.  We do it here both to clarify when the
     * objects are created and to ensure that they exist throughout the parallelized initialization
     * of the WebappActivity.
     */
    public WebappActivity() {
        mWebappInfo = createWebappInfo(null);
    }

    @Override
    public BrowserServicesIntentDataProvider getIntentDataProvider() {
        return mIntentDataProvider;
    }

    @Override
    protected void onNewIntent(Intent intent) {
        if (intent == null) return;

        super.onNewIntent(intent);

        WebappInfo newWebappInfo = popWebappInfo(WebappIntentUtils.idFromIntent(intent));
        if (newWebappInfo == null) newWebappInfo = createWebappInfo(intent);

        if (newWebappInfo == null) {
            Log.e(TAG, "Failed to parse new Intent: " + intent);
            ApiCompatibilityUtils.finishAndRemoveTask(this);
        } else if (newWebappInfo.shouldForceNavigation() && mIsInitialized) {
            mCustomTabIntentHandler.onNewIntent(newWebappInfo.getProvider());
        }
    }

    protected boolean loadUrlIfPostShareTarget(WebappInfo webappInfo) {
        return false;
    }

    protected WebappInfo createWebappInfo(Intent intent) {
        if (intent == null) return WebappInfo.createEmpty();

        WebappInfo info = WebApkInfo.create(intent);
        if (info != null) return info;

        return WebappInfo.create(intent);
    }

    @Override
    public boolean shouldPreferLightweightFre(Intent intent) {
        // We cannot use WebappInfo#webApkPackageName() because
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
        Intent intent = getIntent();
        String id = WebappIntentUtils.idFromIntent(intent);
        WebappInfo info = popWebappInfo(id);
        // When WebappActivity is killed by the Android OS, and an entry stays in "Android Recents"
        // (The user does not swipe it away), when WebappActivity is relaunched it is relaunched
        // with the intent stored in WebappActivity#getIntent() at the time that the WebappActivity
        // was killed. WebappActivity may be relaunched from:

        // (A) An intent from WebappLauncherActivity (e.g. as a result of a notification or a deep
        // link). Android drops the intent from WebappLauncherActivity in favor of
        // WebappActivity#getIntent() at the time that the WebappActivity was killed.

        // (B) The user selecting the WebappActivity in recents. In case (A) we want to use the
        // intent sent to WebappLauncherActivity and ignore WebappActivity#getSavedInstanceState().
        // In case (B) we want to restore to saved tab state.
        if (info == null) {
            info = createWebappInfo(intent);
        } else if (info.shouldForceNavigation()) {
            // Don't restore to previous page, navigate using WebappInfo retrieved from cache.
            resetSavedInstanceState();
        }

        if (info == null) {
            // If {@link info} is null, there isn't much we can do, abort.
            ApiCompatibilityUtils.finishAndRemoveTask(this);
            return;
        }

        mWebappInfo = info;

        // Initialize the WebappRegistry and warm up the shared preferences for this web app. No-ops
        // if the registry and this web app are already initialized. Must override Strict Mode to
        // avoid a violation.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            WebappRegistry.getInstance();
            WebappRegistry.warmUpSharedPrefsForId(id);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }

        // When turning on TalkBack on Android, hitting app switcher to bring a WebappActivity to
        // front will speak "Web App", which is the label of WebappActivity. Therefore, we set title
        // of the WebappActivity explicitly to make it speak the short name of the Web App.
        setTitle(mWebappInfo.shortName());

        super.performPreInflationStartup();

        if (mWebappInfo.displayMode() == WebDisplayMode.FULLSCREEN) {
            new ImmersiveModeController(getLifecycleDispatcher(), this).enterImmersiveMode(
                    LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT, false /*sticky*/);
        }

        initSplash();
    }

    @Override
    protected WebappActivityComponent createComponent(ChromeActivityCommonsModule commonsModule) {
        IntentIgnoringCriterion intentIgnoringCriterion =
                (intent) -> mIntentHandler.shouldIgnoreIntent(intent);

        mIntentDataProvider = mWebappInfo.getProvider();
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
        mDelegateFactory = component.resolveTabDelegateFactory();

        mToolbarColorController.setUseTabThemeColor(true /* useTabThemeColor */);
        mStatusBarColorProvider.setUseTabThemeColor(true /* useTabThemeColor */);

        mNavigationController.setFinishHandler((reason) -> { handleFinishAndClose(); });

        return component;
    }

    @Override
    public void finishNativeInitialization() {
        if (UsageStatsService.isEnabled() && !mWebappInfo.isSplashProvidedByWebApk()) {
            UsageStatsService.getInstance().createPageViewObserver(getTabModelSelector(), this);
        }

        getFullscreenManager().setTab(getActivityTab());
        super.finishNativeInitialization();
        mIsInitialized = true;
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
                getWindow().getDecorView(), getToolbarManager().getBookmarkBridgeSupplier(),
                CustomTabsUiType.MINIMAL_UI_WEBAPP, new ArrayList<String>(),
                true /* is opened by Chrome */, true /* should show share */,
                false /* should show star (bookmarking) */, false /* should show download */,
                false /* is incognito */);
    }

    /**
     * @return Structure containing data about the webapp currently displayed.
     *         The return value should not be cached.
     */
    public WebappInfo getWebappInfo() {
        return mWebappInfo;
    }

    WebContentsDelegateAndroid getWebContentsDelegate() {
        assert mDelegateFactory != null;
        return mDelegateFactory.getWebContentsDelegate();
    }

    public static void addWebappInfo(String id, WebappInfo info) {
        Holder.sWebappInfoMap.put(id, info);
    }

    public static WebappInfo popWebappInfo(String id) {
        return Holder.sWebappInfoMap.remove(id);
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

            @Override
            public void onDidAttachInterstitialPage(Tab tab) {
                int state = ApplicationStatus.getStateForActivity(WebappActivity.this);
                if (state == ActivityState.PAUSED || state == ActivityState.STOPPED
                        || state == ActivityState.DESTROYED) {
                    return;
                }

                // Kick the interstitial navigation to Chrome.
                Intent intent =
                        new Intent(Intent.ACTION_VIEW, Uri.parse(getActivityTab().getUrlString()));
                intent.setPackage(getPackageName());
                intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                IntentHandler.startChromeLauncherActivityForTrustedIntent(intent);

                // Pretend like the navigation never happened.  We delay so that this happens while
                // the Activity is in the background.
                mHandler.postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        if (getActivityTab().canGoBack()) {
                            getActivityTab().goBack();
                        } else {
                            handleFinishAndClose();
                        }
                    }
                }, MS_BEFORE_NAVIGATING_BACK_FROM_INTERSTITIAL);
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

    /**
     * Returns a unique identifier for this WebappActivity.
     * Note: do not call this function when you need {@link WebappInfo#id()}. Subclasses like
     * WebappManagedActivity and WebApkManagedActivity overwrite this function and return the
     * index of the activity.
     */
    protected String getActivityId() {
        return mWebappInfo.id();
    }

    @VisibleForTesting
    SplashController getSplashControllerForTests() {
        return mSplashController;
    }

    @Override
    protected Drawable getBackgroundDrawable() {
        return null;
    }

    /**
     * @return {@link TabDelegateFactory} to be used while creating the associated {@link Tab}.
     */
    private TabDelegateFactory createTabDelegateFactory() {
        return mDelegateFactory;
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
        mSplashController.setConfig(
                new WebappSplashDelegate(this, mTabObserverRegistrar, mWebappInfo),
                isWindowInitiallyTranslucent, WebappSplashDelegate.HIDE_ANIMATION_DURATION_MS);
    }

    @Override
    public void onUpdateStateChanged() {}
}
