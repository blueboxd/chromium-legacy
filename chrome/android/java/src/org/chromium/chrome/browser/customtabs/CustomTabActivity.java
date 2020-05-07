// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_DARK;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT;

import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason.USER_NAVIGATION;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantFacade;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityCoordinator;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler.IntentIgnoringCriterion;
import org.chromium.chrome.browser.customtabs.dependency_injection.BaseCustomTabActivityModule;
import org.chromium.chrome.browser.customtabs.dependency_injection.CustomTabActivityComponent;
import org.chromium.chrome.browser.customtabs.dependency_injection.CustomTabActivityModule;
import org.chromium.chrome.browser.customtabs.features.CustomTabNavigationBarController;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityCommonsModule;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.page_info.ChromePageInfoControllerDelegate;
import org.chromium.chrome.browser.previews.Previews;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

/**
 * The activity for custom tabs. It will be launched on top of a client's task.
 */
public class CustomTabActivity extends BaseCustomTabActivity<CustomTabActivityComponent> {
    private CustomTabIntentDataProvider mIntentDataProvider;
    private CustomTabsSessionToken mSession;
    private CustomTabActivityTabController mTabController;
    private TrustedWebActivityCoordinator mTwaCoordinator;

    private final CustomTabsConnection mConnection = CustomTabsConnection.getInstance();


    private CustomTabActivityTabProvider.Observer mTabChangeObserver =
            new CustomTabActivityTabProvider.Observer() {
        @Override
        public void onInitialTabCreated(@NonNull Tab tab, int mode) {
            resetPostMessageHandlersForCurrentSession();
        }

        @Override
        public void onTabSwapped(@NonNull Tab tab) {
            resetPostMessageHandlersForCurrentSession();
        }

        @Override
        public void onAllTabsClosed() {
            resetPostMessageHandlersForCurrentSession();
        }
    };

    @Override
    protected Drawable getBackgroundDrawable() {
        int initialBackgroundColor = mIntentDataProvider.getInitialBackgroundColor();
        if (mIntentDataProvider.isTrustedIntent() && initialBackgroundColor != Color.TRANSPARENT) {
            return new ColorDrawable(initialBackgroundColor);
        } else {
            return super.getBackgroundDrawable();
        }
    }

    @Override
    public void performPreInflationStartup() {
        // Parse the data from the Intent before calling super to allow the Intent to customize
        // the Activity parameters, including the background of the page.
        // Note that color scheme is fixed for the lifetime of Activity: if the system setting
        // changes, we recreate the activity.
        mIntentDataProvider = (CustomTabIntentDataProvider) buildIntentDataProvider(
                getIntent(), getColorScheme());

        super.performPreInflationStartup();
        mTabProvider.addObserver(mTabChangeObserver);
        // We might have missed an onInitialTabCreated event.
        resetPostMessageHandlersForCurrentSession();

        mSession = mIntentDataProvider.getSession();

        CustomTabNavigationBarController.updateNavigationBarColor(this, mIntentDataProvider);
    }

    private int getColorScheme() {
        if (mNightModeStateController != null) {
            return mNightModeStateController.isInNightMode() ? COLOR_SCHEME_DARK :
                    COLOR_SCHEME_LIGHT;
        }
        assert false : "NightModeStateController should have been already created";
        return COLOR_SCHEME_LIGHT;
    }

    @Override
    public boolean shouldAllocateChildConnection() {
        return mTabController.shouldAllocateChildConnection();
    }

    @Override
    public void performPostInflationStartup() {
        super.performPostInflationStartup();
        getStatusBarColorController().updateStatusBarColor();

        // Properly attach tab's InfoBarContainer to the view hierarchy if the tab is already
        // attached to a ChromeActivity, as the main tab might have been initialized prior to
        // inflation.
        if (mTabProvider.getTab() != null) {
            ViewGroup bottomContainer = (ViewGroup) findViewById(R.id.bottom_container);
            InfoBarContainer.get(mTabProvider.getTab()).setParentView(bottomContainer);
        }

        // Setting task title and icon to be null will preserve the client app's title and icon.
        ApiCompatibilityUtils.setTaskDescription(this, null, null,
                mIntentDataProvider.getToolbarColor());
        getComponent().resolveBottomBarDelegate().showBottomBarIfNecessary();
    }

