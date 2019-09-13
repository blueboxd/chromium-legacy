// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.StrictMode;
import android.text.TextUtils;
import android.view.View;
import android.view.View.OnSystemUiVisibilityChangeListener;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.SingleTabActivity;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.customtabs.CustomTabAppMenuPropertiesDelegate;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.document.DocumentUtils;
import org.chromium.chrome.browser.metrics.WebApkUma;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBrowserControlsState;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabObserverRegistrar;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer;
import org.chromium.chrome.browser.usage_stats.UsageStatsService;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.browser.widget.TintedDrawable;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.ScreenOrientationProvider;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.base.PageTransition;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.HashMap;

/**
 * Displays a webapp in a nearly UI-less Chrome (InfoBars still appear).
 */
public class WebappActivity extends SingleTabActivity {
    public static final String WEBAPP_SCHEME = "webapp";

    private static final String TAG = "WebappActivity";
    private static final String HISTOGRAM_NAVIGATION_STATUS = "Webapp.NavigationStatus";
    private static final long MS_BEFORE_NAVIGATING_BACK_FROM_INTERSTITIAL = 1000;

    private static final int ENTER_IMMERSIVE_MODE_DELAY_MILLIS = 300;
    private static final int RESTORE_IMMERSIVE_MODE_DELAY_MILLIS = 3000;
    static final int IMMERSIVE_MODE_UI_FLAGS = View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION // hide nav bar
            | View.SYSTEM_UI_FLAG_FULLSCREEN // hide status bar
            | View.SYSTEM_UI_FLAG_LOW_PROFILE
            | View.SYSTEM_UI_FLAG_IMMERSIVE;

    private WebappInfo mWebappInfo;

    private TabObserverRegistrar mTabObserverRegistrar;

    private SplashController mSplashController;

    private WebappDisclosureSnackbarController mDisclosureSnackbarController;

    private boolean mIsInitialized;
    private Integer mBrandColor;

    private Bitmap mLargestFavicon;

    private Runnable mSetImmersiveRunnable;

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
        mTabObserverRegistrar = new TabObserverRegistrar(getLifecycleDispatcher());
        mSplashController =
                new SplashController(this, getLifecycleDispatcher(), mTabObserverRegistrar);
        mDisclosureSnackbarController = new WebappDisclosureSnackbarController();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        if (intent == null) return;

        if (AppHooks.get().interceptWebAppIntent(intent, this)) return;

        super.onNewIntent(intent);

        WebappInfo newWebappInfo = popWebappInfo(WebappIntentDataProvider.idFromIntent(intent));
        if (newWebappInfo == null) newWebappInfo = createWebappInfo(intent);

