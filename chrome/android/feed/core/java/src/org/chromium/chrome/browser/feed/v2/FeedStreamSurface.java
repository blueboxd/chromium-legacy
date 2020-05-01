// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import android.content.Context;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.xsurface.FeedActionsHandler;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface.SurfaceActionsHandler;
import org.chromium.chrome.browser.xsurface.SurfaceDependencyProvider;
import org.chromium.chrome.browser.xsurface.SurfaceScope;
import org.chromium.components.feed.proto.FeedUiProto.Slice;
import org.chromium.components.feed.proto.FeedUiProto.StreamUpdate;
import org.chromium.components.feed.proto.FeedUiProto.StreamUpdate.SliceUpdate;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;

/**
 * Bridge class that lets Android code access native code for feed related functionalities.
 *
 * Created once for each StreamSurfaceMediator corresponding to each NTP/start surface.
 */
@JNINamespace("feed")
public class FeedStreamSurface implements SurfaceActionsHandler, FeedActionsHandler {
    private static final String TAG = "FeedStreamSurface";
    private final long mNativeFeedStreamSurface;
    private final FeedListContentManager mContentManager;
    private final TabModelSelector mTabModelSelector;
    private final Supplier<Tab> mTabProvider;
    private final SurfaceScope mSurfaceScope;
    private final View mRootView;
    private final HybridListRenderer mHybridListRenderer;

    private static ProcessScope sXSurfaceProcessScope;

    public static ProcessScope xSurfaceProcessScope() {
        if (sXSurfaceProcessScope == null) {
            sXSurfaceProcessScope =
                    AppHooks.get().getExternalSurfaceProcessScope(new SurfaceDependencyProvider() {
                        @Override
                        public Context getContext() {
                            return ContextUtils.getApplicationContext();
                        }
                    });
        }
        return sXSurfaceProcessScope;
    }

    /**
     * A {@link TabObserver} that observes navigation related events that originate from Feed
     * interactions. Calls reportNavigationDone when navigation completes.
     */
    private class FeedTabNavigationObserver extends EmptyTabObserver {
        private final boolean mInNewTab;

        FeedTabNavigationObserver(boolean inNewTab) {
            mInNewTab = inNewTab;
        }

