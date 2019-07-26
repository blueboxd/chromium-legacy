// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.View;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.NightModeTestUtils;
import org.chromium.chrome.browser.widget.ListMenuButton;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;

/**
 * Tests for the bookmark manager.
 */
// clang-format off
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ChromeFeatureList.REORDER_BOOKMARKS)
@RetryOnFailure
public class BookmarkReorderTest extends BookmarkTest {
    // clang-format on

    // This class extends BookmarkTest because some new features are being added, but all of the
    // old functionality should remain; thus, we want to also run all of the old tests.
    // This seemed the most elegant and efficient way to accomplish this.
    // TODO(crbug.com/160194): Clean up after bookmark reordering launches.

    private static final String TEST_TITLE_A = "a";
    private static final String TEST_URL_A = "http://a.com";
    private static final String TAG = "BookmarkReorderTest";

    @Override
    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Test
    @MediumTest
    public void testEndIconVisibilityInSelectionMode() throws Exception {
        BookmarkId testId = addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_TITLE_A, TEST_URL_A);

        openBookmarkManager();

        BookmarkRow test =
                (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(2).itemView;
        View testMoreButton = test.findViewById(R.id.more);
        View testDragHandle = test.findViewById(R.id.drag_handle);

        View testFolderA = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        View aMoreButton = testFolderA.findViewById(R.id.more);
        View aDragHandle = testFolderA.findViewById(R.id.drag_handle);

        toggleSelectionAndEndAnimation(testId, test);

        // Callback occurs when Item "test" is selected.
        CriteriaHelper.pollUiThread(test::isChecked, "Expected item \"test\" to become selected");

        Assert.assertEquals("Expected more button of selected item to be gone when drag is active.",
                View.GONE, testMoreButton.getVisibility());
        Assert.assertEquals(
                "Expected drag handle of selected item to be visible when drag is active.",
                View.VISIBLE, testDragHandle.getVisibility());
        Assert.assertTrue("Expected drag handle to be enabled when drag is active.",
                testDragHandle.isEnabled());

        Assert.assertEquals(
                "Expected more button of unselected item to be gone when drag is active.",
                View.GONE, aMoreButton.getVisibility());
        Assert.assertEquals(
                "Expected drag handle of unselected item to be visible when drag is active.",
                View.VISIBLE, aDragHandle.getVisibility());
        Assert.assertFalse(
                "Expected drag handle of unselected item to be disabled when drag is active.",
                aDragHandle.isEnabled());
    }

    @Test
    @MediumTest
    public void testEndIconVisiblityInSearchMode() throws Exception {
        BookmarkId testId = addFolder(TEST_FOLDER_TITLE);
        addFolder(TEST_TITLE_A);

        openBookmarkManager();

        View searchButton = mManager.getToolbarForTests().findViewById(R.id.search_menu_id);

        BookmarkRow test =
                (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(2).itemView;
        View testMoreButton = test.findViewById(R.id.more);
        View testDragHandle = test.findViewById(R.id.drag_handle);

        View a = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        View aMoreButton = a.findViewById(R.id.more);
        View aDragHandle = a.findViewById(R.id.drag_handle);

        TestThreadUtils.runOnUiThreadBlocking(searchButton::performClick);

        // Callback occurs when Item "test" is selected.
        CriteriaHelper.pollUiThread(()
                                            -> mBookmarkActivity.getManagerForTesting()
                                                       .getToolbarForTests()
                                                       .isSearching(),
                "Expected to enter search mode");

        toggleSelectionAndEndAnimation(testId, test);

        // Callback occurs when Item "test" is selected.
        CriteriaHelper.pollUiThread(test::isChecked, "Expected item \"test\" to become selected");

        Assert.assertEquals("Expected drag handle of selected item to be gone "
                        + "when selection mode is activated from search.",
                View.GONE, testDragHandle.getVisibility());
        Assert.assertEquals("Expected more button of selected item to be visible "
                        + "when selection mode is activated from search.",
                View.VISIBLE, testMoreButton.getVisibility());
        Assert.assertFalse("Expected more button of selected item to be disabled "
                        + "when selection mode is activated from search.",
                testMoreButton.isEnabled());

        Assert.assertEquals("Expected drag handle of unselected item to be gone "
                        + "when selection mode is activated from search.",
                View.GONE, aDragHandle.getVisibility());
        Assert.assertEquals("Expected more button of unselected item to be visible "
                        + "when selection mode is activated from search.",
                View.VISIBLE, aMoreButton.getVisibility());
        Assert.assertFalse("Expected more button of unselected item to be disabled "
                        + "when selection mode is activated from search.",
                aMoreButton.isEnabled());
    }

    @Test
    @MediumTest
    public void testSmallDrag_Up_BookmarksOnly() throws Exception {
        List<BookmarkId> initial = new ArrayList<>();
        List<BookmarkId> expected = new ArrayList<>();
        BookmarkId fooId = addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo);
        BookmarkId googleId = addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage);
        BookmarkId aId = addBookmark(TEST_TITLE_A, TEST_URL_A);