        if (newWebappInfo == null) {
            Log.e(TAG, "Failed to parse new Intent: " + intent);
            ApiCompatibilityUtils.finishAndRemoveTask(this);
        } else if (newWebappInfo.shouldForceNavigation() && mIsInitialized) {
            loadUrl(newWebappInfo, getActivityTab());
        }
    }

    @Override
    public @ChromeActivity.ActivityType int getActivityType() {
        return ChromeActivity.ActivityType.WEBAPP;
    }

    protected boolean loadUrlIfPostShareTarget(WebappInfo webappInfo) {
        return false;
    }

    protected void loadUrl(WebappInfo webappInfo, Tab tab) {
        if (loadUrlIfPostShareTarget(webappInfo)) {
            // Web Share Target Post was successful, so don't load anything.
            return;
        }
        LoadUrlParams params = new LoadUrlParams(webappInfo.url(), PageTransition.AUTO_TOPLEVEL);
        params.setShouldClearHistoryList(true);
        tab.loadUrl(params);
    }

    protected boolean isInitialized() {
        return mIsInitialized;
    }

    protected WebappInfo createWebappInfo(Intent intent) {
        return (intent == null) ? WebappInfo.createEmpty() : WebappInfo.create(intent);
    }

    @Override
    public void initializeState() {
        super.initializeState();
        mTabObserverRegistrar.addObserversForTab(getActivityTab());
        initializeUI(getSavedInstanceState());
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

    protected void initializeUI(Bundle savedInstanceState) {
        Tab tab = getActivityTab();

        // We do not load URL when restoring from saved instance states. However, it's possible that
        // we saved instance state before loading a URL, so even after restoring from
        // SavedInstanceState we might not have a URL and should initialize from the intent.
        if (tab.getUrl().isEmpty()) {
            loadUrl(mWebappInfo, tab);
        } else {
            if (!mWebappInfo.isForWebApk() && NetworkChangeNotifier.isOnline()) {
                tab.reloadIgnoringCache();
            }
        }
        tab.addObserver(createTabObserver());
    }

    @Override
    public void performPreInflationStartup() {
        Intent intent = getIntent();
        String id = WebappIntentDataProvider.idFromIntent(intent);
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

        applyScreenOrientation();

        if (mWebappInfo.displayMode() == WebDisplayMode.FULLSCREEN) {
            enterImmersiveMode();
        }

        initSplash();
    }

    @Override
    public void finishNativeInitialization() {
        LayoutManager layoutDriver = new LayoutManager(getCompositorViewHolder());
        initializeCompositorContent(layoutDriver, findViewById(R.id.url_bar),
                (ViewGroup) findViewById(android.R.id.content),
                (ToolbarControlContainer) findViewById(R.id.control_container));
        getToolbarManager().initializeWithNative(getTabModelSelector(),
                getFullscreenManager().getBrowserVisibilityDelegate(), null, layoutDriver, null,
                null, null, view -> onToolbarCloseButtonClicked());
        getToolbarManager().setShowTitle(true);
        getToolbarManager().setCloseButtonDrawable(null); // Hides close button.

        if (UsageStatsService.isEnabled() && !mWebappInfo.isSplashProvidedByWebApk()) {
            UsageStatsService.getInstance().createPageViewObserver(getTabModelSelector(), this);
        }

        getFullscreenManager().setTab(getActivityTab());
        super.finishNativeInitialization();
        mIsInitialized = true;
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        saveTabState(outState);
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

    /**
     * Saves the tab data out to a file.
     */
    private void saveTabState(Bundle outState) {
        Tab tab = getActivityTab();
        if (tab == null || tab.getUrl() == null || tab.getUrl().isEmpty()) return;
        if (TabState.saveState(outState, TabState.from(tab))) {
            outState.putInt(BUNDLE_TAB_ID, tab.getId());
        }
    }

    @Override
    protected TabState restoreTabState(Bundle savedInstanceState, int tabId) {
        return TabState.restoreTabState(savedInstanceState);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);

        // Re-enter immersive mode after users switch back to this Activity.
        if (hasFocus) {
            asyncSetImmersive(ENTER_IMMERSIVE_MODE_DELAY_MILLIS);
        }
    }

    /**
     * Sets activity's decor view into an immersive mode.
     * If immersive mode is not supported, this method no-ops.
     */
    private void enterImmersiveMode() {
        // Immersive mode is only supported in API 19+.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) return;

        if (mSetImmersiveRunnable == null) {
            final View decor = getWindow().getDecorView();

            mSetImmersiveRunnable = new Runnable() {
                @Override
                public void run() {
                    int currentFlags = decor.getSystemUiVisibility();
                    int desiredFlags = currentFlags | IMMERSIVE_MODE_UI_FLAGS;
                    if (currentFlags != desiredFlags) {
                        decor.setSystemUiVisibility(desiredFlags);
                    }
                }
            };

            // When we enter immersive mode for the first time, register a
            // SystemUiVisibilityChangeListener that restores immersive mode. This is necessary
            // because user actions like focusing a keyboard will break out of immersive mode.
            decor.setOnSystemUiVisibilityChangeListener(new OnSystemUiVisibilityChangeListener() {
                @Override
                public void onSystemUiVisibilityChange(int newFlags) {
                    if ((newFlags & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0) {
                        asyncSetImmersive(RESTORE_IMMERSIVE_MODE_DELAY_MILLIS);
                    }
                }
            });
        }

        asyncSetImmersive(0);
    }

    /**
     * This method no-ops before {@link #enterImmersiveMode()} is called explicitly.
     */
    private void asyncSetImmersive(int delayInMills) {
        if (mSetImmersiveRunnable == null) return;

        mHandler.removeCallbacks(mSetImmersiveRunnable);
        mHandler.postDelayed(mSetImmersiveRunnable, delayInMills);
    }

    @Override
    public void onResume() {
        if (!isFinishing()) {
            if (getIntent() != null) {
                // Avoid situations where Android starts two Activities with the same data.
                DocumentUtils.finishOtherTasksWithData(getIntent().getData(), getTaskId());
            }
            updateTaskDescription();
        }
        super.onResume();
    }

    @Override
    public void onResumeWithNative() {
        super.onResumeWithNative();
        WebappActionsNotificationManager.maybeShowNotification(getActivityTab(), mWebappInfo);
        WebappDataStorage storage =
                WebappRegistry.getInstance().getWebappDataStorage(mWebappInfo.id());
        if (storage != null) {
            mDisclosureSnackbarController.maybeShowDisclosure(this, storage, false /* force */);
        }
    }

    @Override
    public void onPauseWithNative() {
        WebappActionsNotificationManager.cancelNotification();
        super.onPauseWithNative();
    }

    @Override
    protected void initDeferredStartupForActivity() {
        super.initDeferredStartupForActivity();

        WebappDataStorage storage =
                WebappRegistry.getInstance().getWebappDataStorage(mWebappInfo.id());
        if (storage != null) {
            onDeferredStartupWithStorage(storage);
        } else {
            onDeferredStartupWithNullStorage(mDisclosureSnackbarController);
        }
    }

    @Override
    protected void recordIntentToCreationTime(long timeMs) {
        super.recordIntentToCreationTime(timeMs);

        RecordHistogram.recordTimesHistogram("MobileStartup.IntentToCreationTime.WebApp", timeMs);
    }

    protected void onDeferredStartupWithStorage(WebappDataStorage storage) {
        updateStorage(storage);
    }

    protected void onDeferredStartupWithNullStorage(
            WebappDisclosureSnackbarController disclosureSnackbarController) {
        // Overridden in WebApkActivity
    }

    @Override
    protected int getControlContainerLayoutId() {
        return R.layout.custom_tabs_control_container;
    }

    @Override
    protected int getToolbarLayoutId() {
        return R.layout.custom_tabs_toolbar;
    }

    @Override
    public AppMenuPropertiesDelegate createAppMenuPropertiesDelegate() {
        return new CustomTabAppMenuPropertiesDelegate(this, getActivityTabProvider(),
                getMultiWindowModeStateDispatcher(), getTabModelSelector(), getToolbarManager(),
                getWindow().getDecorView(), CustomTabsUiType.MINIMAL_UI_WEBAPP,
                new ArrayList<String>(), true /* is opened by Chrome */,
                true /* should show share */, false /* should show star (bookmarking) */,
                false /* should show download */, false /* is incognito */);
    }

    /**
     * @return Structure containing data about the webapp currently displayed.
     *         The return value should not be cached.
     */
    public WebappInfo getWebappInfo() {
        return mWebappInfo;
    }

    /**
     * @return A string containing the scope of the webapp opened in this activity.
     */
    public String getWebappScope() {
        return mWebappInfo.scopeUrl();
    }

    public static void addWebappInfo(String id, WebappInfo info) {
        Holder.sWebappInfoMap.put(id, info);
    }

    public static WebappInfo popWebappInfo(String id) {
        return Holder.sWebappInfoMap.remove(id);
    }

    protected void updateStorage(WebappDataStorage storage) {
        // The information in the WebappDataStorage may have been purged by the
        // user clearing their history or not launching the web app recently.
        // Restore the data if necessary from the intent.
        storage.updateFromShortcutIntent(getIntent());

        // A recent last used time is the indicator that the web app is still
        // present on the home screen, and enables sources such as notifications to
        // launch web apps. Thus, we do not update the last used time when the web
        // app is not directly launched from the home screen, as this interferes
        // with the heuristic.
        if (mWebappInfo.isLaunchedFromHomescreen()) {
            boolean previouslyLaunched = storage.hasBeenLaunched();
            long previousUsageTimestamp = storage.getLastUsedTimeMs();
            storage.setHasBeenLaunched();
            // TODO(yusufo): WebappRegistry#unregisterOldWebapps uses this information to delete
            // WebappDataStorage objects for legacy webapps which haven't been used in a while.
            // That will need to be updated to not delete anything for a TWA which remains installed
            storage.updateLastUsedTime();
            onUpdatedLastUsedTime(storage, previouslyLaunched, previousUsageTimestamp);
        }
    }

    /**
     * Called after updating the last used time in {@link WebappDataStorage}.
     * @param previouslyLaunched Whether the webapp has been previously launched from the home
     *     screen.
     * @param previousUsageTimestamp The previous time that the webapp was used.
     */
    protected void onUpdatedLastUsedTime(
            WebappDataStorage storage, boolean previouslyLaunched, long previousUsageTimestamp) {}

    protected TabObserver createTabObserver() {
        return new EmptyTabObserver() {
            @Override
            public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
                if (navigation.hasCommitted() && navigation.isInMainFrame()) {
                    // Notify the renderer to permanently hide the top controls since they do
                    // not apply to fullscreen content views.
                    TabBrowserControlsState.update(
                            tab, TabBrowserControlsState.getConstraints(tab), true);

                    RecordHistogram.recordBooleanHistogram(
                            HISTOGRAM_NAVIGATION_STATUS, !navigation.isErrorPage());

                    updateToolbarCloseButtonVisibility();

                    boolean isNavigationInScope = WebappScopePolicy.isUrlInScope(
                            scopePolicy(), mWebappInfo, navigation.getUrl());
                    if (!isNavigationInScope) {
                        // Briefly show the toolbar for off-scope navigations.
                        getFullscreenManager()
                                .getBrowserVisibilityDelegate()
                                .showControlsTransient();
                    }
                    if (mWebappInfo.isForWebApk()) {
                        WebApkUma.recordNavigation(isNavigationInScope);
                    }
                }
            }

            @Override
            public void onDidChangeThemeColor(Tab tab, int color) {
                mBrandColor = color;
                updateTaskDescription();
            }

            @Override
            public void onTitleUpdated(Tab tab) {
                updateTaskDescription();
            }

            @Override
            public void onFaviconUpdated(Tab tab, Bitmap icon) {
                // No need to cache the favicon if there is an icon declared in app manifest.
                if (mWebappInfo.icon() != null || icon == null) return;
                if (mLargestFavicon == null || icon.getWidth() > mLargestFavicon.getWidth()
                        || icon.getHeight() > mLargestFavicon.getHeight()) {
                    mLargestFavicon = icon;
                    updateTaskDescription();
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
                        new Intent(Intent.ACTION_VIEW, Uri.parse(getActivityTab().getUrl()));
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
                            if (mWebappInfo.isSplashProvidedByWebApk()) {
                                // We need to call into WebAPK to finish activity stack because:
                                // 1) WebApkActivity is not the root of the task.
                                // 2) The activity stack no longer has focus and thus cannot rely on
                                //    the client's Activity#onResume() behaviour.
                                WebApkServiceClient.getInstance().finishAndRemoveTaskSdk23(
                                        (WebApkActivity) WebappActivity.this);
                            } else {
                                ApiCompatibilityUtils.finishAndRemoveTask(WebappActivity.this);
                            }
                        }
                    }
                }, MS_BEFORE_NAVIGATING_BACK_FROM_INTERSTITIAL);
            }
        };
    }

    public @WebappScopePolicy.Type int scopePolicy() {
        return WebappScopePolicy.Type.LEGACY;
    }

    /**
     * @return The package name if this Activity is associated with an APK. Null if there is no
     *         associated Android native client.
     */
    public @Nullable String getWebApkPackageName() {
        return null;
    }

    private void updateToolbarCloseButtonVisibility() {
        if (WebappBrowserControlsDelegate.shouldShowToolbarCloseButton(this)) {
            getToolbarManager().setCloseButtonDrawable(
                    TintedDrawable.constructTintedDrawable(this, R.drawable.btn_close));
            // Applies light or dark tint to icons depending on the theme color.
            getToolbarManager().updateLocationBarVisualsForState();
        } else {
            getToolbarManager().setCloseButtonDrawable(null);
        }
    }

    /**
     * Moves the user back in history to most recent on-origin location.
     */
    private void onToolbarCloseButtonClicked() {
        NavigationController nc = getActivityTab().getWebContents().getNavigationController();

        final int lastIndex = nc.getLastCommittedEntryIndex();
        int index = lastIndex;
        while (index > 0
                && !WebappScopePolicy.isUrlInScope(
                           scopePolicy(), getWebappInfo(), nc.getEntryAtIndex(index).getUrl())) {
            index--;
        }

        if (index != lastIndex) {
            nc.goToNavigationIndex(index);
        }
    }

    private void updateTaskDescription() {
        String title = null;
        if (!TextUtils.isEmpty(mWebappInfo.shortName())) {
            title = mWebappInfo.shortName();
        } else if (getActivityTab() != null) {
            title = getActivityTab().getTitle();
        }

        Bitmap icon = null;
        if (mWebappInfo.icon() != null) {
            icon = mWebappInfo.icon().bitmap();
        } else if (getActivityTab() != null) {
            icon = mLargestFavicon;
        }

        if (mBrandColor == null && mWebappInfo.hasValidThemeColor()) {
            mBrandColor = (int) mWebappInfo.themeColor();
        }

        int taskDescriptionColor =
                ApiCompatibilityUtils.getColor(getResources(), R.color.default_primary_color);

        // Don't use the brand color for the status bars if we're in display: fullscreen. This works
        // around an issue where the status bars go transparent and can't be seen on top of the page
        // content when users swipe them in or they appear because the on-screen keyboard was
        // triggered.
        if (mBrandColor != null && mWebappInfo.displayMode() != WebDisplayMode.FULLSCREEN) {
            taskDescriptionColor = mBrandColor;
            if (getToolbarManager() != null) {
                getToolbarManager().onThemeColorChanged(mBrandColor, false);
            }
        }

        ApiCompatibilityUtils.setTaskDescription(this, title, icon,
                ColorUtils.getOpaqueColor(taskDescriptionColor));
        getStatusBarColorController().updateStatusBarColor(isStatusBarDefaultThemeColor());
    }

    @Override
    public int getBaseStatusBarColor() {
        return isStatusBarDefaultThemeColor() ? Color.BLACK : mBrandColor;
    }

    @Override
    public boolean isStatusBarDefaultThemeColor() {
        // Don't use the brand color for the status bars if we're in display: fullscreen. This works
        // around an issue where the status bars go transparent and can't be seen on top of the page
        // content when users swipe them in or they appear because the on-screen keyboard was
        // triggered.
        return mBrandColor == null || mWebappInfo.displayMode() == WebDisplayMode.FULLSCREEN;
    }

    @Override
    public boolean onMenuOrKeyboardAction(int id, boolean fromMenu) {
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
    public int getControlContainerHeightResource() {
        return R.dimen.custom_tabs_control_container_height;
    }

    @Override
    protected Drawable getBackgroundDrawable() {
        return null;
    }

    @Override
    protected TabDelegateFactory createTabDelegateFactory() {
        return new WebappDelegateFactory(this);
    }

    @Override
    protected TabCreator createNormalTabCreator() {
        return new WebappTabDelegate(false /* incognito */, mWebappInfo);
    }

    // We're temporarily disable CS on webapp since there are some issues. (http://crbug.com/471950)
    // TODO(changwan): re-enable it once the issues are resolved.
    @Override
    protected boolean isContextualSearchAllowed() {
        return false;
    }

    /** Inits the splash screen */
    protected void initSplash() {
        // Splash screen is shown after preInflationStartup() is run and the delegate is set.
        boolean isWindowInitiallyTranslucent = mWebappInfo.isSplashProvidedByWebApk();
        mSplashController.setConfig(
                new WebappSplashDelegate(this, mTabObserverRegistrar, mWebappInfo),
                isWindowInitiallyTranslucent, WebappSplashDelegate.HIDE_ANIMATION_DURATION_MS);
    }

    /** Sets the screen orientation. */
    private void applyScreenOrientation() {
        if (mWebappInfo.isSplashProvidedByWebApk()
                && Build.VERSION.SDK_INT == Build.VERSION_CODES.O) {
            // When the splash screen is provided by the WebAPK, the activity is initially
            // translucent. Setting the screen orientation while the activity is translucent
            // throws an exception on O (but not O MR1). Delay setting it.
            ScreenOrientationProvider.getInstance().delayOrientationRequests(getWindowAndroid());

            addSplashscreenObserver(new SplashscreenObserver() {
                @Override
                public void onTranslucencyRemoved() {
                    ScreenOrientationProvider.getInstance().runDelayedOrientationRequests(
                            getWindowAndroid());
                }

                @Override
                public void onSplashscreenHidden(long startTimestamp, long endTimestamp) {}
            });

            // Fall through and queue up request for the default screen orientation because the web
            // page might change it via JavaScript.
        }
        ScreenOrientationProvider.getInstance().lockOrientation(
                getWindowAndroid(), (byte) mWebappInfo.orientation());
    }

    protected boolean isSplashShowing() {
        return mSplashController.isSplashShowing();
    }

    /**
     * Register an observer to the splashscreen hidden/visible events for this activity.
     */
    protected void addSplashscreenObserver(SplashscreenObserver observer) {
        mSplashController.addObserver(observer);
    }

    /**
     * Deregister an observer to the splashscreen hidden/visible events for this activity.
     */
    protected void removeSplashscreenObserver(SplashscreenObserver observer) {
        mSplashController.removeObserver(observer);
    }
}
