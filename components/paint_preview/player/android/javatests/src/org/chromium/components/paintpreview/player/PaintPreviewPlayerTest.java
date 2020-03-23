// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import android.support.test.filters.MediumTest;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.ui.test.util.DummyUiActivityTestCase;
import org.chromium.url.GURL;

/**
 * Instrumentation tests for the Paint Preview player.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class PaintPreviewPlayerTest extends DummyUiActivityTestCase {
    private static final long TIMEOUT_MS = ScalableTimeout.scaleTimeout(5000);

    private static final String TEST_DATA_DIR = "components/test/data/";
    private static final String TEST_DIRECTORY_KEY = "wikipedia";
    private static final String TEST_URL = "https://en.m.wikipedia.org/wiki/Main_Page";

    @Rule
    public PaintPreviewTestRule mPaintPreviewTestRule = new PaintPreviewTestRule();

    private PlayerManager mPlayerManager;

    /**
     * Tests the the player correctly initializes and displays a sample paint preview with 1 frame.
     */
    @Test
    @MediumTest
    public void singleFrameDisplayTest() {
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            PaintPreviewTestService service =
                    new PaintPreviewTestService(UrlUtils.getIsolatedTestFilePath(TEST_DATA_DIR));
            mPlayerManager = new PlayerManager(new GURL(TEST_URL), getActivity(), service,
                    TEST_DIRECTORY_KEY, success -> { Assert.assertTrue(success); });
            getActivity().setContentView(mPlayerManager.getView());
        });

        // Wait until PlayerManager is initialized.
        CriteriaHelper.pollUiThread(() -> mPlayerManager != null,
                "PlayerManager was not initialized.", TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        final View playerHostView = mPlayerManager.getView();
        final View activityContentView = getActivity().findViewById(android.R.id.content);

        // Assert that the player view is added to the player host view.
        CriteriaHelper.pollUiThread(() -> ((ViewGroup) playerHostView).getChildCount() > 0,
                "Player view is not added to the host view.", TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        // Assert that the player view has the same dimensions as the content view.
        CriteriaHelper.pollUiThread(() -> {
                    boolean contentSizeNonZero = activityContentView.getWidth() > 0
                            && activityContentView.getHeight() > 0;
                    boolean viewSizeMatchContent =
                            activityContentView.getWidth() == playerHostView.getWidth()
                            && activityContentView.getHeight() == playerHostView.getHeight();
                    return contentSizeNonZero && viewSizeMatchContent;
                },
                "Player size doesn't match R.id.content", TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }
}
