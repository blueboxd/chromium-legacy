// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;
import android.widget.LinearLayout;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.Lists;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Supplier;
import org.chromium.chrome.browser.feed.library.api.client.stream.Header;
import org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ContentChangedListener;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.stream.CardConfiguration;
import org.chromium.chrome.browser.feed.library.basicstream.internal.drivers.ContentDriver;
import org.chromium.chrome.browser.feed.library.basicstream.internal.drivers.HeaderDriver;
import org.chromium.chrome.browser.feed.library.basicstream.internal.drivers.LeafFeatureDriver;
import org.chromium.chrome.browser.feed.library.basicstream.internal.drivers.StreamDriver;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.ContinuationViewHolder;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.FeedViewHolder;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.NoContentViewHolder;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.PietViewHolder;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.ViewHolderType;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.ZeroStateViewHolder;
import org.chromium.chrome.browser.feed.library.piet.FrameAdapter;
import org.chromium.chrome.browser.feed.library.piet.PietManager;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler;
import org.chromium.chrome.browser.feed.library.piet.host.EventLogger;
import org.chromium.chrome.browser.feed.library.sharedstream.deepestcontenttracker.DeepestContentTracker;
import org.chromium.chrome.browser.feed.library.sharedstream.piet.PietEventLogger;
import org.chromium.chrome.browser.feed.library.sharedstream.publicapi.scroll.ScrollObservable;
import org.chromium.chrome.browser.feed.library.sharedstream.ui.R;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Collections;
import java.util.List;

