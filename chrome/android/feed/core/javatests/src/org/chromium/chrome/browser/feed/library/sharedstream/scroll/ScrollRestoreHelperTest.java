// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.scroll;

import static com.google.common.truth.Truth.assertThat;

import android.app.Activity;
import android.content.Context;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.testing.android.LinearLayoutManagerForTest;
import org.chromium.components.feed.core.proto.libraries.sharedstream.ScrollStateProto.ScrollState;
import org.chromium.testing.local.LocalRobolectricTestRunner;

@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class ScrollRestoreHelperTest {
    private static final int HEADER_COUNT = 10;
    private static final int ABANDON_RESTORE_BELOW_FOLD_THRESHOLD = 5;
    private static final int FIRST_VISIBLE_ITEM_POSITION = 3;
    private static final int FIRST_VISIBLE_ITEM_OFFSET = 14;

    private LinearLayoutManagerForTest mLinearLayoutManager;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = Robolectric.buildActivity(Activity.class).get();
        mLinearLayoutManager = new LinearLayoutManagerForTest(mContext);
        mLinearLayoutManager.firstVisibleItemPosition = FIRST_VISIBLE_ITEM_POSITION;
        mLinearLayoutManager.firstVisibleViewOffset = FIRST_VISIBLE_ITEM_OFFSET;
    }

    @Test
    public void testGetScrollStateForScrollRestore_noFirstPosition_returnsNull() {
        mLinearLayoutManager.firstVisibleItemPosition = RecyclerView.NO_POSITION;

        assertThat(ScrollRestoreHelper.getScrollStateForScrollRestore(
                           mLinearLayoutManager, new Configuration.Builder().build(), HEADER_COUNT))
                .isNull();
    }

    @Test
    public void testGetScrollStateForScrollRestore_noLastPosition_returnsNull() {
        // configurationMap.put(ConfigKey.ABANDON_RESTORE_BELOW_FOLD, true);

        mLinearLayoutManager.lastVisibleItemPosition = RecyclerView.NO_POSITION;

        assertThat(ScrollRestoreHelper.getScrollStateForScrollRestore(
                           mLinearLayoutManager, new Configuration.Builder().build(), HEADER_COUNT))
                .isNull();
    }

    @Test
    public void testGetScrollStateForScrollRestore_scrolledTooFar_returnsNull() {
        Configuration configuration = new Configuration.Builder()
                                              .put(ConfigKey.ABANDON_RESTORE_BELOW_FOLD, true)
                                              .put(ConfigKey.ABANDON_RESTORE_BELOW_FOLD_THRESHOLD,
                                                      (long) ABANDON_RESTORE_BELOW_FOLD_THRESHOLD)
                                              .build();

        setUpForRestoringBelowTheFold();
        View view = new View(mContext);
        view.setTop(FIRST_VISIBLE_ITEM_OFFSET);
        mLinearLayoutManager.addChildToPosition(FIRST_VISIBLE_ITEM_POSITION, view);

        // With ABANDON_RESTORE_BELOW_FOLD true, we should not restore below the fold.
        assertThat(ScrollRestoreHelper.getScrollStateForScrollRestore(
                           mLinearLayoutManager, configuration, HEADER_COUNT))
                .isNull();
    }

    @Test
    public void testGetScrollStateForScrollRestore_dontAbandonScrollRestore_returnsScrollState() {
        Configuration configuration = new Configuration.Builder()
                                              .put(ConfigKey.ABANDON_RESTORE_BELOW_FOLD, false)
                                              .put(ConfigKey.ABANDON_RESTORE_BELOW_FOLD_THRESHOLD,
                                                      (long) ABANDON_RESTORE_BELOW_FOLD_THRESHOLD)
                                              .build();

        setUpForRestoringBelowTheFold();
        View view = new View(mContext);
        view.setTop(FIRST_VISIBLE_ITEM_OFFSET);
        mLinearLayoutManager.addChildToPosition(FIRST_VISIBLE_ITEM_POSITION, view);

        // With ABANDON_RESTORE_BELOW_FOLD false, we should restore below the fold.
        assertThat(ScrollRestoreHelper.getScrollStateForScrollRestore(
                           mLinearLayoutManager, configuration, HEADER_COUNT))
                .isEqualTo(ScrollState.newBuilder()
                                   .setPosition(FIRST_VISIBLE_ITEM_POSITION)
                                   .setOffset(FIRST_VISIBLE_ITEM_OFFSET)
                                   .build());
    }

    private void setUpForRestoringBelowTheFold() {
        mLinearLayoutManager.lastVisibleItemPosition =
                HEADER_COUNT + ABANDON_RESTORE_BELOW_FOLD_THRESHOLD + 1;
    }
}
