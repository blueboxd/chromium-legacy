// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal;

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

import com.google.android.libraries.feed.api.client.stream.Header;
import com.google.android.libraries.feed.api.client.stream.Stream.ContentChangedListener;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.stream.CardConfiguration;
import com.google.android.libraries.feed.basicstream.internal.drivers.ContentDriver;
import com.google.android.libraries.feed.basicstream.internal.drivers.HeaderDriver;
import com.google.android.libraries.feed.basicstream.internal.drivers.LeafFeatureDriver;
import com.google.android.libraries.feed.basicstream.internal.drivers.StreamDriver;
import com.google.android.libraries.feed.basicstream.internal.viewholders.ContinuationViewHolder;
import com.google.android.libraries.feed.basicstream.internal.viewholders.FeedViewHolder;
import com.google.android.libraries.feed.basicstream.internal.viewholders.NoContentViewHolder;
import com.google.android.libraries.feed.basicstream.internal.viewholders.PietViewHolder;
import com.google.android.libraries.feed.basicstream.internal.viewholders.ViewHolderType;
import com.google.android.libraries.feed.basicstream.internal.viewholders.ZeroStateViewHolder;
import com.google.android.libraries.feed.common.functional.Supplier;
import com.google.android.libraries.feed.piet.FrameAdapter;
import com.google.android.libraries.feed.piet.PietManager;
import com.google.android.libraries.feed.piet.host.ActionHandler;
import com.google.android.libraries.feed.piet.host.EventLogger;
import com.google.android.libraries.feed.sharedstream.deepestcontenttracker.DeepestContentTracker;
import com.google.android.libraries.feed.sharedstream.piet.PietEventLogger;
import com.google.android.libraries.feed.sharedstream.publicapi.scroll.ScrollObservable;
import com.google.android.libraries.feed.sharedstream.ui.R;
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
import org.robolectric.RobolectricTestRunner;

import java.util.Collections;
import java.util.List;

/** Tests for {@link StreamRecyclerViewAdapter}. */
@RunWith(RobolectricTestRunner.class)
public class StreamRecyclerViewAdapterTest {
    private static final int HEADER_COUNT = 2;
    private static final long FEATURE_DRIVER_1_ID = 123;
    private static final long FEATURE_DRIVER_2_ID = 321;
    private static final String INITIAL_CONTENT_ID = "INITIAL_CONTENT_ID";
    private static final String FEATURE_DRIVER_1_CONTENT_ID = "FEATURE_DRIVER_1_CONTENT_ID";
    private static final String FEATURE_DRIVER_2_CONTENT_ID = "FEATURE_DRIVER_2_CONTENT_ID";
    private static final Configuration CONFIGURATION = new Configuration.Builder().build();

    @Mock
    private CardConfiguration cardConfiguration;
    @Mock
    private ContentChangedListener contentChangedListener;
    @Mock
    private PietManager pietManager;
    @Mock
    private FrameAdapter frameAdapter;
    @Mock
    private StreamDriver driver;
    @Mock
    private ContentDriver initialFeatureDriver;
    @Mock
    private ContentDriver featureDriver1;
    @Mock
    private ContentDriver featureDriver2;
    @Mock
    private HeaderDriver headerDriver1;
    @Mock
    private HeaderDriver headerDriver2;
    @Mock
    private DeepestContentTracker deepestContentTracker;
    @Mock
    private Header header1;
    @Mock
    private Header header2;
    @Mock
    private ScrollObservable scrollObservable;
    @Mock
    private PietEventLogger pietEventLogger;

    private Context context;
    private LinearLayout frameContainer;
    private StreamRecyclerViewAdapter streamRecyclerViewAdapter;
    private List<Header> headers;
    private StreamAdapterObserver streamAdapterObserver;

