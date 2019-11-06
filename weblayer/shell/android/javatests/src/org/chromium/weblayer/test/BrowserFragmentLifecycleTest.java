// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.net.Uri;
import android.support.test.filters.SmallTest;
import android.support.v4.app.FragmentManager;

import androidx.annotation.NonNull;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.BrowserController;
import org.chromium.weblayer.Navigation;
import org.chromium.weblayer.NavigationCallback;
import org.chromium.weblayer.NavigationController;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.util.concurrent.CountDownLatch;

@RunWith(BaseJUnit4ClassRunner.class)
public class BrowserFragmentLifecycleTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    @Test
    @SmallTest
    public void successfullyLoadsUrlAfterRecreation() {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        BrowserController controller = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> activity.getBrowserController());

        String url = "data:text,foo";
        mActivityTestRule.navigateAndWait(controller, url, false);

        mActivityTestRule.recreateActivity();

        InstrumentationActivity newActivity = mActivityTestRule.getActivity();
        controller = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> newActivity.getBrowserController());
        url = "data:text,bar";
        mActivityTestRule.navigateAndWait(controller, url, false);
    }

    // https://crbug.com/1021041
    @Test
    @SmallTest
    public void handlesFragmentDestroyWhileNavigating() throws InterruptedException {
        CountDownLatch latch = new CountDownLatch(1);
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            NavigationController navigationController =
                    activity.getBrowserController().getNavigationController();
            navigationController.registerNavigationCallback(new NavigationCallback() {
                @Override
                public void readyToCommitNavigation(@NonNull Navigation navigation) {
                    FragmentManager fm = activity.getSupportFragmentManager();
                    fm.beginTransaction()
                            .remove(fm.getFragments().get(0))
                            .runOnCommit(latch::countDown)
                            .commit();
                }
            });
            navigationController.navigate(Uri.parse("data:text,foo"));
        });
        latch.await();
    }
}
