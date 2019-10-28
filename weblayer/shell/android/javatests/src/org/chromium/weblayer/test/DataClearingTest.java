// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static org.junit.Assert.assertTrue;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.os.Bundle;
import android.support.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.shell.WebLayerShellActivity;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Example test that just starts the weblayer shell.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class DataClearingTest {
    @Rule
    public WebLayerShellActivityTestRule mActivityTestRule = new WebLayerShellActivityTestRule();

    @Test
    @SmallTest
    public void clearDataWithPersistedProfile_TriggersCallback() throws InterruptedException {
        checkTriggersCallbackOnClearData("Profile");
    }

    @Test
    @SmallTest
    public void clearDataWithInMemoryProfile_TriggersCallback() throws InterruptedException {
        checkTriggersCallbackOnClearData("");
    }

    // The tests below should rather be unit tests for ProfileImpl.
    @Test
    @SmallTest
    public void twoSuccesiveRequestsTriggerCallbacks() throws InterruptedException {
        WebLayerShellActivity activity = launchWithProfile("profile");

        CountDownLatch latch = new CountDownLatch(2);
        runOnUiThreadBlocking(() -> {
            Profile profile = activity.getBrowserFragmentController().getProfile();
            profile.clearBrowsingData().addCallback((ignored) -> latch.countDown());
            profile.clearBrowsingData().addCallback((ignored) -> latch.countDown());
        });
        assertTrue(latch.await(3, TimeUnit.SECONDS));
    }

    @Test
    @SmallTest
    public void clearingAgainAfterClearFinished_TriggersCallback() throws InterruptedException {
        WebLayerShellActivity activity = launchWithProfile("profile");

        CountDownLatch latch = new CountDownLatch(1);
        runOnUiThreadBlocking(() -> {
            Profile profile = activity.getBrowserFragmentController().getProfile();
            profile.clearBrowsingData().addCallback((v1) -> {
                profile.clearBrowsingData().addCallback((v2) -> latch.countDown());
            });
        });
        assertTrue(latch.await(3, TimeUnit.SECONDS));
    }

    @Test
    @SmallTest
    public void threeSuccesiveRequestsTriggerCallbacks() throws InterruptedException {
        WebLayerShellActivity activity = launchWithProfile("profile");

        CountDownLatch latch = new CountDownLatch(3);
        runOnUiThreadBlocking(() -> {
            Profile profile = activity.getBrowserFragmentController().getProfile();
            profile.clearBrowsingData().addCallback((ignored) -> latch.countDown());
            profile.clearBrowsingData().addCallback((ignored) -> latch.countDown());
            profile.clearBrowsingData().addCallback((ignored) -> latch.countDown());
        });
        assertTrue(latch.await(3, TimeUnit.SECONDS));
    }

    private void checkTriggersCallbackOnClearData(String profileName) throws InterruptedException {
        WebLayerShellActivity activity = launchWithProfile(profileName);
        CountDownLatch latch = new CountDownLatch(1);
        runOnUiThreadBlocking(() -> activity.getBrowserFragmentController().getProfile()
                .clearBrowsingData().addCallback((ignored) -> latch.countDown()));
        assertTrue(latch.await(3, TimeUnit.SECONDS));
    }

    private WebLayerShellActivity launchWithProfile(String profileName) {
        Bundle extras = new Bundle();
        extras.putString(WebLayerShellActivity.EXTRA_PROFILE_NAME, profileName);
        String url = "data:text,foo";
        WebLayerShellActivity activity = mActivityTestRule.launchShellWithUrl(url, extras);
        return activity;
    }

}
