// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.CommandLine;
import org.chromium.base.IntentUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.UserData;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.infobar.ReaderModeInfoBar;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

/**
 * Manages UI effects for reader mode including hiding and showing the
 * reader mode and reader mode preferences toolbar icon and hiding the
 * browser controls when a reader mode page has finished loading.
 */
public class ReaderModeManager extends EmptyTabObserver implements UserData {
    public static final Class<ReaderModeManager> USER_DATA_KEY = ReaderModeManager.class;

    /** POSSIBLE means reader mode can be entered. */
    public static final int POSSIBLE = 0;

    /** NOT_POSSIBLE means reader mode cannot be entered. */
    public static final int NOT_POSSIBLE = 1;

    /** STARTED means reader mode is currently in reader mode. */
    public static final int STARTED = 2;

    /** The scheme used to access DOM-Distiller. */
    public static final String DOM_DISTILLER_SCHEME = "chrome-distiller";

    /** The intent extra that indicates origin from Reader Mode */
    public static final String EXTRA_READER_MODE_PARENT =
            "org.chromium.chrome.browser.dom_distiller.EXTRA_READER_MODE_PARENT";

    /** The url of the last page visited if the last page was reader mode page.  Otherwise null. */
    private String mReaderModePageUrl;

    /** Whether the fact that the current web page was distillable or not has been recorded. */
    private boolean mIsUmaRecorded;

    /** The tab state of distillation. */
    protected ReaderModeTabInfo mTabStatus;

    /** The tab this manager is attached to. */
    private Tab mTab;

    // Hold on to the InterceptNavigationDelegate that the custom tab uses.
    InterceptNavigationDelegate mCustomTabNavigationDelegate;

    ReaderModeManager(Tab tab) {
        super();
        mTab = tab;
        mTab.addObserver(this);
    }

    /**
     * Create an instance of the {@link ReaderModeManager} for the provided tab.
     * @param tab The tab that will have a manager instance attached to it.
     */
    public static void createForTab(Tab tab) {
        tab.getUserDataHost().setUserData(USER_DATA_KEY, new ReaderModeManager(tab));
    }

    /**
     * Clear the status map and references to other objects.
     */
    @Override
    public void destroy() {
        if (mTabStatus != null && mTabStatus.webContentsObserver != null) {
            mTabStatus.webContentsObserver.destroy();
            mTabStatus = null;
        }
    }

    @Override
    public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
        // If a distiller URL was loaded and this is a custom tab, add a navigation
        // handler to bring any navigations back to the main chrome activity.
        Activity activity = TabUtils.getActivity(tab);
        int uiType = CustomTabsUiType.DEFAULT;
        if (activity != null && activity.getIntent().getExtras() != null) {
            uiType = activity.getIntent().getExtras().getInt(
                    CustomTabIntentDataProvider.EXTRA_UI_TYPE);
        }
        if (tab == null || uiType != CustomTabsUiType.READER_MODE
                || !DomDistillerUrlUtils.isDistilledPage(params.getUrl())) {
            return;
        }

        WebContents webContents = tab.getWebContents();
        if (webContents == null) return;

        mCustomTabNavigationDelegate = (navParams) -> {
            if (DomDistillerUrlUtils.isDistilledPage(navParams.url)
                    || navParams.isExternalProtocol) {
                return false;
            }

            Intent returnIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(navParams.url));
            returnIntent.setClassName(activity, ChromeLauncherActivity.class.getName());

            // Set the parent ID of the tab to be created.
            returnIntent.putExtra(EXTRA_READER_MODE_PARENT,
                    IntentUtils.safeGetInt(activity.getIntent().getExtras(),
                            EXTRA_READER_MODE_PARENT, Tab.INVALID_TAB_ID));

