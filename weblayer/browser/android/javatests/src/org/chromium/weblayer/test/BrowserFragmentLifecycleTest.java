// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.net.Uri;
import android.os.Bundle;
import android.support.test.filters.SmallTest;

import androidx.annotation.NonNull;
import androidx.fragment.app.FragmentManager;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Navigation;
import org.chromium.weblayer.NavigationCallback;
import org.chromium.weblayer.NavigationController;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that fragment lifecycle works as expected.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class BrowserFragmentLifecycleTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private Tab getTab() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mActivityTestRule.getActivity().getTab());
    }

    @Test
    @SmallTest
    public void successfullyLoadsUrlAfterRecreation() {
        mActivityTestRule.launchShellWithUrl("about:blank");
        String url = "data:text,foo";
        mActivityTestRule.navigateAndWait(getTab(), url, false);

        mActivityTestRule.recreateActivity();

        url = "data:text,bar";
        mActivityTestRule.navigateAndWait(getTab(), url, false);
    }

    @Test
    @SmallTest
    public void restoreAfterRecreate() throws Throwable {
        mActivityTestRule.launchShellWithUrl("about:blank");
        String url = "data:text,foo";
        mActivityTestRule.navigateAndWait(getTab(), url, false);

        mActivityTestRule.recreateActivity();

        waitForTabToFinishRestore(getTab(), url);
    }

    // https://crbug.com/1021041
    @Test
    @SmallTest
    public void handlesFragmentDestroyWhileNavigating() {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        BoundedCountDownLatch latch = new BoundedCountDownLatch(1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            NavigationController navigationController = activity.getTab().getNavigationController();
            navigationController.registerNavigationCallback(new NavigationCallback() {
                @Override
                public void onReadyToCommitNavigation(@NonNull Navigation navigation) {
                    FragmentManager fm = activity.getSupportFragmentManager();
                    fm.beginTransaction()
                            .remove(fm.getFragments().get(0))
                            .runOnCommit(latch::countDown)
                            .commit();
                }
            });
            navigationController.navigate(Uri.parse("data:text,foo"));
        });
        latch.timedAwait();
    }

    // Waits for |tab| to finish loadding |url. This is intended to be called after restore.
    private void waitForTabToFinishRestore(Tab tab, String url) {
        BoundedCountDownLatch latch = new BoundedCountDownLatch(1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // It's possible the NavigationController hasn't loaded yet, handle either scenario.
            NavigationController navigationController = tab.getNavigationController();
            for (int i = 0; i < navigationController.getNavigationListSize(); ++i) {
                if (navigationController.getNavigationEntryDisplayUri(i).equals(Uri.parse(url))) {
                    latch.countDown();
                    return;
                }
            }
            navigationController.registerNavigationCallback(new NavigationCallback() {
                @Override
                public void onNavigationCompleted(@NonNull Navigation navigation) {
                    if (navigation.getUri().equals(Uri.parse(url))) {
                        latch.countDown();
                    }
                }
            });
        });
        latch.timedAwait();
    }

    // Recreates the activity and waits for the first tab to be restored. |extras| is the Bundle
    // used to launch the shell.
    private void restoresPreviousSession(Bundle extras) {
        extras.putString(InstrumentationActivity.EXTRA_PERSISTENCE_ID, "x");
        final String url = mActivityTestRule.getTestDataURL("simple_page.html");
        mActivityTestRule.launchShellWithUrl(url, extras);

        mActivityTestRule.recreateActivity();

        Tab tab = getTab();
        Assert.assertNotNull(tab);
        waitForTabToFinishRestore(tab, url);
    }

    @Test
    @SmallTest
    public void restoresPreviousSession() throws Throwable {
        restoresPreviousSession(new Bundle());
    }

    @Test
    @SmallTest
    public void restoresPreviousSessionIncognito() throws Throwable {
        Bundle extras = new Bundle();
        // This forces incognito.
        extras.putString(InstrumentationActivity.EXTRA_PROFILE_NAME, null);
        restoresPreviousSession(extras);
    }

    @Test
    @SmallTest
    public void restoresTabGuid() throws Throwable {
        Bundle extras = new Bundle();
        extras.putString(InstrumentationActivity.EXTRA_PERSISTENCE_ID, "x");
        final String url = mActivityTestRule.getTestDataURL("simple_page.html");
        mActivityTestRule.launchShellWithUrl(url, extras);
        final String initialTabId = TestThreadUtils.runOnUiThreadBlocking(
                () -> { return mActivityTestRule.getActivity().getTab().getGuid(); });
        Assert.assertNotNull(initialTabId);
        Assert.assertFalse(initialTabId.isEmpty());

        mActivityTestRule.recreateActivity();

        Tab tab = getTab();
        Assert.assertNotNull(tab);
        waitForTabToFinishRestore(tab, url);
        final String restoredTabId =
                TestThreadUtils.runOnUiThreadBlockingNoException(() -> { return tab.getGuid(); });
        Assert.assertEquals(initialTabId, restoredTabId);
    }

    @Test
    @SmallTest
    public void restoreTabGuidAfterRecreate() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        final Tab tab = getTab();
        final String initialTabId =
                TestThreadUtils.runOnUiThreadBlocking(() -> { return tab.getGuid(); });
        String url = "data:text,foo";
        mActivityTestRule.navigateAndWait(tab, url, false);

        mActivityTestRule.recreateActivity();

        final Tab restoredTab = getTab();
        Assert.assertNotEquals(tab, restoredTab);
        waitForTabToFinishRestore(restoredTab, url);
        final String restoredTabId = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> { return restoredTab.getGuid(); });
        Assert.assertEquals(initialTabId, restoredTabId);
    }
}