    @Before
    public void setUp() {
        initMocks(this);

        context = Robolectric.buildActivity(Activity.class).get();
        context.setTheme(R.style.Light);
        frameContainer = new LinearLayout(context);

        when(pietManager.createPietFrameAdapter(ArgumentMatchers.<Supplier<ViewGroup>>any(),
                     any(ActionHandler.class), any(EventLogger.class), eq(context)))
                .thenReturn(frameAdapter);
        when(frameAdapter.getFrameContainer()).thenReturn(frameContainer);

        when(featureDriver1.itemId()).thenReturn(FEATURE_DRIVER_1_ID);
        when(featureDriver2.itemId()).thenReturn(FEATURE_DRIVER_2_ID);
        when(initialFeatureDriver.getContentId()).thenReturn(INITIAL_CONTENT_ID);

        headers = ImmutableList.of(header1, header2);

        when(featureDriver1.getContentId()).thenReturn(FEATURE_DRIVER_1_CONTENT_ID);
        when(featureDriver2.getContentId()).thenReturn(FEATURE_DRIVER_2_CONTENT_ID);
        when(headerDriver1.getHeader()).thenReturn(header1);
        when(headerDriver2.getHeader()).thenReturn(header2);

        streamRecyclerViewAdapter = new StreamRecyclerViewAdapter(context,
                new RecyclerView(context), cardConfiguration, pietManager, deepestContentTracker,
                contentChangedListener, scrollObservable, CONFIGURATION, pietEventLogger);
        streamRecyclerViewAdapter.setHeaders(headers);

        when(driver.getLeafFeatureDrivers()).thenReturn(Lists.newArrayList(initialFeatureDriver));
        streamRecyclerViewAdapter.setDriver(driver);

        streamAdapterObserver = new StreamAdapterObserver();
        streamRecyclerViewAdapter.registerAdapterDataObserver(streamAdapterObserver);
    }

    @Test
    public void testCreateViewHolderPiet() {
        FrameLayout parent = new FrameLayout(context);
        ViewHolder viewHolder =
                streamRecyclerViewAdapter.onCreateViewHolder(parent, ViewHolderType.TYPE_CARD);

        FrameLayout cardView = getCardView(viewHolder);
        assertThat(cardView.getChildAt(0)).isEqualTo(frameContainer);
    }

