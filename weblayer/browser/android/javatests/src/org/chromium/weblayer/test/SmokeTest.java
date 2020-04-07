// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.support.test.filters.SmallTest;
import android.support.v4.app.Fragment;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.lang.ref.PhantomReference;
import java.lang.ref.Reference;
import java.lang.ref.ReferenceQueue;

/**
 * Basic tests to make sure WebLayer works as expected.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class SmokeTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    @Test
    @SmallTest
    public void testSetSupportEmbedding() {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { activity.getBrowser().setSupportsEmbedding(true, (result) -> {}); });

        BoundedCountDownLatch latch = new BoundedCountDownLatch(1);
        String url = "data:text,foo";

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            activity.getBrowser().setSupportsEmbedding(true, (result) -> {
                Assert.assertTrue(result);
                latch.countDown();
            });
        });

        latch.timedAwait();
        mActivityTestRule.navigateAndWait(url);
    }

    @Test
    @SmallTest
    public void testActivityShouldNotLeak() {
        ReferenceQueue<InstrumentationActivity> referenceQueue = new ReferenceQueue<>();
        PhantomReference<InstrumentationActivity> reference;
        {
            InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                activity.getTab().setFullscreenCallback(new TestFullscreenCallback());
            });
            mActivityTestRule.recreateActivity();
            boolean destroyed =
                    TestThreadUtils.runOnUiThreadBlockingNoException(() -> activity.isDestroyed());
            Assert.assertTrue(destroyed);

            reference = new PhantomReference<>(activity, referenceQueue);
        }

        Runtime.getRuntime().gc();
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Reference enqueuedReference = referenceQueue.poll();
                if (enqueuedReference == null) {
                    Runtime.getRuntime().gc();
                    return false;
                }
                Assert.assertEquals(reference, enqueuedReference);
                return true;
            }
        });
    }

    @Test
    @SmallTest
    public void testSetRetainInstance() {
        ReferenceQueue<InstrumentationActivity> referenceQueue = new ReferenceQueue<>();
        PhantomReference<InstrumentationActivity> reference;
        {
            InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");

            mActivityTestRule.setRetainInstance(true);
            Fragment firstFragment = mActivityTestRule.getFragment();
            mActivityTestRule.recreateActivity();
            Fragment secondFragment = mActivityTestRule.getFragment();
            Assert.assertEquals(firstFragment, secondFragment);

            boolean destroyed =
                    TestThreadUtils.runOnUiThreadBlockingNoException(() -> activity.isDestroyed());
            Assert.assertTrue(destroyed);
            reference = new PhantomReference<>(activity, referenceQueue);
        }

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Reference enqueuedReference = referenceQueue.poll();
                if (enqueuedReference == null) {
                    Runtime.getRuntime().gc();
                    return false;
                }
                Assert.assertEquals(reference, enqueuedReference);
                return true;
            }
        });
    }
}
