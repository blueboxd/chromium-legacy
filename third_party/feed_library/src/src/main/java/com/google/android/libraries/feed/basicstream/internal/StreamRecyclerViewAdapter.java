// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal;

import static com.google.android.libraries.feed.basicstream.internal.viewholders.ViewHolderType.TYPE_CONTINUATION;
import static com.google.android.libraries.feed.basicstream.internal.viewholders.ViewHolderType.TYPE_HEADER;
import static com.google.android.libraries.feed.basicstream.internal.viewholders.ViewHolderType.TYPE_NO_CONTENT;
import static com.google.android.libraries.feed.basicstream.internal.viewholders.ViewHolderType.TYPE_ZERO_STATE;

import android.content.Context;
import android.support.annotation.VisibleForTesting;
import android.support.v7.util.DiffUtil;
import android.support.v7.util.DiffUtil.DiffResult;
import android.support.v7.util.ListUpdateCallback;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import com.google.android.libraries.feed.api.client.stream.Header;
import com.google.android.libraries.feed.api.client.stream.Stream.ContentChangedListener;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.stream.CardConfiguration;
import com.google.android.libraries.feed.basicstream.internal.drivers.HeaderDriver;
import com.google.android.libraries.feed.basicstream.internal.drivers.LeafFeatureDriver;
import com.google.android.libraries.feed.basicstream.internal.drivers.StreamDriver;
import com.google.android.libraries.feed.basicstream.internal.drivers.StreamDriver.StreamContentListener;
import com.google.android.libraries.feed.basicstream.internal.viewholders.ContinuationViewHolder;
import com.google.android.libraries.feed.basicstream.internal.viewholders.FeedViewHolder;
import com.google.android.libraries.feed.basicstream.internal.viewholders.HeaderViewHolder;
import com.google.android.libraries.feed.basicstream.internal.viewholders.NoContentViewHolder;
import com.google.android.libraries.feed.basicstream.internal.viewholders.PietViewHolder;
import com.google.android.libraries.feed.basicstream.internal.viewholders.ViewHolderType;
import com.google.android.libraries.feed.basicstream.internal.viewholders.ZeroStateViewHolder;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.piet.PietManager;
import com.google.android.libraries.feed.sharedstream.deepestcontenttracker.DeepestContentTracker;
import com.google.android.libraries.feed.sharedstream.piet.PietEventLogger;
import com.google.android.libraries.feed.sharedstream.publicapi.scroll.ScrollObservable;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;

