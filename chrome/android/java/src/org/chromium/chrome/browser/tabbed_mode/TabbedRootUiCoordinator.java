// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.datareduction.DataReductionPromoScreen;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.gesturenav.HistoryNavigationCoordinator;
import org.chromium.chrome.browser.history.HistoryManagerUtils;
import org.chromium.chrome.browser.language.LanguageAskPrompt;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.offlinepages.indicator.OfflineIndicatorControllerV2;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.signin.SigninPromoUtil;
import org.chromium.chrome.browser.status_indicator.StatusIndicatorCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.toolbar.ToolbarButtonInProductHelpController;
import org.chromium.chrome.browser.toolbar.bottom.BottomToolbarConfiguration;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.tablet.emptybackground.EmptyBackgroundViewWrapper;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;

/**
 * A {@link RootUiCoordinator} variant that controls tabbed-mode specific UI.
 */
public class TabbedRootUiCoordinator extends RootUiCoordinator implements NativeInitObserver {
    private static boolean sEnableStatusIndicatorForTests;

    private final ObservableSupplierImpl<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;
    private TabbedSystemUiCoordinator mSystemUiCoordinator;
    private @Nullable EmptyBackgroundViewWrapper mEmptyBackgroundViewWrapper;

    private StatusIndicatorCoordinator mStatusIndicatorCoordinator;
    private StatusIndicatorCoordinator.StatusIndicatorObserver mStatusIndicatorObserver;
    private OfflineIndicatorControllerV2 mOfflineIndicatorController;
    private UrlFocusChangeListener mUrlFocusChangeListener;
    private @Nullable ToolbarButtonInProductHelpController mToolbarButtonInProductHelpController;
    private boolean mIntentWithEffect;

    /**
     * Construct a new TabbedRootUiCoordinator.
     * @param activity The activity whose UI the coordinator is responsible for.
     * @param onOmniboxFocusChangedListener callback to invoke when Omnibox focus
     *         changes.
     * @param intentWithEffect Whether or not {@code activity} was launched with an
     *         intent to open a single tab.
     * @param shareDelegateSupplier
     * @param tabProvider The {@link ActivityTabProvider} to get current tab of the activity.
     * @param profileSupplier Supplier of the currently applicable profile.
     * @param bookmarkBridgeSupplier Supplier of the bookmark bridge for the current profile.
     */
    public TabbedRootUiCoordinator(ChromeActivity activity,
            Callback<Boolean> onOmniboxFocusChangedListener, boolean intentWithEffect,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            ActivityTabProvider tabProvider,
            ObservableSupplierImpl<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
            ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<BookmarkBridge> bookmarkBridgeSupplier) {
        super(activity, onOmniboxFocusChangedListener, shareDelegateSupplier, tabProvider,
                profileSupplier, bookmarkBridgeSupplier);
        mIntentWithEffect = intentWithEffect;
        mEphemeralTabCoordinatorSupplier = ephemeralTabCoordinatorSupplier;
    }

    @Override
    public void destroy() {
        if (mSystemUiCoordinator != null) mSystemUiCoordinator.destroy();
        if (mEmptyBackgroundViewWrapper != null) mEmptyBackgroundViewWrapper.destroy();

        if (mOfflineIndicatorController != null) {
            mOfflineIndicatorController.destroy();
        }

        if (mToolbarManager != null) {
            mToolbarManager.getFakeboxDelegate().removeUrlFocusChangeListener(
                    mUrlFocusChangeListener);
        }

        if (mStatusIndicatorCoordinator != null) {
            mStatusIndicatorCoordinator.removeObserver(mStatusIndicatorObserver);
            mStatusIndicatorCoordinator.removeObserver(mActivity.getStatusBarColorController());
            mStatusIndicatorCoordinator.destroy();
        }

        if (mToolbarButtonInProductHelpController != null) {
            mToolbarButtonInProductHelpController.destroy();
        }

        super.destroy();
    }

    @Override
    public void onPostInflationStartup() {
        super.onPostInflationStartup();

        mSystemUiCoordinator = new TabbedSystemUiCoordinator(mActivity.getWindow(),
                mActivity.getTabModelSelector(), mActivity.getOverviewModeBehaviorSupplier());
    }

    @Override
    protected void onFindToolbarShown() {
        super.onFindToolbarShown();
        EphemeralTabCoordinator coordinator = mEphemeralTabCoordinatorSupplier.get();
        if (coordinator != null && coordinator.isOpened()) coordinator.close();
    }

    /**
     * @return The toolbar button IPH controller for the tabbed UI this coordinator controls.
     * TODO(pnoland, https://crbug.com/865801): remove this in favor of wiring it directly.
     */
    public ToolbarButtonInProductHelpController getToolbarButtonInProductHelpController() {
        return mToolbarButtonInProductHelpController;
    }