    @Override
    public void finishNativeInitialization() {
        if (!mIntentDataProvider.isInfoPage()) FirstRunSignInProcessor.start(this);

        mConnection.showSignInToastIfNecessary(mSession, getIntent());

        super.finishNativeInitialization();

        // We start the Autofill Assistant after the call to super.finishNativeInitialization() as
        // this will initialize the BottomSheet that is used to embed the Autofill Assistant bottom
        // bar.
        if (AutofillAssistantFacade.isAutofillAssistantEnabled(getInitialIntent())) {
            AutofillAssistantFacade.start(this);
        }
    }

    private void resetPostMessageHandlersForCurrentSession() {
        Tab tab = mTabProvider.getTab();
        WebContents webContents = tab == null ? null : tab.getWebContents();
        mConnection.resetPostMessageHandlerForSession(
                mIntentDataProvider.getSession(), webContents);
    }

    @Override
    public void createContextualSearchTab(String searchUrl) {
        if (getActivityTab() == null) return;
        getActivityTab().loadUrl(new LoadUrlParams(searchUrl));
    }

    @Override
    public AppMenuPropertiesDelegate createAppMenuPropertiesDelegate() {
        return new CustomTabAppMenuPropertiesDelegate(this, getActivityTabProvider(),
                getMultiWindowModeStateDispatcher(), getTabModelSelector(), getToolbarManager(),
                getWindow().getDecorView(), mBookmarkBridgeSupplier,
                mIntentDataProvider.getUiType(), mIntentDataProvider.getMenuTitles(),
                mIntentDataProvider.isOpenedByChrome(),
                mIntentDataProvider.shouldShowShareMenuItem(),
                mIntentDataProvider.shouldShowStarButton(),
                mIntentDataProvider.shouldShowDownloadButton(), mIntentDataProvider.isIncognito());
    }

    @Override
    public String getPackageName() {
        if (mShouldOverridePackage) return mIntentDataProvider.getClientPackageName();
        return super.getPackageName();
    }

    @Override
    public boolean onOptionsItemSelected(int itemId, @Nullable Bundle menuItemData) {
        int menuIndex =
                CustomTabAppMenuPropertiesDelegate.getIndexOfMenuItemFromBundle(menuItemData);
        if (menuIndex >= 0) {
            mIntentDataProvider.clickMenuItemWithUrlAndTitle(
                    this, menuIndex, getActivityTab().getUrlString(), getActivityTab().getTitle());
            RecordUserAction.record("CustomTabsMenuCustomMenuItem");
            return true;
        }

        return super.onOptionsItemSelected(itemId, menuItemData);
    }

    @Override
    public boolean onMenuOrKeyboardAction(int id, boolean fromMenu) {
        if (id == R.id.bookmark_this_page_id) {
            addOrEditBookmark(getActivityTab());
            RecordUserAction.record("MobileMenuAddToBookmarks");
            return true;
        } else if (id == R.id.open_in_browser_id) {
            // Need to get tab before calling openCurrentUrlInBrowser or else it will be null.
            Tab tab = mTabProvider.getTab();
            if (mNavigationController.openCurrentUrlInBrowser(false)) {
                RecordUserAction.record("CustomTabsMenuOpenInChrome");
                WebContents webContents = tab == null ? null : tab.getWebContents();
                mConnection.notifyOpenInBrowser(mSession, webContents);
            }
            return true;
        } else if (id == R.id.info_menu_id) {
            Tab tab = getTabModelSelector().getCurrentTab();
            if (tab == null) return false;
            WebContents webContents = tab.getWebContents();
            PageInfoController.show(this, webContents, getToolbarManager().getContentPublisher(),
                    PageInfoController.OpenedFromSource.MENU,
                    new ChromePageInfoControllerDelegate(this, webContents,
                            this::getModalDialogManager,
                            /*offlinePageLoadUrlDelegate=*/
                            new OfflinePageUtils.TabOfflinePageLoadUrlDelegate(tab)));
            return true;
        }
        return super.onMenuOrKeyboardAction(id, fromMenu);
    }

    @Override
    public void onUpdateStateChanged() {}

    @Override
    protected BrowserServicesIntentDataProvider buildIntentDataProvider(
            Intent intent, @CustomTabsIntent.ColorScheme int colorScheme) {
        return new CustomTabIntentDataProvider(intent, this, colorScheme);
    }

    @Override
    public BrowserServicesIntentDataProvider getIntentDataProvider() {
        return mIntentDataProvider;
    }

