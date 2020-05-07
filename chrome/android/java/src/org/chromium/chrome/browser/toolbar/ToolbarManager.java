// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.ComponentCallbacks;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.ActionBar;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.TabLoadStatus;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.ThemeColorProvider.ThemeColorObserver;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.compositor.Invalidator;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeState;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.findinpage.FindToolbarManager;
import org.chromium.chrome.browser.findinpage.FindToolbarObserver;
import org.chromium.chrome.browser.fullscreen.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager.FullscreenListener;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.identity_disc.IdentityDiscController;
import org.chromium.chrome.browser.metrics.OmniboxStartupMetrics;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.ntp.FakeboxDelegate;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.previews.Previews;
import org.chromium.chrome.browser.previews.PreviewsAndroidBridge;
import org.chromium.chrome.browser.previews.PreviewsUma;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabThemeColorHelper;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupPopupUi;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.chrome.browser.toolbar.bottom.BottomTabSwitcherActionMenuCoordinator;
import org.chromium.chrome.browser.toolbar.bottom.BottomToolbarConfiguration;
import org.chromium.chrome.browser.toolbar.bottom.BottomToolbarVariationManager;
import org.chromium.chrome.browser.toolbar.load_progress.LoadProgressCoordinator;
import org.chromium.chrome.browser.toolbar.top.ActionModeController;
import org.chromium.chrome.browser.toolbar.top.ActionModeController.ActionBarDelegate;
import org.chromium.chrome.browser.toolbar.top.TabSwitcherActionMenuCoordinator;
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.toolbar.top.ToolbarActionModeCallback;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator;
import org.chromium.chrome.browser.toolbar.top.ViewShiftingActionBarDelegate;
import org.chromium.chrome.browser.ui.TabObscuringHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuObserver;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.ui.appmenu.MenuButtonDelegate;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.TokenHolder;

import java.util.List;

/**
 * Contains logic for managing the toolbar visual component.  This class manages the interactions
 * with the rest of the application to ensure the toolbar is always visually up to date.
 */
