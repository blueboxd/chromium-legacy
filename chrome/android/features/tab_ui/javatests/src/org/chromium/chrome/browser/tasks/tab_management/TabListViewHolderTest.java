// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.MatcherAssert.assertThat;

import static org.chromium.base.GarbageCollectionTestUtils.canBeGarbageCollected;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.MediumTest;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.ref.WeakReference;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Tests for the {@link android.support.v7.widget.RecyclerView.ViewHolder} classes for {@link
 * TabListCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TabListViewHolderTest extends DummyUiActivityTestCase {
    private TabGridViewHolder mTabGridViewHolder;
    private PropertyModel mGridModel;
    private PropertyModelChangeProcessor mGridMCP;

    private TabStripViewHolder mTabStripViewHolder;
    private PropertyModel mStripModel;
    private PropertyModelChangeProcessor mStripMCP;

    private TabGridViewHolder mSelectableTabGridViewHolder;
    private PropertyModel mSelectableModel;
    private PropertyModelChangeProcessor mSelectableMCP;
    private SelectionDelegate<Integer> mSelectionDelegate;

    private TabListMediator.ThumbnailFetcher mMockThumbnailProvider =
            new TabListMediator.ThumbnailFetcher(new TabListMediator.ThumbnailProvider() {
                @Override
                public void getTabThumbnailWithCallback(Tab tab, Callback<Bitmap> callback,
                        boolean forceUpdate, boolean writeToCache) {
                    Bitmap bitmap = mShouldReturnBitmap
                            ? Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888)
                            : null;
                    callback.onResult(bitmap);
                    mThumbnailFetchedCount.incrementAndGet();
                }
            }, null, false, false);
    private AtomicInteger mThumbnailFetchedCount = new AtomicInteger();

    private TabListMediator.TabActionListener mMockCloseListener =
            new TabListMediator.TabActionListener() {
                @Override
                public void run(int tabId) {
                    mCloseClicked.set(true);
                }
            };
    private AtomicBoolean mCloseClicked = new AtomicBoolean();

    private TabListMediator.TabActionListener mMockSelectedListener =
            new TabListMediator.TabActionListener() {
                @Override
                public void run(int tabId) {
                    mSelectClicked.set(true);
                }
            };
    private AtomicBoolean mSelectClicked = new AtomicBoolean();
    private boolean mShouldReturnBitmap;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        ViewGroup view = new LinearLayout(getActivity());
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getActivity().setContentView(view, params);

            mTabGridViewHolder = TabGridViewHolder.create(
                    view, TabGridViewHolder.TabGridViewItemType.CLOSABLE_TAB);
            mTabStripViewHolder = TabStripViewHolder.create(view, 0);
            mSelectableTabGridViewHolder = TabGridViewHolder.create(
                    view, TabGridViewHolder.TabGridViewItemType.SELECTABLE_TAB);

            view.addView(mTabGridViewHolder.itemView);
            view.addView(mTabStripViewHolder.itemView);
            view.addView(mSelectableTabGridViewHolder.itemView);
        });

        mSelectionDelegate = new SelectionDelegate<>();

        mGridModel = new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                             .with(TabProperties.TAB_SELECTED_LISTENER, mMockSelectedListener)
                             .with(TabProperties.TAB_CLOSED_LISTENER, mMockCloseListener)
                             .build();
        mStripModel = new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_STRIP)
                              .with(TabProperties.TAB_SELECTED_LISTENER, mMockSelectedListener)
                              .with(TabProperties.TAB_CLOSED_LISTENER, mMockCloseListener)
                              .build();
        mSelectableModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.SELECTABLE_TAB_CLICKED_LISTENER, mMockSelectedListener)
                        .with(TabProperties.TAB_SELECTION_DELEGATE, mSelectionDelegate)
                        .build();

        mGridMCP = PropertyModelChangeProcessor.create(mGridModel, mTabGridViewHolder,
                new TestRecyclerViewSimpleViewBinder<>(TabGridViewBinder::onBindViewHolder));
        mStripMCP = PropertyModelChangeProcessor.create(mStripModel, mTabStripViewHolder,
                new TestRecyclerViewSimpleViewBinder<>(TabStripViewBinder::onBindViewHolder));
        mSelectableMCP = PropertyModelChangeProcessor.create(mSelectableModel,
                mSelectableTabGridViewHolder,
                new TestRecyclerViewSimpleViewBinder<>(TabGridViewBinder::onBindViewHolder));
    }

    private void testGridSelected(TabGridViewHolder holder, PropertyModel model) {
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.LOLLIPOP_MR1) {
            model.set(TabProperties.IS_SELECTED, true);
            Assert.assertTrue(((FrameLayout) (holder.itemView)).getForeground() != null);
            model.set(TabProperties.IS_SELECTED, false);
            Assert.assertFalse(((FrameLayout) (holder.itemView)).getForeground() != null);
        } else {
            model.set(TabProperties.IS_SELECTED, true);
            View selectedView = holder.itemView.findViewById(R.id.selected_view_below_lollipop);
            Assert.assertTrue(selectedView.getVisibility() == View.VISIBLE);
            model.set(TabProperties.IS_SELECTED, false);
            Assert.assertTrue(selectedView.getVisibility() == View.GONE);
        }
        mStripModel.set(TabProperties.IS_SELECTED, true);
        Assert.assertTrue(((FrameLayout) (mTabStripViewHolder.itemView)).getForeground() != null);
        mStripModel.set(TabProperties.IS_SELECTED, false);
        Assert.assertFalse(((FrameLayout) (mTabStripViewHolder.itemView)).getForeground() != null);
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testSelected() throws Exception {
        testGridSelected(mTabGridViewHolder, mGridModel);

        mStripModel.set(TabProperties.IS_SELECTED, true);
        Assert.assertTrue(((FrameLayout) (mTabStripViewHolder.itemView)).getForeground() != null);
        mStripModel.set(TabProperties.IS_SELECTED, false);
        Assert.assertFalse(((FrameLayout) (mTabStripViewHolder.itemView)).getForeground() != null);

        testGridSelected(mSelectableTabGridViewHolder, mSelectableModel);
        mSelectableModel.set(TabProperties.IS_SELECTED, true);
        Assert.assertTrue(
                mSelectableTabGridViewHolder.actionButton.getBackground().getLevel() == 1);
        Assert.assertTrue(mSelectableTabGridViewHolder.actionButton.getDrawable() != null);

        mSelectableModel.set(TabProperties.IS_SELECTED, false);
        Assert.assertTrue(
                mSelectableTabGridViewHolder.actionButton.getBackground().getLevel() == 0);
        Assert.assertTrue(mSelectableTabGridViewHolder.actionButton.getDrawable() == null);
    }

    @Test
    @MediumTest
    public void testAnimationRestored() throws Exception {
        View backgroundView = mTabGridViewHolder.itemView.findViewById(R.id.background_view);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mGridModel.set(TabProperties.IS_SELECTED, true);
            mGridModel.set(TabProperties.CARD_ANIMATION_STATUS,
                    ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
        });
        CriteriaHelper.pollUiThread(
                () -> !((ClosableTabGridViewHolder) mTabGridViewHolder).getIsAnimatingForTesting());

        Assert.assertTrue(backgroundView.getVisibility() == View.GONE);
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1) {
            View selectedView =
                    mTabGridViewHolder.itemView.findViewById(R.id.selected_view_below_lollipop);
            Assert.assertTrue(selectedView.getVisibility() == View.VISIBLE);
        } else {
            Drawable selectedDrawable = mTabGridViewHolder.itemView.getForeground();
            Assert.assertNotNull(selectedDrawable);
        }

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mGridModel.set(TabProperties.IS_SELECTED, false);
            mGridModel.set(TabProperties.CARD_ANIMATION_STATUS,
                    ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
        });
        CriteriaHelper.pollUiThread(
                () -> !((ClosableTabGridViewHolder) mTabGridViewHolder).getIsAnimatingForTesting());
        Assert.assertTrue(backgroundView.getVisibility() == View.GONE);

        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1) {
            View selectedView =
                    mTabGridViewHolder.itemView.findViewById(R.id.selected_view_below_lollipop);
            Assert.assertTrue(selectedView.getVisibility() == View.GONE);
        } else {
            Drawable selectedDrawable = mTabGridViewHolder.itemView.getForeground();
            Assert.assertNull(selectedDrawable);
        }
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testTitle() throws Exception {
        final String title = "Surf the cool webz";
        mGridModel.set(TabProperties.TITLE, title);
        Assert.assertEquals(mTabGridViewHolder.title.getText(), title);

        mSelectableModel.set(TabProperties.TITLE, title);
        Assert.assertEquals(mSelectableTabGridViewHolder.title.getText(), title);
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testThumbnail() throws Exception {
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailProvider);
        Assert.assertNull(mTabGridViewHolder.thumbnail.getDrawable());
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, null);

        mShouldReturnBitmap = true;
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailProvider);
        assertThat(mTabGridViewHolder.thumbnail.getDrawable(), instanceOf(BitmapDrawable.class));
        Assert.assertEquals(2, mThumbnailFetchedCount.get());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testThumbnailGCAfterNullBitmap() throws Exception {
        mShouldReturnBitmap = true;
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailProvider);
        assertThat(mTabGridViewHolder.thumbnail.getDrawable(), instanceOf(BitmapDrawable.class));
        Bitmap bitmap = ((BitmapDrawable) mTabGridViewHolder.thumbnail.getDrawable()).getBitmap();
        WeakReference<Bitmap> ref = new WeakReference<>(bitmap);
        bitmap = null;

        Assert.assertFalse(canBeGarbageCollected(ref));

        mShouldReturnBitmap = false;
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailProvider);
        Assert.assertTrue(canBeGarbageCollected(ref));
        Assert.assertEquals(2, mThumbnailFetchedCount.get());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testThumbnailGCAfterNewBitmap() throws Exception {
        mShouldReturnBitmap = true;
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailProvider);
        assertThat(mTabGridViewHolder.thumbnail.getDrawable(), instanceOf(BitmapDrawable.class));
        Bitmap bitmap = ((BitmapDrawable) mTabGridViewHolder.thumbnail.getDrawable()).getBitmap();
        WeakReference<Bitmap> ref = new WeakReference<>(bitmap);
        bitmap = null;

        Assert.assertFalse(canBeGarbageCollected(ref));

        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailProvider);
        Assert.assertTrue(canBeGarbageCollected(ref));
        Assert.assertEquals(2, mThumbnailFetchedCount.get());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testResetThumbnailGC() throws Exception {
        mShouldReturnBitmap = true;
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailProvider);
        assertThat(mTabGridViewHolder.thumbnail.getDrawable(), instanceOf(BitmapDrawable.class));
        Bitmap bitmap = ((BitmapDrawable) mTabGridViewHolder.thumbnail.getDrawable()).getBitmap();
        WeakReference<Bitmap> ref = new WeakReference<>(bitmap);
        bitmap = null;

        Assert.assertFalse(canBeGarbageCollected(ref));

        mTabGridViewHolder.resetThumbnail();
        Assert.assertTrue(canBeGarbageCollected(ref));
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testHiddenGC() throws Exception {
        mShouldReturnBitmap = true;
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailProvider);
        assertThat(mTabGridViewHolder.thumbnail.getDrawable(), instanceOf(BitmapDrawable.class));
        Bitmap bitmap = ((BitmapDrawable) mTabGridViewHolder.thumbnail.getDrawable()).getBitmap();
        WeakReference<Bitmap> ref = new WeakReference<>(bitmap);
        bitmap = null;

        Assert.assertFalse(canBeGarbageCollected(ref));

        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, null);
        Assert.assertTrue(canBeGarbageCollected(ref));
        Assert.assertNull(mTabGridViewHolder.thumbnail.getDrawable());
        Assert.assertEquals(1, mThumbnailFetchedCount.get());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testHiddenThenShow() throws Exception {
        mShouldReturnBitmap = true;
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailProvider);
        assertThat(mTabGridViewHolder.thumbnail.getDrawable(), instanceOf(BitmapDrawable.class));
        Assert.assertEquals(1, mThumbnailFetchedCount.get());

        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, null);
        Assert.assertNull(mTabGridViewHolder.thumbnail.getDrawable());
        Assert.assertEquals(1, mThumbnailFetchedCount.get());

        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailProvider);
        assertThat(mTabGridViewHolder.thumbnail.getDrawable(), instanceOf(BitmapDrawable.class));
        Assert.assertEquals(2, mThumbnailFetchedCount.get());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testClickToSelect() throws Exception {
        mTabGridViewHolder.itemView.performClick();
        Assert.assertTrue(mSelectClicked.get());
        mSelectClicked.set(false);

        mStripModel.set(TabProperties.IS_SELECTED, false);
        mTabStripViewHolder.button.performClick();
        Assert.assertTrue(mSelectClicked.get());
        mSelectClicked.set(false);

        mStripModel.set(TabProperties.IS_SELECTED, true);
        mTabStripViewHolder.button.performClick();
        Assert.assertFalse(mSelectClicked.get());
        mSelectClicked.set(false);

        mSelectableModel.set(TabProperties.IS_SELECTED, false);
        mSelectableTabGridViewHolder.itemView.performClick();
        Assert.assertTrue(mSelectClicked.get());
        mSelectClicked.set(false);

        mSelectableModel.set(TabProperties.IS_SELECTED, true);
        mSelectableTabGridViewHolder.itemView.performClick();
        Assert.assertTrue(mSelectClicked.get());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testClickToClose() throws Exception {
        mTabGridViewHolder.actionButton.performClick();
        Assert.assertTrue(mCloseClicked.get());
        mCloseClicked.set(false);

        mStripModel.set(TabProperties.IS_SELECTED, true);
        mTabStripViewHolder.button.performClick();
        Assert.assertTrue(mCloseClicked.get());
        mCloseClicked.set(false);

        mStripModel.set(TabProperties.IS_SELECTED, false);
        mTabStripViewHolder.button.performClick();
        Assert.assertFalse(mCloseClicked.get());
    }

    @Override
    public void tearDownTest() throws Exception {
        mStripMCP.destroy();
        mGridMCP.destroy();
        mSelectableMCP.destroy();
        super.tearDownTest();
    }
}