    @Override
    public boolean supportsAppMenu() {
        // The media viewer has no default menu items, so if there are also no custom items, we
        // should disable the menu altogether.
        if (mIntentDataProvider.isMediaViewer() && mIntentDataProvider.getMenuTitles().isEmpty()) {
            return false;
        }
        return super.supportsAppMenu();
    }

    /**
     * Show the web page with CustomTabActivity, without any navigation control.
     * Used in showing the terms of services page or help pages for Chrome.
     * @param context The current activity context.
     * @param url The url of the web page.
     */
    public static void showInfoPage(Context context, String url) {
        // TODO(xingliu): The title text will be the html document title, figure out if we want to
        // use Chrome strings here as EmbedContentViewActivity does.
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder()
                        .setShowTitle(true)
                        .setColorScheme(NightModeUtils.isInNightMode(context) ? COLOR_SCHEME_DARK
                                                                              : COLOR_SCHEME_LIGHT)
                        .build();
        customTabIntent.intent.setData(Uri.parse(url));

        Intent intent = LaunchIntentDispatcher.createCustomTabActivityIntent(
                context, customTabIntent.intent);
        intent.setPackage(context.getPackageName());
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.INFO_PAGE);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        if (!(context instanceof Activity)) intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        IntentHandler.addTrustedIntentExtras(intent);

        context.startActivity(intent);
    }

    @Override
    protected boolean requiresFirstRunToBeCompleted(Intent intent) {
        // Custom Tabs can be used to open Chrome help pages before the ToS has been accepted.
        if (CustomTabIntentDataProvider.isTrustedCustomTab(intent, mSession)
                && IntentUtils.safeGetIntExtra(intent, CustomTabIntentDataProvider.EXTRA_UI_TYPE,
                           CustomTabIntentDataProvider.CustomTabsUiType.DEFAULT)
                        == CustomTabIntentDataProvider.CustomTabsUiType.INFO_PAGE) {
            return false;
        }

        return super.requiresFirstRunToBeCompleted(intent);
    }

    @Override
    public boolean canShowTrustedCdnPublisherUrl() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SHOW_TRUSTED_PUBLISHER_URL)) {
            return false;
        }

        if (Previews.isPreview(mTabProvider.getTab())) return false;

        String publisherUrlPackage = mConnection.getTrustedCdnPublisherUrlPackage();
        return publisherUrlPackage != null
                && publisherUrlPackage.equals(mConnection.getClientPackageNameForSession(mSession));
    }

    @Override
    protected CustomTabActivityComponent createComponent(
            ChromeActivityCommonsModule commonsModule) {
        // mIntentHandler comes from the base class.
        IntentIgnoringCriterion intentIgnoringCriterion =
                (intent) -> mIntentHandler.shouldIgnoreIntent(intent);

        BaseCustomTabActivityModule baseCustomTabsModule = new BaseCustomTabActivityModule(
                mIntentDataProvider, mNightModeStateController, intentIgnoringCriterion);
        CustomTabActivityModule customTabsModule =
                new CustomTabActivityModule(getStartupTabPreloader());

        CustomTabActivityComponent component =
                ChromeApplication.getComponent().createCustomTabActivityComponent(
                        commonsModule, baseCustomTabsModule, customTabsModule);

        onComponentCreated(component);

        mTabController = component.resolveTabController();
        component.resolveUmaTracker();
        CustomTabActivityClientConnectionKeeper connectionKeeper =
                component.resolveConnectionKeeper();
        mNavigationController.setFinishHandler((reason) -> {
            if (reason == USER_NAVIGATION) connectionKeeper.recordClientConnectionStatus();
            handleFinishAndClose();
        });
        component.resolveSessionHandler();
        component.resolveCustomTabIncognitoManager();

        if (mIntentDataProvider.isTrustedWebActivity()) {
            mTwaCoordinator = component.resolveTrustedWebActivityCoordinator();
        }

        return component;
    }

    /**
     * @return The package name of the Trusted Web Activity, if the activity is a TWA; null
     * otherwise.
     */
    @Nullable
    public String getTwaPackage() {
        return mTwaCoordinator == null ? null : mTwaCoordinator.getTwaPackage();
    }

    /**
     * @return Whether the app is running in the "Trusted Web Activity" mode, where the TWA-specific
     *         UI is shown.
     */
    @Nullable
    public Boolean isInTwaMode() {
        return mTwaCoordinator == null ? false : mTwaCoordinator.isInTwaMode();
    }
}
