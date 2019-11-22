// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.scroll;

import static com.google.common.truth.Truth.assertThat;

import android.app.Activity;
import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.view.View;

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.sharedstream.proto.ScrollStateProto.ScrollState;
import com.google.android.libraries.feed.testing.android.LinearLayoutManagerForTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

@RunWith(RobolectricTestRunner.class)
public final class ScrollRestoreHelperTest {
    private static final int HEADER_COUNT = 10;
    private static final int ABANDON_RESTORE_BELOW_FOLD_THRESHOLD = 5;
    private static final int FIRST_VISIBLE_ITEM_POSITION = 3;
    private static final int FIRST_VISIBLE_ITEM_OFFSET = 14;

    private LinearLayoutManagerForTest linearLayoutManager;
    private Context context;

    @Before
    public void setUp() {
        context = Robolectric.buildActivity(Activity.class).get();
        linearLayoutManager = new LinearLayoutManagerForTest(context);
        linearLayoutManager.firstVisibleItemPosition = FIRST_VISIBLE_ITEM_POSITION;
        linearLayoutManager.firstVisibleViewOffset = FIRST_VISIBLE_ITEM_OFFSET;
    }

    @Test
    public void testGetScrollStateForScrollRestore_noFirstPosition_returnsNull() {
        linearLayoutManager.firstVisibleItemPosition = RecyclerView.NO_POSITION;

        assertThat(ScrollRestoreHelper.getScrollStateForScrollRestore(
                           linearLayoutManager, new Configuration.Builder().build(), HEADER_COUNT))
                .isNull();
    }

    @Test
    public void testGetScrollStateForScrollRestore_noLastPosition_returnsNull() {
        // configurationMap.put(ConfigKey.ABANDON_RESTORE_BELOW_FOLD, true);

        linearLayoutManager.lastVisibleItemPosition = RecyclerView.NO_POSITION;

        assertThat(ScrollRestoreHelper.getScrollStateForScrollRestore(
                           linearLayoutManager, new Configuration.Builder().build(), HEADER_COUNT))
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
        View view = new View(context);
        view.setTop(FIRST_VISIBLE_ITEM_OFFSET);
        linearLayoutManager.addChildToPosition(FIRST_VISIBLE_ITEM_POSITION, view);

        // With ABANDON_RESTORE_BELOW_FOLD true, we should not restore below the fold.
        assertThat(ScrollRestoreHelper.getScrollStateForScrollRestore(
                           linearLayoutManager, configuration, HEADER_COUNT))
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
        View view = new View(context);
        view.setTop(FIRST_VISIBLE_ITEM_OFFSET);
        linearLayoutManager.addChildToPosition(FIRST_VISIBLE_ITEM_POSITION, view);

        // With ABANDON_RESTORE_BELOW_FOLD false, we should restore below the fold.
        assertThat(ScrollRestoreHelper.getScrollStateForScrollRestore(
                           linearLayoutManager, configuration, HEADER_COUNT))
                .isEqualTo(ScrollState.newBuilder()
                                   .setPosition(FIRST_VISIBLE_ITEM_POSITION)
                                   .setOffset(FIRST_VISIBLE_ITEM_OFFSET)
                                   .build());
    }

    private void setUpForRestoringBelowTheFold() {
        linearLayoutManager.lastVisibleItemPosition =
                HEADER_COUNT + ABANDON_RESTORE_BELOW_FOLD_THRESHOLD + 1;
    }
}
