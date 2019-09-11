// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.support.annotation.Nullable;
import android.support.annotation.StringRes;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.datareduction.DataReductionSavingsMilestonePromo;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.widget.ViewHighlighter;
import org.chromium.chrome.browser.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * A helper class for IPH shown on the toolbar.
 * TODO(jinsukkim): Find a way to break (i.e. invert) the dependency on Destroyable
 *     to be able to split up the build target for toolbar which can be a base construct.
 */
public class ToolbarButtonInProductHelpController implements Destroyable {
    private final ActivityTabTabObserver mPageLoadObserver;
    private final ChromeActivity mActivity;

    /**
     * Create ToolbarButtonInProductHelpController.
     * @param activity ChromeActivity the controller is attached to.
     * @param isShowingPromo {@code true} if promo dialog is being shown.
     */
    public static void create(final ChromeActivity activity, boolean isShowingPromo) {
        ToolbarButtonInProductHelpController controller =
                new ToolbarButtonInProductHelpController(activity);
        activity.getLifecycleDispatcher().register(controller);
        if (!isShowingPromo) controller.maybeShowColdStartIPH();
    }

    private ToolbarButtonInProductHelpController(final ChromeActivity activity) {
        mActivity = activity;
        mPageLoadObserver = new ActivityTabTabObserver(activity.getActivityTabProvider()) {
            /**
             * Stores total data saved at the start of a page load. Used to calculate delta at the
             * end of page load, which is just an estimate of the data saved for the current page
             * load since there may be multiple pages loading at the same time. This estimate is
             * used to get an idea of how widely used the data saver feature is for a particular
             * user at a time (i.e. not since the user started using Chrome).
             */
            private long mDataSavedOnStartPageLoad;

            @Override
            public void onPageLoadStarted(Tab tab, String url) {
                mDataSavedOnStartPageLoad = DataReductionProxySettings.getInstance()
                                                    .getContentLengthSavedInHistorySummary();
            }

            @Override
            public void onPageLoadFinished(Tab tab, String url) {
                long dataSaved = DataReductionProxySettings.getInstance()
                                         .getContentLengthSavedInHistorySummary()
                        - mDataSavedOnStartPageLoad;
                Tracker tracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedProfile());
                if (dataSaved > 0L) tracker.notifyEvent(EventConstants.DATA_SAVED_ON_PAGE_LOAD);
                if (tab.isPreview()) tracker.notifyEvent(EventConstants.PREVIEWS_PAGE_LOADED);
                if (tab.isUserInteractable()) {
                    maybeShowDataSaverDetail(activity);
                    if (dataSaved > 0L) maybeShowDataSaverMilestonePromo(activity);
                    if (tab.isPreview()) maybeShowPreviewVerboseStatus(activity);
                }
            }
        };
    }

    @Override
    public void destroy() {
        mPageLoadObserver.destroy();
    }

    private static int getDataReductionMenuItemHighlight() {
        return FeatureUtilities.isBottomToolbarEnabled() ? R.id.data_reduction_menu_item
                                                         : R.id.app_menu_footer;
    }

    // Attempts to show an IPH text bubble for data saver detail.
    private static void maybeShowDataSaverDetail(ChromeActivity activity) {
        View anchorView = activity.getToolbarManager().getMenuButtonView();
        if (anchorView == null) return;

        // TODO(https://crbug.com/956260): Provide AppMenuHandler rather than pulling off
        //  ToolbarManager.
        setupAndMaybeShowIPHForFeature(FeatureConstants.DATA_SAVER_DETAIL_FEATURE,
                getDataReductionMenuItemHighlight(), false, R.string.iph_data_saver_detail_text,
                R.string.iph_data_saver_detail_accessibility_text, anchorView,
                activity.getToolbarManager().getAppMenuHandler(), Profile.getLastUsedProfile(),
                activity, null);
    }

    // Attempts to show an IPH text bubble for data saver milestone promo.
    private static void maybeShowDataSaverMilestonePromo(ChromeActivity activity) {
        View anchorView = activity.getToolbarManager().getMenuButtonView();
        if (anchorView == null) return;

        final DataReductionSavingsMilestonePromo promo =
                new DataReductionSavingsMilestonePromo(activity,
                        DataReductionProxySettings.getInstance().getTotalHttpContentLengthSaved());
        if (!promo.shouldShowPromo()) return;

        final Runnable dismissCallback = () -> {
            promo.onPromoTextSeen();
        };
        setupAndMaybeShowIPHForFeature(FeatureConstants.DATA_SAVER_MILESTONE_PROMO_FEATURE,
                getDataReductionMenuItemHighlight(), false, promo.getPromoText(),
                promo.getPromoText(), anchorView, activity.getToolbarManager().getAppMenuHandler(),
                Profile.getLastUsedProfile(), activity, dismissCallback);
    }

    // Attempts to show an IPH text bubble for page in preview mode.
    private static void maybeShowPreviewVerboseStatus(ChromeActivity activity) {
        final View anchorView = activity.getToolbarManager().getSecurityIconView();
        if (anchorView == null) return;

        setupAndMaybeShowIPHForFeature(FeatureConstants.PREVIEWS_OMNIBOX_UI_FEATURE, null, true,
                R.string.iph_previews_omnibox_ui_text,
                R.string.iph_previews_omnibox_ui_accessibility_text, anchorView, null,
                Profile.getLastUsedProfile(), activity, null);
    }

    /**
     * Attempts to show an IPH text bubble for those that trigger on a cold start.
     */
    public void maybeShowColdStartIPH() {
        maybeShowDownloadHomeIPH();
    }

    private void maybeShowDownloadHomeIPH() {
        setupAndMaybeShowIPHForFeature(FeatureConstants.DOWNLOAD_HOME_FEATURE,
                R.id.downloads_menu_id, true, R.string.iph_download_home_text,
                R.string.iph_download_home_accessibility_text,
                mActivity.getToolbarManager().getMenuButtonView(),
                mActivity.getToolbarManager().getAppMenuHandler(), Profile.getLastUsedProfile(),
                mActivity, null);
    }

    /**
     * Attempts to show an IPH text bubble for download continuing.
     * @param activity The activity to use for the IPH.
     * @param profile The profile to use for the tracker.
     */
    public static void maybeShowDownloadContinuingIPH(
            ChromeTabbedActivity activity, Profile profile) {
        setupAndMaybeShowIPHForFeature(
                FeatureConstants.DOWNLOAD_INFOBAR_DOWNLOAD_CONTINUING_FEATURE,
                R.id.downloads_menu_id, true,
                R.string.iph_download_infobar_download_continuing_text,
                R.string.iph_download_infobar_download_continuing_text,
                activity.getToolbarManager().getMenuButtonView(),
                activity.getToolbarManager().getAppMenuHandler(), profile, activity, null);
    }

    private static void setupAndMaybeShowIPHForFeature(String featureName,
            Integer highlightMenuItemId, boolean circleHighlight, @StringRes int stringId,
            @StringRes int accessibilityStringId, View anchorView,
            @Nullable AppMenuHandler appMenuHandler, Profile profile, ChromeActivity activity,
            @Nullable Runnable onDismissCallback) {
        final String contentString = activity.getString(stringId);
        final String accessibilityString = activity.getString(accessibilityStringId);
        final Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.addOnInitializedCallback((Callback<Boolean>) success
                -> maybeShowIPH(tracker, featureName, highlightMenuItemId, circleHighlight,
                        contentString, accessibilityString, anchorView, appMenuHandler, activity,
                        onDismissCallback));
    }

    private static void setupAndMaybeShowIPHForFeature(String featureName,
            Integer highlightMenuItemId, boolean circleHighlight, String contentString,
            String accessibilityString, View anchorView, @Nullable AppMenuHandler appMenuHandler,
            Profile profile, ChromeActivity activity, @Nullable Runnable onDismissCallback) {
        final Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.addOnInitializedCallback((Callback<Boolean>) success
                -> maybeShowIPH(tracker, featureName, highlightMenuItemId, circleHighlight,
                        contentString, accessibilityString, anchorView, appMenuHandler, activity,
                        onDismissCallback));
    }

    private static boolean shouldHighlightForIPH(String featureName) {
        switch (featureName) {
            case FeatureConstants.PREVIEWS_OMNIBOX_UI_FEATURE:
                return false;
            default:
                return true;
        }
    }

    private static void maybeShowIPH(Tracker tracker, String featureName,
            Integer highlightMenuItemId, boolean circleHighlight, String contentString,
            String accessibilityString, View anchorView, AppMenuHandler appMenuHandler,
            ChromeActivity activity, @Nullable Runnable onDismissCallback) {
        // Activity was destroyed; don't show IPH.
        if (activity.isActivityFinishingOrDestroyed() || anchorView == null) return;

        assert (contentString.length() > 0);
        assert (accessibilityString.length() > 0);

        // Post a request to show the IPH bubble to allow time for a layout pass. Since the bubble
        // is shown on startup, the anchor view may not have a height initially see
        // https://crbug.com/871537.
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            if (activity.isActivityFinishingOrDestroyed()) return;

            if (!tracker.shouldTriggerHelpUI(featureName)) return;
            ViewRectProvider rectProvider = new ViewRectProvider(anchorView);

            TextBubble textBubble = new TextBubble(
                    activity, anchorView, contentString, accessibilityString, true, rectProvider);
            textBubble.setDismissOnTouchInteraction(true);
            textBubble.addOnDismissListener(() -> anchorView.getHandler().postDelayed(() -> {
                tracker.dismissed(featureName);
                if (onDismissCallback != null) {
                    onDismissCallback.run();
                }
                if (shouldHighlightForIPH(featureName)) {
                    turnOffHighlightForTextBubble(appMenuHandler, anchorView);
                }
            }, ViewHighlighter.IPH_MIN_DELAY_BETWEEN_TWO_HIGHLIGHTS));

            if (shouldHighlightForIPH(featureName)) {
                turnOnHighlightForTextBubble(
                        appMenuHandler, highlightMenuItemId, circleHighlight, anchorView);
            }

            int yInsetPx = activity.getResources().getDimensionPixelOffset(
                    R.dimen.text_bubble_menu_anchor_y_inset);
            rectProvider.setInsetPx(0, 0, 0, yInsetPx);
            textBubble.show();
        });
    }

    private static void turnOnHighlightForTextBubble(AppMenuHandler handler,
            Integer highlightMenuItemId, boolean circleHighlight, View anchorView) {
        if (handler != null) {
            handler.setMenuHighlight(highlightMenuItemId, circleHighlight);
        } else {
            ViewHighlighter.turnOnHighlight(anchorView, circleHighlight);
        }
    }

    private static void turnOffHighlightForTextBubble(AppMenuHandler handler, View anchorView) {
        if (handler != null) {
            handler.clearMenuHighlight();
        } else {
            ViewHighlighter.turnOffHighlight(anchorView);
        }
    }
}