/** Tests for {@link StreamRecyclerViewAdapter}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StreamRecyclerViewAdapterTest {
    private static final int HEADER_COUNT = 2;
    private static final long FEATURE_DRIVER_1_ID = 123;
    private static final long FEATURE_DRIVER_2_ID = 321;
    private static final String INITIAL_CONTENT_ID = "INITIAL_CONTENT_ID";
    private static final String FEATURE_DRIVER_1_CONTENT_ID = "FEATURE_DRIVER_1_CONTENT_ID";
    private static final String FEATURE_DRIVER_2_CONTENT_ID = "FEATURE_DRIVER_2_CONTENT_ID";
    private static final Configuration CONFIGURATION = new Configuration.Builder().build();

    @Mock
    private CardConfiguration mCardConfiguration;
    @Mock
    private ContentChangedListener mContentChangedListener;
    @Mock
    private PietManager mPietManager;
    @Mock
    private FrameAdapter mFrameAdapter;
    @Mock
    private StreamDriver mDriver;
    @Mock
    private ContentDriver mInitialFeatureDriver;
    @Mock
    private ContentDriver mFeatureDriver1;
    @Mock
    private ContentDriver mFeatureDriver2;
    @Mock
    private HeaderDriver mHeaderDriver1;
    @Mock
    private HeaderDriver mHeaderDriver2;
    @Mock
    private DeepestContentTracker mDeepestContentTracker;
    @Mock
    private Header mHeader1;
    @Mock
    private Header mHeader2;
    @Mock
    private ScrollObservable mScrollObservable;
    @Mock
    private PietEventLogger mPietEventLogger;

    private Context mContext;
    private LinearLayout mFrameContainer;
    private StreamRecyclerViewAdapter mStreamRecyclerViewAdapter;
    private List<Header> mHeaders;
    private StreamAdapterObserver mStreamAdapterObserver;

    @Before
    public void setUp() {
        initMocks(this);

        mContext = Robolectric.buildActivity(Activity.class).get();
        mContext.setTheme(R.style.Light);
        mFrameContainer = new LinearLayout(mContext);

        when(mPietManager.createPietFrameAdapter(ArgumentMatchers.<Supplier<ViewGroup>>any(),
                     any(ActionHandler.class), any(EventLogger.class), eq(mContext)))
                .thenReturn(mFrameAdapter);
        when(mFrameAdapter.getFrameContainer()).thenReturn(mFrameContainer);

        when(mFeatureDriver1.itemId()).thenReturn(FEATURE_DRIVER_1_ID);
        when(mFeatureDriver2.itemId()).thenReturn(FEATURE_DRIVER_2_ID);
        when(mInitialFeatureDriver.getContentId()).thenReturn(INITIAL_CONTENT_ID);

        mHeaders = ImmutableList.of(mHeader1, mHeader2);

        when(mFeatureDriver1.getContentId()).thenReturn(FEATURE_DRIVER_1_CONTENT_ID);
        when(mFeatureDriver2.getContentId()).thenReturn(FEATURE_DRIVER_2_CONTENT_ID);
        when(mHeaderDriver1.getHeader()).thenReturn(mHeader1);
        when(mHeaderDriver2.getHeader()).thenReturn(mHeader2);

        mStreamRecyclerViewAdapter = new StreamRecyclerViewAdapter(mContext,
                new RecyclerView(mContext), mCardConfiguration, mPietManager,
                mDeepestContentTracker, mContentChangedListener, mScrollObservable, CONFIGURATION,
                mPietEventLogger);
        mStreamRecyclerViewAdapter.setHeaders(mHeaders);

        when(mDriver.getLeafFeatureDrivers()).thenReturn(Lists.newArrayList(mInitialFeatureDriver));
        mStreamRecyclerViewAdapter.setDriver(mDriver);

        mStreamAdapterObserver = new StreamAdapterObserver();
        mStreamRecyclerViewAdapter.registerAdapterDataObserver(mStreamAdapterObserver);
    }

    @Test
    public void testCreateViewHolderPiet() {
        FrameLayout parent = new FrameLayout(mContext);
        ViewHolder viewHolder =
                mStreamRecyclerViewAdapter.onCreateViewHolder(parent, ViewHolderType.TYPE_CARD);

        FrameLayout cardView = getCardView(viewHolder);
        assertThat(cardView.getChildAt(0)).isEqualTo(mFrameContainer);
    }

    @Test
    public void testCreateViewHolderContinuation() {
        FrameLayout parent = new FrameLayout(mContext);
        ViewHolder viewHolder = mStreamRecyclerViewAdapter.onCreateViewHolder(
                parent, ViewHolderType.TYPE_CONTINUATION);
        FrameLayout viewHolderFrameLayout = getCardView(viewHolder);

        assertThat(viewHolder).isInstanceOf(ContinuationViewHolder.class);
        assertThat(viewHolderFrameLayout.getLayoutParams().height)
                .isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(viewHolderFrameLayout.getLayoutParams().width)
                .isEqualTo(LayoutParams.MATCH_PARENT);
    }

    @Test
    public void testCreateViewHolderNoContent() {
        FrameLayout parent = new FrameLayout(mContext);
        ViewHolder viewHolder = mStreamRecyclerViewAdapter.onCreateViewHolder(
                parent, ViewHolderType.TYPE_NO_CONTENT);
        FrameLayout viewHolderFrameLayout = getCardView(viewHolder);

        assertThat(viewHolder).isInstanceOf(NoContentViewHolder.class);
        assertThat(viewHolderFrameLayout.getLayoutParams().height)
                .isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(viewHolderFrameLayout.getLayoutParams().width)
                .isEqualTo(LayoutParams.MATCH_PARENT);
    }

    @Test
    public void testCreateViewHolderZeroState() {
        FrameLayout parent = new FrameLayout(mContext);
        ViewHolder viewHolder = mStreamRecyclerViewAdapter.onCreateViewHolder(
                parent, ViewHolderType.TYPE_ZERO_STATE);
        FrameLayout viewHolderFrameLayout = getCardView(viewHolder);

        assertThat(viewHolder).isInstanceOf(ZeroStateViewHolder.class);
        assertThat(viewHolderFrameLayout.getLayoutParams().height)
                .isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(viewHolderFrameLayout.getLayoutParams().width)
                .isEqualTo(LayoutParams.MATCH_PARENT);
    }

    @Test
    public void testOnBindViewHolder() {
        FrameLayout parent = new FrameLayout(mContext);
        FeedViewHolder viewHolder =
                mStreamRecyclerViewAdapter.onCreateViewHolder(parent, ViewHolderType.TYPE_CARD);

        mStreamRecyclerViewAdapter.onBindViewHolder(viewHolder, getContentBindingIndex(0));

        verify(mInitialFeatureDriver).bind(viewHolder);
    }

    @Test
    public void testOnViewRecycled() {
        PietViewHolder viewHolder = mock(PietViewHolder.class);

        mStreamRecyclerViewAdapter.onBindViewHolder(viewHolder, getContentBindingIndex(0));

        // Make sure the content model is bound
        verify(mInitialFeatureDriver).bind(viewHolder);

        mStreamRecyclerViewAdapter.onViewRecycled(viewHolder);

        verify(mInitialFeatureDriver).unbind();
    }

    @Test
    public void testSetDriver_initialContentModels() {
        // streamRecyclerViewAdapter.setDriver(driver) is called in setup()
        assertThat(mStreamRecyclerViewAdapter.getLeafFeatureDrivers())
                .containsExactly(mInitialFeatureDriver);
    }

    @Test
    public void testSetDriver_newDriver() {
        StreamDriver newDriver = mock(StreamDriver.class);
        List<LeafFeatureDriver> newFeatureDrivers =
                Lists.newArrayList(mFeatureDriver1, mFeatureDriver2);

        when(newDriver.getLeafFeatureDrivers()).thenReturn(newFeatureDrivers);

        mStreamRecyclerViewAdapter.setDriver(newDriver);

        verify(mDriver).setStreamContentListener(null);
        assertThat(mStreamRecyclerViewAdapter.getItemId(getContentBindingIndex(0)))
                .isEqualTo(FEATURE_DRIVER_1_ID);
        assertThat(mStreamRecyclerViewAdapter.getItemId(getContentBindingIndex(1)))
                .isEqualTo(FEATURE_DRIVER_2_ID);

        InOrder inOrder = Mockito.inOrder(newDriver);

        // getLeafFeatureDrivers() needs to be called before setting the listener, otherwise the
        // adapter could be notified of a child and then given it in getLeafFeatureDrivers().
        inOrder.verify(newDriver).getLeafFeatureDrivers();
        inOrder.verify(newDriver).setStreamContentListener(mStreamRecyclerViewAdapter);
        inOrder.verify(newDriver).maybeRestoreScroll();
    }

    @Test
    public void testNotifyContentsAdded_endOfList() {
        mStreamRecyclerViewAdapter.notifyContentsAdded(
                1, Lists.newArrayList(mFeatureDriver1, mFeatureDriver2));
        assertThat(mStreamRecyclerViewAdapter.getLeafFeatureDrivers())
                .containsAtLeast(mInitialFeatureDriver, mFeatureDriver1, mFeatureDriver2);
    }

    @Test
    public void testNotifyContentsAdded_startOfList() {
        mStreamRecyclerViewAdapter.notifyContentsAdded(
                0, Lists.newArrayList(mFeatureDriver1, mFeatureDriver2));
        assertThat(mStreamRecyclerViewAdapter.getLeafFeatureDrivers())
                .containsAtLeast(mFeatureDriver1, mFeatureDriver2, mInitialFeatureDriver);
    }

    @Test
    public void testNotifyContentsRemoved() {
        mStreamRecyclerViewAdapter.notifyContentRemoved(0);
        assertThat(mStreamRecyclerViewAdapter.getLeafFeatureDrivers()).isEmpty();
        verify(mDeepestContentTracker).removeContentId(0);
    }

    @Test
    public void testNotifyContentsAdded_notifiesListener() {
        mStreamRecyclerViewAdapter.setShown(false);
        mStreamRecyclerViewAdapter.notifyContentsAdded(
                0, Lists.newArrayList(mFeatureDriver1, mFeatureDriver2));

        assertThat(mStreamAdapterObserver.mInsertedStart).isEqualTo(mHeaders.size());
        assertThat(mStreamAdapterObserver.mInsertedCount).isEqualTo(2);
        verify(mContentChangedListener).onContentChanged();
    }

    @Test
    public void testNotifyContentsAdded_doesNotNotifiesContentChangedListener_ifShownTrue() {
        mStreamRecyclerViewAdapter.setShown(true);
        mStreamRecyclerViewAdapter.notifyContentsAdded(
                0, Lists.newArrayList(mFeatureDriver1, mFeatureDriver2));

        verify(mContentChangedListener, never()).onContentChanged();
    }

    @Test
    public void testNotifyContentsAdded_streamVisibleFalse() {
        mStreamRecyclerViewAdapter.setStreamContentVisible(false);
        mStreamAdapterObserver.reset();

        mStreamRecyclerViewAdapter.notifyContentsAdded(
                0, Lists.newArrayList(mFeatureDriver1, mFeatureDriver2));
        assertThat(mStreamAdapterObserver.mInsertedStart).isEqualTo(0);
        assertThat(mStreamAdapterObserver.mInsertedCount).isEqualTo(0);
    }

    @Test
    public void testNotifyContentsRemoved_notifiesListener() {
        mStreamRecyclerViewAdapter.setShown(false);
        mStreamRecyclerViewAdapter.notifyContentRemoved(0);

        assertThat(mStreamAdapterObserver.mRemovedStart).isEqualTo(mHeaders.size());
        assertThat(mStreamAdapterObserver.mRemovedCount).isEqualTo(1);
        verify(mContentChangedListener).onContentChanged();
    }

    @Test
    public void testNotifyContentsRemoved_doesNotNotifiesContentChangedListener_ifShownTrue() {
        mStreamRecyclerViewAdapter.setShown(true);
        mStreamRecyclerViewAdapter.notifyContentRemoved(0);

        verify(mContentChangedListener, never()).onContentChanged();
    }

    @Test
    public void testNotifyContentsRemoved_streamVisibleFalse() {
        mStreamRecyclerViewAdapter.setStreamContentVisible(false);
        mStreamAdapterObserver.reset();

        mStreamRecyclerViewAdapter.notifyContentRemoved(0);
        assertThat(mStreamAdapterObserver.mRemovedStart).isEqualTo(0);
        assertThat(mStreamAdapterObserver.mRemovedCount).isEqualTo(0);
    }

    @Test
    public void testNotifyContentsCleared_notifiesListener() {
        mStreamRecyclerViewAdapter.setShown(false);
        mStreamRecyclerViewAdapter.notifyContentsCleared();

        assertThat(mStreamAdapterObserver.mRemovedStart).isEqualTo(mHeaders.size());
        assertThat(mStreamAdapterObserver.mRemovedCount).isEqualTo(1);
        verify(mContentChangedListener).onContentChanged();
    }

    @Test
    public void testNotifyContentsCleared_doesNotNotifiesContentChangedListener_ifShownTrue() {
        mStreamRecyclerViewAdapter.setShown(true);
        mStreamRecyclerViewAdapter.notifyContentsCleared();

        verify(mContentChangedListener, never()).onContentChanged();
    }

    @Test
    public void testNotifyContentsCleared_streamVisibleFalse() {
        mStreamRecyclerViewAdapter.setStreamContentVisible(false);
        mStreamAdapterObserver.reset();

        mStreamRecyclerViewAdapter.notifyContentsCleared();
        assertThat(mStreamAdapterObserver.mRemovedStart).isEqualTo(0);
        assertThat(mStreamAdapterObserver.mRemovedCount).isEqualTo(0);
    }

    @Test
    public void testOnDestroy() {
        mStreamRecyclerViewAdapter = new StreamRecyclerViewAdapter(mContext,
                new RecyclerView(mContext), mCardConfiguration, mPietManager,
                mDeepestContentTracker, mContentChangedListener, mScrollObservable, CONFIGURATION,
                mPietEventLogger) {
            @Override
            HeaderDriver createHeaderDriver(Header header) {
                if (header == mHeaders.get(0)) {
                    return mHeaderDriver1;
                }
                return mHeaderDriver2;
            }
        };
        mStreamRecyclerViewAdapter.setHeaders(mHeaders);
        mStreamRecyclerViewAdapter.onDestroy();

        verify(mHeaderDriver1).unbind();
        verify(mHeaderDriver1).onDestroy();
        verify(mHeaderDriver2).unbind();
        verify(mHeaderDriver2).onDestroy();
    }

    @Test
    public void testSetHeaders_destroysPreviousHeaderDrivers() {
        mStreamRecyclerViewAdapter = new StreamRecyclerViewAdapter(mContext,
                new RecyclerView(mContext), mCardConfiguration, mPietManager,
                mDeepestContentTracker, mContentChangedListener, mScrollObservable, CONFIGURATION,
                mPietEventLogger) {
            @Override
            HeaderDriver createHeaderDriver(Header header) {
                if (header == mHeaders.get(0)) {
                    return mHeaderDriver1;
                }
                return mHeaderDriver2;
            }
        };
        mStreamRecyclerViewAdapter.setHeaders(mHeaders);

        mStreamRecyclerViewAdapter.setHeaders(Collections.emptyList());

        verify(mHeaderDriver1).onDestroy();
        verify(mHeaderDriver2).onDestroy();
    }

    @Test
    public void testGetItemCount() {
        assertThat(mStreamRecyclerViewAdapter.getItemCount()).isEqualTo(mHeaders.size() + 1);
    }

    @Test
    public void testGetItemCount_streamVisibleFalse() {
        mStreamRecyclerViewAdapter.setStreamContentVisible(false);
        assertThat(mStreamRecyclerViewAdapter.getItemCount()).isEqualTo(mHeaders.size());
    }

    @Test
    public void setStreamContentVisible_hidesContent() {
        mStreamRecyclerViewAdapter.setStreamContentVisible(false);
        assertThat(mStreamAdapterObserver.mRemovedStart).isEqualTo(mHeaders.size());
        assertThat(mStreamAdapterObserver.mRemovedCount).isEqualTo(1);
    }

    @Test
    public void setStreamContentVisible_hidesContent_doubleCall() {
        mStreamRecyclerViewAdapter.setStreamContentVisible(false);

        mStreamAdapterObserver.reset();
        mStreamRecyclerViewAdapter.setStreamContentVisible(false);

        // Nothing additional should get called
        assertThat(mStreamAdapterObserver.mRemovedStart).isEqualTo(0);
        assertThat(mStreamAdapterObserver.mRemovedCount).isEqualTo(0);
    }

    @Test
    public void setStreamContentVisible_showsContent() {
        mStreamRecyclerViewAdapter.setStreamContentVisible(false);
        mStreamAdapterObserver.reset();

        mStreamRecyclerViewAdapter.setStreamContentVisible(true);
        assertThat(mStreamAdapterObserver.mInsertedStart).isEqualTo(mHeaders.size());
        assertThat(mStreamAdapterObserver.mInsertedCount).isEqualTo(1);
    }

    @Test
    public void setStreamContentVisible_showsContent_doubleCall() {
        mStreamRecyclerViewAdapter.setStreamContentVisible(false);
        mStreamAdapterObserver.reset();

        mStreamRecyclerViewAdapter.setStreamContentVisible(true);

        mStreamAdapterObserver.reset();
        mStreamRecyclerViewAdapter.setStreamContentVisible(true);

        // Nothing additional should get called
        assertThat(mStreamAdapterObserver.mInsertedStart).isEqualTo(0);
        assertThat(mStreamAdapterObserver.mInsertedCount).isEqualTo(0);
    }

    @Test
    public void testOnBindViewHolder_updatesDeepestViewedContent() {
        PietViewHolder viewHolder = mock(PietViewHolder.class);

        mStreamRecyclerViewAdapter.onBindViewHolder(viewHolder, getContentBindingIndex(0));

        verify(mDeepestContentTracker).updateDeepestContentTracker(0, INITIAL_CONTENT_ID);
    }

    @Test
    public void testDismissHeader() {
        mStreamRecyclerViewAdapter = new StreamRecyclerViewAdapter(mContext,
                new RecyclerView(mContext), mCardConfiguration, mPietManager,
                mDeepestContentTracker, mContentChangedListener, mScrollObservable, CONFIGURATION,
                mPietEventLogger) {
            @Override
            HeaderDriver createHeaderDriver(Header header) {
                if (header == mHeaders.get(0)) {
                    return mHeaderDriver1;
                }
                return mHeaderDriver2;
            }
        };
        mStreamRecyclerViewAdapter.setHeaders(mHeaders);
        assertThat(mStreamRecyclerViewAdapter.getItemCount()).isEqualTo(2);

        mStreamRecyclerViewAdapter.dismissHeader(mHeader1);

        assertThat(mStreamRecyclerViewAdapter.getItemCount()).isEqualTo(1);
        verify(mHeader1).onDismissed();
        verify(mHeaderDriver1).onDestroy();
    }

    @Test
    public void testDismissHeader_whenNoHeaders() {
        mStreamRecyclerViewAdapter.setHeaders(Collections.emptyList());
        reset(mHeader1);
        assertThat(mStreamRecyclerViewAdapter.getItemCount()).isEqualTo(1);

        mStreamRecyclerViewAdapter.dismissHeader(mHeader1);

        assertThat(mStreamRecyclerViewAdapter.getItemCount()).isEqualTo(1);
        verify(mHeader1, never()).onDismissed();
    }

    @Test
    public void testRebind() {
        PietViewHolder viewHolder = mock(PietViewHolder.class);
        mStreamRecyclerViewAdapter.onBindViewHolder(viewHolder, getContentBindingIndex(0));
        mStreamRecyclerViewAdapter.rebind();
        verify(mInitialFeatureDriver).maybeRebind();
    }

    private FrameLayout getCardView(ViewHolder viewHolder) {
        return (FrameLayout) viewHolder.itemView;
    }

    private int getContentBindingIndex(int index) {
        return HEADER_COUNT + index;
    }

    private class StreamAdapterObserver extends RecyclerView.AdapterDataObserver {
        private int mInsertedStart;
        private int mInsertedCount;
        private int mRemovedStart;
        private int mRemovedCount;

        @Override
        public void onItemRangeInserted(int positionStart, int itemCount) {
            mInsertedStart = positionStart;
            mInsertedCount = itemCount;
        }

        @Override
        public void onItemRangeRemoved(int positionStart, int itemCount) {
            mRemovedStart = positionStart;
            mRemovedCount = itemCount;
        }

        public void reset() {
            mInsertedStart = 0;
            mInsertedCount = 0;
            mRemovedStart = 0;
            mRemovedCount = 0;
        }
    }
}
