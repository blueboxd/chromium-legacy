// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import static org.junit.Assert.assertEquals;

import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.test.util.UiRestriction;

/** This class tests the functionality of the {@link BottomSheet}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
public class BottomSheetTest {
    @Rule
    public BottomSheetTestRule mBottomSheetTestRule = new BottomSheetTestRule();
    private BottomSheetTestRule.Observer mObserver;
    private TestBottomSheetContent mSheetContent;

    @Before
    public void setUp() throws Exception {
        BottomSheet.setSmallScreenForTesting(false);
        mBottomSheetTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetContent = new TestBottomSheetContent(mBottomSheetTestRule.getActivity(),
                    BottomSheetContent.ContentPriority.HIGH, false);
        });
        mObserver = mBottomSheetTestRule.getObserver();
    }

    @Test
    @MediumTest
    public void testCustomPeekRatio() {
        int customToolbarHeight = TestBottomSheetContent.TOOLBAR_HEIGHT + 50;
        mSheetContent.setPeekHeight(customToolbarHeight);

        showContent(mSheetContent);

        assertEquals("Sheet should be peeking at the custom height.", customToolbarHeight,
                mBottomSheetTestRule.getBottomSheetController().getCurrentOffset());
    }

    /** @param content The content to show in the bottom sheet. */
    private void showContent(BottomSheetContent content) {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mBottomSheetTestRule.getBottomSheetController().requestShowContent(content, false);
        });
    }
}