/** A RecyclerView adapter which can show a list of views with Piet Stream features. */
public class StreamRecyclerViewAdapter
        extends RecyclerView.Adapter<FeedViewHolder> implements StreamContentListener {
    private static final String TAG = "StreamRecyclerViewAdapt";

    private final Context context;
    private final View viewport;
    private final CardConfiguration cardConfiguration;
    private final PietManager pietManager;
    private final Configuration configuration;
    private final List<LeafFeatureDriver> leafFeatureDrivers;
    private final List<HeaderDriver> headers;
    private final HashMap<FeedViewHolder, LeafFeatureDriver> boundViewHolderToLeafFeatureDriverMap;
    private final DeepestContentTracker deepestContentTracker;
    private final ContentChangedListener contentChangedListener;
    private final PietEventLogger eventLogger;

    private boolean streamContentVisible;
    private boolean shown;

    /*@Nullable*/ private StreamDriver streamDriver;
    private final ScrollObservable scrollObservable;

    // Suppress initialization warnings for calling setHasStableIds on RecyclerView.Adapter
    @SuppressWarnings("initialization")
    public StreamRecyclerViewAdapter(Context context, View viewport,
            CardConfiguration cardConfiguration, PietManager pietManager,
            DeepestContentTracker deepestContentTracker,
            ContentChangedListener contentChangedListener, ScrollObservable scrollObservable,
            Configuration configuration, PietEventLogger eventLogger) {
        this.context = context;
        this.viewport = viewport;
        this.cardConfiguration = cardConfiguration;
        this.pietManager = pietManager;
        this.contentChangedListener = contentChangedListener;
        this.configuration = configuration;
        headers = new ArrayList<>();
        leafFeatureDrivers = new ArrayList<>();
        setHasStableIds(true);
        boundViewHolderToLeafFeatureDriverMap = new HashMap<>();
        streamContentVisible = true;
        this.deepestContentTracker = deepestContentTracker;
        this.scrollObservable = scrollObservable;
        this.eventLogger = eventLogger;
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
                    configuration, parent.getContext(), frameLayout, cardConfiguration);
        } else if (viewType == TYPE_NO_CONTENT) {
            return new NoContentViewHolder(cardConfiguration, parent.getContext(), frameLayout);
        } else if (viewType == TYPE_ZERO_STATE) {
            return new ZeroStateViewHolder(parent.getContext(), frameLayout, cardConfiguration);
        }

        return new PietViewHolder(cardConfiguration, frameLayout, pietManager, scrollObservable,
                viewport, context, configuration, eventLogger);
    }

    @Override
    public void onBindViewHolder(FeedViewHolder viewHolder, int index) {
        Logger.d(TAG, "onBindViewHolder for index: %s", index);
        if (isHeader(index)) {
            Logger.d(TAG, "Binding header at index %s", index);
            HeaderDriver headerDriver = headers.get(index);
            headerDriver.bind(viewHolder);
            boundViewHolderToLeafFeatureDriverMap.put(viewHolder, headerDriver);
            return;
        }

        int streamIndex = positionToStreamIndex(index);

        Logger.d(TAG, "onBindViewHolder for stream index: %s", streamIndex);
        LeafFeatureDriver leafFeatureDriver = leafFeatureDrivers.get(streamIndex);

        deepestContentTracker.updateDeepestContentTracker(
                streamIndex, leafFeatureDriver.getContentId());

        leafFeatureDriver.bind(viewHolder);
        boundViewHolderToLeafFeatureDriverMap.put(viewHolder, leafFeatureDriver);
    }

    @Override
    public void onViewRecycled(FeedViewHolder viewHolder) {
        LeafFeatureDriver leafFeatureDriver = boundViewHolderToLeafFeatureDriverMap.get(viewHolder);

        if (leafFeatureDriver == null) {
            Logger.wtf(TAG, "Could not find driver for unbinding");
            return;
        }

        leafFeatureDriver.unbind();
        boundViewHolderToLeafFeatureDriverMap.remove(viewHolder);
    }

    @Override
    public int getItemCount() {
        int driverSize = streamContentVisible ? leafFeatureDrivers.size() : 0;
        return driverSize + headers.size();
    }

    @Override
    @ViewHolderType
    public int getItemViewType(int position) {
        if (isHeader(position)) {
            return TYPE_HEADER;
        }

        return leafFeatureDrivers.get(positionToStreamIndex(position)).getItemViewType();
    }

    @Override
    public long getItemId(int position) {
        if (isHeader(position)) {
            return headers.get(position).hashCode();
        }

        return leafFeatureDrivers.get(positionToStreamIndex(position)).itemId();
    }

    public void rebind() {
        for (LeafFeatureDriver driver : boundViewHolderToLeafFeatureDriverMap.values()) {
            driver.maybeRebind();
        }
    }

    @VisibleForTesting
    public List<LeafFeatureDriver> getLeafFeatureDrivers() {
        return leafFeatureDrivers;
    }

    private boolean isHeader(int position) {
        return position < headers.size();
    }

    private int positionToStreamIndex(int position) {
        return position - headers.size();
    }

    public void setHeaders(List<Header> newHeaders) {
        // TODO: Move header orchestration into separate class once header orchestration
        // logic is complex enough.
        List<Header> oldHeaders = new ArrayList<>();
        for (HeaderDriver headerDriver : headers) {
            oldHeaders.add(headerDriver.getHeader());
        }
        DiffResult diffResult =
                DiffUtil.calculateDiff(new HeaderDiffCallback(oldHeaders, newHeaders), true);
        diffResult.dispatchUpdatesTo(new HeaderListUpdateCallback(newHeaders));
    }

    public int getHeaderCount() {
        return headers.size();
    }

    public void setStreamContentVisible(boolean streamContentVisible) {
        if (this.streamContentVisible == streamContentVisible) {
            return;
        }
        this.streamContentVisible = streamContentVisible;

        if (leafFeatureDrivers.isEmpty()) {
            // Nothing to alter in RecyclerView if there is no leaf content.
            return;
        }

        if (streamContentVisible) {
            notifyItemRangeInserted(headers.size(), leafFeatureDrivers.size());
        } else {
            notifyItemRangeRemoved(headers.size(), leafFeatureDrivers.size());
        }
    }

    public void setDriver(StreamDriver newStreamDriver) {
        if (streamDriver != null) {
            streamDriver.setStreamContentListener(null);
        }

        notifyItemRangeRemoved(headers.size(), leafFeatureDrivers.size());
        leafFeatureDrivers.clear();

        // It is important that we get call getLeafFeatureDrivers before setting the content
        // listener. If this is done in the other order, it is possible that the StreamDriver
        // notifies us of something being added inside of the getLeafFeatureDrivers() call,
        // resulting in two copies of the LeafFeatureDriver being shown.
        leafFeatureDrivers.addAll(newStreamDriver.getLeafFeatureDrivers());
        newStreamDriver.setStreamContentListener(this);

        streamDriver = newStreamDriver;
        if (streamContentVisible) {
            notifyItemRangeInserted(headers.size(), leafFeatureDrivers.size());
        }

        newStreamDriver.maybeRestoreScroll();
    }

    @Override
    public void notifyContentsAdded(int index, List<LeafFeatureDriver> newFeatureDrivers) {
        if (newFeatureDrivers.size() == 0) {
            return;
        }

        leafFeatureDrivers.addAll(index, newFeatureDrivers);

        int insertionIndex = headers.size() + index;

        if (streamContentVisible) {
            notifyItemRangeInserted(insertionIndex, newFeatureDrivers.size());
        }
        maybeNotifyContentChanged();
    }

    @Override
    public void notifyContentRemoved(int index) {
        int removalIndex = headers.size() + index;

        leafFeatureDrivers.remove(index);
        deepestContentTracker.removeContentId(index);

        if (streamContentVisible) {
            notifyItemRemoved(removalIndex);
        }
        maybeNotifyContentChanged();
    }

    @Override
    public void notifyContentsCleared() {
        int itemCount = leafFeatureDrivers.size();
        leafFeatureDrivers.clear();

        if (streamContentVisible) {
            notifyItemRangeRemoved(headers.size(), itemCount);
        }
        maybeNotifyContentChanged();
    }

    public void onDestroy() {
        for (HeaderDriver header : headers) {
            header.unbind();
        }

        setHeaders(Collections.emptyList());
    }

    public void setShown(boolean shown) {
        this.shown = shown;
    }

    private void maybeNotifyContentChanged() {
        // If Stream is not shown on screen, host should be notified as animations will not run and
        // the host will not be notified through StreamItemAnimator.
        if (!shown) {
            contentChangedListener.onContentChanged();
        }
    }

    @VisibleForTesting
    void dismissHeader(Header header) {
        int indexToRemove = -1;
        for (int i = 0; i < headers.size(); i++) {
            if (headers.get(i).getHeader() == header) {
                indexToRemove = i;
            }
        }
        if (indexToRemove == -1) {
            Logger.w(TAG, "Header has already been removed.");
            return;
        }

        headers.remove(indexToRemove).onDestroy();
        notifyItemRemoved(indexToRemove);
        header.onDismissed();
    }

    @VisibleForTesting
    HeaderDriver createHeaderDriver(Header header) {
        return new HeaderDriver(header, () -> dismissHeader(header));
    }

    private static class HeaderDiffCallback extends DiffUtil.Callback {
        private final List<Header> oldHeaders;
        private final List<Header> newHeaders;

        private HeaderDiffCallback(List<Header> oldHeaders, List<Header> newHeaders) {
            this.oldHeaders = oldHeaders;
            this.newHeaders = newHeaders;
        }

        @Override
        public int getOldListSize() {
            return oldHeaders.size();
        }

        @Override
        public int getNewListSize() {
            return newHeaders.size();
        }

        @Override
        public boolean areItemsTheSame(int i, int i1) {
            return oldHeaders.get(i).getView().equals(newHeaders.get(i1).getView());
        }

        @Override
        public boolean areContentsTheSame(int i, int i1) {
            return oldHeaders.get(i).getView().equals(newHeaders.get(i1).getView());
        }
    }

    private class HeaderListUpdateCallback implements ListUpdateCallback {
        private final List<Header> newHeaders;

        public HeaderListUpdateCallback(List<Header> newHeaders) {
            this.newHeaders = newHeaders;
        }

        @Override
        public void onInserted(int i, int i1) {
            for (int index = i; index < newHeaders.size() && index < i + i1; index++) {
                HeaderDriver headerDriver = createHeaderDriver(newHeaders.get(index));
                headers.add(index, headerDriver);
            }
            notifyItemRangeInserted(i, i1);
        }

        @Override
        public void onRemoved(int i, int i1) {
            for (int index = i + i1 - 1; index >= 0 && index > i - i1; index--) {
                headers.get(index).onDestroy();
                headers.remove(index);
            }
            notifyItemRangeRemoved(i, i1);
        }

        @Override
        public void onMoved(int i, int i1) {
            notifyItemMoved(i, i1);
        }

        @Override
        public void onChanged(int i, int i1, /*@Nullable*/ Object o) {
            notifyItemRangeChanged(i, i1, o);
        }
    }
}
