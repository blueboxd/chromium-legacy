// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import android.app.Activity;
import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.RequestCoordinatorBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.xsurface.FeedActionsHandler;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface.SurfaceActionsHandler;
import org.chromium.chrome.browser.xsurface.SurfaceDependencyProvider;
import org.chromium.chrome.browser.xsurface.SurfaceScope;
import org.chromium.chrome.browser.xsurface.SurfaceScopeDependencyProvider;
import org.chromium.chrome.feed.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.feed.proto.FeedUiProto.SharedState;
import org.chromium.components.feed.proto.FeedUiProto.Slice;
import org.chromium.components.feed.proto.FeedUiProto.StreamUpdate;
import org.chromium.components.feed.proto.FeedUiProto.StreamUpdate.SliceUpdate;
import org.chromium.components.feed.proto.FeedUiProto.ZeroStateSlice;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;

/**
 * Bridge class that lets Android code access native code for feed related functionalities.
 *
 * Created once for each StreamSurfaceMediator corresponding to each NTP/start surface.
 */
@JNINamespace("feed")
public class FeedStreamSurface implements SurfaceActionsHandler, FeedActionsHandler {
    private static final String TAG = "FeedStreamSurface";

    private static final int SNACKBAR_DURATION_MS_SHORT = 4000;
    private static final int SNACKBAR_DURATION_MS_LONG = 10000;

    @VisibleForTesting
    static final String FEEDBACK_REPORT_TYPE =
            "com.google.chrome.feed.USER_INITIATED_FEEDBACK_REPORT";
    @VisibleForTesting
    static final String FEEDBACK_CONTEXT = "mobile_browser";
    @VisibleForTesting
    static final String XSURFACE_CARD_URL = "Card URL";

    private final long mNativeFeedStreamSurface;
    private final FeedListContentManager mContentManager;
    private final SurfaceScope mSurfaceScope;
    private final RecyclerView mRootView;
    private final HybridListRenderer mHybridListRenderer;
    private final SnackbarManager mSnackbarManager;
    private final Activity mActivity;
    private final BottomSheetController mBottomSheetController;
    @Nullable
    private FeedSliceViewTracker mSliceViewTracker;
    private final NativePageNavigationDelegate mPageNavigationDelegate;
    private final HelpAndFeedback mHelpAndFeedback;

    private int mHeaderCount;
    private BottomSheetContent mBottomSheetContent;
    private final int mLoadMoreTriggerLookahead;
    private boolean mIsLoadingMoreContent;

    private static ProcessScope sXSurfaceProcessScope;

    public static ProcessScope xSurfaceProcessScope() {
        if (sXSurfaceProcessScope == null) {
            sXSurfaceProcessScope = AppHooks.get().getExternalSurfaceProcessScope(
                    new FeedSurfaceDependencyProvider());
        }
        return sXSurfaceProcessScope;
    }

    // This must match the FeedSendFeedbackType enum in enums.xml.
    public @interface FeedFeedbackType {
        int FEEDBACK_TAPPED_ON_CARD = 0;
        int FEEDBACK_TAPPED_ON_PAGE = 1;
        int NUM_ENTRIES = 2;
    }

    // We avoid attaching surfaces until after |startup()| is called. This ensures that
    // the correct sign-in state is used if attaching the surface triggers a fetch.

    private static boolean sStartupCalled;
    private static HashSet<FeedStreamSurface> sWaitingSurfaces;

    public static void startup() {
        if (sStartupCalled) return;
        sStartupCalled = true;
        FeedServiceBridge.startup();
        xSurfaceProcessScope();
        if (sWaitingSurfaces != null) {
            for (FeedStreamSurface surface : sWaitingSurfaces) {
                surface.surfaceOpened();
            }
            sWaitingSurfaces = null;
        }
    }

