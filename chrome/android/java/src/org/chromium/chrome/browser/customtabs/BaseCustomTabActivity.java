// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.content.Intent;
import android.util.Pair;
import android.view.KeyEvent;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.KeyboardShortcuts;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabFactory;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler;
import org.chromium.chrome.browser.customtabs.content.TabCreationMode;
import org.chromium.chrome.browser.customtabs.dependency_injection.BaseCustomTabActivityComponent;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarColorController;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.night_mode.PowerSavingModeMonitor;
import org.chromium.chrome.browser.night_mode.SystemNightModeMonitor;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.usage_stats.UsageStatsService;
import org.chromium.chrome.browser.webapps.SameTaskWebApkActivity;
import org.chromium.chrome.browser.webapps.WebappActivityCoordinator;
import org.chromium.chrome.browser.webapps.WebappExtras;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;

/**
 * Contains functionality which is shared between {@link WebappActivity} and
 * {@link CustomTabActivity}. Purpose of the class is to simplify merging {@link WebappActivity}
 * and {@link CustomTabActivity}.
 * @param <C> - type of associated Dagger component.
 */
public abstract class BaseCustomTabActivity<C extends BaseCustomTabActivityComponent>
        extends ChromeActivity<C> {
    protected CustomTabDelegateFactory mDelegateFactory;
    protected CustomTabToolbarCoordinator mToolbarCoordinator;
    protected CustomTabActivityNavigationController mNavigationController;
    protected CustomTabActivityTabProvider mTabProvider;
    protected CustomTabToolbarColorController mToolbarColorController;
    protected CustomTabStatusBarColorProvider mStatusBarColorProvider;
    protected CustomTabActivityTabFactory mTabFactory;
    protected CustomTabIntentHandler mCustomTabIntentHandler;
    protected CustomTabNightModeStateController mNightModeStateController;
    protected @Nullable WebappActivityCoordinator mWebappActivityCoordinator;

    // This is to give the right package name while using the client's resources during an
    // overridePendingTransition call.
    // TODO(ianwen, yusufo): Figure out a solution to extract external resources without having to
    // change the package name.
    protected boolean mShouldOverridePackage;

    /**
     * Builds {@link BrowserServicesIntentDataProvider} for this {@link CustomTabActivity}.
     */
    protected abstract BrowserServicesIntentDataProvider buildIntentDataProvider(
            Intent intent, @CustomTabsIntent.ColorScheme int colorScheme);

    /**
     * @return The {@link BrowserServicesIntentDataProvider} for this {@link CustomTabActivity}.
     */
    @VisibleForTesting
    public abstract BrowserServicesIntentDataProvider getIntentDataProvider();

    /**
     * @return Whether the activity window is initially translucent.
     */
    public static boolean isWindowInitiallyTranslucent(Activity activity) {
        return activity instanceof TranslucentCustomTabActivity
                || activity instanceof SameTaskWebApkActivity;
    }

    @Override
    protected NightModeStateProvider createNightModeStateProvider() {
        // This is called before Dagger component is created, so using getInstance() directly.
        mNightModeStateController = new CustomTabNightModeStateController(getLifecycleDispatcher(),
                SystemNightModeMonitor.getInstance(), PowerSavingModeMonitor.getInstance());
        return mNightModeStateController;
    }

    @Override
    protected void initializeNightModeStateProvider() {
        mNightModeStateController.initialize(getDelegate(), getIntent());
    }

    @Override
    public void onNewIntent(Intent intent) {
        Intent originalIntent = getIntent();
        super.onNewIntent(intent);
        // Currently we can't handle arbitrary updates of intent parameters, so make sure
        // getIntent() returns the same intent as before.
        setIntent(originalIntent);

        // Color scheme doesn't matter here: currently we don't support updating UI using Intents.
        BrowserServicesIntentDataProvider dataProvider =
                buildIntentDataProvider(intent, CustomTabsIntent.COLOR_SCHEME_LIGHT);

        mCustomTabIntentHandler.onNewIntent(dataProvider);
    }

    @Override
    protected RootUiCoordinator createRootUiCoordinator() {
        return new BaseCustomTabRootUiCoordinator(this, getShareDelegateSupplier(),
                mToolbarCoordinator, mNavigationController, getActivityTabProvider(),
                mTabModelProfileSupplier, mBookmarkBridgeSupplier);
    }

    /**
     * Called when the {@link BaseCustomTabActivityComponent} was created.
     */
    protected void onComponentCreated(BaseCustomTabActivityComponent component) {
        mDelegateFactory = component.resolveTabDelegateFactory();
        mToolbarCoordinator = component.resolveToolbarCoordinator();
        mNavigationController = component.resolveNavigationController();
        mTabProvider = component.resolveTabProvider();
        mToolbarColorController = component.resolveToolbarColorController();
        mStatusBarColorProvider = component.resolveCustomTabStatusBarColorProvider();
        mTabFactory = component.resolveTabFactory();
        mCustomTabIntentHandler = component.resolveIntentHandler();

        component.resolveCompositorContentInitializer();
        component.resolveTaskDescriptionHelper();

        BrowserServicesIntentDataProvider intentDataProvider = getIntentDataProvider();
        if (intentDataProvider.isWebappOrWebApkActivity()) {
            mWebappActivityCoordinator = component.resolveWebappActivityCoordinator();
        }
        if (intentDataProvider.isWebApkActivity()) {
            component.resolveWebApkActivityCoordinator();
        }
    }

    /**
     * Return true when the activity has been launched in a separate task. The default behavior is
     * to reuse the same task and put the activity on top of the previous one (i.e hiding it). A
     * separate task creates a new entry in the Android recent screen.
     */
    protected boolean useSeparateTask() {
        final int separateTaskFlags =
                Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT;
        return (getIntent().getFlags() & separateTaskFlags) != 0;
    }

    @Override
    public void performPreInflationStartup() {
        super.performPreInflationStartup();

        WebappExtras webappExtras = getIntentDataProvider().getWebappExtras();
        if (webappExtras != null) {
            // Set the title for web apps so that TalkBack says the web app's short name instead of
            // 'Chrome' or the activity's label ("Web app") when either launching the web app or
            // bringing it to the foreground via Android Recents.
            setTitle(webappExtras.shortName);
        }
    }

    @Override
    public void finishNativeInitialization() {
        if (isTaskRoot() && UsageStatsService.isEnabled()) {
            UsageStatsService.getInstance().createPageViewObserver(getTabModelSelector(), this);
        }

        super.finishNativeInitialization();
    }

    @Override
    protected TabModelSelector createTabModelSelector() {
        return mTabFactory.createTabModelSelector();
    }

    @Override
    protected Pair<ChromeTabCreator, ChromeTabCreator> createTabCreators() {
        return mTabFactory.createTabCreators();
    }

    @Override
    @ActivityType
    public int getActivityType() {
        return getIntentDataProvider().getActivityType();
    }

    @Override
    public void initializeCompositor() {
        super.initializeCompositor();
        getTabModelSelector().onNativeLibraryReady(getTabContentManager());
    }

    @Override
    public TabModelSelectorImpl getTabModelSelector() {
        return (TabModelSelectorImpl) super.getTabModelSelector();
    }

    @Override
    @Nullable
    public Tab getActivityTab() {
        return mTabProvider.getTab();
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
    public int getControlContainerHeightResource() {
        return R.dimen.custom_tabs_control_container_height;
    }

    @Override
    public boolean shouldPostDeferredStartupForReparentedTab() {
        if (!super.shouldPostDeferredStartupForReparentedTab()) return false;

        // Check {@link CustomTabActivityTabProvider#getInitialTabCreationMode()} because the
        // tab has not yet started loading in the common case due to ordering of
        // {@link ChromeActivity#onStartWithNative()} and
        // {@link CustomTabActivityTabController#onFinishNativeInitialization()}.
        @TabCreationMode
        int mode = mTabProvider.getInitialTabCreationMode();
        return (mode == TabCreationMode.HIDDEN || mode == TabCreationMode.EARLY);
    }

    @Override
    protected boolean handleBackPressed() {
        return mNavigationController.navigateOnBack();
    }

    @Override
    public void finish() {
        super.finish();
        BrowserServicesIntentDataProvider intentDataProvider = getIntentDataProvider();
        if (intentDataProvider != null && intentDataProvider.shouldAnimateOnFinish()) {
            mShouldOverridePackage = true;
            overridePendingTransition(intentDataProvider.getAnimationEnterRes(),
                    intentDataProvider.getAnimationExitRes());
            mShouldOverridePackage = false;
        } else if (intentDataProvider != null && intentDataProvider.isOpenedByChrome()) {
            overridePendingTransition(R.anim.no_anim, R.anim.activity_close_exit);
        }
    }

    /**
     * Internal implementation that finishes the activity and removes the references from Android
     * recents.
     */
    protected void handleFinishAndClose() {
        Runnable defaultBehavior = () -> {
            if (useSeparateTask()) {
                ApiCompatibilityUtils.finishAndRemoveTask(this);
            } else {
                finish();
            }
        };
        BrowserServicesIntentDataProvider intentDataProvider = getIntentDataProvider();
        if (intentDataProvider.isTrustedWebActivity()
                || intentDataProvider.isWebappOrWebApkActivity()) {
            // TODO(pshmakov): extract all finishing logic from BaseCustomTabActivity.
            // In addition to TwaFinishHandler, create DefaultFinishHandler, PaymentsFinishHandler,
            // and SeparateTaskActivityFinishHandler, all implementing
            // CustomTabActivityNavigationController#FinishHandler. Pass the mode enum into
            // CustomTabActivityModule, so that it can provide the correct implementation.
            getComponent().resolveTwaFinishHandler().onFinish(defaultBehavior);
        } else {
            defaultBehavior.run();
        }
    }

    @Override
    public boolean canShowAppMenu() {
        if (getActivityTab() == null || !mToolbarCoordinator.toolbarIsInitialized()) return false;

        return super.canShowAppMenu();
    }

    @Override
    public int getActivityThemeColor() {
        BrowserServicesIntentDataProvider intentDataProvider = getIntentDataProvider();
        if (!intentDataProvider.isOpenedByChrome() && intentDataProvider.hasCustomToolbarColor()) {
            return intentDataProvider.getToolbarColor();
        }
        return TabState.UNSPECIFIED_THEME_COLOR;
    }

    @Override
    public int getBaseStatusBarColor(Tab tab) {
        return mStatusBarColorProvider.getBaseStatusBarColor(tab, super.getBaseStatusBarColor(tab));
    }

    @Override
    public void initDeferredStartupForActivity() {
        if (mWebappActivityCoordinator != null) {
            mWebappActivityCoordinator.initDeferredStartupForActivity();
        }
        super.initDeferredStartupForActivity();
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        Boolean result = KeyboardShortcuts.dispatchKeyEvent(
                event, this, mToolbarCoordinator.toolbarIsInitialized());
        return result != null ? result : super.dispatchKeyEvent(event);
    }

    @Override
    public void recordIntentToCreationTime(long timeMs) {
        super.recordIntentToCreationTime(timeMs);

        RecordHistogram.recordTimesHistogram(
                "MobileStartup.IntentToCreationTime.CustomTabs", timeMs);
        @ActivityType
        int activityType = getActivityType();
        if (activityType == ActivityType.WEBAPP || activityType == ActivityType.WEB_APK) {
            RecordHistogram.recordTimesHistogram(
                    "MobileStartup.IntentToCreationTime.Webapp", timeMs);
        }
        if (activityType == ActivityType.WEB_APK) {
            RecordHistogram.recordTimesHistogram(
                    "MobileStartup.IntentToCreationTime.WebApk", timeMs);
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (!mToolbarCoordinator.toolbarIsInitialized()) {
            return super.onKeyDown(keyCode, event);
        }
        return KeyboardShortcuts.onKeyDown(event, this, true, false)
                || super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onMenuOrKeyboardAction(int id, boolean fromMenu) {
        // Disable creating new tabs, bookmark, history, print, help, focus_url, etc.
        if (id == R.id.focus_url_bar || id == R.id.all_bookmarks_menu_id || id == R.id.help_id
                || id == R.id.recent_tabs_menu_id || id == R.id.new_incognito_tab_menu_id
                || id == R.id.new_tab_menu_id || id == R.id.open_history_menu_id) {
            return true;
        }
        return super.onMenuOrKeyboardAction(id, fromMenu);
    }

    public WebContentsDelegateAndroid getWebContentsDelegate() {
        assert mDelegateFactory != null;
        return mDelegateFactory.getWebContentsDelegate();
    }
}
