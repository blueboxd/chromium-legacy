// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.content.ContentResolver;
import android.net.Uri;
import android.os.Build;
import android.os.StrictMode;
import android.provider.MediaStore;
import android.support.test.filters.LargeTest;
import android.view.View;
import android.view.animation.Animation;
import android.view.animation.Animation.AnimationListener;
import android.widget.Button;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate.SelectionObserver;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.PhotoPickerListener;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;

/**
 * Tests for the PhotoPickerDialog class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP) // See crbug.com/888931 for details.
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PhotoPickerDialogTest implements PhotoPickerListener, SelectionObserver<PickerBitmap>,
                                              DecoderServiceHost.DecoderStatusCallback,
                                              PickerVideoPlayer.VideoPlaybackStatusCallback,
                                              AnimationListener {
    @ClassRule
    public static DisableAnimationsTestRule mDisableAnimationsTestRule =
            new DisableAnimationsTestRule();

    // The timeout (in seconds) to wait for the decoder service to be ready.
    private static final long WAIT_TIMEOUT_SECONDS = 30L;

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule = new ChromeRenderTestRule();

    // The dialog we are testing.
    private PhotoPickerDialog mDialog;

    // The data to show in the dialog (A map of filepath to last-modified time).
    // Map<String, Long> mTestFiles;
    private List<PickerBitmap> mTestFiles;

    // The selection delegate for the dialog.
    private SelectionDelegate<PickerBitmap> mSelectionDelegate;

    // The last action recorded in the dialog (e.g. photo selected).
    private @PhotoPickerAction int mLastActionRecorded;

    // The final set of photos picked by the dialog. Can be an empty array, if
    // nothing was selected.
    private Uri[] mLastSelectedPhotos;

    // The list of currently selected photos (built piecemeal).
    private List<PickerBitmap> mCurrentPhotoSelection;

    // A callback that fires when something is selected in the dialog.
    public final CallbackHelper mOnSelectionCallback = new CallbackHelper();

    // A callback that fires when an action is taken in the dialog (cancel/done etc).
    public final CallbackHelper mOnActionCallback = new CallbackHelper();

    // A callback that fires when the decoder is ready.
    public final CallbackHelper mOnDecoderReadyCallback = new CallbackHelper();

    // A callback that fires when the decoder is idle.
    public final CallbackHelper mOnDecoderIdleCallback = new CallbackHelper();

    // A callback that fires when a PickerBitmapView is animated in the dialog.
    public final CallbackHelper mOnAnimatedCallback = new CallbackHelper();

    // A callback that fires when playback starts for a video.
    public final CallbackHelper mOnVideoPlayingCallback = new CallbackHelper();

    // A callback that fires when playback ends for a video.
    public final CallbackHelper mOnVideoEndedCallback = new CallbackHelper();

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        PickerVideoPlayer.setProgressCallback(this);
        PickerBitmapView.setAnimationListenerForTest(this);
        DecoderServiceHost.setStatusCallback(this);
    }

    private void setupTestFiles() {
        mTestFiles = new ArrayList<>();
        mTestFiles.add(new PickerBitmap(Uri.parse("a"), 5L, PickerBitmap.TileTypes.PICTURE));
        mTestFiles.add(new PickerBitmap(Uri.parse("b"), 4L, PickerBitmap.TileTypes.PICTURE));
        mTestFiles.add(new PickerBitmap(Uri.parse("c"), 3L, PickerBitmap.TileTypes.PICTURE));
        mTestFiles.add(new PickerBitmap(Uri.parse("d"), 2L, PickerBitmap.TileTypes.PICTURE));
        mTestFiles.add(new PickerBitmap(Uri.parse("e"), 1L, PickerBitmap.TileTypes.PICTURE));
        mTestFiles.add(new PickerBitmap(Uri.parse("f"), 0L, PickerBitmap.TileTypes.PICTURE));
        PickerCategoryView.setTestFiles(mTestFiles);
    }

    private void setupTestFilesWith80ColoredSquares() {
        mTestFiles = new ArrayList<>();
        String green = "green100x100.jpg";
        String yellow = "yellow100x100.jpg";
        String red = "red100x100.jpg";
        String blue = "blue100x100.jpg";
        String filePath =
                UrlUtils.getIsolatedTestFilePath("chrome/test/data/android/photo_picker/");

        // The actual value of lastModified is not important, except that each entry must have a
        // unique lastModified stamp in order to ensure a stable order (tiles are ordered in
        // descending order by lastModified). Also, by decrementing this when adding entries (as
        // opposed to incrementing) the tiles will appear in same order as they are added.
        long lastModified = 1000;
        for (int i = 0; i < 50; ++i) {
            mTestFiles.add(new PickerBitmap(Uri.fromFile(new File(filePath + green)),
                    lastModified--, PickerBitmap.TileTypes.PICTURE));
            mTestFiles.add(new PickerBitmap(Uri.fromFile(new File(filePath + yellow)),
                    lastModified--, PickerBitmap.TileTypes.PICTURE));
            mTestFiles.add(new PickerBitmap(Uri.fromFile(new File(filePath + red)), lastModified--,
                    PickerBitmap.TileTypes.PICTURE));
            mTestFiles.add(new PickerBitmap(Uri.fromFile(new File(filePath + blue)), lastModified--,
                    PickerBitmap.TileTypes.PICTURE));
        }
        PickerCategoryView.setTestFiles(mTestFiles);
    }

    // PhotoPickerDialog.PhotoPickerListener:

    @Override
    public void onPhotoPickerUserAction(@PhotoPickerAction int action, Uri[] photos) {
        mLastActionRecorded = action;
        mLastSelectedPhotos = photos != null ? photos.clone() : null;
        if (mLastSelectedPhotos != null) Arrays.sort(mLastSelectedPhotos);
        mOnActionCallback.notifyCalled();
    }

    // DecoderServiceHost.DecoderStatusCallback:

    @Override
    public void serviceReady() {
        mOnDecoderReadyCallback.notifyCalled();
    }

    @Override
    public void decoderIdle() {
        mOnDecoderIdleCallback.notifyCalled();
    }

    // PickerCategoryView.VideoStatusCallback:

    @Override
    public void onVideoPlaying() {
        mOnVideoPlayingCallback.notifyCalled();
    }

    @Override
    public void onVideoEnded() {
        mOnVideoEndedCallback.notifyCalled();
    }

    // SelectionObserver:

    @Override
    public void onSelectionStateChange(List<PickerBitmap> photosSelected) {
        mCurrentPhotoSelection = new ArrayList<>(photosSelected);
        mOnSelectionCallback.notifyCalled();
    }

    // AnimationListener:
    @Override
    public void onAnimationStart(Animation animation) {}

    @Override
    public void onAnimationEnd(Animation animation) {
        mOnAnimatedCallback.notifyCalled();
    }

    @Override
    public void onAnimationRepeat(Animation animation) {}

    private RecyclerView getRecyclerView() {
        return (RecyclerView) mDialog.findViewById(R.id.recycler_view);
    }

    private PhotoPickerDialog createDialogWithContentResolver(final ContentResolver contentResolver,
            final boolean multiselect, final List<String> mimeTypes) throws Exception {
        final PhotoPickerDialog dialog =
                TestThreadUtils.runOnUiThreadBlocking(new Callable<PhotoPickerDialog>() {
                    @Override
                    public PhotoPickerDialog call() {
                        final PhotoPickerDialog dialog = new PhotoPickerDialog(
                                mActivityTestRule.getActivity(), contentResolver,
                                PhotoPickerDialogTest.this, multiselect, mimeTypes);
                        dialog.show();
                        return dialog;
                    }
                });

        mSelectionDelegate = dialog.getCategoryViewForTesting().getSelectionDelegateForTesting();
        if (!multiselect) mSelectionDelegate.setSingleSelectionMode();
        mSelectionDelegate.addObserver(this);
        mDialog = dialog;

        return dialog;
    }

    private PhotoPickerDialog createDialog(final boolean multiselect, final List<String> mimeTypes)
            throws Exception {
        return createDialogWithContentResolver(
                mActivityTestRule.getActivity().getContentResolver(), multiselect, mimeTypes);
    }

    private void waitForDecoder() throws Exception {
        int callCount = mOnDecoderReadyCallback.getCallCount();
        mOnDecoderReadyCallback.waitForCallback(
                callCount, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    private void waitForDecoderIdle() throws Exception {
        int callCount = mOnDecoderIdleCallback.getCallCount();
        mOnDecoderIdleCallback.waitForCallback(
                callCount, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    private void clickView(final int position, final int expectedSelectionCount) throws Exception {
        RecyclerView recyclerView = getRecyclerView();
        RecyclerViewTestUtils.waitForView(recyclerView, position);

        int callCount = mOnSelectionCallback.getCallCount();
        TouchCommon.singleClickView(
                recyclerView.findViewHolderForAdapterPosition(position).itemView);
        mOnSelectionCallback.waitForCallback(callCount, 1);

        // Validate the correct selection took place.
        Assert.assertEquals(expectedSelectionCount, mCurrentPhotoSelection.size());
        Assert.assertTrue(mSelectionDelegate.isItemSelected(mTestFiles.get(position)));
    }

    private void clickDone() throws Exception {
        mLastActionRecorded = PhotoPickerAction.NUM_ENTRIES;

        PhotoPickerToolbar toolbar = (PhotoPickerToolbar) mDialog.findViewById(R.id.action_bar);
        Button done = (Button) toolbar.findViewById(R.id.done);
        int callCount = mOnActionCallback.getCallCount();
        TouchCommon.singleClickView(done);
        mOnActionCallback.waitForCallback(callCount, 1);
        Assert.assertEquals(PhotoPickerAction.PHOTOS_SELECTED, mLastActionRecorded);
    }

    private void clickCancel() throws Exception {
        mLastActionRecorded = PhotoPickerAction.NUM_ENTRIES;

        PickerCategoryView categoryView = mDialog.getCategoryViewForTesting();
        View cancel = new View(mActivityTestRule.getActivity());
        int callCount = mOnActionCallback.getCallCount();
        categoryView.onClick(cancel);
        mOnActionCallback.waitForCallback(callCount, 1);
        Assert.assertEquals(PhotoPickerAction.CANCEL, mLastActionRecorded);
    }

    private void playVideo(Uri uri) throws Exception {
        int callCount = mOnVideoPlayingCallback.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mDialog.getCategoryViewForTesting().startVideoPlaybackAsync(uri); });
        mOnVideoPlayingCallback.waitForCallback(callCount, 1);
    }

    private void dismissDialog() {
        TestThreadUtils.runOnUiThreadBlocking(() -> mDialog.dismiss());
    }

    /**
     * Tests what happens when the ContentResolver returns a null cursor when query() is called (a
     * regression test for https://crbug.com/1072415).
     * Note: This test does not call setupTestFiles() so that the real FileEnumWorkerTask is used.
     */
    @Test
    @LargeTest
    public void testNoCrashWhenContentResolverQueryReturnsNull() throws Throwable {
        ContentResolver contentResolver = Mockito.mock(ContentResolver.class);
        Uri contentUri = MediaStore.Files.getContentUri("external");
        Mockito.doReturn(null)
                .when(contentResolver)
                .query(contentUri, new String[] {}, "", new String[] {}, "");

        createDialogWithContentResolver(
                contentResolver, false, Arrays.asList("image/*")); // Multi-select = false.
        Assert.assertTrue(mDialog.isShowing());
        waitForDecoder();

        // The test should not have crashed at this point, as per https://crbug.com/1072415,
        // so the loading should have aborted (gracefully) because the image cursor could not be
        // constructed.
        dismissDialog();
    }

    @Test
    @LargeTest
    public void testNoSelection() throws Throwable {
        setupTestFiles();
        createDialog(false, Arrays.asList("image/*")); // Multi-select = false.
        Assert.assertTrue(mDialog.isShowing());
        waitForDecoder();

        int expectedSelectionCount = 1;
        clickView(0, expectedSelectionCount);
        clickCancel();

        Assert.assertNull(mLastSelectedPhotos);
        Assert.assertEquals(PhotoPickerAction.CANCEL, mLastActionRecorded);

        dismissDialog();
    }

    @Test
    @LargeTest
    public void testSingleSelectionPhoto() throws Throwable {
        setupTestFiles();
        createDialog(false, Arrays.asList("image/*")); // Multi-select = false.
        Assert.assertTrue(mDialog.isShowing());
        waitForDecoder();

        // Expected selection count is 1 because clicking on a new view unselects other.
        int expectedSelectionCount = 1;

        // Click the first view.
        int callCount = mOnAnimatedCallback.getCallCount();
        clickView(0, expectedSelectionCount);
        mOnAnimatedCallback.waitForCallback(callCount, 1);

        // Click the second view.
        callCount = mOnAnimatedCallback.getCallCount();
        clickView(1, expectedSelectionCount);
        mOnAnimatedCallback.waitForCallback(callCount, 1);

        clickDone();

        Assert.assertEquals(1, mLastSelectedPhotos.length);
        Assert.assertEquals(PhotoPickerAction.PHOTOS_SELECTED, mLastActionRecorded);
        Assert.assertEquals(mTestFiles.get(1).getUri().getPath(), mLastSelectedPhotos[0].getPath());

        dismissDialog();
    }

    @Test
    @LargeTest
    public void testMultiSelectionPhoto() throws Throwable {
        setupTestFiles();
        createDialog(true, Arrays.asList("image/*")); // Multi-select = true.
        Assert.assertTrue(mDialog.isShowing());
        waitForDecoder();

        // Multi-selection is enabled, so each click is counted.
        int expectedSelectionCount = 1;

        // Click first view.
        int callCount = mOnAnimatedCallback.getCallCount();
        clickView(0, expectedSelectionCount++);
        mOnAnimatedCallback.waitForCallback(callCount, 1);

        // Click third view.
        callCount = mOnAnimatedCallback.getCallCount();
        clickView(2, expectedSelectionCount++);
        mOnAnimatedCallback.waitForCallback(callCount, 1);

        // Click fifth view.
        callCount = mOnAnimatedCallback.getCallCount();
        clickView(4, expectedSelectionCount++);
        mOnAnimatedCallback.waitForCallback(callCount, 1);

        clickDone();

        Assert.assertEquals(3, mLastSelectedPhotos.length);
        Assert.assertEquals(PhotoPickerAction.PHOTOS_SELECTED, mLastActionRecorded);
        Assert.assertEquals(mTestFiles.get(0).getUri().getPath(), mLastSelectedPhotos[0].getPath());
        Assert.assertEquals(mTestFiles.get(2).getUri().getPath(), mLastSelectedPhotos[1].getPath());
        Assert.assertEquals(mTestFiles.get(4).getUri().getPath(), mLastSelectedPhotos[2].getPath());

        dismissDialog();
    }

    @Test
    @LargeTest
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.N) // Video is only supported on N+.
    public void testVideoPlayerPlayAndRestart() throws Throwable {
        // Requesting to play a video is not a case of an accidental disk read on the UI thread.
        StrictMode.ThreadPolicy oldPolicy = TestThreadUtils.runOnUiThreadBlocking(
                () -> { return StrictMode.allowThreadDiskReads(); });

        try {
            setupTestFiles();
            createDialog(true, Arrays.asList("image/*")); // Multi-select = true.
            Assert.assertTrue(mDialog.isShowing());
            waitForDecoder();

            PickerCategoryView categoryView = mDialog.getCategoryViewForTesting();

            View container = categoryView.findViewById(R.id.playback_container);
            Assert.assertTrue(container.getVisibility() == View.GONE);

            // This test video takes one second to play.
            String fileName = "chrome/test/data/android/photo_picker/noogler_1sec.mp4";
            File file = new File(UrlUtils.getIsolatedTestFilePath(fileName));

            int callCount = mOnVideoEndedCallback.getCallCount();

            playVideo(Uri.fromFile(file));
            Assert.assertTrue(container.getVisibility() == View.VISIBLE);

            mOnVideoEndedCallback.waitForCallback(callCount, 1);

            TestThreadUtils.runOnUiThreadBlocking(() -> {
                View mute = categoryView.findViewById(R.id.mute);
                categoryView.getVideoPlayerForTesting().onClick(mute);
            });

            // Clicking the play button should restart playback.
            callCount = mOnVideoEndedCallback.getCallCount();

            TestThreadUtils.runOnUiThreadBlocking(() -> {
                View playbutton = categoryView.findViewById(R.id.video_player_play_button);
                categoryView.getVideoPlayerForTesting().onClick(playbutton);
            });

            mOnVideoEndedCallback.waitForCallback(callCount, 1);

            dismissDialog();
        } finally {
            TestThreadUtils.runOnUiThreadBlocking(() -> { StrictMode.setThreadPolicy(oldPolicy); });
        }
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testBorderPersistence() throws Exception {
        setupTestFilesWith80ColoredSquares();
        createDialog(false, Arrays.asList("image/*")); // Multi-select = false.
        waitForDecoderIdle();

        mRenderTestRule.render(mDialog.getCategoryViewForTesting(), "initial_load");

        // Click the first view.
        int expectedSelectionCount = 1;
        int callCount = mOnAnimatedCallback.getCallCount();
        clickView(0, expectedSelectionCount);
        mOnAnimatedCallback.waitForCallback(callCount, 1);

        mRenderTestRule.render(mDialog.getCategoryViewForTesting(), "first_view_clicked");

        // Now test that you can scroll the image out of view and back in again, and the selection
        // border should be maintained.
        RecyclerView recyclerView = getRecyclerView();
        RecyclerViewTestUtils.scrollToBottom(recyclerView);

        callCount = mOnAnimatedCallback.getCallCount();
        RecyclerViewTestUtils.scrollToView(recyclerView, 0);
        mOnAnimatedCallback.waitForCallback(callCount, 1);

        mRenderTestRule.render(mDialog.getCategoryViewForTesting(), "first_view_clicked");

        dismissDialog();
    }
}