    private static void openSurfaceAtStartup(FeedStreamSurface surface) {
        if (sWaitingSurfaces == null) {
            sWaitingSurfaces = new HashSet<FeedStreamSurface>();
        }
        sWaitingSurfaces.add(surface);
    }

    private static void cancelOpenSurfaceAtStartup(FeedStreamSurface surface) {
        if (sWaitingSurfaces != null) {
            sWaitingSurfaces.remove(surface);
        }
    }

    /**
     * Provides logging and context for all surfaces.
     *
     * TODO(rogerm): Find a more global home for this.
     * TODO(rogerm): implement getClientInstanceId.
     */
    private static class FeedSurfaceDependencyProvider implements SurfaceDependencyProvider {
        FeedSurfaceDependencyProvider() {}

        @Override
        public Context getContext() {
            return ContextUtils.getApplicationContext();
        }

        @Override
        public String getAccountName() {
            CoreAccountInfo primaryAccount =
                    IdentityServicesProvider.get()
                            .getIdentityManager(Profile.getLastUsedRegularProfile())
                            .getPrimaryAccountInfo(ConsentLevel.NOT_REQUIRED);
            return primaryAccount == null ? "" : primaryAccount.getEmail();
        }

        @Override
        public int[] getExperimentIds() {
            return FeedStreamSurfaceJni.get().getExperimentIds();
        }
    }

    /**
     * Provides activity and darkmode context for a single surface.
     */
    private static class FeedSurfaceScopeDependencyProvider
            implements SurfaceScopeDependencyProvider {
        final Context mActivityContext;
        final boolean mDarkMode;
        FeedSurfaceScopeDependencyProvider(Context activityContext, boolean darkMode) {
            mActivityContext = activityContext;
            mDarkMode = darkMode;
        }

        @Override
        public Context getActivityContext() {
            return mActivityContext;
        }

        @Override
        public boolean isDarkModeEnabled() {
            return mDarkMode;
        }
    }

    /**
     * A {@link TabObserver} that observes navigation related events that originate from Feed
     * interactions. Calls reportPageLoaded when navigation completes.
     */
    private class FeedTabNavigationObserver extends EmptyTabObserver {
        private final boolean mInNewTab;

        FeedTabNavigationObserver(boolean inNewTab) {
            mInNewTab = inNewTab;
        }

        @Override
        public void onPageLoadFinished(Tab tab, String url) {
            // TODO(jianli): onPageLoadFinished is called on successful load, and if a user manually
            // stops the page load. We should only capture successful page loads.
            FeedStreamSurfaceJni.get().reportPageLoaded(
                    mNativeFeedStreamSurface, FeedStreamSurface.this, url, mInNewTab);
            tab.removeObserver(this);
        }

        @Override
        public void onPageLoadFailed(Tab tab, int errorCode) {
            tab.removeObserver(this);
        }

        @Override
        public void onCrash(Tab tab) {
            tab.removeObserver(this);
        }

        @Override
        public void onDestroyed(Tab tab) {
            tab.removeObserver(this);
        }
    }

