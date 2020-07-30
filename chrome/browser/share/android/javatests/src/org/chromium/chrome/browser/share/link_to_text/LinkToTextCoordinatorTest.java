// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.link_to_text;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.support.test.rule.ActivityTestRule;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.test.util.DummyUiActivity;

/**
 * Tests for {@link LinkToTextCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class LinkToTextCoordinatorTest {
    @Rule
    public ActivityTestRule<DummyUiActivity> mActivityTestRule =
            new ActivityTestRule<>(DummyUiActivity.class);

    @Mock
    private ChromeOptionShareCallback mShareCallback;

    @Mock
    private WindowAndroid mWindow;

    private Context mContext;
    private static final String SELECTED_TEXT = "selection";
    private static final String VISIBLE_URL = "www.example.com";

    @Before
    public void setUp() {
        mContext = mActivityTestRule.getActivity();

        MockitoAnnotations.initMocks(this);
        doNothing()
                .when(mShareCallback)
                .showThirdPartyShareSheetWithMessage(anyString(), any(), any(), anyLong());
    }

    @Test
    @SmallTest
    public void getTextToShareTest() {
        String selector = "selector";
        String expectedTextToShare = "\"selection\"\nwww.example.com:~:text=selector";
        LinkToTextCoordinator coordinator = new LinkToTextCoordinator(
                mContext, mWindow, mShareCallback, VISIBLE_URL, SELECTED_TEXT);
        Assert.assertEquals(expectedTextToShare, coordinator.getTextToShare(selector));
    }

    @Test
    @SmallTest
    public void getTextToShareTest_EmptySelector() {
        String selector = "";
        String expectedTextToShare = "\"selection\"\nwww.example.com";
        LinkToTextCoordinator coordinator = new LinkToTextCoordinator(
                mContext, mWindow, mShareCallback, VISIBLE_URL, SELECTED_TEXT);
        Assert.assertEquals(expectedTextToShare, coordinator.getTextToShare(selector));
    }

    @Test
    @SmallTest
    public void onSelectorReadyTest() {
        LinkToTextCoordinator coordinator = new LinkToTextCoordinator(
                mContext, mWindow, mShareCallback, VISIBLE_URL, SELECTED_TEXT);
        // OnSelectorReady should call back the share sheet.
        verify(mShareCallback)
                .showThirdPartyShareSheetWithMessage(anyString(), any(), any(), anyLong());
    }
}
