// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.support.test.filters.SmallTest;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import com.google.protobuf.ByteString;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.xsurface.FeedActionsHandler;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.feed.proto.FeedUiProto.Slice;
import org.chromium.components.feed.proto.FeedUiProto.StreamUpdate;
import org.chromium.components.feed.proto.FeedUiProto.StreamUpdate.SliceUpdate;
import org.chromium.components.feed.proto.FeedUiProto.XSurfaceSlice;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

/** Unit tests for {@link FeedStreamSurface}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedStreamSurfaceTest {
    private static final String TEST_DATA = "test";
    private static final String TEST_URL = "https://www.chromium.org";
    private static final int LOAD_MORE_TRIGGER_LOOKAHEAD = 5;
    private FeedStreamSurface mFeedStreamSurface;
    private Activity mActivity;
    private RecyclerView mRecyclerView;
    private LinearLayout mParent;
    private FakeLinearLayoutManager mLayoutManager;
    private FeedListContentManager mContentManager;

    @Mock
    private SnackbarManager mSnackbarManager;
    @Mock
    private FeedActionsHandler.SnackbarController mSnackbarController;
    @Mock
    private BottomSheetController mBottomSheetController;
    @Mock
    private NativePageNavigationDelegate mPageNavigationDelegate;
    @Mock
    private HelpAndFeedback mHelpAndFeedback;
    @Mock
    Profile mProfileMock;
    @Mock
    private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;

    @Captor
    private ArgumentCaptor<Map<String, String>> mMapCaptor;

    @Rule
    public JniMocker mocker = new JniMocker();
    // Enable the Features class, so we can call code which checks to see if features are enabled
    // without crashing.
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Mock
    private FeedStreamSurface.Natives mFeedStreamSurfaceJniMock;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mParent = new LinearLayout(mActivity);
        mocker.mock(FeedStreamSurfaceJni.TEST_HOOKS, mFeedStreamSurfaceJniMock);
        mocker.mock(FeedServiceBridgeJni.TEST_HOOKS, mFeedServiceBridgeJniMock);

        when(mFeedServiceBridgeJniMock.getLoadMoreTriggerLookahead())
                .thenReturn(LOAD_MORE_TRIGGER_LOOKAHEAD);

        Profile.setLastUsedProfileForTesting(mProfileMock);
        mFeedStreamSurface = new FeedStreamSurface(mActivity, false, mSnackbarManager,
                mPageNavigationDelegate, mBottomSheetController, mHelpAndFeedback);
        mContentManager = mFeedStreamSurface.getFeedListContentManagerForTesting();

        mRecyclerView = (RecyclerView) mFeedStreamSurface.getView();
        mLayoutManager = new FakeLinearLayoutManager(mActivity);
        mRecyclerView.setLayoutManager(mLayoutManager);
    }

    @Test
    @SmallTest
    public void testAddSlicesOnStreamUpdated() {
        // Add 3 new slices at first.
        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("c"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(3, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("a"));
        assertEquals(1, mContentManager.findContentPositionByKey("b"));
        assertEquals(2, mContentManager.findContentPositionByKey("c"));

        // Add 2 more slices.
        update = StreamUpdate.newBuilder()
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("a"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("b"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("c"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("d"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("e"))
                         .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(5, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("a"));
        assertEquals(1, mContentManager.findContentPositionByKey("b"));
        assertEquals(2, mContentManager.findContentPositionByKey("c"));
        assertEquals(3, mContentManager.findContentPositionByKey("d"));
        assertEquals(4, mContentManager.findContentPositionByKey("e"));
    }

    @Test
    @SmallTest
    public void testAddNewSlicesWithSameIds() {
        // Add 2 new slices at first.
        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(2, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("a"));
        assertEquals(1, mContentManager.findContentPositionByKey("b"));

        // Add 2 new slice with same ids as before.
        update = StreamUpdate.newBuilder()
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                         .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(2, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("b"));
        assertEquals(1, mContentManager.findContentPositionByKey("a"));
    }

    @Test
    @SmallTest
    public void testRemoveSlicesOnStreamUpdated() {
        // Add 3 new slices at first.
        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("c"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(3, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("a"));
        assertEquals(1, mContentManager.findContentPositionByKey("b"));
        assertEquals(2, mContentManager.findContentPositionByKey("c"));

        // Remove 1 slice.
        update = StreamUpdate.newBuilder()
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("a"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("c"))
                         .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(2, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("a"));
        assertEquals(1, mContentManager.findContentPositionByKey("c"));

        // Remove 2 slices.
        update = StreamUpdate.newBuilder().build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(0, mContentManager.getItemCount());
    }

    @Test
    @SmallTest
    public void testReorderSlicesOnStreamUpdated() {
        // Add 3 new slices at first.
        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("c"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(3, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("a"));
        assertEquals(1, mContentManager.findContentPositionByKey("b"));
        assertEquals(2, mContentManager.findContentPositionByKey("c"));

        // Reorder 1 slice.
        update = StreamUpdate.newBuilder()
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("c"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("a"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("b"))
                         .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(3, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("c"));
        assertEquals(1, mContentManager.findContentPositionByKey("a"));
        assertEquals(2, mContentManager.findContentPositionByKey("b"));

        // Reorder 2 slices.
        update = StreamUpdate.newBuilder()
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("a"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("b"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("c"))
                         .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(3, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("a"));
        assertEquals(1, mContentManager.findContentPositionByKey("b"));
        assertEquals(2, mContentManager.findContentPositionByKey("c"));
    }

    @Test
    @SmallTest
    public void testComplexOperationsOnStreamUpdated() {
        // Add 3 new slices at first.
        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("c"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("d"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("e"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(5, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("a"));
        assertEquals(1, mContentManager.findContentPositionByKey("b"));
        assertEquals(2, mContentManager.findContentPositionByKey("c"));
        assertEquals(3, mContentManager.findContentPositionByKey("d"));
        assertEquals(4, mContentManager.findContentPositionByKey("e"));

        // Combo of add, remove and reorder operations.
        update = StreamUpdate.newBuilder()
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("f"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("g"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("a"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("h"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("c"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("e"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("i"))
                         .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(7, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("f"));
        assertEquals(1, mContentManager.findContentPositionByKey("g"));
        assertEquals(2, mContentManager.findContentPositionByKey("a"));
        assertEquals(3, mContentManager.findContentPositionByKey("h"));
        assertEquals(4, mContentManager.findContentPositionByKey("c"));
        assertEquals(5, mContentManager.findContentPositionByKey("e"));
        assertEquals(6, mContentManager.findContentPositionByKey("i"));
    }

    @Test
    @SmallTest
    public void testAddHeaderViews() {
        View v0 = new View(mActivity);
        View v1 = new View(mActivity);

        mFeedStreamSurface.setHeaderViews(Arrays.asList(v0, v1));
        assertEquals(2, mContentManager.getItemCount());
        assertEquals(v0, getNativeView(0));
        assertEquals(v1, getNativeView(1));
    }

    @Test
    @SmallTest
    public void testUpdateHeaderViews() {
        View v0 = new View(mActivity);
        View v1 = new View(mActivity);

        mFeedStreamSurface.setHeaderViews(Arrays.asList(v0, v1));
        assertEquals(2, mContentManager.getItemCount());
        assertEquals(v0, getNativeView(0));
        assertEquals(v1, getNativeView(1));

        View v2 = new View(mActivity);
        View v3 = new View(mActivity);

        mFeedStreamSurface.setHeaderViews(Arrays.asList(v2, v0, v3));
        assertEquals(3, mContentManager.getItemCount());
        assertEquals(v2, getNativeView(0));
        assertEquals(v0, getNativeView(1));
        assertEquals(v3, getNativeView(2));
    }

    @Test
    @SmallTest
    public void testComplexOperationsOnStreamUpdatedAfterSetHeaderViews() {
        // Set 2 header views first. These should always be there throughout stream update.
        View v0 = new View(mActivity);
        View v1 = new View(mActivity);
        mFeedStreamSurface.setHeaderViews(Arrays.asList(v0, v1));
        assertEquals(2, mContentManager.getItemCount());
        assertEquals(v0, getNativeView(0));
        assertEquals(v1, getNativeView(1));
        final int headers = 2;

        // Add 3 new slices at first.
        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("c"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("d"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("e"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(headers + 5, mContentManager.getItemCount());
        assertEquals(headers + 0, mContentManager.findContentPositionByKey("a"));
        assertEquals(headers + 1, mContentManager.findContentPositionByKey("b"));
        assertEquals(headers + 2, mContentManager.findContentPositionByKey("c"));
        assertEquals(headers + 3, mContentManager.findContentPositionByKey("d"));
        assertEquals(headers + 4, mContentManager.findContentPositionByKey("e"));

        // Combo of add, remove and reorder operations.
        update = StreamUpdate.newBuilder()
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("f"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("g"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("a"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("h"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("c"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("e"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("i"))
                         .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(headers + 7, mContentManager.getItemCount());
        assertEquals(headers + 0, mContentManager.findContentPositionByKey("f"));
        assertEquals(headers + 1, mContentManager.findContentPositionByKey("g"));
        assertEquals(headers + 2, mContentManager.findContentPositionByKey("a"));
        assertEquals(headers + 3, mContentManager.findContentPositionByKey("h"));
        assertEquals(headers + 4, mContentManager.findContentPositionByKey("c"));
        assertEquals(headers + 5, mContentManager.findContentPositionByKey("e"));
        assertEquals(headers + 6, mContentManager.findContentPositionByKey("i"));
    }

    @Test
    @SmallTest
    public void testNavigateTab() {
        when(mPageNavigationDelegate.openUrl(anyInt(), any())).thenReturn(new MockTab(1, false));
        mFeedStreamSurface.navigateTab(TEST_URL);
        verify(mPageNavigationDelegate)
                .openUrl(ArgumentMatchers.eq(WindowOpenDisposition.CURRENT_TAB), any());
    }

    @Test
    @SmallTest
    public void testNavigateNewTab() {
        when(mPageNavigationDelegate.openUrl(anyInt(), any())).thenReturn(new MockTab(1, false));
        mFeedStreamSurface.navigateNewTab(TEST_URL);
        verify(mPageNavigationDelegate)
                .openUrl(ArgumentMatchers.eq(WindowOpenDisposition.NEW_FOREGROUND_TAB), any());
    }

    @Test
    @SmallTest
    public void testNavigateIncognitoTab() {
        when(mPageNavigationDelegate.openUrl(anyInt(), any())).thenReturn(new MockTab(1, false));
        mFeedStreamSurface.navigateIncognitoTab(TEST_URL);
        verify(mPageNavigationDelegate)
                .openUrl(ArgumentMatchers.eq(WindowOpenDisposition.OFF_THE_RECORD), any());
    }

    @Test
    @SmallTest
    public void testSendFeedback() {
        final String testUrl = "https://www.chromium.org";
        final String testTitle = "Chromium based browsers for the win!";
        final String xSurfaceCardTitle = "Card Title";
        final String cardTitle = "CardTitle";
        final String cardUrl = "CardUrl";
        // Arrange.
        Map<String, String> productSpecificDataMap = new HashMap<>();
        productSpecificDataMap.put(FeedStreamSurface.XSURFACE_CARD_URL, testUrl);
        productSpecificDataMap.put(xSurfaceCardTitle, testTitle);

        // Act.
        mFeedStreamSurface.sendFeedback(productSpecificDataMap);

        // Assert.
        verify(mHelpAndFeedback)
                .showFeedback(any(), any(), eq(testUrl), eq(FeedStreamSurface.FEEDBACK_REPORT_TYPE),
                        mMapCaptor.capture(), eq(FeedStreamSurface.FEEDBACK_CONTEXT));

        // Check that the map contents are as expected.
        assertThat(mMapCaptor.getValue()).containsEntry(cardUrl, testUrl);
        assertThat(mMapCaptor.getValue()).containsEntry(cardTitle, testTitle);
    }

    @Test
    @SmallTest
    public void testShowSnackbar() {
        mFeedStreamSurface.showSnackbar(
                "message", "Undo", FeedActionsHandler.SnackbarDuration.SHORT, mSnackbarController);
        verify(mSnackbarManager).showSnackbar(any());
    }

    @Test
    @SmallTest
    public void testShowBottomSheet() {
        mFeedStreamSurface.showBottomSheet(new TextView(mActivity));
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testDismissBottomSheet() {
        mFeedStreamSurface.showBottomSheet(new TextView(mActivity));
        mFeedStreamSurface.dismissBottomSheet();
        verify(mBottomSheetController).hideContent(any(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testSurfaceClosed() {
        FeedListContentManager mContentManager =
                mFeedStreamSurface.getFeedListContentManagerForTesting();

        // Set 2 header views first.
        View v0 = new View(mActivity);
        View v1 = new View(mActivity);
        mFeedStreamSurface.setHeaderViews(Arrays.asList(v0, v1));
        assertEquals(2, mContentManager.getItemCount());
        assertEquals(v0, getNativeView(0));
        assertEquals(v1, getNativeView(1));
        final int headers = 2;

        // Add 3 new slices.
        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("c"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(headers + 3, mContentManager.getItemCount());

        // Closing the surface should remove all non-header contents.
        mFeedStreamSurface.surfaceClosed();
        assertEquals(headers, mContentManager.getItemCount());
        assertEquals(v0, getNativeView(0));
        assertEquals(v1, getNativeView(1));
    }

    @Test
    @SmallTest
    public void testLoadMoreOnDismissal() {
        final int itemCount = 10;

        // loadMore not triggered due to last visible item not falling into lookahead range.
        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD - 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStreamSurface.commitDismissal(0);
        verify(mFeedStreamSurfaceJniMock, never())
                .loadMore(anyLong(), any(FeedStreamSurface.class), any(Callback.class));

        // loadMore triggered.
        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD + 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStreamSurface.commitDismissal(0);
        verify(mFeedStreamSurfaceJniMock)
                .loadMore(anyLong(), any(FeedStreamSurface.class), any(Callback.class));
    }

    @Test
    @SmallTest
    public void testLoadMoreOnNavigateNewTab() {
        final int itemCount = 10;

        // loadMore not triggered due to last visible item not falling into lookahead range.
        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD - 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStreamSurface.navigateNewTab("");
        verify(mFeedStreamSurfaceJniMock, never())
                .loadMore(anyLong(), any(FeedStreamSurface.class), any(Callback.class));

        // loadMore triggered.
        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD + 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStreamSurface.navigateNewTab("");
        verify(mFeedStreamSurfaceJniMock)
                .loadMore(anyLong(), any(FeedStreamSurface.class), any(Callback.class));
    }

    @Test
    @SmallTest
    public void testLoadMoreOnNavigateIncognitoTab() {
        final int itemCount = 10;

        // loadMore not triggered due to last visible item not falling into lookahead range.
        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD - 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStreamSurface.navigateIncognitoTab("");
        verify(mFeedStreamSurfaceJniMock, never())
                .loadMore(anyLong(), any(FeedStreamSurface.class), any(Callback.class));

        // loadMore triggered.
        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD + 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStreamSurface.navigateIncognitoTab("");
        verify(mFeedStreamSurfaceJniMock)
                .loadMore(anyLong(), any(FeedStreamSurface.class), any(Callback.class));
    }

    private SliceUpdate createSliceUpdateForExistingSlice(String sliceId) {
        return SliceUpdate.newBuilder().setSliceId(sliceId).build();
    }

    private SliceUpdate createSliceUpdateForNewXSurfaceSlice(String sliceId) {
        return SliceUpdate.newBuilder().setSlice(createXSurfaceSSlice(sliceId)).build();
    }

    private Slice createXSurfaceSSlice(String sliceId) {
        return Slice.newBuilder()
                .setSliceId(sliceId)
                .setXsurfaceSlice(XSurfaceSlice.newBuilder()
                                          .setXsurfaceFrame(ByteString.copyFromUtf8(TEST_DATA))
                                          .build())
                .build();
    }

    private View getNativeView(int index) {
        View view = ((FeedListContentManager.NativeViewContent) mContentManager.getContent(index))
                            .getNativeView(mParent);
        assertNotNull(view);
        assertTrue(view instanceof FrameLayout);
        return ((FrameLayout) view).getChildAt(0);
    }
}