        // When bookmarks are added, they are added to the top of the list.
        // The current bookmark order is the reverse of the order in which they were added.
        initial.add(aId);
        initial.add(googleId);
        initial.add(fooId);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Bookmarks were not added in the expected order.", initial,
                    mBookmarkModel.getChildIDs(mBookmarkModel.getDefaultFolder(), true, true)
                            .subList(0, 3));
        });

        expected.add(fooId);
        expected.add(aId);
        expected.add(googleId);

        openBookmarkManager();

        // Callback occurs upon changes inside of the bookmark model.
        CallbackHelper modelReorderHelper = new CallbackHelper();
        BookmarkBridge.BookmarkModelObserver bookmarkModelObserver =
                new BookmarkBridge.BookmarkModelObserver() {
                    @Override
                    public void bookmarkModelChanged() {
                        modelReorderHelper.notifyCalled();
                    }
                };

        // Perform registration to make callbacks work.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBookmarkModel.addObserver(bookmarkModelObserver);
        });

        View foo = mItemsContainer.findViewHolderForAdapterPosition(3).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_PAGE_TITLE_FOO,
                ((BookmarkItemRow) foo).getTitle());
        toggleSelectionAndEndAnimation(fooId, (BookmarkRow) foo);

        // Starts as last bookmark (2nd index) and ends as 0th bookmark (promo header not included).
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ((ReorderBookmarkItemsAdapter) mItemsContainer.getAdapter()).simulateDragForTests(3, 1);
        });

        modelReorderHelper.waitForCallback(0, 1);
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<BookmarkId> observed =
                    mBookmarkModel.getChildIDs(mBookmarkModel.getDefaultFolder(), true, true);
            // Exclude partner bookmarks folder
            Assert.assertEquals(expected, observed.subList(0, 3));
        });
    }

    @Test
    @MediumTest
    public void testSmallDrag_Down_FoldersOnly() throws Exception {
        List<BookmarkId> initial = new ArrayList<>();
        List<BookmarkId> expected = new ArrayList<>();
        BookmarkId aId = addFolder("a");
        BookmarkId bId = addFolder("b");
        BookmarkId cId = addFolder("c");
        BookmarkId testId = addFolder(TEST_FOLDER_TITLE);

        initial.add(testId);
        initial.add(cId);
        initial.add(bId);
        initial.add(aId);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Bookmarks were not added in the expected order.", initial,
                    mBookmarkModel.getChildIDs(mBookmarkModel.getDefaultFolder(), true, true)
                            .subList(0, 4));
        });

        expected.add(cId);
        expected.add(bId);
        expected.add(aId);
        expected.add(testId);

        openBookmarkManager();

        // Callback occurs upon changes inside of the bookmark model.
        CallbackHelper modelReorderHelper = new CallbackHelper();
        BookmarkBridge.BookmarkModelObserver bookmarkModelObserver =
                new BookmarkBridge.BookmarkModelObserver() {
                    @Override
                    public void bookmarkModelChanged() {
                        modelReorderHelper.notifyCalled();
                    }
                };

        // Perform registration to make callbacks work.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBookmarkModel.addObserver(bookmarkModelObserver);
        });

        View test = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE,
                ((BookmarkFolderRow) test).getTitle());

        toggleSelectionAndEndAnimation(testId, (BookmarkRow) test);

        // Starts as 0th bookmark (not counting promo header) and ends as last (index 3).
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ((ReorderBookmarkItemsAdapter) mItemsContainer.getAdapter()).simulateDragForTests(1, 4);
        });

        modelReorderHelper.waitForCallback(0, 1);
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<BookmarkId> observed =
                    mBookmarkModel.getChildIDs(mBookmarkModel.getDefaultFolder(), true, true);
            // Exclude partner bookmarks folder
            Assert.assertEquals(expected, observed.subList(0, 4));
        });
    }

    @Test
    @MediumTest
    public void testSmallDrag_Down_MixedFoldersAndBookmarks() throws Exception {
        List<BookmarkId> initial = new ArrayList<>();
        List<BookmarkId> expected = new ArrayList<>();
        BookmarkId aId = addFolder("a");
        BookmarkId bId = addBookmark("b", "http://b.com");
        BookmarkId testId = addFolder(TEST_FOLDER_TITLE);

        initial.add(testId);
        initial.add(bId);
        initial.add(aId);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Bookmarks were not added in the expected order.", initial,
                    mBookmarkModel.getChildIDs(mBookmarkModel.getDefaultFolder(), true, true)
                            .subList(0, 3));
        });

        expected.add(bId);
        expected.add(testId);
        expected.add(aId);

        openBookmarkManager();

        // Callback occurs upon changes inside of the bookmark model.
        CallbackHelper modelReorderHelper = new CallbackHelper();
        BookmarkBridge.BookmarkModelObserver bookmarkModelObserver =
                new BookmarkBridge.BookmarkModelObserver() {
                    @Override
                    public void bookmarkModelChanged() {
                        modelReorderHelper.notifyCalled();
                    }
                };
        // Perform registration to make callbacks work.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBookmarkModel.addObserver(bookmarkModelObserver);
        });

        View test = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE,
                ((BookmarkFolderRow) test).getTitle());

        toggleSelectionAndEndAnimation(testId, (BookmarkRow) test);

        // Starts as 0th bookmark (not counting promo header) and ends at the 1st index.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ((ReorderBookmarkItemsAdapter) mItemsContainer.getAdapter()).simulateDragForTests(1, 2);
        });

        modelReorderHelper.waitForCallback(0, 1);
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<BookmarkId> observed =
                    mBookmarkModel.getChildIDs(mBookmarkModel.getDefaultFolder(), true, true);
            // Exclude partner bookmarks folder
            Assert.assertEquals(expected, observed.subList(0, 3));
        });
    }

    @Test
    @MediumTest
    public void testPromoDraggability() throws Exception {
        BookmarkId testId = addFolder(TEST_FOLDER_TITLE);

        openBookmarkManager();

        ViewHolder promo = mItemsContainer.findViewHolderForAdapterPosition(0);

        toggleSelectionAndEndAnimation(
                testId, (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(1).itemView);

        ReorderBookmarkItemsAdapter adapter =
                ((ReorderBookmarkItemsAdapter) mItemsContainer.getAdapter());
        Assert.assertFalse("Promo header should not be passively draggable",
                adapter.isPassivelyDraggable(promo));
        Assert.assertFalse("Promo header should not be actively draggable",
                adapter.isActivelyDraggable(promo));
    }

    @Test
    @MediumTest
    public void testPartnerFolderDraggability() throws Exception {
        BookmarkId testId = addFolderWithPartner(TEST_FOLDER_TITLE);
        openBookmarkManager();

        ViewHolder partner = mItemsContainer.findViewHolderForAdapterPosition(2);

        toggleSelectionAndEndAnimation(
                testId, (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(1).itemView);

        ReorderBookmarkItemsAdapter adapter =
                ((ReorderBookmarkItemsAdapter) mItemsContainer.getAdapter());
        Assert.assertFalse("Partner bookmarks folder should not be passively draggable",
                adapter.isPassivelyDraggable(partner));
        Assert.assertFalse("Partner bookmarks folder should not be actively draggable",
                adapter.isActivelyDraggable(partner));
    }

    @Test
    @MediumTest
    public void testUnselectedItemDraggability() throws Exception {
        BookmarkId aId = addBookmark("a", "http://a.com");
        addFolder(TEST_FOLDER_TITLE);

        openBookmarkManager();

        ViewHolder test = mItemsContainer.findViewHolderForAdapterPosition(1);
        Assert.assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE,
                ((BookmarkFolderRow) test.itemView).getTitle());

        toggleSelectionAndEndAnimation(
                aId, (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(2).itemView);

        ReorderBookmarkItemsAdapter adapter =
                ((ReorderBookmarkItemsAdapter) mItemsContainer.getAdapter());
        Assert.assertTrue("Unselected rows should be passively draggable",
                adapter.isPassivelyDraggable(test));
        Assert.assertFalse("Unselected rows should not be actively draggable",
                adapter.isActivelyDraggable(test));
    }

    @Test
    @MediumTest
    public void testCannotSelectPromo() throws Exception {
        addFolder(TEST_FOLDER_TITLE);

        openBookmarkManager();

        View promo = mItemsContainer.findViewHolderForAdapterPosition(0).itemView;
        TouchCommon.longPressView(promo);
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
        Assert.assertFalse("Expected that we would not be in selection mode "
                        + "after long pressing on promo view.",
                mManager.getSelectionDelegate().isSelectionEnabled());
    }

    @Test
    @MediumTest
    public void testCannotSelectPartner() throws Exception {
        addFolderWithPartner(TEST_FOLDER_TITLE);
        openBookmarkManager();

        View partner = mItemsContainer.findViewHolderForAdapterPosition(2).itemView;
        TouchCommon.longPressView(partner);
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
        Assert.assertFalse("Expected that we would not be in selection mode "
                        + "after long pressing on partner bookmark.",
                mManager.getSelectionDelegate().isSelectionEnabled());
    }

    @Test
    @MediumTest
    public void testMoveUpMenuItem() throws Exception {
        addBookmark(TEST_PAGE_TITLE_GOOGLE, TEST_URL_A);
        addFolder(TEST_FOLDER_TITLE);
        openBookmarkManager();

        View google = mItemsContainer.findViewHolderForAdapterPosition(2).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_PAGE_TITLE_GOOGLE,
                ((BookmarkItemRow) google).getTitle());
        View more = google.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Move up")).perform(click());

        // Confirm that the "Google" bookmark is now on top, and that the "test" folder is 2nd
        Assert.assertTrue(
                ((BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(1).itemView)
                        .getTitle()
                        .equals(TEST_PAGE_TITLE_GOOGLE));
        Assert.assertTrue(
                ((BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(2).itemView)
                        .getTitle()
                        .equals(TEST_FOLDER_TITLE));
    }

    @Test
    @MediumTest
    public void testMoveDownMenuItem() throws Exception {
        addBookmark(TEST_PAGE_TITLE_GOOGLE, TEST_URL_A);
        addFolder(TEST_FOLDER_TITLE);
        openBookmarkManager();

        View testFolder = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE,
                ((BookmarkFolderRow) testFolder).getTitle());
        ListMenuButton more = testFolder.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Move down")).perform(click());

        // Confirm that the "Google" bookmark is now on top, and that the "test" folder is 2nd
        Assert.assertTrue(
                ((BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(1).itemView)
                        .getTitle()
                        .equals(TEST_PAGE_TITLE_GOOGLE));
        Assert.assertTrue(
                ((BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(2).itemView)
                        .getTitle()
                        .equals(TEST_FOLDER_TITLE));
    }

    @Test
    @MediumTest
    public void testMoveDownGoneForBottomElement() throws Exception {
        addBookmarkWithPartner(TEST_PAGE_TITLE_GOOGLE, TEST_URL_A);
        addFolderWithPartner(TEST_FOLDER_TITLE);
        openBookmarkManager();

        View google = mItemsContainer.findViewHolderForAdapterPosition(2).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_PAGE_TITLE_GOOGLE,
                ((BookmarkItemRow) google).getTitle());
        View more = google.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Move down")).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testMoveUpGoneForTopElement() throws Exception {
        addBookmark(TEST_PAGE_TITLE_GOOGLE, TEST_URL_A);
        addFolder(TEST_FOLDER_TITLE);
        openBookmarkManager();

        View testFolder = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE,
                ((BookmarkFolderRow) testFolder).getTitle());
        ListMenuButton more = testFolder.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Move up")).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testMoveButtonsGoneInSearchMode() throws Exception {
        addFolder(TEST_FOLDER_TITLE);
        openBookmarkManager();

        View searchButton = mManager.getToolbarForTests().findViewById(R.id.search_menu_id);
        TestThreadUtils.runOnUiThreadBlocking(searchButton::performClick);

        // Callback occurs when Item "test" is selected.
        CriteriaHelper.pollUiThread(()
                                            -> mBookmarkActivity.getManagerForTesting()
                                                       .getToolbarForTests()
                                                       .isSearching(),
                "Expected to enter search mode");

        View testFolder = mItemsContainer.findViewHolderForAdapterPosition(0).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE,
                ((BookmarkFolderRow) testFolder).getTitle());
        View more = testFolder.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more::callOnClick);

        onView(withText("Move up")).check(doesNotExist());
        onView(withText("Move down")).check(doesNotExist());
    }

    @Override
    protected void openBookmarkManager() throws InterruptedException {
        super.openBookmarkManager();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mManager.getDragStateDelegate().setA11yStateForTesting(false);
            BookmarkPromoHeader.forcePromoStateForTests(BookmarkPromoHeader.PromoState.PROMO_SYNC);
        });
    }

    /**
     * Loads an empty partner bookmarks folder for testing. The partner bookmarks folder will appear
     * in the mobile bookmarks folder.
     *
     * @throws InterruptedException If the loading process is interrupted.
     */
    private void loadEmptyPartnerBookmarksForTesting() throws InterruptedException {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mBookmarkModel.loadEmptyPartnerBookmarkShimForTesting(); });
        BookmarkTestUtil.waitForBookmarkModelLoaded();
    }

    /**
     * Adds a bookmark in the scenario where we have partner bookmarks.
     *
     * @param title The title of the bookmark to add.
     * @param url The url of the bookmark to add.
     * @return The BookmarkId of the added bookmark.
     * @throws InterruptedException If this operation is interrupted.
     * @throws ExecutionException If something goes wrong while we are trying to add the bookmark.
     */
    private BookmarkId addBookmarkWithPartner(String title, String url)
            throws InterruptedException, ExecutionException {
        loadEmptyPartnerBookmarksForTesting();
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.addBookmark(mBookmarkModel.getDefaultFolder(), 0, title, url));
    }

    /**
     * Adds a folder in the scenario where we have partner bookmarks.
     *
     * @param title The title of the folder to add.
     * @return The BookmarkId of the added folder.
     * @throws InterruptedException If this operation is interrupted.
     * @throws ExecutionException If something goes wrong while we are trying to add the bookmark.
     */
    private BookmarkId addFolderWithPartner(String title)
            throws InterruptedException, ExecutionException {
        loadEmptyPartnerBookmarksForTesting();
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.addFolder(mBookmarkModel.getDefaultFolder(), 0, title));
    }

    private ReorderBookmarkItemsAdapter getReorderAdapter() {
        return (ReorderBookmarkItemsAdapter) getAdapter();
    }

    @Override
    protected BookmarkManager getBookmarkManager() {
        return (BookmarkManager) getReorderAdapter().getDelegateForTesting();
    }

    @Override
    protected BookmarkId getIdByPosition(int pos) {
        return getReorderAdapter().getIdByPosition(pos);
    }

    @Override
    protected void searchBookmarks(final String query) {
        TestThreadUtils.runOnUiThreadBlocking(() -> getReorderAdapter().search(query));
    }
}