    /**
     * Creates a {@link FeedStreamSurface} for creating native side bridge to access native feed
     * client implementation.
     */
    public FeedStreamSurface(Activity activity, boolean isBackgroundDark,
            SnackbarManager snackbarManager, NativePageNavigationDelegate pageNavigationDelegate,
            BottomSheetController bottomSheetController, HelpAndFeedback helpAndFeedback) {
        mNativeFeedStreamSurface = FeedStreamSurfaceJni.get().init(FeedStreamSurface.this);
        mSnackbarManager = snackbarManager;
        mActivity = activity;
        mHelpAndFeedback = helpAndFeedback;

        mPageNavigationDelegate = pageNavigationDelegate;
        mBottomSheetController = bottomSheetController;
        mLoadMoreTriggerLookahead = FeedServiceBridge.getLoadMoreTriggerLookahead();

        mContentManager = new FeedListContentManager(this, this);

        Context context = new ContextThemeWrapper(
                activity, (isBackgroundDark ? R.style.Dark : R.style.Light));

        ProcessScope processScope = xSurfaceProcessScope();
        if (processScope != null) {
            mSurfaceScope = processScope.obtainSurfaceScope(
                    new FeedSurfaceScopeDependencyProvider(context, isBackgroundDark));
            ;
        } else {
            mSurfaceScope = null;
        }

        if (mSurfaceScope != null) {
            mHybridListRenderer = mSurfaceScope.provideListRenderer();
        } else {
            mHybridListRenderer = new NativeViewListRenderer(context);
        }

        if (mHybridListRenderer != null) {
            // XSurface returns a View, but it should be a RecyclerView.
            mRootView = (RecyclerView) mHybridListRenderer.bind(mContentManager);

            mSliceViewTracker =
                    new FeedSliceViewTracker(mRootView, mContentManager, (String sliceId) -> {
                        FeedStreamSurfaceJni.get().reportSliceViewed(
                                mNativeFeedStreamSurface, FeedStreamSurface.this, sliceId);
                    });
        } else {
            mRootView = null;
        }
    }

    /**
     * Performs all necessary cleanups.
     */
    public void destroy() {
        if (mSliceViewTracker != null) {
            mSliceViewTracker.destroy();
            mSliceViewTracker = null;
        }
        mHybridListRenderer.unbind();
        surfaceClosed();
    }

    /**
     * Puts a list of header views at the beginning.
     */
    public void setHeaderViews(List<View> headerViews) {
        ArrayList<FeedListContentManager.FeedContent> newContentList =
                new ArrayList<FeedListContentManager.FeedContent>();

        // First add new header contents. Some of them may appear in the existing list.
        for (int i = 0; i < headerViews.size(); ++i) {
            View view = headerViews.get(i);
            String key = "Header" + view.hashCode();
            FeedListContentManager.NativeViewContent headerContent =
                    new FeedListContentManager.NativeViewContent(key, view);
            newContentList.add(headerContent);
        }

        // Then add all existing feed stream contents.
        for (int i = mHeaderCount; i < mContentManager.getItemCount(); ++i) {
            newContentList.add(mContentManager.getContent(i));
        }

        updateContentsInPlace(newContentList);

        mHeaderCount = headerViews.size();
    }

    /**
     * @return The android {@link View} that the surface is supposed to show.
     */
    public View getView() {
        return mRootView;
    }

    /**
     * Attempts to load more content if it can be triggered.
     * @return true if loading more content can be triggered.
     */
    public boolean maybeLoadMore() {
        // Checks if loading more can be triggered.
        boolean canLoadMore = false;
        LinearLayoutManager layoutManager = (LinearLayoutManager) mRootView.getLayoutManager();
        if (layoutManager == null) {
            return false;
        }
        int totalItemCount = layoutManager.getItemCount();
        int lastVisibleItem = layoutManager.findLastVisibleItemPosition();
        if (totalItemCount - lastVisibleItem > mLoadMoreTriggerLookahead) {
            return false;
        }

        // Starts to load more content if not yet.
        if (!mIsLoadingMoreContent) {
            mIsLoadingMoreContent = true;
            FeedStreamSurfaceJni.get().loadMore(mNativeFeedStreamSurface, FeedStreamSurface.this,
                    (Boolean success) -> { mIsLoadingMoreContent = false; });
        }

        return true;
    }

    @VisibleForTesting
    FeedListContentManager getFeedListContentManagerForTesting() {
        return mContentManager;
    }

