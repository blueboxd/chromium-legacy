// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.shell.WebLayerShellActivity;

import java.util.concurrent.CountDownLatch;

@RunWith(BaseJUnit4ClassRunner.class)
public class SmokeTest {
    @Rule
    public WebLayerShellActivityTestRule mActivityTestRule = new WebLayerShellActivityTestRule();

    @Test
    @SmallTest
    public void testSetSupportEmbedding() {
        WebLayerShellActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { activity.getBrowserFragmentController().setSupportsEmbedding(true); });

        CountDownLatch latch = new CountDownLatch(1);
        String url = "data:text,foo";

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            activity.getBrowserFragmentController().setSupportsEmbedding(true).addCallback(
                    (Boolean result) -> {
                        Assert.assertTrue(result);
                        latch.countDown();
                    });
        });

        try {
            latch.await();
        } catch (InterruptedException e) {
            Assert.fail(e.toString());
        }
        mActivityTestRule.navigateAndWait(url);
    }
}
