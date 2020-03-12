// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.action;

import android.app.Activity;

import androidx.annotation.NonNull;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.feed.FeedLoggingBridge;
import org.chromium.chrome.browser.feed.FeedOfflineIndicator;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.ContentMetadata;
import org.chromium.chrome.browser.feed.library.api.host.action.ActionApi;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.NavigationRecorder;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.HashMap;

/**
 * Handles the actions user can trigger on the feed.
 */
public class FeedActionHandler implements ActionApi {
    private final NativePageNavigationDelegate mDelegate;
    private final Runnable mSuggestionConsumedObserver;
    private final FeedOfflineIndicator mOfflineIndicator;
    private final OfflinePageBridge mOfflinePageBridge;
    private final FeedLoggingBridge mLoggingBridge;
    private final Activity mActivity;
    private final Profile mProfile;
    private static final String TAG = "FeedActionHandler";
    private static final String FEEDBACK_REPORT_TYPE = "USER_INITIATED_FEEDBACK_REPORT";

    // This must match the FeedSendFeedbackType enum in enums.xml.
    public @interface FeedFeedbackType {
        int FEEDBACK_TAPPED_ON_CARD = 0;
        int FEEDBACK_TAPPED_ON_PAGE = 1;
        int NUM_ENTRIES = 2;
    }

    // Map key names - These will be printed with the values in the feedback report.
    public static final String CARD_URL = "CardUrl";
    public static final String CARD_PUBLISHER = "CardPublisher";
    public static final String CARD_PUBLISHING_DATE = "CardPublishingDate";
    public static final String CARD_TITLE = "CardTitle";

    /**
     * @param delegate The {@link NativePageNavigationDelegate} that this handler calls when
     * handling some of the actions.
     * @param suggestionConsumedObserver An observer that is interested in any time a suggestion is
     * consumed by the user.
     * @param offlineIndicator Tracks offline pages and can supply this handler with offline ids.
     * @param offlinePageBridge Capable of updating {@link LoadUrlParams} to include offline ids.
     * @param loggingBridge Reports pages visiting time.
     */
    public FeedActionHandler(@NonNull NativePageNavigationDelegate delegate,
            @NonNull Runnable suggestionConsumedObserver,
            @NonNull FeedOfflineIndicator offlineIndicator,
            @NonNull OfflinePageBridge offlinePageBridge,
            @NonNull FeedLoggingBridge loggingBridge,
            Activity activity,
            Profile profile) {
        mDelegate = delegate;
        mSuggestionConsumedObserver = suggestionConsumedObserver;
        mOfflineIndicator = offlineIndicator;
        mOfflinePageBridge = offlinePageBridge;
        mLoggingBridge = loggingBridge;
        mActivity = activity;
        mProfile = profile;
    }

    @Override
    public void openUrl(String url) {
        openOfflineIfPossible(WindowOpenDisposition.CURRENT_TAB, url);
        mLoggingBridge.reportFeedInteraction();
    }

    @Override
    public boolean canOpenUrl() {
        return true;
    }

    @Override
    public void openUrlInIncognitoMode(String url) {
        mDelegate.openUrl(WindowOpenDisposition.OFF_THE_RECORD, createLoadUrlParams(url));
        mSuggestionConsumedObserver.run();
        mLoggingBridge.reportFeedInteraction();
    }

    @Override
    public boolean canOpenUrlInIncognitoMode() {
        return mDelegate.isOpenInIncognitoEnabled();
    }

    @Override
    public void openUrlInNewTab(String url) {
        openOfflineIfPossible(WindowOpenDisposition.NEW_BACKGROUND_TAB, url);
        mLoggingBridge.reportFeedInteraction();
    }

    @Override
    public boolean canOpenUrlInNewTab() {
        return true;
    }

    @Override
    public void openUrlInNewWindow(String url) {
        openOfflineIfPossible(WindowOpenDisposition.NEW_WINDOW, url);
    }

    @Override
    public boolean canOpenUrlInNewWindow() {
        return mDelegate.isOpenInNewWindowEnabled();
    }

    @Override
    public void downloadUrl(ContentMetadata contentMetadata) {
        mDelegate.openUrl(
                WindowOpenDisposition.SAVE_TO_DISK, createLoadUrlParams(contentMetadata.getUrl()));
        mSuggestionConsumedObserver.run();
        mLoggingBridge.reportFeedInteraction();
    }

    @Override
    public boolean canDownloadUrl() {
        return true;
    }

