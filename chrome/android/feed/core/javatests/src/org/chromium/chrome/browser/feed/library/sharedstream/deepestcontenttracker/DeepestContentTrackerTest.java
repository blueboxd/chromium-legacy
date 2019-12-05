// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.deepestcontenttracker;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link DeepestContentTracker}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DeepestContentTrackerTest {
    private static final String CONTENT_ID_1 = "CONTENT_ID_1";
    private static final String CONTENT_ID_2 = "CONTENT_ID_2";
    private static final String CONTENT_ID_3 = "CONTENT_ID_3";

    private DeepestContentTracker mDeepestContentTracker;

    @Before
    public void setUp() {
        mDeepestContentTracker = new DeepestContentTracker();
    }

    @Test
    public void testUpdateDeepestContentTracker() {
        mDeepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_1);

        assertThat(mDeepestContentTracker.getChildViewDepth()).isEqualTo(CONTENT_ID_1);
    }

    @Test
    public void testUpdateDeepestContentTracker_updatesFromDeeperContent() {
        mDeepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_1);
        mDeepestContentTracker.updateDeepestContentTracker(1, CONTENT_ID_2);

        assertThat(mDeepestContentTracker.getChildViewDepth()).isEqualTo(CONTENT_ID_2);
    }

    @Test
    public void testUpdateDeepestContentTracker_doesNotAddContentIdIfAlreadyTracked() {
        mDeepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_2);
        mDeepestContentTracker.updateDeepestContentTracker(1, CONTENT_ID_1);
        mDeepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_2);

        assertThat(mDeepestContentTracker.getChildViewDepth()).isEqualTo(CONTENT_ID_1);
    }

    @Test
    public void testUpdateDeepestContentTracker_ignoresNullContentId() {
        mDeepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_1);
        mDeepestContentTracker.updateDeepestContentTracker(1, /* contentId= */ null);

        assertThat(mDeepestContentTracker.getChildViewDepth()).isEqualTo(CONTENT_ID_1);
    }

    @Test
    public void testUpdateDeepestContentTracker_updatingPreviousKnownPosition() {
        mDeepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_1);
        mDeepestContentTracker.updateDeepestContentTracker(1, CONTENT_ID_2);
        mDeepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_3);

        assertThat(mDeepestContentTracker.getContentItAtPosition(0)).isEqualTo(CONTENT_ID_3);
    }

    @Test
    public void testUpdateDeepestContentTracker_sparsePopulate() {
        mDeepestContentTracker.updateDeepestContentTracker(1, CONTENT_ID_2);

        assertThat(mDeepestContentTracker.getContentItAtPosition(0)).isNull();
        assertThat(mDeepestContentTracker.getContentItAtPosition(1)).isEqualTo(CONTENT_ID_2);

        mDeepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_1);
        assertThat(mDeepestContentTracker.getContentItAtPosition(0)).isEqualTo(CONTENT_ID_1);
    }

    @Test
    public void testRemoveContentId() {
        mDeepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_1);
        mDeepestContentTracker.updateDeepestContentTracker(1, CONTENT_ID_2);

        mDeepestContentTracker.removeContentId(1);

        assertThat(mDeepestContentTracker.getChildViewDepth()).isEqualTo(CONTENT_ID_1);
    }

    @Test
    public void testRemoveContentId_removingShallowerContentIdRetainsDeeperId() {
        mDeepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_1);
        mDeepestContentTracker.updateDeepestContentTracker(1, CONTENT_ID_2);

        mDeepestContentTracker.removeContentId(0);

        assertThat(mDeepestContentTracker.getChildViewDepth()).isEqualTo(CONTENT_ID_2);
    }

    @Test
    public void testRemoveContentId_doesNotRemoveContentId() {
        mDeepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_1);

        mDeepestContentTracker.removeContentId(1);

        assertThat(mDeepestContentTracker.getChildViewDepth()).isEqualTo(CONTENT_ID_1);
    }

    @Test
    public void testReset() {
        mDeepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_1);

        mDeepestContentTracker.reset();

        assertThat(mDeepestContentTracker.getChildViewDepth()).isNull();
    }
}