    @Override
    public void onFinishNativeInitialization() {
        // TODO(twellington): Supply TabModelSelector as well and move initialization earlier.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            AppMenuHandler appMenuHandler =
                    mAppMenuCoordinator == null ? null : mAppMenuCoordinator.getAppMenuHandler();
            mEmptyBackgroundViewWrapper = new EmptyBackgroundViewWrapper(
                    mActivity.getTabModelSelector(), mActivity.getTabCreator(false), mActivity,
                    appMenuHandler, mActivity.getSnackbarManager(),
                    mActivity.getOverviewModeBehaviorSupplier());
            mEmptyBackgroundViewWrapper.initialize();
        }

        if (EphemeralTabCoordinator.isSupported()) {
            mEphemeralTabCoordinatorSupplier.set(new EphemeralTabCoordinator(mActivity,
                    mActivity.getWindowAndroid(), mActivity.getWindow().getDecorView(),
                    mActivity.getActivityTabProvider(), mActivity::getCurrentTabCreator,
                    mActivity::getBottomSheetController, () -> !mActivity.isCustomTab()));
        }

        PostTask.postTask(UiThreadTaskTraits.DEFAULT, this::initializeIPH);
    }

    // Protected class methods

    @Override
    protected void onLayoutManagerAvailable(LayoutManager layoutManager) {
        super.onLayoutManagerAvailable(layoutManager);

        initStatusIndicatorCoordinator(layoutManager);

        // clang-format off
        HistoryNavigationCoordinator.create(mActivity.getLifecycleDispatcher(),
                mActivity.getCompositorViewHolder(), mActivity.getActivityTabProvider(),
                mActivity.getInsetObserverView(),
                mActivity::backShouldCloseTab,
                mActivity::onBackPressed,
                tab -> HistoryManagerUtils.showHistoryManager(mActivity, tab),
                mActivity.getResources().getString(R.string.show_full_history),
                () -> mActivity.isActivityFinishingOrDestroyed() ? null
                                                                 : getBottomSheetController());
        // clang-format on
    }

    @Override
    protected void initializeToolbar() {
        super.initializeToolbar();
        if (!mActivity.isTablet()
                && (BottomToolbarConfiguration.isBottomToolbarEnabled()
                        || TabUiFeatureUtilities.isTabGroupsAndroidEnabled())) {
            getToolbarManager().enableBottomToolbar();
        }
    }

    // Private class methods

    private void initializeIPH() {
        WindowAndroid window = mActivity.getWindowAndroid();
        mToolbarButtonInProductHelpController =
                new ToolbarButtonInProductHelpController(mActivity, mAppMenuCoordinator,
                        mActivity.getLifecycleDispatcher(), mActivity.getActivityTabProvider());
        if (!triggerPromo()) {
            mToolbarButtonInProductHelpController.showColdStartIPH();
        }
    }

    private void initStatusIndicatorCoordinator(LayoutManager layoutManager) {
        // TODO(crbug.com/1035584): Disable on tablets for now as we need to do one or two extra
        // things for tablets.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)
                || (!ChromeFeatureList.isEnabled(ChromeFeatureList.OFFLINE_INDICATOR_V2)
                        && !sEnableStatusIndicatorForTests)) {
            return;
        }

        final ChromeFullscreenManager fullscreenManager = mActivity.getFullscreenManager();
        Supplier<Boolean> canAnimateBrowserControls = () -> {
            final Tab tab = mActivity.getActivityTabProvider().get();
            return tab != null && tab.isUserInteractable() && !tab.isNativePage();
        };
        mToolbarManager.setCanAnimateNativeBrowserControlsSupplier(canAnimateBrowserControls);
        mStatusIndicatorCoordinator = new StatusIndicatorCoordinator(mActivity,
                mActivity.getCompositorViewHolder().getResourceManager(), fullscreenManager,
                mActivity.getStatusBarColorController()::getStatusBarColorWithoutStatusIndicator,
                canAnimateBrowserControls, layoutManager::requestUpdate);
        layoutManager.setStatusIndicatorSceneOverlay(mStatusIndicatorCoordinator.getSceneLayer());
        mStatusIndicatorObserver = new StatusIndicatorCoordinator.StatusIndicatorObserver() {
            @Override
            public void onStatusIndicatorHeightChanged(int indicatorHeight) {
                final int resourceId = mActivity.getControlContainerHeightResource();
                final int topControlsNewHeight =
                        mActivity.getResources().getDimensionPixelSize(resourceId)
                        + indicatorHeight;
                fullscreenManager.setAnimateBrowserControlsHeightChanges(true);
                fullscreenManager.setTopControlsHeight(topControlsNewHeight, indicatorHeight);
                fullscreenManager.setAnimateBrowserControlsHeightChanges(false);
            }
        };
        mStatusIndicatorCoordinator.addObserver(mStatusIndicatorObserver);
        mStatusIndicatorCoordinator.addObserver(mActivity.getStatusBarColorController());

        // Don't initialize the offline indicator controller if the feature is disabled.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.OFFLINE_INDICATOR_V2)) {
            return;
        }

        ObservableSupplierImpl<Boolean> isUrlBarFocusedSupplier = new ObservableSupplierImpl<>();
        isUrlBarFocusedSupplier.set(mToolbarManager.getFakeboxDelegate().isUrlBarFocused());
        mUrlFocusChangeListener = new UrlFocusChangeListener() {
            @Override
            public void onUrlFocusChange(boolean hasFocus) {
                // Offline indicator should assume the UrlBar is focused if it's focusing.
                if (hasFocus) {
                    isUrlBarFocusedSupplier.set(true);
                }
            }

            @Override
            public void onUrlAnimationFinished(boolean hasFocus) {
                // Wait for the animation to finish before notifying that UrlBar is unfocused.
                if (!hasFocus) {
                    isUrlBarFocusedSupplier.set(false);
                }
            }
        };
        mOfflineIndicatorController = new OfflineIndicatorControllerV2(
                mActivity, mStatusIndicatorCoordinator, isUrlBarFocusedSupplier);
        mToolbarManager.getFakeboxDelegate().addUrlFocusChangeListener(mUrlFocusChangeListener);
    }

    @VisibleForTesting
    public StatusIndicatorCoordinator getStatusIndicatorCoordinatorForTesting() {
        return mStatusIndicatorCoordinator;
    }

    @VisibleForTesting
    public static void setEnableStatusIndicatorForTests(boolean disable) {
        sEnableStatusIndicatorForTests = disable;
    }

    @VisibleForTesting
    public EphemeralTabCoordinator getEphemeralTabCoordinatorForTesting() {
        return mEphemeralTabCoordinatorSupplier.get();
    }

    /**
     * Triggers the display of an appropriate promo, if any, returning true if a promo is actually
     * displayed.
     */
    private boolean triggerPromo() {
        try (TraceEvent e = TraceEvent.scoped("TabbedRootUiCoordinator.triggerPromo")) {
            SharedPreferencesManager preferenceManager = SharedPreferencesManager.getInstance();
            // Promos can only be shown when we start with ACTION_MAIN intent and
            // after FRE is complete. Native initialization can finish before the FRE flow is
            // complete, and this will only show promos on the second opportunity. This is
            // because the FRE is shown on the first opportunity, and we don't want to show such
            // content back to back.
            //
            // TODO(https://crbug.com/865801, pnoland): Unify promo dialog logic and move into a
            // single PromoDialogCoordinator.
            boolean isShowingPromo =
                    LocaleManager.getInstance().hasShownSearchEnginePromoThisSession();
            // Promo dialogs in multiwindow mode are broken on some devices:
            // http://crbug.com/354696
            boolean isLegacyMultiWindow =
                    MultiWindowUtils.getInstance().isLegacyMultiWindow(mActivity);
            if (!isShowingPromo && !mIntentWithEffect && FirstRunStatus.getFirstRunFlowComplete()
                    && preferenceManager.readBoolean(
                            ChromePreferenceKeys.PROMOS_SKIPPED_ON_FIRST_START, false)
                    && !VrModuleProvider.getDelegate().isInVr()
                    // VrModuleProvider.getDelegate().isInVr may not return true at this point
                    // even though Chrome is about to enter VR, so we need to also check whether
                    // we're launching into VR.
                    && !VrModuleProvider.getIntentDelegate().isLaunchingIntoVr(
                            mActivity, mActivity.getIntent())
                    && !isLegacyMultiWindow) {
                isShowingPromo = maybeShowPromo();
            } else {
                preferenceManager.writeBoolean(
                        ChromePreferenceKeys.PROMOS_SKIPPED_ON_FIRST_START, true);
            }
            return isShowingPromo;
        }
    }

    private boolean maybeShowPromo() {
        // Only one promo can be shown in one run to avoid nagging users too much.
        if (SigninPromoUtil.launchSigninPromoIfNeeded(mActivity)) return true;
        if (DataReductionPromoScreen.launchDataReductionPromo(
                    mActivity, mActivity.getTabModelSelector().getCurrentModel().isIncognito())) {
            return true;
        }

        return LanguageAskPrompt.maybeShowLanguageAskPrompt(mActivity);
    }
}