    @Override
    public void sendFeedback(ContentMetadata contentMetadata) {
        RecordHistogram.recordEnumeratedHistogram("ContentSuggestions.Feed.SendFeedback",
                FeedFeedbackType.FEEDBACK_TAPPED_ON_CARD, FeedFeedbackType.NUM_ENTRIES);
        HashMap<String, String> feedContext = new HashMap<String, String>();

        if (contentMetadata != null) {
            // Get the pieces of metadata that we need.
            String articlePublishingDate = String.valueOf(contentMetadata.getTimePublished());
            String articleUrl = contentMetadata.getUrl();
            String title = contentMetadata.getTitle();
            String originalPublisher = contentMetadata.getPublisher();
            // TODO(https://crbug.com/1992269) - Get the other data to add in.

            // Fill the context map with the card specific data.
            feedContext.put(CARD_URL, articleUrl);
            feedContext.put(CARD_PUBLISHER, originalPublisher);
            feedContext.put(CARD_PUBLISHING_DATE, articlePublishingDate);
            feedContext.put(CARD_TITLE, title);
        }

        String feedbackContext = "mobile_browser";
        // Reports for Chrome mobile must have a contextTag of the form
        // com.chrome.canary.USER_INITIATED_FEEDBACK_REPORT, or they will be discarded.
        String contextTag =
                ContextUtils.getApplicationContext().getPackageName() + "." + FEEDBACK_REPORT_TYPE;

        HelpAndFeedback.getInstance().showFeedback(mActivity, mProfile, contentMetadata.getUrl(),
                contextTag, feedContext, feedbackContext);
        mLoggingBridge.reportFeedInteraction();
        return;
    }

    @Override
    public void learnMore() {
        mDelegate.navigateToHelpPage();
        mLoggingBridge.reportFeedInteraction();
    }

    @Override
    public boolean canLearnMore() {
        return true;
    }

    private LoadUrlParams createLoadUrlParams(String url) {
        LoadUrlParams params = new LoadUrlParams(url, PageTransition.AUTO_BOOKMARK);
        params.setReferrer(
                new Referrer(SuggestionsConfig.getReferrerUrl(
                                     ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS),
                        ReferrerPolicy.ALWAYS));
        return params;
    }

    /**
     * Opens the given url in offline mode if possible by setting load params so the offline
     * interceptor will handle the request. If there is no offline id, fall back to just opening the
     * |url| through the |mDelegate|.
     *
     * @param disposition How to open the article. Should not be OFF_THE_RECORD, as offline pages
     * does not support opening articles while incognito.
     * @param url The url of the article. Should match what was previously requested by Feed to
     * OfflineIndicatorApi implementation exactly.
     */
    private void openOfflineIfPossible(int disposition, String url) {
        Long maybeOfflineId = mOfflineIndicator.getOfflineIdIfPageIsOfflined(url);
        if (maybeOfflineId == null) {
            openAndRecord(disposition, createLoadUrlParams(url), /*isOffline*/ false);
        } else {
            mOfflinePageBridge.getLoadUrlParamsByOfflineId(
                    maybeOfflineId, LaunchLocation.SUGGESTION, (loadUrlParams) -> {
                        if (loadUrlParams == null) {
                            // Fall back to opening online if the lookup failed.
                            openAndRecord(
                                    disposition, createLoadUrlParams(url), /*isOffline*/ false);
                        } else {
                            // Offline headers need to be moved to be read correctly.
                            loadUrlParams.setVerbatimHeaders(loadUrlParams.getExtraHeadersString());
                            openAndRecord(disposition, loadUrlParams, /*isOffline*/ true);
                        }
                    });
        }
    }
    /**
     * Opens the given resource, specified by params, and records how much time the user spends on
     * the suggested page.
     *
     * @param disposition How to open the article.
     * @param loadUrlParams Parameters specifying the URL to load and other navigation details.
     * @param isOffline If the page should open in offline mode or not, for metrics reporting.
     */
    private void openAndRecord(int disposition, LoadUrlParams loadUrlParams, boolean isOffline) {
        Tab loadingTab = mDelegate.openUrl(disposition, loadUrlParams);
        if (loadingTab != null) {
            // Records how long the user spending on the suggested page, and whether the user got
            // back to the NTP.
            NavigationRecorder.record(loadingTab,
                    visitData
                    -> mLoggingBridge.onContentTargetVisited(
                            visitData.duration, isOffline, NewTabPage.isNTPUrl(visitData.endUrl)));
        }
        mSuggestionConsumedObserver.run();
    }
}
