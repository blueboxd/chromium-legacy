// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import android.os.Environment;
import android.support.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

/**
 * Instrumentation tests for the Paint Preview player.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class PaintPreviewPlayerTest extends DummyUiActivityTestCase {
    private static final long TIMEOUT_MS = ScalableTimeout.scaleTimeout(5000);
    private static final long POLLING_INTERVAL_MS = ScalableTimeout.scaleTimeout(50);

    // TODO(crbug.com/1049303) Change to test data directory when test Proto and SKP files are
    //  added.
    private static final String TEST_DATA_DIR = Environment.getExternalStorageDirectory().getPath();
    private static final String TEST_DIRECTORY_KEY = "test_key";

    @Rule
    public PaintPreviewTestRule mPaintPreviewTestRule = new PaintPreviewTestRule();

    private PlayerManager mPlayerManager;

    /**
     * Initializes {@link TestImplementerService} and {@link PlayerManager}.
     */
    @Test
    @MediumTest
    public void smokeTest() {
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            TestImplementerService service = new TestImplementerService(TEST_DATA_DIR);
            mPlayerManager = new PlayerManager(getActivity(), service, TEST_DIRECTORY_KEY);
        });
        CriteriaHelper.pollUiThread(() -> mPlayerManager != null,
                "PlayerManager took too long to initialize.", TIMEOUT_MS,
            POLLING_INTERVAL_MS);
    }
}