            activity.startActivity(returnIntent);
            activity.finish();
            return true;
        };

        DomDistillerTabUtils.setInterceptNavigationDelegate(
                mCustomTabNavigationDelegate, webContents);
    }

    @Override
    public void onShown(Tab shownTab, @TabSelectionType int type) {
        // If the reader infobar was dismissed, stop here.
        if (mTabStatus != null && mTabStatus.isDismissed) return;

        // If there is no state info for this tab, create it.
        if (mTabStatus == null) {
            mTabStatus = new ReaderModeTabInfo();
            mTabStatus.status = NOT_POSSIBLE;
            mTabStatus.url = shownTab.getUrlString();
        }

        if (mTabStatus.distillabilityObserver == null) setDistillabilityObserver(shownTab);

        if (DomDistillerUrlUtils.isDistilledPage(shownTab.getUrlString())
                && !mTabStatus.isViewingReaderModePage) {
            mTabStatus.onStartedReaderMode();
        }

        // Make sure there is a WebContentsObserver on this tab's WebContents.
        if (mTabStatus.webContentsObserver == null) {
            mTabStatus.webContentsObserver = createWebContentsObserver(shownTab);
        }

        tryShowingInfoBar(shownTab);
    }

    @Override
    public void onHidden(Tab tab, @TabHidingType int reason) {
        if (mTabStatus != null && mTabStatus.isViewingReaderModePage) {
            long timeMs = mTabStatus.onExitReaderMode();
            recordReaderModeViewDuration(timeMs);
        }
    }

    @Override
    public void onDestroyed(Tab tab) {
        if (tab == null) return;

        // If the infobar was not shown for the previous navigation, record it now.
        if (mTabStatus != null) {
            if (!mTabStatus.showInfoBarRecorded) {
                recordInfoBarVisibilityForNavigation(false);
            }
            if (mTabStatus.isViewingReaderModePage) {
                long timeMs = mTabStatus.onExitReaderMode();
                recordReaderModeViewDuration(timeMs);
            }
            TabDistillabilityProvider.get(tab).removeObserver(mTabStatus.distillabilityObserver);
        }

        removeTabState();
    }

    /** Clear the reader mode state for this manager. */
    private void removeTabState() {
        if (mTabStatus == null) return;
        if (mTabStatus.webContentsObserver != null) {
            mTabStatus.webContentsObserver.destroy();
        }
        mTabStatus = null;
    }

    @Override
    public void onContentChanged(Tab tab) {
        // If the content change was because of distiller switching web contents or Reader Mode has
        // already been dismissed for this tab do nothing.
        if (mTabStatus != null && mTabStatus.isDismissed
                && !DomDistillerUrlUtils.isDistilledPage(tab.getUrlString())) {
            return;
        }

        if (mTabStatus == null) {
            mTabStatus = new ReaderModeTabInfo();
        }
        // If the tab state already existed, only reset the relevant data. Things like view duration
        // need to be preserved.
        mTabStatus.status = NOT_POSSIBLE;
        mTabStatus.url = tab.getUrlString();

        if (tab.getWebContents() != null) {
            mTabStatus.webContentsObserver = createWebContentsObserver(tab);
            if (DomDistillerUrlUtils.isDistilledPage(tab.getUrlString())) {
                mTabStatus.status = STARTED;
                mReaderModePageUrl = tab.getUrlString();
            }
        }
    }

    /**
     * Record if the infobar became visible on the current page. This can be overridden for testing.
     * @param visible If the infobar was visible at any time.
     */
    protected void recordInfoBarVisibilityForNavigation(boolean visible) {
        RecordHistogram.recordBooleanHistogram("DomDistiller.ReaderShownForPageLoad", visible);
    }

    /**
     * Notify the manager that the panel has completely closed.
     */
    public void onClosed(Tab tab, @StateChangeReason int reason) {
        RecordHistogram.recordBooleanHistogram("DomDistiller.InfoBarUsage", false);

        if (mTabStatus == null) return;
        mTabStatus.isDismissed = true;
    }

    protected WebContentsObserver createWebContentsObserver(final Tab tab) {
        return new WebContentsObserver(tab.getWebContents()) {
            /** Whether or not the previous navigation should be removed. */
            private boolean mShouldRemovePreviousNavigation;

            /** The index of the last committed distiller page in history. */
            private int mLastDistillerPageIndex;

            @Override
            public void didStartNavigation(NavigationHandle navigation) {
                if (!navigation.isInMainFrame() || navigation.isSameDocument()) return;

                // Reader Mode should not pollute the navigation stack. To avoid this, watch for
                // navigations and prepare to remove any that are "chrome-distiller" urls.
                NavigationController controller = mWebContents.get().getNavigationController();
                int index = controller.getLastCommittedEntryIndex();
                NavigationEntry entry = controller.getEntryAtIndex(index);

                if (entry != null && DomDistillerUrlUtils.isDistilledPage(entry.getUrl())) {
                    mShouldRemovePreviousNavigation = true;
                    mLastDistillerPageIndex = index;
                }

                // Make sure the tab was not destroyed.
                if (mTabStatus == null) return;

                mTabStatus.url = navigation.getUrl();
                if (DomDistillerUrlUtils.isDistilledPage(navigation.getUrl())) {
                    mTabStatus.status = STARTED;
                    mReaderModePageUrl = navigation.getUrl();
                }
            }

            @Override
            public void didFinishNavigation(NavigationHandle navigation) {
                // TODO(cjhopman): This should possibly ignore navigations that replace the entry
                // (like those from history.replaceState()).
                if (!navigation.hasCommitted() || !navigation.isInMainFrame()
                        || navigation.isSameDocument()) {
                    return;
                }

                if (mShouldRemovePreviousNavigation) {
                    mShouldRemovePreviousNavigation = false;
                    NavigationController controller = mWebContents.get().getNavigationController();
                    if (controller.getEntryAtIndex(mLastDistillerPageIndex) != null) {
                        controller.removeEntryAtIndex(mLastDistillerPageIndex);
                    }
                }

                // Make sure the tab was not destroyed.
                if (mTabStatus == null) return;

                mTabStatus.status = POSSIBLE;
                if (!TextUtils.equals(navigation.getUrl(),
                            DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(
                                    mReaderModePageUrl))) {
                    mTabStatus.status = NOT_POSSIBLE;
                    mIsUmaRecorded = false;
                }
                mReaderModePageUrl = null;

                if (mTabStatus.status == POSSIBLE) tryShowingInfoBar(tab);
            }

            @Override
            public void navigationEntryCommitted() {
                // Make sure the tab was not destroyed.
                if (mTabStatus == null) return;
                // Reset closed state of reader mode in this tab once we know a navigation is
                // happening.
                mTabStatus.isDismissed = false;

                // If the infobar was not shown for the previous navigation, record it now.
                if (tab != null && !tab.isNativePage() && !tab.isBeingRestored()) {
                    recordInfoBarVisibilityForNavigation(false);
                }
                mTabStatus.showInfoBarRecorded = false;

                if (tab != null && !DomDistillerUrlUtils.isDistilledPage(tab.getUrlString())
                        && mTabStatus.isViewingReaderModePage) {
                    long timeMs = mTabStatus.onExitReaderMode();
                    recordReaderModeViewDuration(timeMs);
                }
            }
        };
    }

    /**
     * Record the amount of time the user spent in Reader Mode.
     * @param timeMs The amount of time in ms that the user spent in Reader Mode.
     */
    private void recordReaderModeViewDuration(long timeMs) {
        RecordHistogram.recordLongTimesHistogram("DomDistiller.Time.ViewingReaderModePage", timeMs);
    }

    /**
     * Try showing the reader mode infobar.
     * @param tab The tab to show the infobar on.
     */
    private void tryShowingInfoBar(Tab tab) {
        if (tab == null || tab.getWebContents() == null) return;

        // Test if the user is requesting the desktop site. Ignore this if distiller is set to
        // ALWAYS_TRUE.
        boolean usingRequestDesktopSite =
                tab.getWebContents().getNavigationController().getUseDesktopUserAgent()
                && !DomDistillerTabUtils.isHeuristicAlwaysTrue();

        if (mTabStatus == null || usingRequestDesktopSite || mTabStatus.status != POSSIBLE
                || mTabStatus.isDismissed) {
            return;
        }

        ReaderModeInfoBar.showReaderModeInfoBar(tab);
    }

    public void activateReaderMode(Tab tab) {
        RecordHistogram.recordBooleanHistogram("DomDistiller.InfoBarUsage", true);

        if (DomDistillerTabUtils.isCctMode() && !SysUtils.isLowEndDevice()) {
            distillInCustomTab(tab);
        } else {
            navigateToReaderMode(tab);
        }
    }

    /**
     * Navigate the current tab to a Reader Mode URL.
     */
    private void navigateToReaderMode(Tab tab) {
        WebContents webContents = tab.getWebContents();
        if (webContents == null) return;

        String url = webContents.getLastCommittedUrl();
        if (url == null) return;

        if (mTabStatus != null) mTabStatus.onStartedReaderMode();

        // Make sure to exit fullscreen mode before navigating.
        getFullscreenManager(tab).onExitFullscreen(tab);

        // RenderWidgetHostViewAndroid hides the controls after transitioning to reader mode.
        // See the long history of the issue in https://crbug.com/825765, https://crbug.com/853686,
        // https://crbug.com/861618, https://crbug.com/922388.
        // TODO(pshmakov): find a proper solution instead of this workaround.
        getFullscreenManager(tab).getBrowserVisibilityDelegate().showControlsTransient();

        DomDistillerTabUtils.distillCurrentPageAndView(webContents);
    }

    private ChromeFullscreenManager getFullscreenManager(Tab tab) {
        // TODO(1069815): Remove this ChromeActivity cast once NightModeStateProvider is
        //                accessible via another mechanism.
        ChromeActivity activity = (ChromeActivity) TabUtils.getActivity(tab);
        return activity.getFullscreenManager();
    }

    private NightModeStateProvider getNightModeStateProvider(Tab tab) {
        // TODO(1069815): Remove this ChromeActivity cast once ChromeFullscreenManager is
        //                accessible via another mechanism.
        ChromeActivity activity = (ChromeActivity) TabUtils.getActivity(tab);
        return activity.getNightModeStateProvider();
    }

    private void distillInCustomTab(Tab tab) {
        Activity activity = TabUtils.getActivity(tab);
        WebContents webContents = tab.getWebContents();
        if (webContents == null) return;

        String url = webContents.getLastCommittedUrl();
        if (url == null) return;

        if (mTabStatus != null) mTabStatus.onStartedReaderMode();

        DomDistillerTabUtils.distillCurrentPage(webContents);

        String distillerUrl = DomDistillerUrlUtils.getDistillerViewUrlFromUrl(
                DOM_DISTILLER_SCHEME, url, webContents.getTitle());

        CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setShowTitle(true);
        builder.setColorScheme(getNightModeStateProvider(tab).isInNightMode()
                        ? CustomTabsIntent.COLOR_SCHEME_DARK
                        : CustomTabsIntent.COLOR_SCHEME_LIGHT);
        CustomTabsIntent customTabsIntent = builder.build();
        customTabsIntent.intent.setClassName(activity, CustomTabActivity.class.getName());

        // Customize items on menu as Reader Mode UI to show 'Find in page' and 'Preference' only.
        CustomTabIntentDataProvider.addReaderModeUIExtras(customTabsIntent.intent);

        // Add the parent ID as an intent extra for back button functionality.
        customTabsIntent.intent.putExtra(EXTRA_READER_MODE_PARENT, tab.getId());

        customTabsIntent.launchUrl(activity, Uri.parse(distillerUrl));
    }

    /**
     * Set the observer for updating reader mode status based on whether or not the page should
     * be viewed in reader mode.
     * @param tabToObserve The tab to attach the observer to.
     */
    private void setDistillabilityObserver(final Tab tabToObserve) {
        mTabStatus.distillabilityObserver = (tab, isDistillable, isLast, isMobileOptimized) -> {
            // It is possible that the tab was destroyed before this callback happens.
            // TODO(wychen/mdjones): Remove the callback when a Tab/WebContents is
            // destroyed so that this never happens.
            if (mTabStatus == null) return;

            // Make sure the page didn't navigate while waiting for a response.
            if (!tab.getUrlString().equals(mTabStatus.url)) return;

            boolean excludedMobileFriendly =
                    DomDistillerTabUtils.shouldExcludeMobileFriendly() && isMobileOptimized;
            if (isDistillable && !excludedMobileFriendly) {
                mTabStatus.status = POSSIBLE;
                tryShowingInfoBar(tab);
            } else {
                mTabStatus.status = NOT_POSSIBLE;
            }
            if (!mIsUmaRecorded && (mTabStatus.status == POSSIBLE || isLast)) {
                mIsUmaRecorded = true;
                RecordHistogram.recordBooleanHistogram(
                        "DomDistiller.PageDistillable", mTabStatus.status == POSSIBLE);
            }
        };
        TabDistillabilityProvider.get(tabToObserve).addObserver(mTabStatus.distillabilityObserver);
    }

    /** @return Whether Reader mode and its new UI are enabled. */
    public static boolean isEnabled() {
        boolean enabled = CommandLine.getInstance().hasSwitch(ChromeSwitches.ENABLE_DOM_DISTILLER)
                && !CommandLine.getInstance().hasSwitch(
                           ChromeSwitches.DISABLE_READER_MODE_BOTTOM_BAR)
                && DomDistillerTabUtils.isDistillerHeuristicsEnabled();
        return enabled;
    }

    /**
     * Determine if Reader Mode created the intent for a tab being created.
     * @param intent The Intent creating a new tab.
     * @return True whether the intent was created by Reader Mode.
     */
    public static boolean isReaderModeCreatedIntent(@NonNull Intent intent) {
        int readerParentId = IntentUtils.safeGetInt(
                intent.getExtras(), ReaderModeManager.EXTRA_READER_MODE_PARENT, Tab.INVALID_TAB_ID);
        return readerParentId != Tab.INVALID_TAB_ID;
    }
}
