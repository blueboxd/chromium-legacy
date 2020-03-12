// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal;

import static org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.ViewHolderType.TYPE_CONTINUATION;
import static org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.ViewHolderType.TYPE_HEADER;
import static org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.ViewHolderType.TYPE_NO_CONTENT;
import static org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.ViewHolderType.TYPE_ZERO_STATE;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.DiffUtil;
import androidx.recyclerview.widget.DiffUtil.DiffResult;
import androidx.recyclerview.widget.ListUpdateCallback;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.feed.library.api.client.stream.Header;
import org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ContentChangedListener;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.stream.CardConfiguration;
import org.chromium.chrome.browser.feed.library.basicstream.internal.drivers.HeaderDriver;
import org.chromium.chrome.browser.feed.library.basicstream.internal.drivers.LeafFeatureDriver;
import org.chromium.chrome.browser.feed.library.basicstream.internal.drivers.StreamDriver;
import org.chromium.chrome.browser.feed.library.basicstream.internal.drivers.StreamDriver.StreamContentListener;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.ContinuationViewHolder;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.FeedViewHolder;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.HeaderViewHolder;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.NoContentViewHolder;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.PietViewHolder;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.ViewHolderType;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.ZeroStateViewHolder;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.piet.PietManager;
import org.chromium.chrome.browser.feed.library.sharedstream.deepestcontenttracker.DeepestContentTracker;
import org.chromium.chrome.browser.feed.library.sharedstream.piet.PietEventLogger;
import org.chromium.chrome.browser.feed.library.sharedstream.publicapi.scroll.ScrollObservable;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;

