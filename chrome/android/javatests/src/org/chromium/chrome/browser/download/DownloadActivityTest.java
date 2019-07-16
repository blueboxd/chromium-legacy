// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.action.ViewActions.longClick;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withContentDescription;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;

import android.content.Intent;
import android.content.SharedPreferences.Editor;
import android.os.Handler;
import android.os.Looper;
import android.support.annotation.StringRes;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.rule.ActivityTestRule;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.text.TextUtils;
import android.view.View;
import android.widget.Spinner;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.download.ui.DownloadHistoryAdapter;
import org.chromium.chrome.browser.download.ui.DownloadHistoryItemViewHolder;
import org.chromium.chrome.browser.download.ui.DownloadHistoryItemWrapper;
import org.chromium.chrome.browser.download.ui.DownloadHistoryItemWrapper.OfflineItemWrapper;
import org.chromium.chrome.browser.download.ui.DownloadItemView;
import org.chromium.chrome.browser.download.ui.DownloadManagerToolbar;
import org.chromium.chrome.browser.download.ui.DownloadManagerUi;
import org.chromium.chrome.browser.download.ui.SpaceDisplay;
import org.chromium.chrome.browser.download.ui.StubbedProvider;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.widget.ListMenuButton.Item;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate.SelectionObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DisableAnimationsTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.HashMap;
import java.util.List;