    /**
     * Called when the stream update content is available. The content will get passed to UI
     */
    @CalledByNative
    void onStreamUpdated(byte[] data) {
        StreamUpdate streamUpdate;
        try {
            streamUpdate = StreamUpdate.parseFrom(data);
        } catch (com.google.protobuf.InvalidProtocolBufferException e) {
            Log.wtf(TAG, "Unable to parse StreamUpdate proto data", e);
            return;
        }

        // Update using shared states.
        for (SharedState state : streamUpdate.getNewSharedStatesList()) {
            mHybridListRenderer.update(state.getXsurfaceSharedState().toByteArray());
        }

        // Builds the new list containing:
        // * existing headers
        // * both new and existing contents
        ArrayList<FeedListContentManager.FeedContent> newContentList =
                new ArrayList<FeedListContentManager.FeedContent>();
        for (int i = 0; i < mHeaderCount; ++i) {
            newContentList.add(mContentManager.getContent(i));
        }
        for (SliceUpdate sliceUpdate : streamUpdate.getUpdatedSlicesList()) {
            if (sliceUpdate.hasSlice()) {
                newContentList.add(createContentFromSlice(sliceUpdate.getSlice()));
            } else {
                String existingSliceId = sliceUpdate.getSliceId();
                int position = mContentManager.findContentPositionByKey(existingSliceId);
                if (position != -1) {
                    newContentList.add(mContentManager.getContent(position));
                }
            }
        }

        updateContentsInPlace(newContentList);
    }

    @CalledByNative
    void replaceDataStoreEntry(String key, byte[] data) {
        if (mSurfaceScope != null) mSurfaceScope.replaceDataStoreEntry(key, data);
    }

    @CalledByNative
    void removeDataStoreEntry(String key) {
        if (mSurfaceScope != null) mSurfaceScope.removeDataStoreEntry(key);
    }

    private void updateContentsInPlace(
            ArrayList<FeedListContentManager.FeedContent> newContentList) {
        // 1) Builds the hash set based on keys of new contents.
        HashSet<String> newContentKeySet = new HashSet<String>();
        for (int i = 0; i < newContentList.size(); ++i) {
            newContentKeySet.add(newContentList.get(i).getKey());
        }

        // 2) Builds the hash map of existing content list for fast look up by key.
        HashMap<String, FeedListContentManager.FeedContent> existingContentMap =
                new HashMap<String, FeedListContentManager.FeedContent>();
        for (int i = 0; i < mContentManager.getItemCount(); ++i) {
            FeedListContentManager.FeedContent content = mContentManager.getContent(i);
            existingContentMap.put(content.getKey(), content);
        }

        // 3) Removes those existing contents that do not appear in the new list.
        for (int i = mContentManager.getItemCount() - 1; i >= 0; --i) {
            String key = mContentManager.getContent(i).getKey();
            if (!newContentKeySet.contains(key)) {
                mContentManager.removeContents(i, 1);
                existingContentMap.remove(key);
            }
        }

        // 4) Iterates through the new list to add the new content or move the existing content
        //    if needed.
        int i = 0;
        while (i < newContentList.size()) {
            FeedListContentManager.FeedContent content = newContentList.get(i);

            // If this is an existing content, moves it to new position.
            if (existingContentMap.containsKey(content.getKey())) {
                mContentManager.moveContent(
                        mContentManager.findContentPositionByKey(content.getKey()), i);
                ++i;
                continue;
            }

            // Otherwise, this is new content. Add it together with all adjacent new contents.
            int startIndex = i++;
            while (i < newContentList.size()
                    && !existingContentMap.containsKey(newContentList.get(i).getKey())) {
                ++i;
            }
            mContentManager.addContents(startIndex, newContentList.subList(startIndex, i));
        }
    }