/** A RecyclerView adapter which can show a list of views with Piet Stream features. */
public class StreamRecyclerViewAdapter
        extends RecyclerView.Adapter<FeedViewHolder> implements StreamContentListener {
    private static final String TAG = "StreamRecyclerViewAdapt";

    private final Context mContext;
    private final View mViewport;
    private final CardConfiguration mCardConfiguration;
    private final PietManager mPietManager;
    private final Configuration mConfiguration;
    private final List<LeafFeatureDriver> mLeafFeatureDrivers;
    private final List<HeaderDriver> mHeaders;
    private final HashMap<FeedViewHolder, LeafFeatureDriver> mBoundViewHolderToLeafFeatureDriverMap;
    private final DeepestContentTracker mDeepestContentTracker;
    private final ContentChangedListener mContentChangedListener;
    private final PietEventLogger mEventLogger;

    private boolean mStreamContentVisible;
    private boolean mShown;

    @Nullable
    private StreamDriver mStreamDriver;
    private final ScrollObservable mScrollObservable;

    // Suppress initialization warnings for calling setHasStableIds on RecyclerView.Adapter
    @SuppressWarnings("initialization")
    public StreamRecyclerViewAdapter(Context context, View viewport,
            CardConfiguration cardConfiguration, PietManager pietManager,
            DeepestContentTracker deepestContentTracker,
            ContentChangedListener contentChangedListener, ScrollObservable scrollObservable,
            Configuration configuration, PietEventLogger eventLogger) {
        this.mContext = context;
        this.mViewport = viewport;
        this.mCardConfiguration = cardConfiguration;
        this.mPietManager = pietManager;
        this.mContentChangedListener = contentChangedListener;
        this.mConfiguration = configuration;
        mHeaders = new ArrayList<>();
        mLeafFeatureDrivers = new ArrayList<>();
        setHasStableIds(true);
        mBoundViewHolderToLeafFeatureDriverMap = new HashMap<>();
        mStreamContentVisible = true;
        this.mDeepestContentTracker = deepestContentTracker;
        this.mScrollObservable = scrollObservable;
        this.mEventLogger = eventLogger;
    }

    @Override
    public FeedViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        FrameLayout frameLayout = new FrameLayout(parent.getContext());
        frameLayout.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        if (viewType == TYPE_HEADER) {
            return new HeaderViewHolder(frameLayout);
        } else if (viewType == TYPE_CONTINUATION) {
            return new ContinuationViewHolder(
                    mConfiguration, parent.getContext(), frameLayout, mCardConfiguration);
        } else if (viewType == TYPE_NO_CONTENT) {
            return new NoContentViewHolder(mCardConfiguration, parent.getContext(), frameLayout);
        } else if (viewType == TYPE_ZERO_STATE) {
            return new ZeroStateViewHolder(parent.getContext(), frameLayout, mCardConfiguration);
        }

        return new PietViewHolder(mCardConfiguration, frameLayout, mPietManager, mScrollObservable,
                mViewport, mContext, mConfiguration, mEventLogger);
    }

    @Override
    public void onBindViewHolder(FeedViewHolder viewHolder, int index) {
        Logger.d(TAG, "onBindViewHolder for index: %s", index);
        if (isHeader(index)) {
            Logger.d(TAG, "Binding header at index %s", index);
            HeaderDriver headerDriver = mHeaders.get(index);
            headerDriver.bind(viewHolder);
            mBoundViewHolderToLeafFeatureDriverMap.put(viewHolder, headerDriver);
            return;
        }

        int streamIndex = positionToStreamIndex(index);

        Logger.d(TAG, "onBindViewHolder for stream index: %s", streamIndex);
        LeafFeatureDriver leafFeatureDriver = mLeafFeatureDrivers.get(streamIndex);

        mDeepestContentTracker.updateDeepestContentTracker(
                streamIndex, leafFeatureDriver.getContentId());

        leafFeatureDriver.bind(viewHolder);
        mBoundViewHolderToLeafFeatureDriverMap.put(viewHolder, leafFeatureDriver);
    }

    @Override
    public void onViewRecycled(FeedViewHolder viewHolder) {
        LeafFeatureDriver leafFeatureDriver =
                mBoundViewHolderToLeafFeatureDriverMap.get(viewHolder);

        if (leafFeatureDriver == null) {
            Logger.wtf(TAG, "Could not find driver for unbinding");
            return;
        }

        leafFeatureDriver.unbind();
        mBoundViewHolderToLeafFeatureDriverMap.remove(viewHolder);
    }

    @Override
    public int getItemCount() {
        int driverSize = mStreamContentVisible ? mLeafFeatureDrivers.size() : 0;
        return driverSize + mHeaders.size();
    }

    @Override
    @ViewHolderType
    public int getItemViewType(int position) {
        if (isHeader(position)) {
            return TYPE_HEADER;
        }

        return mLeafFeatureDrivers.get(positionToStreamIndex(position)).getItemViewType();
    }

    @Override
    public long getItemId(int position) {
        if (isHeader(position)) {
            return mHeaders.get(position).hashCode();
        }

        return mLeafFeatureDrivers.get(positionToStreamIndex(position)).itemId();
    }

    public void rebind() {
        for (LeafFeatureDriver driver : mBoundViewHolderToLeafFeatureDriverMap.values()) {
            driver.maybeRebind();
        }
    }

    @VisibleForTesting
    public List<LeafFeatureDriver> getLeafFeatureDrivers() {
        return mLeafFeatureDrivers;
    }

    private boolean isHeader(int position) {
        return position < mHeaders.size();
    }

    private int positionToStreamIndex(int position) {
        return position - mHeaders.size();
    }

    public void setHeaders(List<Header> newHeaders) {
        // TODO: Move header orchestration into separate class once header orchestration
        // logic is complex enough.
        List<Header> oldHeaders = new ArrayList<>();
        for (HeaderDriver headerDriver : mHeaders) {
            oldHeaders.add(headerDriver.getHeader());
        }
        DiffResult diffResult =
                DiffUtil.calculateDiff(new HeaderDiffCallback(oldHeaders, newHeaders), true);
        diffResult.dispatchUpdatesTo(new HeaderListUpdateCallback(newHeaders));
    }

    public int getHeaderCount() {
        return mHeaders.size();
    }

    public void setStreamContentVisible(boolean streamContentVisible) {
        if (this.mStreamContentVisible == streamContentVisible) {
            return;
        }
        this.mStreamContentVisible = streamContentVisible;

        if (mLeafFeatureDrivers.isEmpty()) {
            // Nothing to alter in RecyclerView if there is no leaf content.
            return;
        }

        if (streamContentVisible) {
            notifyItemRangeInserted(mHeaders.size(), mLeafFeatureDrivers.size());
        } else {
            notifyItemRangeRemoved(mHeaders.size(), mLeafFeatureDrivers.size());
        }
    }

    public void setDriver(StreamDriver newStreamDriver) {
        if (mStreamDriver != null) {
            mStreamDriver.setStreamContentListener(null);
        }

        notifyItemRangeRemoved(mHeaders.size(), mLeafFeatureDrivers.size());
        mLeafFeatureDrivers.clear();

        // It is important that we get call getLeafFeatureDrivers before setting the content
        // listener. If this is done in the other order, it is possible that the StreamDriver
        // notifies us of something being added inside of the getLeafFeatureDrivers() call,
        // resulting in two copies of the LeafFeatureDriver being shown.
        mLeafFeatureDrivers.addAll(newStreamDriver.getLeafFeatureDrivers());
        newStreamDriver.setStreamContentListener(this);

        mStreamDriver = newStreamDriver;
        if (mStreamContentVisible) {
            notifyItemRangeInserted(mHeaders.size(), mLeafFeatureDrivers.size());
        }

        newStreamDriver.maybeRestoreScroll();
    }

    @Override
    public void notifyContentsAdded(int index, List<LeafFeatureDriver> newFeatureDrivers) {
        if (newFeatureDrivers.size() == 0) {
            return;
        }

        mLeafFeatureDrivers.addAll(index, newFeatureDrivers);

        int insertionIndex = mHeaders.size() + index;

        if (mStreamContentVisible) {
            notifyItemRangeInserted(insertionIndex, newFeatureDrivers.size());
        }
        maybeNotifyContentChanged();
    }

    @Override
    public void notifyContentRemoved(int index) {
        int removalIndex = mHeaders.size() + index;

        mLeafFeatureDrivers.remove(index);
        mDeepestContentTracker.removeContentId(index);

        if (mStreamContentVisible) {
            notifyItemRemoved(removalIndex);
        }
        maybeNotifyContentChanged();
    }

    @Override
    public void notifyContentsCleared() {
        int itemCount = mLeafFeatureDrivers.size();
        mLeafFeatureDrivers.clear();

        if (mStreamContentVisible) {
            notifyItemRangeRemoved(mHeaders.size(), itemCount);
        }
        maybeNotifyContentChanged();
    }

    public void onDestroy() {
        for (HeaderDriver header : mHeaders) {
            header.unbind();
        }

        setHeaders(Collections.emptyList());
    }

    public void setShown(boolean shown) {
        this.mShown = shown;
    }

    private void maybeNotifyContentChanged() {
        // If Stream is not shown on screen, host should be notified as animations will not run and
        // the host will not be notified through StreamItemAnimator.
        if (!mShown) {
            mContentChangedListener.onContentChanged();
        }
    }

    @VisibleForTesting
    void dismissHeader(Header header) {
        int indexToRemove = -1;
        for (int i = 0; i < mHeaders.size(); i++) {
            if (mHeaders.get(i).getHeader() == header) {
                indexToRemove = i;
            }
        }
        if (indexToRemove == -1) {
            Logger.w(TAG, "Header has already been removed.");
            return;
        }

        mHeaders.remove(indexToRemove).onDestroy();
        notifyItemRemoved(indexToRemove);
        header.onDismissed();
    }

    @VisibleForTesting
    HeaderDriver createHeaderDriver(Header header) {
        return new HeaderDriver(header, () -> dismissHeader(header));
    }

    private static class HeaderDiffCallback extends DiffUtil.Callback {
        private final List<Header> mOldHeaders;
        private final List<Header> mNewHeaders;

        private HeaderDiffCallback(List<Header> oldHeaders, List<Header> newHeaders) {
            this.mOldHeaders = oldHeaders;
            this.mNewHeaders = newHeaders;
        }

        @Override
        public int getOldListSize() {
            return mOldHeaders.size();
        }

        @Override
        public int getNewListSize() {
            return mNewHeaders.size();
        }

        @Override
        public boolean areItemsTheSame(int i, int i1) {
            return mOldHeaders.get(i).getView().equals(mNewHeaders.get(i1).getView());
        }

        @Override
        public boolean areContentsTheSame(int i, int i1) {
            return mOldHeaders.get(i).getView().equals(mNewHeaders.get(i1).getView());
        }
    }

    private class HeaderListUpdateCallback implements ListUpdateCallback {
        private final List<Header> mNewHeaders;

        public HeaderListUpdateCallback(List<Header> newHeaders) {
            this.mNewHeaders = newHeaders;
        }

        @Override
        public void onInserted(int i, int i1) {
            for (int index = i; index < mNewHeaders.size() && index < i + i1; index++) {
                HeaderDriver headerDriver = createHeaderDriver(mNewHeaders.get(index));
                mHeaders.add(index, headerDriver);
            }
            notifyItemRangeInserted(i, i1);
        }

        @Override
        public void onRemoved(int i, int i1) {
            for (int index = i + i1 - 1; index >= 0 && index > i - i1; index--) {
                mHeaders.get(index).onDestroy();
                mHeaders.remove(index);
            }
            notifyItemRangeRemoved(i, i1);
        }

        @Override
        public void onMoved(int i, int i1) {
            notifyItemMoved(i, i1);
        }

        @Override
        public void onChanged(int i, int i1, @Nullable Object o) {
            notifyItemRangeChanged(i, i1, o);
        }
    }
}
