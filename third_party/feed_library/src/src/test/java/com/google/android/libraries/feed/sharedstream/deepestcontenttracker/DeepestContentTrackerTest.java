// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.deepestcontenttracker;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link DeepestContentTracker}. */
@RunWith(RobolectricTestRunner.class)
public class DeepestContentTrackerTest {
    private static final String CONTENT_ID_1 = "CONTENT_ID_1";
    private static final String CONTENT_ID_2 = "CONTENT_ID_2";
    private static final String CONTENT_ID_3 = "CONTENT_ID_3";

    private DeepestContentTracker deepestContentTracker;

    @Before
    public void setUp() {
        deepestContentTracker = new DeepestContentTracker();
    }

    @Test
    public void testUpdateDeepestContentTracker() {
        deepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_1);

        assertThat(deepestContentTracker.getChildViewDepth()).isEqualTo(CONTENT_ID_1);
    }

    @Test
    public void testUpdateDeepestContentTracker_updatesFromDeeperContent() {
        deepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_1);
        deepestContentTracker.updateDeepestContentTracker(1, CONTENT_ID_2);

        assertThat(deepestContentTracker.getChildViewDepth()).isEqualTo(CONTENT_ID_2);
    }

    @Test
    public void testUpdateDeepestContentTracker_doesNotAddContentIdIfAlreadyTracked() {
        deepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_2);
        deepestContentTracker.updateDeepestContentTracker(1, CONTENT_ID_1);
        deepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_2);

        assertThat(deepestContentTracker.getChildViewDepth()).isEqualTo(CONTENT_ID_1);
    }

    @Test
    public void testUpdateDeepestContentTracker_ignoresNullContentId() {
        deepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_1);
        deepestContentTracker.updateDeepestContentTracker(1, /* contentId= */ null);

        assertThat(deepestContentTracker.getChildViewDepth()).isEqualTo(CONTENT_ID_1);
    }

    @Test
    public void testUpdateDeepestContentTracker_updatingPreviousKnownPosition() {
        deepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_1);
        deepestContentTracker.updateDeepestContentTracker(1, CONTENT_ID_2);
        deepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_3);

        assertThat(deepestContentTracker.getContentItAtPosition(0)).isEqualTo(CONTENT_ID_3);
    }

    @Test
    public void testUpdateDeepestContentTracker_sparsePopulate() {
        deepestContentTracker.updateDeepestContentTracker(1, CONTENT_ID_2);

        assertThat(deepestContentTracker.getContentItAtPosition(0)).isNull();
        assertThat(deepestContentTracker.getContentItAtPosition(1)).isEqualTo(CONTENT_ID_2);

        deepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_1);
        assertThat(deepestContentTracker.getContentItAtPosition(0)).isEqualTo(CONTENT_ID_1);
    }

    @Test
    public void testRemoveContentId() {
        deepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_1);
        deepestContentTracker.updateDeepestContentTracker(1, CONTENT_ID_2);

        deepestContentTracker.removeContentId(1);

        assertThat(deepestContentTracker.getChildViewDepth()).isEqualTo(CONTENT_ID_1);
    }

    @Test
    public void testRemoveContentId_removingShallowerContentIdRetainsDeeperId() {
        deepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_1);
        deepestContentTracker.updateDeepestContentTracker(1, CONTENT_ID_2);

        deepestContentTracker.removeContentId(0);

        assertThat(deepestContentTracker.getChildViewDepth()).isEqualTo(CONTENT_ID_2);
    }

    @Test
    public void testRemoveContentId_doesNotRemoveContentId() {
        deepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_1);

        deepestContentTracker.removeContentId(1);

        assertThat(deepestContentTracker.getChildViewDepth()).isEqualTo(CONTENT_ID_1);
    }

    @Test
    public void testReset() {
        deepestContentTracker.updateDeepestContentTracker(0, CONTENT_ID_1);

        deepestContentTracker.reset();

        assertThat(deepestContentTracker.getChildViewDepth()).isNull();
    }
}