    private FeedListContentManager.FeedContent createContentFromSlice(Slice slice) {
        String sliceId = slice.getSliceId();
        if (slice.hasXsurfaceSlice()) {
            return new FeedListContentManager.ExternalViewContent(
                    sliceId, slice.getXsurfaceSlice().getXsurfaceFrame().toByteArray());
        } else if (slice.hasLoadingSpinnerSlice()) {
            return new FeedListContentManager.NativeViewContent(sliceId, R.layout.feed_spinner);
        }
        assert slice.hasZeroStateSlice();
        if (slice.getZeroStateSlice().getType() == ZeroStateSlice.Type.CANT_REFRESH) {
            return new FeedListContentManager.NativeViewContent(sliceId, R.layout.no_connection);
        }
        assert slice.getZeroStateSlice().getType() == ZeroStateSlice.Type.NO_CARDS_AVAILABLE;
        return new FeedListContentManager.NativeViewContent(sliceId, R.layout.no_content_v2);
    }

    @Override
    public void navigateTab(String url) {
        openUrl(url, WindowOpenDisposition.CURRENT_TAB);

        // Attempts to load more content if needed.
        maybeLoadMore();
    }

    @Override
    public void navigateNewTab(String url) {
        openUrl(url, WindowOpenDisposition.NEW_FOREGROUND_TAB);

        // Attempts to load more content if needed.
        maybeLoadMore();
    }

    @Override
    public void navigateIncognitoTab(String url) {
        openUrl(url, WindowOpenDisposition.OFF_THE_RECORD);

        // Attempts to load more content if needed.
        maybeLoadMore();
    }

    @Override
    public void downloadLink(String url) {
        RequestCoordinatorBridge.getForProfile(Profile.getLastUsedRegularProfile())
                .savePageLater(url, OfflinePageBridge.SUGGESTED_ARTICLES_NAMESPACE,
                        true /* user requested*/);
    }

    @Override
    public void showBottomSheet(View view) {
        dismissBottomSheet();

        // Make a sheetContent with the view.
        mBottomSheetContent = new CardMenuBottomSheetContent(view);
        mBottomSheetController.requestShowContent(mBottomSheetContent, true);
    }

    @Override
    public void dismissBottomSheet() {
        if (mBottomSheetContent != null) {
            mBottomSheetController.hideContent(mBottomSheetContent, true);
        }
        mBottomSheetContent = null;
    }

    @Override
    public void loadMore() {
        // TODO(jianli): Remove this from FeedActionsHandler interface.
    }

    @Override
    public void processThereAndBackAgainData(byte[] data) {
        FeedStreamSurfaceJni.get().processThereAndBackAgain(
                mNativeFeedStreamSurface, FeedStreamSurface.this, data);
    }

    @Override
    public void sendFeedback(Map<String, String> productSpecificDataMap) {
        Profile profile = Profile.getLastUsedRegularProfile();
        if (profile == null) {
            return;
        }

        String url = productSpecificDataMap.get(XSURFACE_CARD_URL);
        if (url == null) {
            return;
        }

        Map<String, String> feedContext = convertNameFormat(productSpecificDataMap);

        // FEEDBACK_CONTEXT: This identifies this feedback as coming from Chrome for Android (as
        // opposed to desktop).
        // FEEDBACK_REPORT_TYPE: Reports for Chrome mobile must have a contextTag of the form
        // com.chrome.feed.USER_INITIATED_FEEDBACK_REPORT, or they will be discarded for not
        // matching an allow list rule.
        mHelpAndFeedback.showFeedback(
                mActivity, profile, url, FEEDBACK_REPORT_TYPE, feedContext, FEEDBACK_CONTEXT);
    }