        @Override
        public void onPageLoadFinished(Tab tab, String url) {
            FeedStreamSurfaceJni.get().reportNavigationDone(
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
    public FeedStreamSurface(TabModelSelector tabModelSelector, Supplier<Tab> tabProvider) {
        mNativeFeedStreamSurface = FeedStreamSurfaceJni.get().init(FeedStreamSurface.this);
        mTabModelSelector = tabModelSelector;
        mTabProvider = tabProvider;

        mContentManager = new FeedListContentManager(this, this);

        ProcessScope processScope = xSurfaceProcessScope();
        if (processScope != null) {
            mSurfaceScope = xSurfaceProcessScope().obtainSurfaceScope();
        } else {
            mSurfaceScope = null;
        }

        if (mSurfaceScope != null) {
            mHybridListRenderer = mSurfaceScope.provideListRenderer();
        } else {
            mHybridListRenderer = null;
        }

        if (mHybridListRenderer != null) {
            mRootView = mHybridListRenderer.bind(mContentManager);
        } else {
            mRootView = null;
        }
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

        // 1) Builds the hash map of existing content list for fast look up by slice id.
        HashMap<String, FeedListContentManager.FeedContent> existingContentMap =
                new HashMap<String, FeedListContentManager.FeedContent>();
        for (int i = 0; i < mContentManager.getItemCount(); ++i) {
            FeedListContentManager.FeedContent content = mContentManager.getContent(i);
            existingContentMap.put(content.getKey(), content);
        }

        // 2) Builds the new list containing both new and existing contents.
        ArrayList<FeedListContentManager.FeedContent> newContentList =
                new ArrayList<FeedListContentManager.FeedContent>();
        HashSet<String> existingIdsInNewContentList = new HashSet<String>();
        for (SliceUpdate sliceUpdate : streamUpdate.getUpdatedSlicesList()) {
            if (sliceUpdate.hasSlice()) {
                newContentList.add(createContentFromSlice(sliceUpdate.getSlice()));
            } else {
                String existingSliceId = sliceUpdate.getSliceId();
                FeedListContentManager.FeedContent content =
                        existingContentMap.get(existingSliceId);
                if (content != null) {
                    newContentList.add(content);
                    existingIdsInNewContentList.add(existingSliceId);
                }
            }
        }

        // 3) Removes those contents that do not appear in the new list as the existing contents.
        //    Sometimes we may add new content with same id as the one in current list. In this
        //    case, we will remove it from current list and add it again later as new content.
        for (int i = mContentManager.getItemCount() - 1; i >= 0; --i) {
            String id = mContentManager.getContent(i).getKey();
            if (!existingIdsInNewContentList.contains(id)) {
                mContentManager.removeContents(i, 1);
                existingContentMap.remove(id);
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
        } else {
            // TODO(jianli): Create native view for ZeroStateSlice.
            View view = null;
            return new FeedListContentManager.NativeViewContent(sliceId, view);
        }
    }

    @Override
    public void navigateTab(String url) {
        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        loadUrlParams.setTransitionType(PageTransition.AUTO_BOOKMARK);
        mTabProvider.get().loadUrl(loadUrlParams);
        FeedStreamSurfaceJni.get().reportNavigationStarted(
                mNativeFeedStreamSurface, FeedStreamSurface.this, url, false /*inNewTab*/);
        mTabProvider.get().addObserver(new FeedTabNavigationObserver(false));
    }

    @Override
    public void navigateNewTab(String url) {
        Tab tab = mTabProvider.get();
        Tab newTab = mTabModelSelector.openNewTab(
                new LoadUrlParams(url), TabLaunchType.FROM_CHROME_UI, tab, tab.isIncognito());
        FeedStreamSurfaceJni.get().reportNavigationStarted(
                mNativeFeedStreamSurface, FeedStreamSurface.this, url, true /*inNewTab*/);
        newTab.addObserver(new FeedTabNavigationObserver(true));
    }

    @Override
    public void loadMore() {
        FeedStreamSurfaceJni.get().loadMore(mNativeFeedStreamSurface, FeedStreamSurface.this);
    }

    @Override
    public void processThereAndBackAgainData(byte[] data) {
        FeedStreamSurfaceJni.get().processThereAndBackAgain(
                mNativeFeedStreamSurface, FeedStreamSurface.this, data);
    }

    @Override
    public int requestDismissal() {
        // TODO(jianli): may need to pass parameters from UI.
        List<byte[]> serializedDataOperations = new ArrayList<byte[]>();
        // TODO(jianli): append data to serializedDataOperations.
        return FeedStreamSurfaceJni.get().executeEphemeralChange(
                mNativeFeedStreamSurface, FeedStreamSurface.this, serializedDataOperations);
    }

    @Override
    public void commitDismissal(int changeId) {
        FeedStreamSurfaceJni.get().commitEphemeralChange(
                mNativeFeedStreamSurface, FeedStreamSurface.this, changeId);
    }

    @Override
    public void discardDismissal(int changeId) {
        FeedStreamSurfaceJni.get().discardEphemeralChange(
                mNativeFeedStreamSurface, FeedStreamSurface.this, changeId);
    }

    /**
     * Informs that the surface is opened. We can request the initial set of content now. Once
     * the content is available, onStreamUpdated will be called.
     */
    public void surfaceOpened() {
        FeedStreamSurfaceJni.get().surfaceOpened(mNativeFeedStreamSurface, FeedStreamSurface.this);
    }

    /**
     * Informs that the surface is closed. Any cleanup can be performed now.
     */
    public void surfaceClosed() {
        FeedStreamSurfaceJni.get().surfaceClosed(mNativeFeedStreamSurface, FeedStreamSurface.this);
    }

    @NativeMethods
    interface Natives {
        long init(FeedStreamSurface caller);
        // TODO(jianli): Call this function at the appropriate time.
        void reportSliceViewed(
                long nativeFeedStreamSurface, FeedStreamSurface caller, String sliceId);
        void reportNavigationStarted(long nativeFeedStreamSurface, FeedStreamSurface caller,
                String url, boolean inNewTab);
        // TODO(jianli): Call this function at the appropriate time.
        void reportNavigationDone(long nativeFeedStreamSurface, FeedStreamSurface caller,
                String url, boolean inNewTab);
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
        void loadMore(long nativeFeedStreamSurface, FeedStreamSurface caller);
        void processThereAndBackAgain(
                long nativeFeedStreamSurface, FeedStreamSurface caller, byte[] data);
        int executeEphemeralChange(long nativeFeedStreamSurface, FeedStreamSurface caller,
                List<byte[]> serializedDataOperations);
        void commitEphemeralChange(
                long nativeFeedStreamSurface, FeedStreamSurface caller, int changeId);
        void discardEphemeralChange(
                long nativeFeedStreamSurface, FeedStreamSurface caller, int changeId);
        void surfaceOpened(long nativeFeedStreamSurface, FeedStreamSurface caller);
        void surfaceClosed(long nativeFeedStreamSurface, FeedStreamSurface caller);
    }
}
