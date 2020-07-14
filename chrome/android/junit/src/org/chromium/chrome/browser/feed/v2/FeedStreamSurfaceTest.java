// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.support.test.filters.SmallTest;
import android.view.View;
import android.widget.TextView;

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
    private FeedStreamSurface mFeedStreamSurface;
    private Activity mActivity;

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
        mocker.mock(FeedStreamSurfaceJni.TEST_HOOKS, mFeedStreamSurfaceJniMock);
        Profile.setLastUsedProfileForTesting(mProfileMock);
        mFeedStreamSurface = new FeedStreamSurface(mActivity, false, mSnackbarManager,
                mPageNavigationDelegate, mBottomSheetController, mHelpAndFeedback);
    }

    @Test
    @SmallTest
    public void testAddSlicesOnStreamUpdated() {
        FeedListContentManager contentManager =
                mFeedStreamSurface.getFeedListContentManagerForTesting();

        // Add 3 new slices at first.
        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("c"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(3, contentManager.getItemCount());
        assertEquals(0, contentManager.findContentPositionByKey("a"));
        assertEquals(1, contentManager.findContentPositionByKey("b"));
        assertEquals(2, contentManager.findContentPositionByKey("c"));

        // Add 2 more slices.
        update = StreamUpdate.newBuilder()
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("a"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("b"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("c"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("d"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("e"))
                         .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(5, contentManager.getItemCount());
        assertEquals(0, contentManager.findContentPositionByKey("a"));
        assertEquals(1, contentManager.findContentPositionByKey("b"));
        assertEquals(2, contentManager.findContentPositionByKey("c"));
        assertEquals(3, contentManager.findContentPositionByKey("d"));
        assertEquals(4, contentManager.findContentPositionByKey("e"));
    }

    @Test
    @SmallTest
    public void testAddNewSlicesWithSameIds() {
        FeedListContentManager contentManager =
                mFeedStreamSurface.getFeedListContentManagerForTesting();

        // Add 2 new slices at first.
        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(2, contentManager.getItemCount());
        assertEquals(0, contentManager.findContentPositionByKey("a"));
        assertEquals(1, contentManager.findContentPositionByKey("b"));

        // Add 2 new slice with same ids as before.
        update = StreamUpdate.newBuilder()
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                         .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(2, contentManager.getItemCount());
        assertEquals(0, contentManager.findContentPositionByKey("b"));
        assertEquals(1, contentManager.findContentPositionByKey("a"));
    }

    @Test
    @SmallTest
    public void testRemoveSlicesOnStreamUpdated() {
        FeedListContentManager contentManager =
                mFeedStreamSurface.getFeedListContentManagerForTesting();

        // Add 3 new slices at first.
        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("c"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(3, contentManager.getItemCount());
        assertEquals(0, contentManager.findContentPositionByKey("a"));
        assertEquals(1, contentManager.findContentPositionByKey("b"));
        assertEquals(2, contentManager.findContentPositionByKey("c"));

        // Remove 1 slice.
        update = StreamUpdate.newBuilder()
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("a"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("c"))
                         .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(2, contentManager.getItemCount());
        assertEquals(0, contentManager.findContentPositionByKey("a"));
        assertEquals(1, contentManager.findContentPositionByKey("c"));

        // Remove 2 slices.
        update = StreamUpdate.newBuilder().build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(0, contentManager.getItemCount());
    }

    @Test
    @SmallTest
    public void testReorderSlicesOnStreamUpdated() {
        FeedListContentManager contentManager =
                mFeedStreamSurface.getFeedListContentManagerForTesting();

        // Add 3 new slices at first.
        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("c"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(3, contentManager.getItemCount());
        assertEquals(0, contentManager.findContentPositionByKey("a"));
        assertEquals(1, contentManager.findContentPositionByKey("b"));
        assertEquals(2, contentManager.findContentPositionByKey("c"));

        // Reorder 1 slice.
        update = StreamUpdate.newBuilder()
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("c"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("a"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("b"))
                         .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(3, contentManager.getItemCount());
        assertEquals(0, contentManager.findContentPositionByKey("c"));
        assertEquals(1, contentManager.findContentPositionByKey("a"));
        assertEquals(2, contentManager.findContentPositionByKey("b"));

        // Reorder 2 slices.
        update = StreamUpdate.newBuilder()
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("a"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("b"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("c"))
                         .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(3, contentManager.getItemCount());
        assertEquals(0, contentManager.findContentPositionByKey("a"));
        assertEquals(1, contentManager.findContentPositionByKey("b"));
        assertEquals(2, contentManager.findContentPositionByKey("c"));
    }

    @Test
    @SmallTest
    public void testComplexOperationsOnStreamUpdated() {
        FeedListContentManager contentManager =
                mFeedStreamSurface.getFeedListContentManagerForTesting();

        // Add 3 new slices at first.
        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("c"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("d"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("e"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(5, contentManager.getItemCount());
        assertEquals(0, contentManager.findContentPositionByKey("a"));
        assertEquals(1, contentManager.findContentPositionByKey("b"));
        assertEquals(2, contentManager.findContentPositionByKey("c"));
        assertEquals(3, contentManager.findContentPositionByKey("d"));
        assertEquals(4, contentManager.findContentPositionByKey("e"));

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
        assertEquals(7, contentManager.getItemCount());
        assertEquals(0, contentManager.findContentPositionByKey("f"));
        assertEquals(1, contentManager.findContentPositionByKey("g"));
        assertEquals(2, contentManager.findContentPositionByKey("a"));
        assertEquals(3, contentManager.findContentPositionByKey("h"));
        assertEquals(4, contentManager.findContentPositionByKey("c"));
        assertEquals(5, contentManager.findContentPositionByKey("e"));
        assertEquals(6, contentManager.findContentPositionByKey("i"));
    }

    @Test
    @SmallTest
    public void testAddHeaderViews() {
        FeedListContentManager contentManager =
                mFeedStreamSurface.getFeedListContentManagerForTesting();

        View v0 = new View(mActivity);
        View v1 = new View(mActivity);

        mFeedStreamSurface.setHeaderViews(Arrays.asList(v0, v1));
        assertEquals(2, contentManager.getItemCount());
        assertEquals(v0,
                ((FeedListContentManager.NativeViewContent) contentManager.getContent(0))
                        .getNativeView(null));
        assertEquals(v1,
                ((FeedListContentManager.NativeViewContent) contentManager.getContent(1))
                        .getNativeView(null));
    }

    @Test
    @SmallTest
    public void testUpdateHeaderViews() {
        FeedListContentManager contentManager =
                mFeedStreamSurface.getFeedListContentManagerForTesting();

        View v0 = new View(mActivity);
        View v1 = new View(mActivity);

        mFeedStreamSurface.setHeaderViews(Arrays.asList(v0, v1));
        assertEquals(2, contentManager.getItemCount());
        assertEquals(v0,
                ((FeedListContentManager.NativeViewContent) contentManager.getContent(0))
                        .getNativeView(null));
        assertEquals(v1,
                ((FeedListContentManager.NativeViewContent) contentManager.getContent(1))
                        .getNativeView(null));

        View v2 = new View(mActivity);
        View v3 = new View(mActivity);

        mFeedStreamSurface.setHeaderViews(Arrays.asList(v2, v0, v3));
        assertEquals(3, contentManager.getItemCount());
        assertEquals(v2,
                ((FeedListContentManager.NativeViewContent) contentManager.getContent(0))
                        .getNativeView(null));
        assertEquals(v0,
                ((FeedListContentManager.NativeViewContent) contentManager.getContent(1))
                        .getNativeView(null));
        assertEquals(v3,
                ((FeedListContentManager.NativeViewContent) contentManager.getContent(2))
                        .getNativeView(null));
    }

    @Test
    @SmallTest
    public void testComplexOperationsOnStreamUpdatedAfterSetHeaderViews() {
        FeedListContentManager contentManager =
                mFeedStreamSurface.getFeedListContentManagerForTesting();

        // Set 2 header views first. These should always be there throughout stream update.
        View v0 = new View(mActivity);
        View v1 = new View(mActivity);
        mFeedStreamSurface.setHeaderViews(Arrays.asList(v0, v1));
        assertEquals(2, contentManager.getItemCount());
        assertEquals(v0,
                ((FeedListContentManager.NativeViewContent) contentManager.getContent(0))
                        .getNativeView(null));
        assertEquals(v1,
                ((FeedListContentManager.NativeViewContent) contentManager.getContent(1))
                        .getNativeView(null));
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
        assertEquals(headers + 5, contentManager.getItemCount());
        assertEquals(headers + 0, contentManager.findContentPositionByKey("a"));
        assertEquals(headers + 1, contentManager.findContentPositionByKey("b"));
        assertEquals(headers + 2, contentManager.findContentPositionByKey("c"));
        assertEquals(headers + 3, contentManager.findContentPositionByKey("d"));
        assertEquals(headers + 4, contentManager.findContentPositionByKey("e"));

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
        assertEquals(headers + 7, contentManager.getItemCount());
        assertEquals(headers + 0, contentManager.findContentPositionByKey("f"));
        assertEquals(headers + 1, contentManager.findContentPositionByKey("g"));
        assertEquals(headers + 2, contentManager.findContentPositionByKey("a"));
        assertEquals(headers + 3, contentManager.findContentPositionByKey("h"));
        assertEquals(headers + 4, contentManager.findContentPositionByKey("c"));
        assertEquals(headers + 5, contentManager.findContentPositionByKey("e"));
        assertEquals(headers + 6, contentManager.findContentPositionByKey("i"));
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
        FeedListContentManager contentManager =
                mFeedStreamSurface.getFeedListContentManagerForTesting();

        // Set 2 header views first.
        View v0 = new View(mActivity);
        View v1 = new View(mActivity);
        mFeedStreamSurface.setHeaderViews(Arrays.asList(v0, v1));
        assertEquals(2, contentManager.getItemCount());
        assertEquals(v0,
                ((FeedListContentManager.NativeViewContent) contentManager.getContent(0))
                        .getNativeView(null));
        assertEquals(v1,
                ((FeedListContentManager.NativeViewContent) contentManager.getContent(1))
                        .getNativeView(null));
        final int headers = 2;

        // Add 3 new slices.
        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("c"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(headers + 3, contentManager.getItemCount());

        // Closing the surface should remove all non-header contents.
        mFeedStreamSurface.surfaceClosed();
        assertEquals(headers, contentManager.getItemCount());
        assertEquals(v0,
                ((FeedListContentManager.NativeViewContent) contentManager.getContent(0))
                        .getNativeView(null));
        assertEquals(v1,
                ((FeedListContentManager.NativeViewContent) contentManager.getContent(1))
                        .getNativeView(null));
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
}