    @Test
    public void testCreateViewHolderContinuation() {
        FrameLayout parent = new FrameLayout(context);
        ViewHolder viewHolder = streamRecyclerViewAdapter.onCreateViewHolder(
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
        FrameLayout parent = new FrameLayout(context);
        ViewHolder viewHolder = streamRecyclerViewAdapter.onCreateViewHolder(
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
        FrameLayout parent = new FrameLayout(context);
        ViewHolder viewHolder = streamRecyclerViewAdapter.onCreateViewHolder(
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
        FrameLayout parent = new FrameLayout(context);
        FeedViewHolder viewHolder =
                streamRecyclerViewAdapter.onCreateViewHolder(parent, ViewHolderType.TYPE_CARD);

        streamRecyclerViewAdapter.onBindViewHolder(viewHolder, getContentBindingIndex(0));

        verify(initialFeatureDriver).bind(viewHolder);
    }

    @Test
    public void testOnViewRecycled() {
        PietViewHolder viewHolder = mock(PietViewHolder.class);

        streamRecyclerViewAdapter.onBindViewHolder(viewHolder, getContentBindingIndex(0));

        // Make sure the content model is bound
        verify(initialFeatureDriver).bind(viewHolder);

        streamRecyclerViewAdapter.onViewRecycled(viewHolder);

        verify(initialFeatureDriver).unbind();
    }

    @Test
    public void testSetDriver_initialContentModels() {
        // streamRecyclerViewAdapter.setDriver(driver) is called in setup()
        assertThat(streamRecyclerViewAdapter.getLeafFeatureDrivers())
                .containsExactly(initialFeatureDriver);
    }

    @Test
    public void testSetDriver_newDriver() {
        StreamDriver newDriver = mock(StreamDriver.class);
        List<LeafFeatureDriver> newFeatureDrivers =
                Lists.newArrayList(featureDriver1, featureDriver2);

        when(newDriver.getLeafFeatureDrivers()).thenReturn(newFeatureDrivers);

        streamRecyclerViewAdapter.setDriver(newDriver);

        verify(driver).setStreamContentListener(null);
        assertThat(streamRecyclerViewAdapter.getItemId(getContentBindingIndex(0)))
                .isEqualTo(FEATURE_DRIVER_1_ID);
        assertThat(streamRecyclerViewAdapter.getItemId(getContentBindingIndex(1)))
                .isEqualTo(FEATURE_DRIVER_2_ID);

        InOrder inOrder = Mockito.inOrder(newDriver);

        // getLeafFeatureDrivers() needs to be called before setting the listener, otherwise the
        // adapter could be notified of a child and then given it in getLeafFeatureDrivers().
        inOrder.verify(newDriver).getLeafFeatureDrivers();
        inOrder.verify(newDriver).setStreamContentListener(streamRecyclerViewAdapter);
        inOrder.verify(newDriver).maybeRestoreScroll();
    }

    @Test
    public void testNotifyContentsAdded_endOfList() {
        streamRecyclerViewAdapter.notifyContentsAdded(
                1, Lists.newArrayList(featureDriver1, featureDriver2));
        assertThat(streamRecyclerViewAdapter.getLeafFeatureDrivers())
                .containsAtLeast(initialFeatureDriver, featureDriver1, featureDriver2);
    }

    @Test
    public void testNotifyContentsAdded_startOfList() {
        streamRecyclerViewAdapter.notifyContentsAdded(
                0, Lists.newArrayList(featureDriver1, featureDriver2));
        assertThat(streamRecyclerViewAdapter.getLeafFeatureDrivers())
                .containsAtLeast(featureDriver1, featureDriver2, initialFeatureDriver);
    }

    @Test
    public void testNotifyContentsRemoved() {
        streamRecyclerViewAdapter.notifyContentRemoved(0);
        assertThat(streamRecyclerViewAdapter.getLeafFeatureDrivers()).isEmpty();
        verify(deepestContentTracker).removeContentId(0);
    }

    @Test
    public void testNotifyContentsAdded_notifiesListener() {
        streamRecyclerViewAdapter.setShown(false);
        streamRecyclerViewAdapter.notifyContentsAdded(
                0, Lists.newArrayList(featureDriver1, featureDriver2));

        assertThat(streamAdapterObserver.insertedStart).isEqualTo(headers.size());
        assertThat(streamAdapterObserver.insertedCount).isEqualTo(2);
        verify(contentChangedListener).onContentChanged();
    }

    @Test
    public void testNotifyContentsAdded_doesNotNotifiesContentChangedListener_ifShownTrue() {
        streamRecyclerViewAdapter.setShown(true);
        streamRecyclerViewAdapter.notifyContentsAdded(
                0, Lists.newArrayList(featureDriver1, featureDriver2));

        verify(contentChangedListener, never()).onContentChanged();
    }

    @Test
    public void testNotifyContentsAdded_streamVisibleFalse() {
        streamRecyclerViewAdapter.setStreamContentVisible(false);
        streamAdapterObserver.reset();

        streamRecyclerViewAdapter.notifyContentsAdded(
                0, Lists.newArrayList(featureDriver1, featureDriver2));
        assertThat(streamAdapterObserver.insertedStart).isEqualTo(0);
        assertThat(streamAdapterObserver.insertedCount).isEqualTo(0);
    }

    @Test
    public void testNotifyContentsRemoved_notifiesListener() {
        streamRecyclerViewAdapter.setShown(false);
        streamRecyclerViewAdapter.notifyContentRemoved(0);

        assertThat(streamAdapterObserver.removedStart).isEqualTo(headers.size());
        assertThat(streamAdapterObserver.removedCount).isEqualTo(1);
        verify(contentChangedListener).onContentChanged();
    }

    @Test
    public void testNotifyContentsRemoved_doesNotNotifiesContentChangedListener_ifShownTrue() {
        streamRecyclerViewAdapter.setShown(true);
        streamRecyclerViewAdapter.notifyContentRemoved(0);

        verify(contentChangedListener, never()).onContentChanged();
    }

    @Test
    public void testNotifyContentsRemoved_streamVisibleFalse() {
        streamRecyclerViewAdapter.setStreamContentVisible(false);
        streamAdapterObserver.reset();

        streamRecyclerViewAdapter.notifyContentRemoved(0);
        assertThat(streamAdapterObserver.removedStart).isEqualTo(0);
        assertThat(streamAdapterObserver.removedCount).isEqualTo(0);
    }

    @Test
    public void testNotifyContentsCleared_notifiesListener() {
        streamRecyclerViewAdapter.setShown(false);
        streamRecyclerViewAdapter.notifyContentsCleared();

        assertThat(streamAdapterObserver.removedStart).isEqualTo(headers.size());
        assertThat(streamAdapterObserver.removedCount).isEqualTo(1);
        verify(contentChangedListener).onContentChanged();
    }

    @Test
    public void testNotifyContentsCleared_doesNotNotifiesContentChangedListener_ifShownTrue() {
        streamRecyclerViewAdapter.setShown(true);
        streamRecyclerViewAdapter.notifyContentsCleared();

        verify(contentChangedListener, never()).onContentChanged();
    }

    @Test
    public void testNotifyContentsCleared_streamVisibleFalse() {
        streamRecyclerViewAdapter.setStreamContentVisible(false);
        streamAdapterObserver.reset();

        streamRecyclerViewAdapter.notifyContentsCleared();
        assertThat(streamAdapterObserver.removedStart).isEqualTo(0);
        assertThat(streamAdapterObserver.removedCount).isEqualTo(0);
    }

    @Test
    public void testOnDestroy() {
        streamRecyclerViewAdapter = new StreamRecyclerViewAdapter(context,
                new RecyclerView(context), cardConfiguration, pietManager, deepestContentTracker,
                contentChangedListener, scrollObservable, CONFIGURATION, pietEventLogger) {
            @Override
            HeaderDriver createHeaderDriver(Header header) {
                if (header == headers.get(0)) {
                    return headerDriver1;
                }
                return headerDriver2;
            }
        };
        streamRecyclerViewAdapter.setHeaders(headers);
        streamRecyclerViewAdapter.onDestroy();

        verify(headerDriver1).unbind();
        verify(headerDriver1).onDestroy();
        verify(headerDriver2).unbind();
        verify(headerDriver2).onDestroy();
    }

    @Test
    public void testSetHeaders_destroysPreviousHeaderDrivers() {
        streamRecyclerViewAdapter = new StreamRecyclerViewAdapter(context,
                new RecyclerView(context), cardConfiguration, pietManager, deepestContentTracker,
                contentChangedListener, scrollObservable, CONFIGURATION, pietEventLogger) {
            @Override
            HeaderDriver createHeaderDriver(Header header) {
                if (header == headers.get(0)) {
                    return headerDriver1;
                }
                return headerDriver2;
            }
        };
        streamRecyclerViewAdapter.setHeaders(headers);

        streamRecyclerViewAdapter.setHeaders(Collections.emptyList());

        verify(headerDriver1).onDestroy();
        verify(headerDriver2).onDestroy();
    }

    @Test
    public void testGetItemCount() {
        assertThat(streamRecyclerViewAdapter.getItemCount()).isEqualTo(headers.size() + 1);
    }

    @Test
    public void testGetItemCount_streamVisibleFalse() {
        streamRecyclerViewAdapter.setStreamContentVisible(false);
        assertThat(streamRecyclerViewAdapter.getItemCount()).isEqualTo(headers.size());
    }

    @Test
    public void setStreamContentVisible_hidesContent() {
        streamRecyclerViewAdapter.setStreamContentVisible(false);
        assertThat(streamAdapterObserver.removedStart).isEqualTo(headers.size());
        assertThat(streamAdapterObserver.removedCount).isEqualTo(1);
    }

    @Test
    public void setStreamContentVisible_hidesContent_doubleCall() {
        streamRecyclerViewAdapter.setStreamContentVisible(false);

        streamAdapterObserver.reset();
        streamRecyclerViewAdapter.setStreamContentVisible(false);

        // Nothing additional should get called
        assertThat(streamAdapterObserver.removedStart).isEqualTo(0);
        assertThat(streamAdapterObserver.removedCount).isEqualTo(0);
    }

    @Test
    public void setStreamContentVisible_showsContent() {
        streamRecyclerViewAdapter.setStreamContentVisible(false);
        streamAdapterObserver.reset();

        streamRecyclerViewAdapter.setStreamContentVisible(true);
        assertThat(streamAdapterObserver.insertedStart).isEqualTo(headers.size());
        assertThat(streamAdapterObserver.insertedCount).isEqualTo(1);
    }

    @Test
    public void setStreamContentVisible_showsContent_doubleCall() {
        streamRecyclerViewAdapter.setStreamContentVisible(false);
        streamAdapterObserver.reset();

        streamRecyclerViewAdapter.setStreamContentVisible(true);

        streamAdapterObserver.reset();
        streamRecyclerViewAdapter.setStreamContentVisible(true);

        // Nothing additional should get called
        assertThat(streamAdapterObserver.insertedStart).isEqualTo(0);
        assertThat(streamAdapterObserver.insertedCount).isEqualTo(0);
    }

    @Test
    public void testOnBindViewHolder_updatesDeepestViewedContent() {
        PietViewHolder viewHolder = mock(PietViewHolder.class);

        streamRecyclerViewAdapter.onBindViewHolder(viewHolder, getContentBindingIndex(0));

        verify(deepestContentTracker).updateDeepestContentTracker(0, INITIAL_CONTENT_ID);
    }

    @Test
    public void testDismissHeader() {
        streamRecyclerViewAdapter = new StreamRecyclerViewAdapter(context,
                new RecyclerView(context), cardConfiguration, pietManager, deepestContentTracker,
                contentChangedListener, scrollObservable, CONFIGURATION, pietEventLogger) {
            @Override
            HeaderDriver createHeaderDriver(Header header) {
                if (header == headers.get(0)) {
                    return headerDriver1;
                }
                return headerDriver2;
            }
        };
        streamRecyclerViewAdapter.setHeaders(headers);
        assertThat(streamRecyclerViewAdapter.getItemCount()).isEqualTo(2);

        streamRecyclerViewAdapter.dismissHeader(header1);

        assertThat(streamRecyclerViewAdapter.getItemCount()).isEqualTo(1);
        verify(header1).onDismissed();
        verify(headerDriver1).onDestroy();
    }

    @Test
    public void testDismissHeader_whenNoHeaders() {
        streamRecyclerViewAdapter.setHeaders(Collections.emptyList());
        reset(header1);
        assertThat(streamRecyclerViewAdapter.getItemCount()).isEqualTo(1);

        streamRecyclerViewAdapter.dismissHeader(header1);

        assertThat(streamRecyclerViewAdapter.getItemCount()).isEqualTo(1);
        verify(header1, never()).onDismissed();
    }

    @Test
    public void testRebind() {
        PietViewHolder viewHolder = mock(PietViewHolder.class);
        streamRecyclerViewAdapter.onBindViewHolder(viewHolder, getContentBindingIndex(0));
        streamRecyclerViewAdapter.rebind();
        verify(initialFeatureDriver).maybeRebind();
    }

    private FrameLayout getCardView(ViewHolder viewHolder) {
        return (FrameLayout) viewHolder.itemView;
    }

    private int getContentBindingIndex(int index) {
        return HEADER_COUNT + index;
    }

    private class StreamAdapterObserver extends RecyclerView.AdapterDataObserver {
        private int insertedStart;
        private int insertedCount;
        private int removedStart;
        private int removedCount;

        @Override
        public void onItemRangeInserted(int positionStart, int itemCount) {
            insertedStart = positionStart;
            insertedCount = itemCount;
        }

        @Override
        public void onItemRangeRemoved(int positionStart, int itemCount) {
            removedStart = positionStart;
            removedCount = itemCount;
        }

        public void reset() {
            insertedStart = 0;
            insertedCount = 0;
            removedStart = 0;
            removedCount = 0;
        }
    }
}