public class ToolbarManager implements UrlFocusChangeListener, ThemeColorObserver,
                                       MenuButtonDelegate, AccessibilityUtil.Observer {
    /**
     * Handle UI updates of menu icons. Only applicable for phones.
     */
    public interface MenuDelegatePhone {
        /**
         * Called when current tab's loading status changes.
         *
         * @param isLoading Whether the current tab is loading.
         */
        void updateReloadButtonState(boolean isLoading);
    }

    /**
     * The number of ms to wait before reporting to UMA omnibox interaction metrics.
     */
    private static final int RECORD_UMA_PERFORMANCE_METRICS_DELAY_MS = 30000;

    private final IncognitoStateProvider mIncognitoStateProvider;
    private final TabCountProvider mTabCountProvider;
    private final ThemeColorProvider mTabThemeColorProvider;
    private final AppThemeColorProvider mAppThemeColorProvider;
    private final TopToolbarCoordinator mToolbar;
    private final ToolbarControlContainer mControlContainer;
    private final FullscreenListener mFullscreenListener;
    private final TabObscuringHandler mTabObscuringHandler;
    private final ScrimCoordinator mScrimCoordinator;

    private BottomControlsCoordinator mBottomControlsCoordinator;
    private TabModelSelector mTabModelSelector;
    private TabModelSelectorObserver mTabModelSelectorObserver;
    private ActivityTabProvider.ActivityTabTabObserver mActivityTabTabObserver;
    private final ActivityTabProvider mActivityTabProvider;
    private MenuDelegatePhone mMenuDelegatePhone;
    private final LocationBarModel mLocationBarModel;
    private ObservableSupplier<BookmarkBridge> mBookmarkBridgeSupplier;
    private ObservableSupplier<Profile> mProfileSupplier;
    private final Callback<BookmarkBridge> mBookmarkBridgeSupplierObserver;
    private final Callback<Profile> mProfileSupplierObserver;
    private TemplateUrlServiceObserver mTemplateUrlObserver;
    private LocationBar mLocationBar;
    private FindToolbarManager mFindToolbarManager;
    private @Nullable AppMenuPropertiesDelegate mAppMenuPropertiesDelegate;
    private OverviewModeBehavior mOverviewModeBehavior;
    private LayoutManager mLayoutManager;
    private final ObservableSupplier<ShareDelegate> mShareDelegateSupplier;
    private ObservableSupplierImpl<View> mTabGroupPopUiParentSupplier;
    private @Nullable TabGroupPopupUi mTabGroupPopupUi;

    private TabObserver mTabObserver;
    private BookmarkBridge.BookmarkModelObserver mBookmarksObserver;
    private FindToolbarObserver mFindToolbarObserver;
    private OverviewModeObserver mOverviewModeObserver;
    private SceneChangeObserver mSceneChangeObserver;
    private final ActionBarDelegate mActionBarDelegate;
    private ActionModeController mActionModeController;
    private final Callback<Boolean> mUrlFocusChangedCallback;
    private final Handler mHandler = new Handler();
    private final ChromeActivity mActivity;
    private final ChromeFullscreenManager mFullscreenManager;
    private UrlFocusChangeListener mLocationBarFocusObserver;
    private ComponentCallbacks mComponentCallbacks;
    private final LoadProgressCoordinator mProgressBarCoordinator;
    private final ToolbarTabControllerImpl mToolbarTabController;

    private BrowserStateBrowserControlsVisibilityDelegate mControlsVisibilityDelegate;
    private int mFullscreenFocusToken = TokenHolder.INVALID_TOKEN;
    private int mFullscreenFindInPageToken = TokenHolder.INVALID_TOKEN;
    private int mFullscreenMenuToken = TokenHolder.INVALID_TOKEN;
    private int mFullscreenHighlightToken = TokenHolder.INVALID_TOKEN;

    private boolean mNativeLibraryReady;
    private boolean mTabRestoreCompleted;

    private AppMenuButtonHelper mAppMenuButtonHelper;

    private boolean mInitializedWithNative;
    private Runnable mOnInitializedRunnable;

    private boolean mShouldUpdateToolbarPrimaryColor = true;
    private int mCurrentThemeColor;

    private OmniboxStartupMetrics mOmniboxStartupMetrics;

    private boolean mIsBottomToolbarVisible;

    private AppMenuHandler mAppMenuHandler;

    private int mCurrentOrientation;

    private Supplier<Boolean> mCanAnimateNativeBrowserControls;

    /**
     * Runnable for the home and search accelerator button when Start Surface home page is enabled.
     */
    private Supplier<Boolean> mShowStartSurfaceSupplier;
    // TODO(https://crbug.com/865801): Consolidate isBottomToolbarVisible(),
    // onBottomToolbarVisibilityChanged, etc. to all use mBottomToolbarVisibilitySupplier.
    private final ObservableSupplierImpl<Boolean> mBottomToolbarVisibilitySupplier;

    /** A token held while the toolbar/omnibox is obscuring all visible tabs. */
    private int mTabObscuringToken;

    /**
     * Creates a ToolbarManager object.
     * @param controlContainer The container of the toolbar.
     * @param invalidator Handler for synchronizing invalidations across UI elements.
     * @param urlFocusChangedCallback The callback to be notified when the URL focus changes.
     * @param themeColorProvider The ThemeColorProvider object.
     * @param tabObscuringHandler Delegate object handling obscuring views.
     * @param shareDelegateSupplier Supplier for ShareDelegate.
     * @param bottomToolbarVisibilitySupplier
     * @param identityDiscController The controller that coordinates the state of the identity disc
     * @param buttonDataProviders The list of button data providers for the optional toolbar button
     *         in the browsing mode toolbar, given in precedence order.
     * @param tabProvider The {@link ActivityTabProvider} for accessing current activity tab.
     * @param scrimCoordinator A means of showing the scrim.
     * @param toolbarActionModeCallback Callback that communicates changes in the conceptual mode
     *                                  of toolbar interaction.
     * @param findToolbarManager The manager for the find in page function.
     * @param profileSupplier Supplier of the currently applicable profile.
     * @param bookmarkBridgeSupplier Supplier of the bookmark bridge for the current profile.
     */
    public ToolbarManager(ChromeActivity activity, ChromeFullscreenManager fullscreenManager,
            ToolbarControlContainer controlContainer, Invalidator invalidator,
            Callback<Boolean> urlFocusChangedCallback, ThemeColorProvider themeColorProvider,
            TabObscuringHandler tabObscuringHandler,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            ObservableSupplierImpl<Boolean> bottomToolbarVisibilitySupplier,
            IdentityDiscController identityDiscController,
            List<ButtonDataProvider> buttonDataProviders, ActivityTabProvider tabProvider,
            ScrimCoordinator scrimCoordinator, ToolbarActionModeCallback toolbarActionModeCallback,
            FindToolbarManager findToolbarManager, ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<BookmarkBridge> bookmarkBridgeSupplier) {
        mActivity = activity;
        mFullscreenManager = fullscreenManager;
        mActionBarDelegate = new ViewShiftingActionBarDelegate(activity, controlContainer);
        mShareDelegateSupplier = shareDelegateSupplier;
        mBottomToolbarVisibilitySupplier = bottomToolbarVisibilitySupplier;
        mScrimCoordinator = scrimCoordinator;
        mTabObscuringToken = TokenHolder.INVALID_TOKEN;

        mLocationBarModel = new LocationBarModel(activity);
        mControlContainer = controlContainer;
        assert mControlContainer != null;
        mUrlFocusChangedCallback = urlFocusChangedCallback;

        mBookmarkBridgeSupplier = bookmarkBridgeSupplier;
        // We need to capture a reference to setBookmarkBridge/setCurrentProfile in order to remove
        // them later; there is no guarantee in the JLS that referencing the same method later will
        // reference the same object.
        mBookmarkBridgeSupplierObserver = this::setBookmarkBridge;
        mBookmarkBridgeSupplier.addObserver(mBookmarkBridgeSupplierObserver);
        mProfileSupplier = profileSupplier;
        mProfileSupplierObserver = this::setCurrentProfile;
        mProfileSupplier.addObserver(mProfileSupplierObserver);

        mComponentCallbacks = new ComponentCallbacks() {
            @Override
            public void onConfigurationChanged(Configuration configuration) {
                int newOrientation = configuration.orientation;
                if (newOrientation == mCurrentOrientation) {
                    return;
                }
                mCurrentOrientation = newOrientation;
                onOrientationChange(newOrientation);
            }
            @Override
            public void onLowMemory() {}
        };
        mActivity.registerComponentCallbacks(mComponentCallbacks);

        mLocationBarFocusObserver = new UrlFocusChangeListener() {
            /** The params used to control how the scrim behaves when shown for the omnibox. */
            private PropertyModel mScrimModel;

            /** Whether the scrim was shown on focus. */
            private boolean mScrimShown;

            /** The light color to use for the scrim on the NTP. */
            private int mLightScrimColor;

            @Override
            public void onUrlFocusChange(boolean hasFocus) {
                if (mScrimModel == null) {
                    Resources res = mActivity.getResources();
                    int topMargin = res.getDimensionPixelSize(R.dimen.tab_strip_height);
                    mLightScrimColor = ApiCompatibilityUtils.getColor(
                            res, R.color.omnibox_focused_fading_background_color_light);
                    View scrimTarget = mActivity.getCompositorViewHolder();

                    Callback<Boolean> visibilityChangeCallback = (visible) -> {
                        if (visible) {
                            // It's possible for the scrim to unfocus and refocus without the
                            // visibility actually changing. In this case we have to make sure we
                            // unregister the previous token before acquiring a new one.
                            // TODO(mdjones): Consider calling the visibility change event in the
                            //                scrim between requests the show.
                            int oldToken = mTabObscuringToken;
                            mTabObscuringToken = mTabObscuringHandler.obscureAllTabs();
                            if (oldToken != TokenHolder.INVALID_TOKEN) {
                                mTabObscuringHandler.unobscureAllTabs(oldToken);
                            }
                        } else {
                            mTabObscuringHandler.unobscureAllTabs(mTabObscuringToken);
                            mTabObscuringToken = TokenHolder.INVALID_TOKEN;
                        }
                    };

                    Runnable clickDelegate =
                            () -> setUrlBarFocus(false, LocationBar.OmniboxFocusReason.UNFOCUS);

                    mScrimModel = new PropertyModel.Builder(ScrimProperties.ALL_KEYS)
                                          .with(ScrimProperties.ANCHOR_VIEW, scrimTarget)
                                          .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, true)
                                          .with(ScrimProperties.AFFECTS_STATUS_BAR, false)
                                          .with(ScrimProperties.TOP_MARGIN, topMargin)
                                          .with(ScrimProperties.CLICK_DELEGATE, clickDelegate)
                                          .with(ScrimProperties.VISIBILITY_CALLBACK,
                                                  visibilityChangeCallback)
                                          .with(ScrimProperties.BACKGROUND_COLOR,
                                                  ScrimProperties.INVALID_COLOR)
                                          .build();
                }

                boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity);
                boolean useLightColor = !isTablet && !mLocationBarModel.isIncognito()
                        && !mActivity.getNightModeStateProvider().isInNightMode();
                mScrimModel.set(ScrimProperties.BACKGROUND_COLOR,
                        useLightColor ? mLightScrimColor : ScrimProperties.INVALID_COLOR);

                if (hasFocus && !showScrimAfterAnimationCompletes()) {
                    mScrimCoordinator.showScrim(mScrimModel);
                    mScrimShown = true;
                } else if (!hasFocus && mScrimShown) {
                    mScrimCoordinator.hideScrim(true);
                    mScrimShown = false;
                }
            }

            @Override
            public void onUrlAnimationFinished(boolean hasFocus) {
                if (hasFocus && showScrimAfterAnimationCompletes()) {
                    mScrimCoordinator.showScrim(mScrimModel);
                    mScrimShown = true;
                }
            }

            /**
             * @return Whether the scrim should wait to be shown until after the omnibox is done
             *         animating.
             */
            private boolean showScrimAfterAnimationCompletes() {
                if (mLocationBarModel.getNewTabPageForCurrentTab() == null) return false;
                return mLocationBarModel.getNewTabPageForCurrentTab().isLocationBarShownInNTP();
            }
        };

        mIncognitoStateProvider = new IncognitoStateProvider(mActivity);
        mTabCountProvider = new TabCountProvider();
        mTabThemeColorProvider = themeColorProvider;
        mTabThemeColorProvider.addThemeColorObserver(this);

        mAppThemeColorProvider = new AppThemeColorProvider(mActivity);

        mTabObscuringHandler = tabObscuringHandler;
        mActivityTabProvider = tabProvider;
        mToolbarTabController = new ToolbarTabControllerImpl(mLocationBarModel::getTab,
                // clang-format off
                this::isBottomToolbarVisible,
                () -> mShowStartSurfaceSupplier != null && mShowStartSurfaceSupplier.get(),
                mProfileSupplier, () -> mBottomControlsCoordinator, this::updateButtonStatus);
        // clang-format on

        mToolbar = new TopToolbarCoordinator(controlContainer, mActivity.findViewById(R.id.toolbar),
                identityDiscController, mLocationBarModel, mToolbarTabController,
                new UserEducationHelper(mActivity), buttonDataProviders);

        mActionModeController =
                new ActionModeController(mActivity, mActionBarDelegate, toolbarActionModeCallback);

        mToolbar.setPaintInvalidator(invalidator);
        mActionModeController.setTabStripHeight(mToolbar.getTabStripHeight());
        mLocationBar = mToolbar.getLocationBar();
        mLocationBar.setToolbarDataProvider(mLocationBarModel);
        mLocationBar.addUrlFocusChangeListener(this);
        mLocationBar.setDefaultTextEditActionModeCallback(
                mActionModeController.getActionModeCallback());
        mLocationBar.initializeControls(new WindowDelegate(mActivity.getWindow()),
                mActivity.getWindowAndroid(), mActivityTabProvider,
                mActivity::getModalDialogManager, mActivity.getShareDelegateSupplier(),
                mIncognitoStateProvider);
        mLocationBar.addUrlFocusChangeListener(mLocationBarFocusObserver);
        mProgressBarCoordinator =
                new LoadProgressCoordinator(mActivityTabProvider, mToolbar.getProgressBar());

        mToolbar.addUrlExpansionObserver(activity.getStatusBarColorController());

        mOmniboxStartupMetrics = new OmniboxStartupMetrics(activity);

        mActivityTabTabObserver = new ActivityTabProvider.ActivityTabTabObserver(
                mActivityTabProvider) {
            @Override
            public void onObservingDifferentTab(Tab tab) {
                // ActivityTabProvider will null out the tab passed to onObservingDifferentTab when
                // the tab is non-interactive (e.g. when entering the TabSwitcher), but in those
                // cases we actually still want to use the most recently selected tab.
                if (tab == null) return;

                refreshSelectedTab(tab);
            }

            @Override
            public void onSSLStateUpdated(Tab tab) {
                if (mLocationBarModel.getTab() == null) return;

                assert tab == mLocationBarModel.getTab();
                mLocationBar.updateStatusIcon();
                mLocationBar.setUrlToPageUrl();
            }

            @Override
            public void onTitleUpdated(Tab tab) {
                mLocationBar.setTitleToPageTitle();
            }

            @Override
            public void onUrlUpdated(Tab tab) {
                // Update the SSL security state as a result of this notification as it will
                // sometimes be the only update we receive.
                updateTabLoadingState(true);

                // A URL update is a decent enough indicator that the toolbar widget is in
                // a stable state to capture its bitmap for use in fullscreen.
                mControlContainer.setReadyForBitmapCapture(true);
            }

            @Override
            public void onShown(Tab tab, @TabSelectionType int type) {
                if (TextUtils.isEmpty(tab.getUrlString())) return;
                mControlContainer.setReadyForBitmapCapture(true);
            }

            @Override
            public void onCrash(Tab tab) {
                updateTabLoadingState(false);
                updateButtonStatus();
            }

            @Override
            public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
                if (!toDifferentDocument) return;
                updateButtonStatus();
                updateTabLoadingState(true);
            }

            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                if (!toDifferentDocument) return;
                updateTabLoadingState(true);
            }

            @Override
            public void onContentChanged(Tab tab) {
                if (tab.isNativePage()) TabThemeColorHelper.get(tab).updateIfNeeded(false);
                mToolbar.onTabContentViewChanged();
                if (shouldShowCursorInLocationBar()) {
                    mLocationBar.showUrlBarCursorWithoutFocusAnimations();
                }
            }

            @Override
            public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
                if (!didStartLoad) return;
                mLocationBar.updateLoadingState(true);
            }

            @Override
            public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                NewTabPage ntp = mLocationBarModel.getNewTabPageForCurrentTab();
                if (ntp == null) return;
                if (!NewTabPage.isNTPUrl(params.getUrl())
                        && loadType != TabLoadStatus.PAGE_LOAD_FAILED) {
                    ntp.setUrlFocusAnimationsDisabled(true);
                    mToolbar.onTabOrModelChanged();
                }
            }

            private boolean hasPendingNonNtpNavigation(Tab tab) {
                WebContents webContents = tab.getWebContents();
                if (webContents == null) return false;

                NavigationController navigationController = webContents.getNavigationController();
                if (navigationController == null) return false;

                NavigationEntry pendingEntry = navigationController.getPendingEntry();
                if (pendingEntry == null) return false;

                return !NewTabPage.isNTPUrl(pendingEntry.getUrl());
            }

            @Override
            public void onDidStartNavigation(Tab tab, NavigationHandle navigation) {
                if (!navigation.isInMainFrame()) return;
                // Update URL as soon as it becomes available when it's a new tab.
                // But we want to update only when it's a new tab. So we check whether the current
                // navigation entry is initial, meaning whether it has the same target URL as the
                // initial URL of the tab.
                if (tab.getWebContents() != null
                        && tab.getWebContents().getNavigationController() != null
                        && tab.getWebContents().getNavigationController().isInitialNavigation()) {
                    mLocationBar.setUrlToPageUrl();
                }
            }

            @Override
            public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
                if (navigation.hasCommitted() && navigation.isInMainFrame()
                        && !navigation.isSameDocument()) {
                    mToolbar.onNavigatedToDifferentPage();
                }

                if (navigation.hasCommitted() && Previews.isPreview(tab)) {
                    // Some previews are not fully decided until the page commits. If this
                    // is a preview, update the security icon which will also update the verbose
                    // status view to make sure the "Lite" badge is displayed.
                    mLocationBar.updateStatusIcon();
                    PreviewsUma.recordLitePageAtCommit(
                            PreviewsAndroidBridge.getInstance().getPreviewsType(
                                    tab.getWebContents()),
                            navigation.isInMainFrame());
                }

                // If the load failed due to a different navigation, there is no need to reset the
                // location bar animations.
                if (navigation.errorCode() != 0 && navigation.isInMainFrame()
                        && !hasPendingNonNtpNavigation(tab)) {
                    NewTabPage ntp = mLocationBarModel.getNewTabPageForCurrentTab();
                    if (ntp == null) return;

                    ntp.setUrlFocusAnimationsDisabled(false);
                    mToolbar.onTabOrModelChanged();
                }
            }

            @Override
            public void onNavigationEntriesDeleted(Tab tab) {
                if (tab == mLocationBarModel.getTab()) {
                    updateButtonStatus();
                }
            }
        };

        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabStateInitialized() {
                mTabRestoreCompleted = true;
                handleTabRestoreCompleted();
            }

            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                if (mTabModelSelector != null) {
                    refreshSelectedTab(mTabModelSelector.getCurrentTab());
                }
            }
        };

        mBookmarksObserver = new BookmarkBridge.BookmarkModelObserver() {
            @Override
            public void bookmarkModelChanged() {
                updateBookmarkButtonStatus();
            }
        };

        mFullscreenListener = new FullscreenListener() {
            @Override
            public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
                if (mFindToolbarManager != null) mFindToolbarManager.hideToolbar();
            }

            @Override
            public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
                    int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
                // If the browser controls can't be animated, we shouldn't listen for the offset
                // changes.
                if (mCanAnimateNativeBrowserControls == null
                        || !mCanAnimateNativeBrowserControls.get()) {
                    return;
                }

                // Controls need to be offset to match the composited layer, which is
                // anchored at the bottom of the controls container.
                setControlContainerTopMargin(getToolbarExtraYOffset());
            }

            @Override
            public void onTopControlsHeightChanged(
                    int topControlsHeight, int topControlsMinHeight) {
                // If the browser controls can be animated, we shouldn't set the extra offset here.
                // Instead, that should happen when the animation starts (i.e. we get new offsets)
                // to prevent the Android view from jumping before the animation starts.
                if (mCanAnimateNativeBrowserControls == null
                        || mCanAnimateNativeBrowserControls.get()) {
                    return;
                }

                // Controls need to be offset to match the composited layer, which is
                // anchored at the bottom of the controls container.
                setControlContainerTopMargin(getToolbarExtraYOffset());
            }
        };
        mFullscreenManager.addListener(mFullscreenListener);

        mFindToolbarObserver = new FindToolbarObserver() {
            @Override
            public void onFindToolbarShown() {
                mToolbar.handleFindLocationBarStateChange(true);
                if (mControlsVisibilityDelegate != null) {
                    mFullscreenFindInPageToken =
                            mControlsVisibilityDelegate.showControlsPersistentAndClearOldToken(
                                    mFullscreenFindInPageToken);
                }
            }

            @Override
            public void onFindToolbarHidden() {
                mToolbar.handleFindLocationBarStateChange(false);
                if (mControlsVisibilityDelegate != null) {
                    mControlsVisibilityDelegate.releasePersistentShowingToken(
                            mFullscreenFindInPageToken);
                }
            }
        };

        mOverviewModeObserver = new EmptyOverviewModeObserver() {
            @Override
            public void onOverviewModeStartedShowing(boolean showToolbar) {
                mToolbar.setTabSwitcherMode(true, showToolbar, false);
                updateButtonStatus();
                if (BottomToolbarConfiguration.isBottomToolbarEnabled()
                        && !BottomToolbarVariationManager
                                    .shouldBottomToolbarBeVisibleInOverviewMode()) {
                    // TODO(crbug.com/1036474): don't tell mToolbar the visibility of bottom toolbar
                    // has been changed when entering overview, so it won't draw extra animations
                    // or buttons during the transition. Before, bottom toolbar was always visible
                    // in portrait mode, so the visibility was equivalent to the orientation change.
                    // We may have to tell mToolbar extra information, like orientation, in future.
                    // mToolbar.onBottomToolbarVisibilityChanged(false);
                    mBottomControlsCoordinator.setBottomControlsVisible(false);
                }
            }

            @Override
            public void onOverviewModeStateChanged(
                    @OverviewModeState int overviewModeState, boolean showTabSwitcherToolbar) {
                assert StartSurfaceConfiguration.isStartSurfaceEnabled();
                mToolbar.updateTabSwitcherToolbarState(showTabSwitcherToolbar);
            }

            @Override
            public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {
                mToolbar.setTabSwitcherMode(false, showToolbar, delayAnimation);
                updateButtonStatus();
                if (BottomToolbarConfiguration.isBottomToolbarEnabled()
                        && !BottomToolbarVariationManager
                                    .shouldBottomToolbarBeVisibleInOverviewMode()) {
                    // User may enter overview mode in landscape mode but exit in portrait mode.

                    boolean isBottomToolbarVisible =
                            !BottomToolbarConfiguration.isAdaptiveToolbarEnabled()
                            || mActivity.getResources().getConfiguration().orientation
                                    != Configuration.ORIENTATION_LANDSCAPE;
                    setBottomToolbarVisible(isBottomToolbarVisible);
                }
            }

            @Override
            public void onOverviewModeFinishedHiding() {
                mToolbar.onTabSwitcherTransitionFinished();
                updateButtonStatus();
            }
        };

        mSceneChangeObserver = new SceneChangeObserver() {
            @Override
            public void onTabSelectionHinted(int tabId) {
                Tab tab = mTabModelSelector != null ? mTabModelSelector.getTabById(tabId) : null;
                refreshSelectedTab(tab);

                if (mToolbar.setForceTextureCapture(true)) {
                    mControlContainer.invalidateBitmap();
                }
            }

            @Override
            public void onSceneChange(Layout layout) {
                mToolbar.setContentAttached(layout.shouldDisplayContentOverlay());
            }
        };

        mToolbar.setTabCountProvider(mTabCountProvider);
        mToolbar.setIncognitoStateProvider(mIncognitoStateProvider);
        mToolbar.setThemeColorProvider(
                mActivity.isTablet() ? mAppThemeColorProvider : mTabThemeColorProvider);

        AccessibilityUtil.addObserver(this);

        mFindToolbarManager = findToolbarManager;
        mFindToolbarManager.addObserver(mFindToolbarObserver);
    }

    /**
     * Called when the app menu and related properties delegate are available.
     * @param appMenuCoordinator The coordinator for interacting with the menu.
     */
    public void onAppMenuInitialized(AppMenuCoordinator appMenuCoordinator) {
        AppMenuHandler appMenuHandler = appMenuCoordinator.getAppMenuHandler();
        MenuDelegatePhone menuDelegate = new MenuDelegatePhone() {
            @Override
            public void updateReloadButtonState(boolean isLoading) {
                if (mAppMenuPropertiesDelegate != null) {
                    mAppMenuPropertiesDelegate.loadingStateChanged(isLoading);
                    appMenuHandler.menuItemContentChanged(R.id.icon_row_menu_id);
                }
            }
        };
        setMenuDelegatePhone(menuDelegate);
        setAppMenuHandler(appMenuHandler);
        mAppMenuPropertiesDelegate = appMenuCoordinator.getAppMenuPropertiesDelegate();
    }

    /**
     * Called when the contextual action bar's visibility has changed (i.e. the widget shown
     * when you can copy/paste text after long press).
     * @param visible Whether the contextual action bar is now visible.
     */
    public void onActionBarVisibilityChanged(boolean visible) {
        ActionBar actionBar = mActionBarDelegate.getSupportActionBar();
        if (!visible && actionBar != null) actionBar.hide();
        if (mActivity.isTablet()) {
            if (visible) {
                mActionModeController.startShowAnimation();
            } else {
                mActionModeController.startHideAnimation();
            }
        }
    }

    /**
     * @return  Whether the UrlBar currently has focus.
     */
    public boolean isUrlBarFocused() {
        return mLocationBar.isUrlBarFocused();
    }

    /**
     * Enable the bottom toolbar.
     */
    public void enableBottomToolbar() {
        if (TabUiFeatureUtilities.isDuetTabStripIntegrationAndroidEnabled()
                && BottomToolbarConfiguration.isBottomToolbarEnabled()) {
            mTabGroupPopUiParentSupplier = new ObservableSupplierImpl<>();
            mTabGroupPopupUi = TabManagementModuleProvider.getDelegate().createTabGroupPopUi(
                    mAppThemeColorProvider, mTabGroupPopUiParentSupplier);
        }

        mBottomControlsCoordinator = new BottomControlsCoordinator(mFullscreenManager,
                mActivity.findViewById(R.id.bottom_controls_stub), mActivityTabProvider,
                mTabGroupPopupUi != null
                        ? mTabGroupPopupUi.getLongClickListenerForTriggering()
                        : BottomTabSwitcherActionMenuCoordinator.createOnLongClickListener(
                                id -> mActivity.onOptionsItemSelected(id, null)),
                mAppThemeColorProvider, mShareDelegateSupplier, mShowStartSurfaceSupplier,
                mToolbarTabController::openHomepage, (reason) -> setUrlBarFocus(true, reason));

        boolean isBottomToolbarVisible = BottomToolbarConfiguration.isBottomToolbarEnabled()
                && (!BottomToolbarConfiguration.isAdaptiveToolbarEnabled()
                        || mActivity.getResources().getConfiguration().orientation
                                != Configuration.ORIENTATION_LANDSCAPE);
        setBottomToolbarVisible(isBottomToolbarVisible);
    }

    /**
     * @return Whether the bottom toolbar is visible.
     */
    public boolean isBottomToolbarVisible() {
        return mIsBottomToolbarVisible;
    }

    /**
     * @return The coordinator for the bottom toolbar if it exists.
     */
    public BottomControlsCoordinator getBottomToolbarCoordinator() {
        return mBottomControlsCoordinator;
    }

    /**
     * Initialize the manager with the components that had native initialization dependencies.
     * <p>
     * Calling this must occur after the native library have completely loaded.
     *
     * @param tabModelSelector           The selector that handles tab management.
     * @param controlsVisibilityDelegate The delegate to handle visibility of browser controls.
     * @param overviewModeBehavior       The overview mode manager.
     * @param layoutManager              A {@link LayoutManager} instance used to watch for scene
     *                                   changes.
     */
    public void initializeWithNative(TabModelSelector tabModelSelector,
            BrowserStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate,
            OverviewModeBehavior overviewModeBehavior, LayoutManager layoutManager,
            OnClickListener tabSwitcherClickHandler, OnClickListener newTabClickHandler,
            OnClickListener bookmarkClickHandler, OnClickListener customTabsBackClickHandler,
            Supplier<Boolean> showStartSurfaceSupplier) {
        assert !mInitializedWithNative;

        mTabModelSelector = tabModelSelector;

        mShowStartSurfaceSupplier = showStartSurfaceSupplier;

        OnLongClickListener tabSwitcherLongClickHandler = null;

        if (mTabGroupPopupUi != null) {
            tabSwitcherLongClickHandler = mTabGroupPopupUi.getLongClickListenerForTriggering();
        } else {
            tabSwitcherLongClickHandler =
                    TabSwitcherActionMenuCoordinator.createOnLongClickListener(
                            (id) -> mActivity.onOptionsItemSelected(id, null));
        }

        mToolbar.initializeWithNative(tabModelSelector, layoutManager, tabSwitcherClickHandler,
                tabSwitcherLongClickHandler, newTabClickHandler, bookmarkClickHandler,
                customTabsBackClickHandler, overviewModeBehavior);

        mToolbar.addOnAttachStateChangeListener(new OnAttachStateChangeListener() {
            @Override
            public void onViewDetachedFromWindow(View v) {}

            @Override
            public void onViewAttachedToWindow(View v) {
                // As we have only just registered for notifications, any that were sent prior
                // to this may have been missed. Calling refreshSelectedTab in case we missed
                // the initial selection notification.
                refreshSelectedTab(mActivityTabProvider.get());
            }
        });

        if (mTabGroupPopupUi != null) {
            mTabGroupPopUiParentSupplier.set(
                    mIsBottomToolbarVisible && BottomToolbarVariationManager.isTabSwitcherOnBottom()
                            ? mActivity.findViewById(R.id.bottom_controls)
                            : mActivity.findViewById(R.id.toolbar));
            mTabGroupPopupUi.initializeWithNative(mActivity);
        }

        mLocationBarModel.initializeWithNative();
        mLocationBarModel.setShouldShowOmniboxInOverviewMode(
                StartSurfaceConfiguration.isStartSurfaceEnabled());

        assert controlsVisibilityDelegate != null;
        mControlsVisibilityDelegate = controlsVisibilityDelegate;

        mNativeLibraryReady = false;

        if (overviewModeBehavior != null) {
            mOverviewModeBehavior = overviewModeBehavior;
            mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);
            mAppThemeColorProvider.setOverviewModeBehavior(mOverviewModeBehavior);
            mLocationBarModel.setOverviewModeBehavior(mOverviewModeBehavior);
        }

        if (layoutManager != null) {
            mLayoutManager = layoutManager;
            mLayoutManager.addSceneChangeObserver(mSceneChangeObserver);
        }

        if (mBottomControlsCoordinator != null) {
            Runnable closeAllTabsAction = () -> {
                mTabModelSelector.getModel(mIncognitoStateProvider.isIncognitoSelected())
                        .closeAllTabs();
            };
            mBottomControlsCoordinator.initializeWithNative(mActivity,
                    mActivity.getCompositorViewHolder().getResourceManager(),
                    mActivity.getCompositorViewHolder().getLayoutManager(), tabSwitcherClickHandler,
                    newTabClickHandler, mAppMenuButtonHelper, mOverviewModeBehavior,
                    mActivity.getWindowAndroid(), mTabCountProvider, mIncognitoStateProvider,
                    mActivity.findViewById(R.id.control_container), closeAllTabsAction);

            // Allow the bottom toolbar to be focused in accessibility after the top toolbar.
            ApiCompatibilityUtils.setAccessibilityTraversalBefore(
                    mLocationBar.getContainerView(), R.id.bottom_toolbar);
        }

        onNativeLibraryReady();
        mInitializedWithNative = true;
        if (mOnInitializedRunnable != null) {
            mOnInitializedRunnable.run();
            mOnInitializedRunnable = null;
        }

        setCurrentProfile(mProfileSupplier.get());
    }

    /**
     * Show the update badge in both the top and bottom toolbar.
     * TODO(amaralp): Only the top or bottom menu should be visible.
     */
    public void showAppMenuUpdateBadge() {
        mToolbar.showAppMenuUpdateBadge();
    }

    /**
     * Remove the update badge in both the top and bottom toolbar.
     * TODO(amaralp): Only the top or bottom menu should be visible.
     */
    public void removeAppMenuUpdateBadge(boolean animate) {
        mToolbar.removeAppMenuUpdateBadge(animate);
    }

    /**
     * @return Whether the badge is showing (either in the top or bottom toolbar).
     * TODO(amaralp): Only the top or bottom menu should be visible.
     */
    public boolean isShowingAppMenuUpdateBadge() {
        return mToolbar.isShowingAppMenuUpdateBadge();
    }

    /**
     * @return The toolbar interface that this manager handles.
     */
    public Toolbar getToolbar() {
        return mToolbar;
    }

    /**
     * @return Whether the UI has been initialized.
     */
    public boolean isInitialized() {
        return mInitializedWithNative;
    }

    @Override
    public @Nullable View getMenuButtonView() {
        return mToolbar.getMenuButton();
    }

    @Override
    public boolean isMenuFromBottom() {
        return mIsBottomToolbarVisible && BottomToolbarVariationManager.isMenuButtonOnBottom();
    }

    /**
     * TODO(twellington): Try to remove this method. It's only used to return an in-product help
     *                    bubble anchor view... which should be moved out of tab and perhaps into
     *                    the status bar icon component.
     * @return The view containing the security icon.
     */
    public View getSecurityIconView() {
        return mLocationBar.getSecurityIconView();
    }

    /**
     * Adds a custom action button to the {@link Toolbar}, if it is supported.
     * @param drawable The {@link Drawable} to use as the background for the button.
     * @param description The content description for the custom action button.
     * @param listener The {@link OnClickListener} to use for clicks to the button.
     * @see #updateCustomActionButton
     */
    public void addCustomActionButton(
            Drawable drawable, String description, OnClickListener listener) {
        mToolbar.addCustomActionButton(drawable, description, listener);
    }

    /**
     * Updates the visual appearance of a custom action button in the {@link Toolbar},
     * if it is supported.
     * @param index The index of the button to update.
     * @param drawable The {@link Drawable} to use as the background for the button.
     * @param description The content description for the custom action button.
     * @see #addCustomActionButton
     */
    public void updateCustomActionButton(int index, Drawable drawable, String description) {
        mToolbar.updateCustomActionButton(index, drawable, description);
    }

    /**
     * Call to tear down all of the toolbar dependencies.
     */
    public void destroy() {
        if (mInitializedWithNative) {
            mFindToolbarManager.removeObserver(mFindToolbarObserver);
        }
        if (mTabModelSelector != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        }
        if (mBookmarkBridgeSupplier != null) {
            BookmarkBridge bridge = mBookmarkBridgeSupplier.get();
            if (bridge != null) bridge.removeObserver(mBookmarksObserver);

            mBookmarkBridgeSupplier.removeObserver(mBookmarkBridgeSupplierObserver);
            mBookmarkBridgeSupplier = null;
        }
        if (mTemplateUrlObserver != null) {
            TemplateUrlServiceFactory.get().removeObserver(mTemplateUrlObserver);
            mTemplateUrlObserver = null;
        }
        if (mOverviewModeBehavior != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
            mOverviewModeBehavior = null;
        }
        if (mLayoutManager != null) {
            mLayoutManager.removeSceneChangeObserver(mSceneChangeObserver);
            mLayoutManager = null;
        }

        if (mBottomControlsCoordinator != null) {
            mBottomControlsCoordinator.destroy();
            mBottomControlsCoordinator = null;
        }

        if (mOmniboxStartupMetrics != null) {
            // Record the histogram before destroying, if we have the data.
            if (mInitializedWithNative) mOmniboxStartupMetrics.maybeRecordHistograms();
            mOmniboxStartupMetrics.destroy();
            mOmniboxStartupMetrics = null;
        }

        if (mLocationBar != null) {
            mLocationBar.removeUrlFocusChangeListener(this);
        }

        if (mTabGroupPopupUi != null) {
            mTabGroupPopupUi.destroy();
            mTabGroupPopupUi = null;
        }

        mToolbar.removeUrlExpansionObserver(mActivity.getStatusBarColorController());
        mToolbar.destroy();

        if (mTabObserver != null) {
            Tab currentTab = mLocationBarModel.getTab();
            if (currentTab != null) currentTab.removeObserver(mTabObserver);
            mTabObserver = null;
        }

        mIncognitoStateProvider.destroy();
        mTabCountProvider.destroy();

        mLocationBarModel.destroy();
        mHandler.removeCallbacksAndMessages(null); // Cancel delayed tasks.
        mFullscreenManager.removeListener(mFullscreenListener);
        if (mLocationBar != null) {
            mLocationBar.removeUrlFocusChangeListener(mLocationBarFocusObserver);
            mLocationBarFocusObserver = null;
        }

        if (mTabThemeColorProvider != null) mTabThemeColorProvider.removeThemeColorObserver(this);
        if (mAppThemeColorProvider != null) mAppThemeColorProvider.destroy();

        if (mActivityTabTabObserver != null) {
            mActivityTabTabObserver.destroy();
            mActivityTabTabObserver = null;
        }

        if (mFindToolbarManager != null) {
            mFindToolbarManager.removeObserver(mFindToolbarObserver);
            mFindToolbarManager = null;
        }

        if (mProfileSupplier != null) {
            mProfileSupplier.removeObserver(mProfileSupplierObserver);
            mProfileSupplier = null;
        }

        mActivity.unregisterComponentCallbacks(mComponentCallbacks);
        mComponentCallbacks = null;
        AccessibilityUtil.removeObserver(this);
    }

    /**
     * Called when the orientation of the activity has changed.
     */
    private void onOrientationChange(int newOrientation) {
        if (mActionModeController != null) mActionModeController.showControlsOnOrientationChange();

        if (mBottomControlsCoordinator != null
                && BottomToolbarConfiguration.isBottomToolbarEnabled()
                && BottomToolbarConfiguration.isAdaptiveToolbarEnabled()) {
            boolean isBottomToolbarVisible = newOrientation != Configuration.ORIENTATION_LANDSCAPE;
            if (!BottomToolbarVariationManager.shouldBottomToolbarBeVisibleInOverviewMode()
                    && isBottomToolbarVisible) {
                isBottomToolbarVisible = !mActivity.isInOverviewMode();
            }
            setBottomToolbarVisible(isBottomToolbarVisible);
            if (mAppMenuButtonHelper != null) {
                mAppMenuButtonHelper.setMenuShowsFromBottom(isMenuFromBottom());
            }

            if (mTabGroupPopupUi != null) {
                mTabGroupPopUiParentSupplier.set(mIsBottomToolbarVisible
                                        && BottomToolbarVariationManager.isTabSwitcherOnBottom()
                                ? mActivity.findViewById(R.id.bottom_controls)
                                : mActivity.findViewById(R.id.toolbar));
            }
        }
    }

    @Override
    public void onAccessibilityModeChanged(boolean enabled) {
        mToolbar.onAccessibilityStatusChanged(enabled);
    }

    private void registerTemplateUrlObserver() {
        final TemplateUrlService templateUrlService = TemplateUrlServiceFactory.get();
        assert mTemplateUrlObserver == null;
        mTemplateUrlObserver = new TemplateUrlServiceObserver() {
            private TemplateUrl mSearchEngine =
                    templateUrlService.getDefaultSearchEngineTemplateUrl();

            @Override
            public void onTemplateURLServiceChanged() {
                TemplateUrl searchEngine = templateUrlService.getDefaultSearchEngineTemplateUrl();
                if ((mSearchEngine == null && searchEngine == null)
                        || (mSearchEngine != null && mSearchEngine.equals(searchEngine))) {
                    return;
                }

                mSearchEngine = searchEngine;
                mLocationBar.updateSearchEngineStatusIcon(
                        SearchEngineLogoUtils.shouldShowSearchEngineLogo(
                                mLocationBarModel.isIncognito()),
                        TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle(),
                        SearchEngineLogoUtils.getSearchLogoUrl());
                mToolbar.onDefaultSearchEngineChanged();
            }
        };
        templateUrlService.addObserver(mTemplateUrlObserver);

        // Force an update once to populate initial data.
        mLocationBar.updateSearchEngineStatusIcon(
                SearchEngineLogoUtils.shouldShowSearchEngineLogo(mLocationBarModel.isIncognito()),
                TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle(),
                SearchEngineLogoUtils.getSearchLogoUrl());
    }

    private void onNativeLibraryReady() {
        mNativeLibraryReady = true;

        final TemplateUrlService templateUrlService = TemplateUrlServiceFactory.get();
        TemplateUrlService.LoadListener mTemplateServiceLoadListener =
                new TemplateUrlService.LoadListener() {
                    @Override
                    public void onTemplateUrlServiceLoaded() {
                        registerTemplateUrlObserver();
                        templateUrlService.unregisterLoadListener(this);
                    }
                };
        templateUrlService.registerLoadListener(mTemplateServiceLoadListener);
        if (templateUrlService.isLoaded()) {
            mTemplateServiceLoadListener.onTemplateUrlServiceLoaded();
        } else {
            templateUrlService.load();
        }

        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        refreshSelectedTab(mActivityTabProvider.get());
        if (mTabModelSelector.isTabStateInitialized()) mTabRestoreCompleted = true;
        handleTabRestoreCompleted();
        mTabCountProvider.setTabModelSelector(mTabModelSelector);
        mIncognitoStateProvider.setTabModelSelector(mTabModelSelector);
        mAppThemeColorProvider.setIncognitoStateProvider(mIncognitoStateProvider);
    }

    private void handleTabRestoreCompleted() {
        if (!mTabRestoreCompleted || !mNativeLibraryReady) return;
        mToolbar.onStateRestored();
    }

    /**
     * Sets the handler for any special case handling related with the menu button.
     * @param appMenuHandler The handler to be used.
     */
    private void setAppMenuHandler(AppMenuHandler appMenuHandler) {
        mAppMenuHandler = appMenuHandler;
        mAppMenuHandler.addObserver(new AppMenuObserver() {
            @Override
            public void onMenuVisibilityChanged(boolean isVisible) {
                if (isVisible) {
                    // Defocus here to avoid handling focus in multiple places, e.g., when the
                    // forward button is pressed. (see crbug.com/414219)
                    setUrlBarFocus(false, LocationBar.OmniboxFocusReason.UNFOCUS);

                    if (!mActivity.isInOverviewMode() && isShowingAppMenuUpdateBadge()) {
                        // The app menu badge should be removed the first time the menu is opened.
                        removeAppMenuUpdateBadge(true);
                        mActivity.getCompositorViewHolder().requestRender();
                    }
                }

                if (mControlsVisibilityDelegate != null) {
                    if (isVisible) {
                        mFullscreenMenuToken =
                                mControlsVisibilityDelegate.showControlsPersistentAndClearOldToken(
                                        mFullscreenMenuToken);
                    } else {
                        mControlsVisibilityDelegate.releasePersistentShowingToken(
                                mFullscreenMenuToken);
                    }
                }

                MenuButton menuButton = getMenuButtonWrapper();
                if (isVisible && menuButton != null && menuButton.isShowingAppMenuUpdateBadge()) {
                    UpdateMenuItemHelper.getInstance().onMenuButtonClicked();
                }
            }

            @Override
            public void onMenuHighlightChanged(boolean highlighting) {
                final MenuButton menuButton = getMenuButtonWrapper();
                if (menuButton != null) menuButton.setMenuButtonHighlight(highlighting);

                if (mControlsVisibilityDelegate == null) return;
                if (highlighting) {
                    mFullscreenHighlightToken =
                            mControlsVisibilityDelegate.showControlsPersistentAndClearOldToken(
                                    mFullscreenHighlightToken);
                } else {
                    mControlsVisibilityDelegate.releasePersistentShowingToken(
                            mFullscreenHighlightToken);
                }
            }
        });
        mAppMenuButtonHelper = mAppMenuHandler.createAppMenuButtonHelper();
        mAppMenuButtonHelper.setMenuShowsFromBottom(isMenuFromBottom());
        mAppMenuButtonHelper.setOnAppMenuShownListener(() -> {
            RecordUserAction.record("MobileToolbarShowMenu");
            if (isMenuFromBottom()) {
                RecordUserAction.record("MobileBottomToolbarShowMenu");
            } else {
                RecordUserAction.record("MobileTopToolbarShowMenu");
            }
            mToolbar.onMenuShown();

            // Assume data saver footer is shown only if data reduction proxy is enabled and
            // Chrome home is not
            if (DataReductionProxySettings.getInstance().isDataReductionProxyEnabled()) {
                boolean isIncognito = mTabModelSelector.getCurrentModel().isIncognito();
                Profile profile = isIncognito
                        ? Profile.getLastUsedRegularProfile().getOffTheRecordProfile()
                        : Profile.getLastUsedRegularProfile();
                Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
                tracker.notifyEvent(EventConstants.OVERFLOW_OPENED_WITH_DATA_SAVER_SHOWN);
            }
        });
        mToolbar.setAppMenuButtonHelper(mAppMenuButtonHelper);
    }

    @Nullable
    private MenuButton getMenuButtonWrapper() {
        return mToolbar.getMenuButtonWrapper();
    }

    /**
     * Set the delegate that will handle updates from toolbar driven state changes.
     * @param menuDelegatePhone The menu delegate to be updated (only applicable to phones).
     */
    private void setMenuDelegatePhone(MenuDelegatePhone menuDelegatePhone) {
        mMenuDelegatePhone = menuDelegatePhone;
    }

    // TODO(https://crbug.com/865801): remove the below two methods if possible.
    public boolean back() {
        return mToolbarTabController.back();
    }

    public boolean forward() {
        return mToolbarTabController.forward();
    }

    /**
     * Triggered when the URL input field has gained or lost focus.
     * @param hasFocus Whether the URL field has gained focus.
     */
    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        mToolbar.onUrlFocusChange(hasFocus);

        if (hasFocus) mOmniboxStartupMetrics.onUrlBarFocused();

        if (mFindToolbarManager != null && hasFocus) mFindToolbarManager.hideToolbar();

        if (mControlsVisibilityDelegate == null) return;
        if (hasFocus) {
            mFullscreenFocusToken =
                    mControlsVisibilityDelegate.showControlsPersistentAndClearOldToken(
                            mFullscreenFocusToken);
        } else {
            mControlsVisibilityDelegate.releasePersistentShowingToken(mFullscreenFocusToken);
        }

        mUrlFocusChangedCallback.onResult(hasFocus);
    }

    /**
     * Updates the primary color used by the model to the given color.
     * @param color The primary color for the current tab.
     * @param shouldAnimate Whether the change of color should be animated.
     */
    @Override
    public void onThemeColorChanged(int color, boolean shouldAnimate) {
        if (!mShouldUpdateToolbarPrimaryColor) return;

        boolean colorChanged = mCurrentThemeColor != color;
        if (!colorChanged) return;

        mCurrentThemeColor = color;
        mLocationBarModel.setPrimaryColor(color);
        mToolbar.onPrimaryColorChanged(shouldAnimate);
    }

    /**
     * @param shouldUpdate Whether we should be updating the toolbar primary color based on updates
     *                     from the Tab.
     */
    public void setShouldUpdateToolbarPrimaryColor(boolean shouldUpdate) {
        mShouldUpdateToolbarPrimaryColor = shouldUpdate;
    }

    /**
     * @return The primary toolbar color.
     */
    public int getPrimaryColor() {
        return mLocationBarModel.getPrimaryColor();
    }

    /**
     * Sets the visibility of the Toolbar shadow.
     */
    public void setToolbarShadowVisibility(int visibility) {
        View toolbarShadow = mControlContainer.findViewById(R.id.toolbar_shadow);
        if (toolbarShadow != null) toolbarShadow.setVisibility(visibility);
    }

    /**
     * Gets the Toolbar view.
     */
    @Nullable
    public View getToolbarView() {
        return mControlContainer.findViewById(R.id.toolbar);
    }

    /**
     * We use getTopControlOffset to position the top controls. However, the toolbar's height may
     * be less than the total top controls height. If that's the case, this method will return the
     * extra offset needed to align the toolbar at the bottom of the top controls.
     * @return The extra Y offset for the toolbar in pixels.
     */
    private int getToolbarExtraYOffset() {
        final int stripAndToolbarHeight = mActivity.getResources().getDimensionPixelSize(
                R.dimen.tab_strip_and_toolbar_height);
        return mFullscreenManager.getTopControlsHeight() - stripAndToolbarHeight;
    }

    /**
     * Sets the drawable that the close button shows, or hides it if {@code drawable} is
     * {@code null}.
     */
    public void setCloseButtonDrawable(Drawable drawable) {
        mToolbar.setCloseButtonImageResource(drawable);
    }

    /**
     * Sets whether a title should be shown within the Toolbar.
     * @param showTitle Whether a title should be shown.
     */
    public void setShowTitle(boolean showTitle) {
        mToolbar.setShowTitle(showTitle);
    }

    /**
     * @see TopToolbarCoordinator#setUrlBarHidden(boolean)
     */
    public void setUrlBarHidden(boolean hidden) {
        mToolbar.setUrlBarHidden(hidden);
    }

    /**
     * @see TopToolbarCoordinator#getContentPublisher()
     */
    public String getContentPublisher() {
        return mToolbar.getContentPublisher();
    }

    /**
     * Focuses or unfocuses the URL bar.
     *
     * If you request focus and the UrlBar was already focused, this will select all of the text.
     *
     * @param focused Whether URL bar should be focused.
     * @param reason The given reason.
     */
    public void setUrlBarFocus(boolean focused, @LocationBar.OmniboxFocusReason int reason) {
        if (!isInitialized()) return;
        boolean wasFocused = mLocationBar.isUrlBarFocused();
        mLocationBar.setUrlBarFocus(focused, null, reason);
        if (wasFocused && focused) {
            mLocationBar.selectAll();
        }
    }

    /**
     * See {@link #setUrlBarFocus}, but if native is not loaded it will queue the request instead
     * of dropping it.
     */
    public void setUrlBarFocusOnceNativeInitialized(
            boolean focused, @LocationBar.OmniboxFocusReason int reason) {
        if (isInitialized()) {
            setUrlBarFocus(focused, reason);
            return;
        }

        if (focused) {
            // Remember requests to focus the Url bar and replay them once native has been
            // initialized. This is important for the Launch to Incognito Tab flow (see
            // IncognitoTabLauncher.
            mOnInitializedRunnable = () -> {
                setUrlBarFocus(focused, reason);
            };
        } else {
            mOnInitializedRunnable = null;
        }
    }

    /**
     * Reverts any pending edits of the location bar and reset to the page state.  This does not
     * change the focus state of the location bar.
     */
    public void revertLocationBarChanges() {
        mLocationBar.revertChanges();
    }

    /**
     * Handle all necessary tasks that can be delayed until initialization completes.
     * @param activityCreationTimeMs The time of creation for the activity this toolbar belongs to.
     * @param activityName Simple class name for the activity this toolbar belongs to.
     */
    public void onDeferredStartup(final long activityCreationTimeMs, final String activityName) {
        recordStartupHistograms(activityCreationTimeMs, activityName);
        mLocationBar.onDeferredStartup();
    }

    /**
     * Set a supplier that will provide a boolean indicating whether the browser controls in native
     * can be animated. E.g. true if the current tab is user interactable.
     * @param canAnimateNativeBrowserControlsSupplier The boolean supplier.
     */
    public void setCanAnimateNativeBrowserControlsSupplier(
            Supplier<Boolean> canAnimateNativeBrowserControlsSupplier) {
        mCanAnimateNativeBrowserControls = canAnimateNativeBrowserControlsSupplier;
    }

    /**
     * Record histograms covering Chrome startup.
     * This method will collect metrics no sooner than RECORD_UMA_PERFORMANCE_METRICS_DELAY_MS since
     * Activity creation to ensure availability of collected data.
     *
     * Histograms will not be collected if Chrome is destroyed before the above timeout passed.
     */
    private void recordStartupHistograms(
            final long activityCreationTimeMs, final String activityName) {
        // Schedule call to self if minimum time since activity creation has not yet passed.
        long elapsedTime = SystemClock.elapsedRealtime() - activityCreationTimeMs;
        if (elapsedTime < RECORD_UMA_PERFORMANCE_METRICS_DELAY_MS) {
            // clang-format off
            mHandler.postDelayed(
                    () -> recordStartupHistograms(activityCreationTimeMs, activityName),
                    RECORD_UMA_PERFORMANCE_METRICS_DELAY_MS - elapsedTime);
            // clang-format on
            return;
        }

        RecordHistogram.recordTimesHistogram("MobileStartup.ToolbarFirstDrawTime2." + activityName,
                mToolbar.getFirstDrawTime() - activityCreationTimeMs);

        mOmniboxStartupMetrics.maybeRecordHistograms();
    }

    /**
     * Finish any toolbar animations.
     */
    public void finishAnimations() {
        if (isInitialized()) mToolbar.finishAnimations();
    }

    /**
     * Updates the current button states and calls appropriate abstract visibility methods, giving
     * inheriting classes the chance to update the button visuals as well.
     */
    private void updateButtonStatus() {
        Tab currentTab = mLocationBarModel.getTab();
        boolean tabCrashed = currentTab != null && SadTab.isShowing(currentTab);

        mToolbar.updateButtonVisibility();
        mToolbar.updateBackButtonVisibility(currentTab != null && currentTab.canGoBack());
        mToolbar.updateForwardButtonVisibility(currentTab != null && currentTab.canGoForward());
        updateReloadState(tabCrashed);
        updateBookmarkButtonStatus();
        if (mToolbar.getMenuButtonWrapper() != null && !isBottomToolbarVisible()) {
            mToolbar.getMenuButtonWrapper().setVisibility(View.VISIBLE);
        }
    }

    private void updateBookmarkButtonStatus() {
        Tab currentTab = mLocationBarModel.getTab();
        boolean isBookmarked = currentTab != null && BookmarkBridge.hasBookmarkIdForTab(currentTab);
        BookmarkBridge bridge = mBookmarkBridgeSupplier.get();
        boolean editingAllowed =
                currentTab == null || bridge == null || bridge.isEditBookmarksEnabled();
        mToolbar.updateBookmarkButton(isBookmarked, editingAllowed);
    }

    private void updateReloadState(boolean tabCrashed) {
        Tab currentTab = mLocationBarModel.getTab();
        boolean isLoading = false;
        if (!tabCrashed) {
            isLoading = (currentTab != null && currentTab.isLoading()) || !mNativeLibraryReady;
        }
        mToolbar.updateReloadButtonVisibility(isLoading);
        if (mMenuDelegatePhone != null) mMenuDelegatePhone.updateReloadButtonState(isLoading);
    }

    /**
     * Triggered when the selected tab has changed.
     */
    private void refreshSelectedTab(Tab tab) {
        boolean wasIncognito = mLocationBarModel.isIncognito();
        Tab previousTab = mLocationBarModel.getTab();

        boolean isIncognito =
                tab != null ? tab.isIncognito() : mTabModelSelector.isIncognitoSelected();
        mLocationBarModel.setTab(tab, isIncognito);

        updateCurrentTabDisplayStatus();

        // This method is called prior to action mode destroy callback for incognito <-> normal
        // tab switch. Makes sure the action mode toolbar is hidden before selecting the new tab.
        if (previousTab != null && wasIncognito != isIncognito && mActivity.isTablet()) {
            mActionModeController.startHideAnimation();
        }
        if (previousTab != tab || wasIncognito != isIncognito) {
            int defaultPrimaryColor =
                    ChromeColors.getDefaultThemeColor(mActivity.getResources(), isIncognito);
            int primaryColor =
                    tab != null ? TabThemeColorHelper.getColor(tab) : defaultPrimaryColor;
            onThemeColorChanged(primaryColor, false);

            mToolbar.onTabOrModelChanged();

            if (tab != null) {
                mToolbar.onNavigatedToDifferentPage();
            }

            // Ensure the URL bar loses focus if the tab it was interacting with is changed from
            // underneath it.
            setUrlBarFocus(false, LocationBar.OmniboxFocusReason.UNFOCUS);

            // Place the cursor in the Omnibox if applicable.  We always clear the focus above to
            // ensure the shield placed over the content is dismissed when switching tabs.  But if
            // needed, we will refocus the omnibox and make the cursor visible here.
            if (shouldShowCursorInLocationBar()) {
                mLocationBar.showUrlBarCursorWithoutFocusAnimations();
            }
        }

        updateButtonStatus();
    }

    // TODO(https://crbug.com/865801): Inject a profile supplier directly to mLocationBar and remove
    // setCurrentProfile.
    private void setCurrentProfile(Profile profile) {
        if (profile != null && mInitializedWithNative) {
            mLocationBar.setAutocompleteProfile(profile);
            mLocationBar.setShowIconsWhenUrlFocused(
                    SearchEngineLogoUtils.shouldShowSearchEngineLogo(
                            mLocationBarModel.isIncognito()));
        }
    }

    private void setBookmarkBridge(BookmarkBridge bookmarkBridge) {
        if (bookmarkBridge == null) return;
        bookmarkBridge.addObserver(mBookmarksObserver);
    }

    private void updateCurrentTabDisplayStatus() {
        mLocationBar.setUrlToPageUrl();
        updateTabLoadingState(true);
    }

    private void updateTabLoadingState(boolean updateUrl) {
        mLocationBar.updateLoadingState(updateUrl);
        if (updateUrl) updateButtonStatus();
    }

    private void setBottomToolbarVisible(boolean visible) {
        mIsBottomToolbarVisible = visible;
        mToolbar.onBottomToolbarVisibilityChanged(visible);
        mBottomToolbarVisibilitySupplier.set(visible);
        mBottomControlsCoordinator.setBottomControlsVisible(visible);
    }

    /**
     * @param enabled Whether the progress bar is enabled.
     */
    public void setProgressBarEnabled(boolean enabled) {
        mToolbar.setProgressBarEnabled(enabled);
    }

    /**
     * @return The {@link FakeboxDelegate}.
     */
    public FakeboxDelegate getFakeboxDelegate() {
        // TODO(crbug.com/1000295): Split fakebox component out of ntp package.
        return mLocationBar;
    }

    private boolean shouldShowCursorInLocationBar() {
        Tab tab = mLocationBarModel.getTab();
        if (tab == null) return false;
        NativePage nativePage = tab.getNativePage();
        if (!(nativePage instanceof NewTabPage) && !(nativePage instanceof IncognitoNewTabPage)) {
            return false;
        }

        return mActivity.isTablet()
                && mActivity.getResources().getConfiguration().keyboard
                == Configuration.KEYBOARD_QWERTY;
    }

    /**
     * Sets the top margin for the control container.
     * @param margin The margin in pixels.
     */
    private void setControlContainerTopMargin(int margin) {
        final ViewGroup.MarginLayoutParams layoutParams =
                ((ViewGroup.MarginLayoutParams) mControlContainer.getLayoutParams());
        if (layoutParams.topMargin == margin) {
            return;
        }

        layoutParams.topMargin = margin;
        mControlContainer.setLayoutParams(layoutParams);
    }

    /** Return the location bar model for testing purposes. */
    @VisibleForTesting
    public LocationBarModel getLocationBarModelForTesting() {
        return mLocationBarModel;
    }

    /**
     * @return The {@link ToolbarLayout} that constitutes the toolbar.
     */
    @VisibleForTesting
    public ToolbarLayout getToolbarLayoutForTesting() {
        return mToolbar.getToolbarLayoutForTesting();
    }

    /**
     * Get the home button on the top toolbar to verify the button status.
     * Note that this home button is not always the home button that on the UI, and the button is
     * not always visible.
     * @return The {@link HomeButton} that lives in the top toolbar.
     */
    @VisibleForTesting
    public HomeButton getHomeButtonForTesting() {
        return mToolbar.getToolbarLayoutForTesting().getHomeButtonForTesting();
    }
}