/**
 * Tests the DownloadActivity and the DownloadManagerUi.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
public class DownloadActivityTest {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule disableAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public ActivityTestRule<DownloadActivity> mActivityTestRule =
            new ActivityTestRule<>(DownloadActivity.class);

    private static class TestObserver extends RecyclerView.AdapterDataObserver
            implements SelectionObserver<DownloadHistoryItemWrapper>, SpaceDisplay.Observer {
        public final CallbackHelper onChangedCallback = new CallbackHelper();
        public final CallbackHelper onSelectionCallback = new CallbackHelper();
        public final CallbackHelper onSpaceDisplayUpdatedCallback = new CallbackHelper();

        private List<DownloadHistoryItemWrapper> mOnSelectionItems;
        private Handler mHandler;

        public TestObserver() {
            mHandler = new Handler(Looper.getMainLooper());
        }

        @Override
        public void onChanged() {
            // To guarantee that all real Observers have had a chance to react to the event, post
            // the CallbackHelper.notifyCalled() call.
            mHandler.post(() -> onChangedCallback.notifyCalled());
        }

        @Override
        public void onSelectionStateChange(List<DownloadHistoryItemWrapper> selectedItems) {
            mOnSelectionItems = selectedItems;
            mHandler.post(() -> onSelectionCallback.notifyCalled());
        }

        @Override
        public void onSpaceDisplayUpdated(SpaceDisplay display) {
            mHandler.post(() -> onSpaceDisplayUpdatedCallback.notifyCalled());
        }
    }

    private static final String PREF_SHOW_STORAGE_INFO_HEADER =
            "download_home_show_storage_info_header";

    private StubbedProvider mStubbedProvider;
    private TestObserver mAdapterObserver;
    private DownloadManagerUi mUi;
    private DownloadHistoryAdapter mAdapter;

    private RecyclerView mRecyclerView;
    private TextView mSpaceUsedDisplay;

    @Before
    public void setUp() throws Exception {
        Editor editor = ContextUtils.getAppSharedPreferences().edit();
        editor.putBoolean(PREF_SHOW_STORAGE_INFO_HEADER, true).apply();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefServiceBridge.getInstance().setPromptForDownloadAndroid(
                    DownloadPromptStatus.DONT_SHOW);
        });

        HashMap<String, Boolean> features = new HashMap<String, Boolean>();
        features.put(ChromeFeatureList.DOWNLOADS_LOCATION_CHANGE, false);
        features.put(ChromeFeatureList.DOWNLOAD_HOME_SHOW_STORAGE_INFO, false);
        features.put(ChromeFeatureList.DOWNLOAD_HOME_V2, false);
        features.put(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY, false);
        features.put(ChromeFeatureList.OVERSCROLL_HISTORY_NAVIGATION, false);
        features.put(ChromeFeatureList.DOWNLOAD_OFFLINE_CONTENT_PROVIDER, false);
        features.put(ChromeFeatureList.DOWNLOAD_RENAME, false);
        ChromeFeatureList.setTestFeatures(features);

        mStubbedProvider = new StubbedProvider();
        DownloadManagerUi.setProviderForTests(mStubbedProvider);

        mAdapterObserver = new TestObserver();
        mStubbedProvider.getSelectionDelegate().addObserver(mAdapterObserver);

        startDownloadActivity();
        mUi = mActivityTestRule.getActivity().getDownloadManagerUiForTests();
        mStubbedProvider.setUIDelegate(mUi);
        mAdapter = mUi.getDownloadHistoryAdapterForTests();
        mAdapter.registerAdapterDataObserver(mAdapterObserver);

        mSpaceUsedDisplay = (TextView) mActivityTestRule.getActivity().findViewById(
                org.chromium.chrome.download.R.id.size_downloaded);
        mRecyclerView =
                ((RecyclerView) mActivityTestRule.getActivity().findViewById(R.id.recycler_view));

        mAdapter.getSpaceDisplayForTests().addObserverForTests(mAdapterObserver);
    }

    @Test
    @MediumTest
    public void testSpaceDisplay() throws Exception {
        // This first check is a Criteria because initialization of the Adapter is asynchronous.
        onView(withText("6.00 GB downloaded")).check(matches(isDisplayed()));

        // Add a new item.
        int callCount = mAdapterObserver.onChangedCallback.getCallCount();
        int spaceDisplayCallCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();
        final DownloadItem updateItem = StubbedProvider.createDownloadItem(7, "20151021 07:28");
        PostTask.runOrPostTask(
                UiThreadTaskTraits.DEFAULT, () -> mAdapter.onDownloadItemCreated(updateItem));
        mAdapterObserver.onChangedCallback.waitForCallback(callCount, 2);
        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(spaceDisplayCallCount);
        // Use Criteria here because the text for SpaceDisplay is updated through an AsyncTask.
        onView(withText("6.50 GB downloaded")).check(matches(isDisplayed()));

        // Mark one download as deleted on disk, which should prevent it from being counted.
        callCount = mAdapterObserver.onChangedCallback.getCallCount();
        spaceDisplayCallCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();
        final DownloadItem deletedItem = StubbedProvider.createDownloadItem(6, "20151021 07:28");
        deletedItem.setHasBeenExternallyRemoved(true);
        PostTask.runOrPostTask(
                UiThreadTaskTraits.DEFAULT, () -> mAdapter.onDownloadItemUpdated(deletedItem));
        mAdapterObserver.onChangedCallback.waitForCallback(callCount, 2);
        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(spaceDisplayCallCount);
        onView(withText("5.50 GB downloaded")).check(matches(isDisplayed()));

        // Say that the offline page has been deleted.
        callCount = mAdapterObserver.onChangedCallback.getCallCount();
        spaceDisplayCallCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();
        final OfflineItem deletedPage = StubbedProvider.createOfflineItem(3, "20151021 07:28");
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                () -> mStubbedProvider.getOfflineContentProvider().observer.onItemRemoved(
                                deletedPage.id));
        mAdapterObserver.onChangedCallback.waitForCallback(callCount, 2);
        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(spaceDisplayCallCount);
        onView(withText("512.00 MB downloaded")).check(matches(isDisplayed()));
    }

    /**
     * Clicking on filters affects various things in the UI.
     */
    @DisabledTest(message = "crbug.com/855389")
    @Test
    @MediumTest
    public void testFilters() throws Exception {
        // This first check is a Criteria because initialization of the Adapter is asynchronous.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return TextUtils.equals("6.00 GB downloaded", mSpaceUsedDisplay.getText());
            }
        });

        // Change the filter to Pages. Only the space display, offline page and the date header
        // should stay.
        int spaceDisplayCallCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();
        clickOnFilter(mUi, 1);
        Assert.assertEquals(3, mAdapter.getItemCount());

        // Check that the number of items displayed is correct.
        // We need to poll because RecyclerView doesn't animate changes immediately.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mRecyclerView.getChildCount() == 3;
            }
        });

        // Filtering doesn't affect the total download size.
        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(spaceDisplayCallCount);
        Assert.assertEquals("6.00 GB downloaded", mSpaceUsedDisplay.getText());
    }

    @Test
    @MediumTest
    @RetryOnFailure
    @FlakyTest(message = "crbug.com/854241")
    public void testDeleteFiles() throws Exception {
        SnackbarManager.setDurationForTesting(1);

        // This first check is a Criteria because initialization of the Adapter is asynchronous.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return TextUtils.equals("6.00 GB downloaded", mSpaceUsedDisplay.getText());
            }
        });

        // Select the first two items.
        toggleItemSelection(2);
        toggleItemSelection(3);

        // Click the delete button, which should delete the items and reset the toolbar.
        Assert.assertEquals(12, mAdapter.getItemCount());
        // checkForExternallyRemovedFiles() should have been called once already in onResume().
        Assert.assertEquals(
                1, mStubbedProvider.getDownloadDelegate().checkExternalCallback.getCallCount());
        Assert.assertEquals(
                0, mStubbedProvider.getDownloadDelegate().removeDownloadCallback.getCallCount());
        Assert.assertEquals(
                0, mStubbedProvider.getOfflineContentProvider().deleteItemCallback.getCallCount());
        int callCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                () -> Assert.assertTrue(
                                mUi.getDownloadManagerToolbarForTests()
                                        .getMenu()
                                        .performIdentifierAction(
                                                R.id.selection_mode_delete_menu_id, 0)));

        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(callCount);
        Assert.assertEquals(
                1, mStubbedProvider.getDownloadDelegate().checkExternalCallback.getCallCount());
        Assert.assertFalse(mStubbedProvider.getSelectionDelegate().isSelectionEnabled());
        Assert.assertEquals(9, mAdapter.getItemCount());
        Assert.assertEquals("0.65 KB downloaded", mSpaceUsedDisplay.getText());
    }

    @Test
    @MediumTest
    @RetryOnFailure
    @FlakyTest(message = "crbug.com/855219")
    public void testDeleteFileFromMenu() throws Exception {
        SnackbarManager.setDurationForTesting(1);

        // This first check is a Criteria because initialization of the Adapter is asynchronous.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return TextUtils.equals("6.00 GB downloaded", mSpaceUsedDisplay.getText());
            }
        });

        Assert.assertEquals(12, mAdapter.getItemCount());
        // checkForExternallyRemovedFiles() should have been called once already in onResume().
        Assert.assertEquals(
                1, mStubbedProvider.getDownloadDelegate().checkExternalCallback.getCallCount());
        Assert.assertEquals(
                0, mStubbedProvider.getDownloadDelegate().removeDownloadCallback.getCallCount());
        Assert.assertEquals(
                0, mStubbedProvider.getOfflineContentProvider().deleteItemCallback.getCallCount());
        int callCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();

        // Simulate a delete context menu action on the item.
        simulateContextMenu(2, R.string.delete);
        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(callCount);
        Assert.assertEquals(
                1, mStubbedProvider.getDownloadDelegate().checkExternalCallback.getCallCount());
        Assert.assertFalse(mStubbedProvider.getSelectionDelegate().isSelectionEnabled());
        Assert.assertEquals(11, mAdapter.getItemCount());
        Assert.assertEquals("5.00 GB downloaded", mSpaceUsedDisplay.getText());
    }

    @DisabledTest(message = "crbug.com/855389")
    @Test
    @MediumTest
    @RetryOnFailure
    public void testUndoDelete() throws Exception {
        // Adapter positions:
        // 0 = space display
        // 1 = date
        // 2 = download item #7
        // 3 = download item #8
        // 4 = date
        // 5 = download item #6
        // 6 = offline page #3

        SnackbarManager.setDurationForTesting(5000);

        // Add duplicate items.
        int callCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();
        final DownloadItem item7 = StubbedProvider.createDownloadItem(7, "20161021 07:28");
        final DownloadItem item8 = StubbedProvider.createDownloadItem(8, "20161021 17:28");
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            mAdapter.onDownloadItemCreated(item7);
            mAdapter.onDownloadItemCreated(item8);
        });

        // The criteria is needed because an AsyncTask is fired to update the space display, which
        // can result in either 1 or 2 updates.
        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(callCount);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return TextUtils.equals("6.50 GB downloaded", mSpaceUsedDisplay.getText());
            }
        });

        // Select download item #7 and offline page #3.
        toggleItemSelection(2);
        toggleItemSelection(6);

        Assert.assertEquals(15, mAdapter.getItemCount());

        // Click the delete button.
        callCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                () -> Assert.assertTrue(
                                mUi.getDownloadManagerToolbarForTests()
                                        .getMenu()
                                        .performIdentifierAction(
                                                R.id.selection_mode_delete_menu_id, 0)));
        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(callCount);

        // Assert that items are temporarily removed from the adapter. The two selected items,
        // one duplicate item, and one date bucket should be removed.
        Assert.assertEquals(11, mAdapter.getItemCount());
        Assert.assertEquals("1.00 GB downloaded", mSpaceUsedDisplay.getText());

        // Click "Undo" on the snackbar.
        callCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();
        final View rootView = mUi.getView().getRootView();
        Assert.assertNotNull(rootView.findViewById(R.id.snackbar));
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                (Runnable) () -> rootView.findViewById(R.id.snackbar_button).callOnClick());

        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(callCount);

        // Assert that items are restored.
        Assert.assertEquals(
                0, mStubbedProvider.getDownloadDelegate().removeDownloadCallback.getCallCount());
        Assert.assertEquals(
                0, mStubbedProvider.getOfflineContentProvider().deleteItemCallback.getCallCount());
        Assert.assertFalse(mStubbedProvider.getSelectionDelegate().isSelectionEnabled());
        Assert.assertEquals(15, mAdapter.getItemCount());
        Assert.assertEquals("6.50 GB downloaded", mSpaceUsedDisplay.getText());
    }

    @DisabledTest(message = "crbug.com/855389")
    @Test
    @MediumTest
    @RetryOnFailure
    public void testUndoDeleteFromMenu() throws Exception {
        // Adapter positions:
        // 0 = space display
        // 1 = date
        // 2 = download item #7
        // 3 = download item #8
        // 4 = date
        // 5 = download item #6
        // 6 = offline page #3

        SnackbarManager.setDurationForTesting(5000);

        // Add duplicate items.
        int callCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();
        final DownloadItem item7 = StubbedProvider.createDownloadItem(7, "20161021 07:28");
        final DownloadItem item8 = StubbedProvider.createDownloadItem(8, "20161021 17:28");
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            mAdapter.onDownloadItemCreated(item7);
            mAdapter.onDownloadItemCreated(item8);
        });

        // The criteria is needed because an AsyncTask is fired to update the space display, which
        // can result in either 1 or 2 updates.
        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(callCount);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return TextUtils.equals("6.50 GB downloaded", mSpaceUsedDisplay.getText());
            }
        });

        Assert.assertEquals(15, mAdapter.getItemCount());
        callCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();

        // Simulate a delete context menu action on the item.
        simulateContextMenu(2, R.string.delete);
        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(callCount);

        // Assert that items are temporarily removed from the adapter. The two selected items,
        // one duplicate item, and one date bucket should be removed.
        Assert.assertEquals(12, mAdapter.getItemCount());
        Assert.assertEquals("6.00 GB downloaded", mSpaceUsedDisplay.getText());

        // Click "Undo" on the snackbar.
        callCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();
        final View rootView = mUi.getView().getRootView();
        Assert.assertNotNull(rootView.findViewById(R.id.snackbar));
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                (Runnable) () -> rootView.findViewById(R.id.snackbar_button).callOnClick());

        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(callCount);

        // Assert that items are restored.
        Assert.assertEquals(
                0, mStubbedProvider.getDownloadDelegate().removeDownloadCallback.getCallCount());
        Assert.assertEquals(
                0, mStubbedProvider.getOfflineContentProvider().deleteItemCallback.getCallCount());
        Assert.assertFalse(mStubbedProvider.getSelectionDelegate().isSelectionEnabled());
        Assert.assertEquals(15, mAdapter.getItemCount());
        Assert.assertEquals("6.50 GB downloaded", mSpaceUsedDisplay.getText());
    }

    @DisabledTest(message = "crbug.com/855389")
    @Test
    @MediumTest
    @RetryOnFailure
    public void testUndoDeleteDuplicatesSelected() throws Exception {
        // Adapter positions:
        // 0 = space display
        // 1 = date
        // 2 = download item #7
        // 3 = download item #8
        // ....

        SnackbarManager.setDurationForTesting(5000);

        // Add duplicate items.
        int callCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();
        final DownloadItem item7 = StubbedProvider.createDownloadItem(7, "20161021 07:28");
        final DownloadItem item8 = StubbedProvider.createDownloadItem(8, "20161021 17:28");
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            mAdapter.onDownloadItemCreated(item7);
            mAdapter.onDownloadItemCreated(item8);
        });

        // The criteria is needed because an AsyncTask is fired to update the space display, which
        // can result in either 1 or 2 updates.
        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(callCount);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return TextUtils.equals("6.50 GB downloaded", mSpaceUsedDisplay.getText());
            }
        });

        // Select download item #7 and download item #8.
        toggleItemSelection(2);
        toggleItemSelection(3);

        Assert.assertEquals(15, mAdapter.getItemCount());

        // Click the delete button.
        callCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                () -> Assert.assertTrue(
                                mUi.getDownloadManagerToolbarForTests()
                                        .getMenu()
                                        .performIdentifierAction(
                                                R.id.selection_mode_delete_menu_id, 0)));
        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(callCount);

        // Assert that the two items and their date bucket are temporarily removed from the adapter.
        Assert.assertEquals(12, mAdapter.getItemCount());
        Assert.assertEquals("6.00 GB downloaded", mSpaceUsedDisplay.getText());

        // Click "Undo" on the snackbar.
        callCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();
        final View rootView = mUi.getView().getRootView();
        Assert.assertNotNull(rootView.findViewById(R.id.snackbar));
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                (Runnable) () -> rootView.findViewById(R.id.snackbar_button).callOnClick());

        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(callCount);

        // Assert that items are restored.
        Assert.assertEquals(15, mAdapter.getItemCount());
        Assert.assertEquals("6.50 GB downloaded", mSpaceUsedDisplay.getText());
    }

    @Test
    @MediumTest
    @DisableFeatures("OfflinePagesSharing")
    @FlakyTest(message = "crbug.com/855167")
    public void testShareFiles() throws Exception {
        // Adapter positions:
        // 0 = space display
        // 1 = date
        // 2 = download item #6
        // 3 = offline page #3
        // 4 = date
        // 5 = download item #3
        // 6 = download item #4
        // 7 = download item #5
        // 8 = date
        // 9 = download item #0
        // 10 = download item #1
        // 11 = download item #2

        // Select an image, download item #6.
        toggleItemSelection(2);
        Intent shareIntent = DownloadUtils.createShareIntent(
                mUi.getBackendProvider().getSelectionDelegate().getSelectedItemsAsList(), null);
        Assert.assertEquals("Incorrect intent action", Intent.ACTION_SEND, shareIntent.getAction());
        Assert.assertEquals("Incorrect intent mime type", "image/png", shareIntent.getType());
        Assert.assertNotNull(
                "Intent expected to have stream", shareIntent.getExtras().get(Intent.EXTRA_STREAM));
        Assert.assertNull("Intent not expected to have parcelable ArrayList",
                shareIntent.getParcelableArrayListExtra(Intent.EXTRA_STREAM));

        // Scroll to ensure the item at position 8 is visible.
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> mRecyclerView.scrollToPosition(9));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Select another image, download item #0.
        toggleItemSelection(9);
        shareIntent = DownloadUtils.createShareIntent(
                mUi.getBackendProvider().getSelectionDelegate().getSelectedItemsAsList(), null);
        Assert.assertEquals(
                "Incorrect intent action", Intent.ACTION_SEND_MULTIPLE, shareIntent.getAction());
        Assert.assertEquals("Incorrect intent mime type", "image/*", shareIntent.getType());
        Assert.assertEquals("Intent expected to have parcelable ArrayList", 2,
                shareIntent.getParcelableArrayListExtra(Intent.EXTRA_STREAM).size());

        // Scroll to ensure the item at position 5 is visible.
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> mRecyclerView.scrollToPosition(6));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Select non-image item, download item #4.
        toggleItemSelection(6);
        shareIntent = DownloadUtils.createShareIntent(
                mUi.getBackendProvider().getSelectionDelegate().getSelectedItemsAsList(), null);
        Assert.assertEquals(
                "Incorrect intent action", Intent.ACTION_SEND_MULTIPLE, shareIntent.getAction());
        Assert.assertEquals("Incorrect intent mime type", "*/*", shareIntent.getType());
        Assert.assertEquals("Intent expected to have parcelable ArrayList", 3,
                shareIntent.getParcelableArrayListExtra(Intent.EXTRA_STREAM).size());

        // Scroll to ensure the item at position 2 is visible.
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> mRecyclerView.scrollToPosition(3));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Select an offline page #3.
        toggleItemSelection(3);
        shareIntent = DownloadUtils.createShareIntent(
                mUi.getBackendProvider().getSelectionDelegate().getSelectedItemsAsList(), null);
        Assert.assertEquals(
                "Incorrect intent action", Intent.ACTION_SEND_MULTIPLE, shareIntent.getAction());
        Assert.assertEquals("Incorrect intent mime type", "*/*", shareIntent.getType());
        Assert.assertEquals("Intent expected to have parcelable ArrayList", 3,
                shareIntent.getParcelableArrayListExtra(Intent.EXTRA_STREAM).size());
        Assert.assertEquals("Intent expected to have plain text for offline page URL",
                "https://thangs.com",
                IntentUtils.safeGetStringExtra(shareIntent, Intent.EXTRA_TEXT));
    }

    // TODO(carlosk): OfflineItems used here come from StubbedProvider so this might not be the best
    // place to test peer-2-peer sharing.
    @DisabledTest(message = "crbug.com/855389")
    @Test
    @MediumTest
    @EnableFeatures("OfflinePagesSharing")
    public void testShareOfflinePageWithP2PSharingEnabled() throws Exception {
        // Scroll to ensure the item at position 2 is visible.
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> mRecyclerView.scrollToPosition(3));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Select the offline page located at position #3.
        toggleItemSelection(3);
        List<DownloadHistoryItemWrapper> selectedItems =
                mUi.getBackendProvider().getSelectionDelegate().getSelectedItemsAsList();
        Assert.assertEquals("There should be only one item selected", 1, selectedItems.size());
        Intent shareIntent = DownloadUtils.createShareIntent(selectedItems, null);

        Assert.assertEquals("Incorrect intent action", Intent.ACTION_SEND, shareIntent.getAction());
        Assert.assertEquals("Incorrect intent mime type", "*/*", shareIntent.getType());
        Assert.assertNotNull("Intent expected to have parcelable ArrayList",
                shareIntent.getParcelableExtra(Intent.EXTRA_STREAM));
        Assert.assertEquals("Intent expected to have parcelable Uri",
                "file:///data/fake_path/Downloads/4",
                shareIntent.getParcelableExtra(Intent.EXTRA_STREAM).toString());
        Assert.assertNull("Intent expected to not have any text for offline page",
                IntentUtils.safeGetStringExtra(shareIntent, Intent.EXTRA_TEXT));

        // Pass a map that contains a new file path.
        HashMap<String, String> newFilePathMap = new HashMap<String, String>();
        newFilePathMap.put(((OfflineItemWrapper) selectedItems.get(0)).getId(),
                "/data/new_fake_path/Downloads/4");
        shareIntent = DownloadUtils.createShareIntent(selectedItems, newFilePathMap);

        Assert.assertEquals("Incorrect intent action", Intent.ACTION_SEND, shareIntent.getAction());
        Assert.assertEquals("Incorrect intent mime type", "*/*", shareIntent.getType());
        Assert.assertNotNull("Intent expected to have parcelable ArrayList",
                shareIntent.getParcelableExtra(Intent.EXTRA_STREAM));
        Assert.assertEquals("Intent expected to have parcelable Uri",
                "file:///data/new_fake_path/Downloads/4",
                shareIntent.getParcelableExtra(Intent.EXTRA_STREAM).toString());
        Assert.assertNull("Intent expected to not have any text for offline page",
                IntentUtils.safeGetStringExtra(shareIntent, Intent.EXTRA_TEXT));
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.DOWNLOADS_LOCATION_CHANGE)
    public void testLongClickItem() throws Exception {
        // The selection toolbar should not be showing.
        onView(withContentDescription("Cancel selection")).check(doesNotExist());
        onView(withId(R.id.close_menu_id)).check(matches(isDisplayed()));
        onView(withId(R.id.selection_mode_number)).check(matches(not(isDisplayed())));
        onView(withId(org.chromium.chrome.download.R.id.selection_mode_share_menu_id))
                .check(doesNotExist());
        onView(withId(R.id.selection_mode_delete_menu_id)).check(doesNotExist());
        Assert.assertFalse(mStubbedProvider.getSelectionDelegate().isSelectionEnabled());

        // Select an item.
        onView(withText("huge_image.png")).perform(longClick());

        // The toolbar should flip states to allow doing things with the selected items.
        onView(withId(R.id.close_menu_id)).check(doesNotExist());
        onView(withId(R.id.selection_mode_number)).check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.download.R.id.selection_mode_share_menu_id))
                .check(matches(isDisplayed()));
        onView(withId(R.id.selection_mode_delete_menu_id)).check(matches(isDisplayed()));
        Assert.assertTrue(mStubbedProvider.getSelectionDelegate().isSelectionEnabled());

        // Deselect the same item.
        onView(withText("huge_image.png")).perform(longClick());

        // The toolbar should flip back.
        onView(withContentDescription("Cancel selection")).check(doesNotExist());
        onView(withId(R.id.close_menu_id)).check(matches(isDisplayed()));
        onView(withId(R.id.selection_mode_number)).check(matches(not(isDisplayed())));
        onView(withId(org.chromium.chrome.download.R.id.selection_mode_share_menu_id))
                .check(doesNotExist());
        onView(withId(R.id.selection_mode_delete_menu_id)).check(doesNotExist());
        Assert.assertFalse(mStubbedProvider.getSelectionDelegate().isSelectionEnabled());
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.DOWNLOADS_LOCATION_CHANGE)
    public void testSearchView() throws Exception {
        final DownloadManagerToolbar toolbar = mUi.getDownloadManagerToolbarForTests();
        onView(withId(R.id.search_text)).check(matches(not(isDisplayed())));

        onView(withText("huge_image.png")).perform(longClick());

        Assert.assertTrue(mStubbedProvider.getSelectionDelegate().isSelectionEnabled());

        int callCount = mAdapterObserver.onSelectionCallback.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> toolbar.getMenu().performIdentifierAction(R.id.search_menu_id, 0));

        // The selection should be cleared when a search is started.
        mAdapterObserver.onSelectionCallback.waitForCallback(callCount, 1);
        Assert.assertFalse(mStubbedProvider.getSelectionDelegate().isSelectionEnabled());
        onView(withId(R.id.search_text)).check(matches(isDisplayed()));

        // Select an item and assert that the search view is no longer showing.
        onView(withText("huge_image.png")).perform(longClick());
        Assert.assertTrue(mStubbedProvider.getSelectionDelegate().isSelectionEnabled());
        onView(withId(R.id.search_text)).check(matches(not(isDisplayed())));

        // Clear the selection and assert that the search view is showing again.
        onView(withText("huge_image.png")).perform(longClick());
        Assert.assertFalse(mStubbedProvider.getSelectionDelegate().isSelectionEnabled());
        onView(withId(R.id.search_text)).check(matches(isDisplayed()));

        // Close the search view.
        onView(withContentDescription("Go back")).perform(click());
        onView(withId(R.id.search_text)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.DOWNLOADS_LOCATION_CHANGE)
    public void testSpinner() throws Exception {
        // Open spinner.
        onView(withId(org.chromium.chrome.download.R.id.spinner)).perform(click());

        // Check all TextViews displayed.
        onView(withText("All")).check(matches(isDisplayed()));
        onView(withText("Pages")).check(matches(isDisplayed()));
        onView(withText("Video")).check(matches(isDisplayed()));
        onView(withText("Audio")).check(matches(isDisplayed()));
        onView(withText("Images")).check(matches(isDisplayed()));
        onView(withText("Documents")).check(matches(isDisplayed()));
        onView(withText("Other")).check(matches(isDisplayed()));

        // Click Pages.
        onView(withText("Pages")).perform(click());
        onView(withText("page 4")).check(matches(isDisplayed()));

        // Click Video.
        onView(withId(org.chromium.chrome.download.R.id.spinner)).perform(click());
        onView(withText("Video")).perform(click());
        onView(withText("four.webm")).check(matches(isDisplayed()));

        // Click Audio.
        onView(withId(org.chromium.chrome.download.R.id.spinner)).perform(click());
        onView(withText("Audio")).perform(click());
        onView(withText("five.mp3")).check(matches(isDisplayed()));
        onView(withText("six.mp3")).check(matches(isDisplayed()));

        // Click Images.
        onView(withId(org.chromium.chrome.download.R.id.spinner)).perform(click());
        onView(withText("Images")).perform(click());
        onView(withText("huge_image.png")).check(matches(isDisplayed()));
        onView(withText("first_file.jpg")).check(matches(isDisplayed()));
        onView(withText("second_file.gif")).check(matches(isDisplayed()));

        // Click Documents.
        onView(withId(org.chromium.chrome.download.R.id.spinner)).perform(click());
        onView(withText("Documents")).perform(click());
        onView(withText("third_file")).check(matches(isDisplayed()));

        // Click Other.
        onView(withId(org.chromium.chrome.download.R.id.spinner)).perform(click());
        onView(withText("Other")).perform(click());
        onView(withText("Files that you download appear here")).check(matches(isDisplayed()));
    }

    private DownloadActivity startDownloadActivity() throws Exception {
        // Load up the downloads lists.
        DownloadItem item0 = StubbedProvider.createDownloadItem(0, "19551112 06:38");
        DownloadItem item1 = StubbedProvider.createDownloadItem(1, "19551112 06:38");
        DownloadItem item2 = StubbedProvider.createDownloadItem(2, "19551112 06:38");
        DownloadItem item3 = StubbedProvider.createDownloadItem(3, "19851026 09:00");
        DownloadItem item4 = StubbedProvider.createDownloadItem(4, "19851026 09:00");
        DownloadItem item5 = StubbedProvider.createDownloadItem(5, "19851026 09:00");
        DownloadItem item6 = StubbedProvider.createDownloadItem(6, "20151021 07:28");
        OfflineItem item7 = StubbedProvider.createOfflineItem(3, "20151021 07:28");
        mStubbedProvider.getDownloadDelegate().regularItems.add(item0);
        mStubbedProvider.getDownloadDelegate().regularItems.add(item1);
        mStubbedProvider.getDownloadDelegate().regularItems.add(item2);
        mStubbedProvider.getDownloadDelegate().regularItems.add(item3);
        mStubbedProvider.getDownloadDelegate().regularItems.add(item4);
        mStubbedProvider.getDownloadDelegate().regularItems.add(item5);
        mStubbedProvider.getDownloadDelegate().regularItems.add(item6);
        mStubbedProvider.getOfflineContentProvider().items.add(item7);

        // Start the activity up.
        Intent intent = new Intent();
        intent.setClass(InstrumentationRegistry.getTargetContext(), DownloadActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return mActivityTestRule.launchActivity(intent);
    }

    private void clickOnFilter(final DownloadManagerUi ui, final int position) throws Exception {
        int previousCount = mAdapterObserver.onChangedCallback.getCallCount();
        final Spinner spinner = mUi.getDownloadManagerToolbarForTests().getSpinnerForTests();
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            spinner.performClick();
            spinner.setSelection(position);
        });
        mAdapterObserver.onChangedCallback.waitForCallback(previousCount);
    }

    private void toggleItemSelection(int position) throws Exception {
        int callCount = mAdapterObserver.onSelectionCallback.getCallCount();
        final DownloadItemView itemView = getView(position);
        PostTask.runOrPostTask(
                UiThreadTaskTraits.DEFAULT, (Runnable) () -> itemView.performLongClick());
        mAdapterObserver.onSelectionCallback.waitForCallback(callCount, 1);
    }

    private void simulateContextMenu(int position, @StringRes int text) throws Exception {
        final DownloadItemView view = getView(position);
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, (Runnable) () -> {
            Item[] items = view.getItems();
            for (Item item : items) {
                if (item.getTextId() == text) {
                    view.onItemSelected(item);
                    return;
                }
            }
            throw new IllegalStateException("Context menu option not found " + text);
        });
    }

    private DownloadItemView getView(int position) throws Exception {
        int callCount = mAdapterObserver.onSelectionCallback.getCallCount();
        ViewHolder mostRecentHolder = mRecyclerView.findViewHolderForAdapterPosition(position);
        Assert.assertTrue(mostRecentHolder instanceof DownloadHistoryItemViewHolder);
        return ((DownloadHistoryItemViewHolder) mostRecentHolder).getItemView();
    }
}