    // Since the XSurface client strings are slightly different than the Feed strings, convert the
    // name from the XSurface format to the format that can be handled by the feedback system.  Any
    // new strings that are added on the XSurface side will need a code change here, and adding the
    // PSD to the allow list.
    private Map<String, String> convertNameFormat(Map<String, String> xSurfaceMap) {
        Map<String, String> feedbackNameConversionMap = new HashMap<>();
        feedbackNameConversionMap.put("Card URL", "CardUrl");
        feedbackNameConversionMap.put("Card Title", "CardTitle");
        feedbackNameConversionMap.put("Card Snippet", "CardSnippet");
        feedbackNameConversionMap.put("Card category", "CardCategory");
        feedbackNameConversionMap.put("Doc Creation Date", "DocCreationDate");

        // For each <name, value> entry in the input map, convert the name to the new name, and
        // write the new <name, value> pair into the output map.
        Map<String, String> feedbackMap = new HashMap<>();
        for (Map.Entry<String, String> entry : xSurfaceMap.entrySet()) {
            String newName = feedbackNameConversionMap.get(entry.getKey());
            if (newName != null) {
                feedbackMap.put(newName, entry.getValue());
            } else {
                Log.v(TAG, "Found an entry with no conversion available.");
                // We will put the entry into the map if untranslatable. It will be discarded
                // unless it matches an allow list on the server, though. This way we can choose
                // to allow it on the server if desired.
                feedbackMap.put(entry.getKey(), entry.getValue());
            }
        }

        return feedbackMap;
    }

    @Override
    public int requestDismissal(byte[] data) {
        return FeedStreamSurfaceJni.get().executeEphemeralChange(
                mNativeFeedStreamSurface, FeedStreamSurface.this, data);
    }

    @Override
    public void commitDismissal(int changeId) {
        FeedStreamSurfaceJni.get().commitEphemeralChange(
                mNativeFeedStreamSurface, FeedStreamSurface.this, changeId);

        // Attempts to load more content if needed.
        maybeLoadMore();
    }

    @Override
    public void discardDismissal(int changeId) {
        FeedStreamSurfaceJni.get().discardEphemeralChange(
                mNativeFeedStreamSurface, FeedStreamSurface.this, changeId);
    }

    @Override
    public void showSnackbar(String text, String actionLabel,
            FeedActionsHandler.SnackbarDuration duration,
            FeedActionsHandler.SnackbarController controller) {
        int durationMs = SNACKBAR_DURATION_MS_SHORT;
        if (duration == FeedActionsHandler.SnackbarDuration.LONG) {
            durationMs = SNACKBAR_DURATION_MS_LONG;
        }

        mSnackbarManager.showSnackbar(
                Snackbar.make(text,
                                new SnackbarManager.SnackbarController() {
                                    @Override
                                    public void onAction(Object actionData) {
                                        controller.onAction();
                                    }
                                    @Override
                                    public void onDismissNoAction(Object actionData) {
                                        controller.onDismissNoAction();
                                    }
                                },
                                Snackbar.TYPE_ACTION, Snackbar.UMA_FEED_NTP_STREAM)
                        .setAction(actionLabel, /*actionData=*/null)
                        .setDuration(durationMs));
    }

    /**
     * Informs that the surface is opened. We can request the initial set of content now. Once
     * the content is available, onStreamUpdated will be called.
     */
    public void surfaceOpened() {
        if (!sStartupCalled) {
            openSurfaceAtStartup(this);
        } else {
            FeedStreamSurfaceJni.get().surfaceOpened(
                    mNativeFeedStreamSurface, FeedStreamSurface.this);
        }
    }

    /**
     * Informs that the surface is closed.
     */
    public void surfaceClosed() {
        int feedCount = mContentManager.getItemCount() - mHeaderCount;
        if (feedCount > 0) {
            mContentManager.removeContents(mHeaderCount, feedCount);
        }

        if (!sStartupCalled) {
            cancelOpenSurfaceAtStartup(this);
        } else {
            FeedStreamSurfaceJni.get().surfaceClosed(
                    mNativeFeedStreamSurface, FeedStreamSurface.this);
        }
    }

    private void openUrl(String url, int disposition) {
        LoadUrlParams params = new LoadUrlParams(url, PageTransition.AUTO_BOOKMARK);
        params.setReferrer(
                new Referrer(SuggestionsConfig.getReferrerUrl(ChromeFeatureList.INTEREST_FEED_V2),
                        ReferrerPolicy.ALWAYS));
        Tab tab = mPageNavigationDelegate.openUrl(disposition, params);

        boolean inNewTab = (disposition == WindowOpenDisposition.NEW_BACKGROUND_TAB
                || disposition == WindowOpenDisposition.OFF_THE_RECORD);

        FeedStreamSurfaceJni.get().reportNavigationStarted(
                mNativeFeedStreamSurface, FeedStreamSurface.this);
        if (tab != null) {
            tab.addObserver(new FeedTabNavigationObserver(inNewTab));
        }
    }

    @NativeMethods
    interface Natives {
        long init(FeedStreamSurface caller);
        int[] getExperimentIds();
        // TODO(jianli): Call this function at the appropriate time.
        void reportSliceViewed(
                long nativeFeedStreamSurface, FeedStreamSurface caller, String sliceId);
        void reportNavigationStarted(long nativeFeedStreamSurface, FeedStreamSurface caller);
        // TODO(jianli): Call this function at the appropriate time.
        void reportPageLoaded(long nativeFeedStreamSurface, FeedStreamSurface caller, String url,
                boolean inNewTab);
        // TODO(jianli): Call this function at the appropriate time.
        void reportOpenAction(
                long nativeFeedStreamSurface, FeedStreamSurface caller, String sliceId);
        // TODO(jianli): Call this function at the appropriate time.
        void reportOpenInNewTabAction(
                long nativeFeedStreamSurface, FeedStreamSurface caller, String sliceId);
        // TODO(jianli): Call this function at the appropriate time.
        void reportOpenInNewIncognitoTabAction(
                long nativeFeedStreamSurface, FeedStreamSurface caller);
        // TODO(jianli): Call this function at the appropriate time.
        void reportSendFeedbackAction(long nativeFeedStreamSurface, FeedStreamSurface caller);
        // TODO(jianli): Call this function at the appropriate time.
        void reportLearnMoreAction(long nativeFeedStreamSurface, FeedStreamSurface caller);
        // TODO(jianli): Call this function at the appropriate time.
        void reportDownloadAction(long nativeFeedStreamSurface, FeedStreamSurface caller);
        // TODO(jianli): Call this function at the appropriate time.
        void reportRemoveAction(long nativeFeedStreamSurface, FeedStreamSurface caller);
        // TODO(jianli): Call this function at the appropriate time.
        void reportNotInterestedInAction(long nativeFeedStreamSurface, FeedStreamSurface caller);
        // TODO(jianli): Call this function at the appropriate time.
        void reportManageInterestsAction(long nativeFeedStreamSurface, FeedStreamSurface caller);
        // TODO(jianli): Call this function at the appropriate time.
        void reportContextMenuOpened(long nativeFeedStreamSurface, FeedStreamSurface caller);
        // TODO(jianli): Call this function at the appropriate time.
        void reportStreamScrolled(
                long nativeFeedStreamSurface, FeedStreamSurface caller, int distanceDp);
        // TODO(jianli): Call this function at the appropriate time.
        void reportStreamScrollStart(long nativeFeedStreamSurface, FeedStreamSurface caller);
        void loadMore(
                long nativeFeedStreamSurface, FeedStreamSurface caller, Callback<Boolean> callback);
        void processThereAndBackAgain(
                long nativeFeedStreamSurface, FeedStreamSurface caller, byte[] data);
        int executeEphemeralChange(
                long nativeFeedStreamSurface, FeedStreamSurface caller, byte[] data);
        void commitEphemeralChange(
                long nativeFeedStreamSurface, FeedStreamSurface caller, int changeId);
        void discardEphemeralChange(
                long nativeFeedStreamSurface, FeedStreamSurface caller, int changeId);
        void surfaceOpened(long nativeFeedStreamSurface, FeedStreamSurface caller);
        void surfaceClosed(long nativeFeedStreamSurface, FeedStreamSurface caller);
    }
}
