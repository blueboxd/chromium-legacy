// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.net.Uri;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Navigation;
import org.chromium.weblayer.NavigationObserver;
import org.chromium.weblayer.shell.WebLayerShellActivity;

import java.util.concurrent.TimeoutException;

/**
 * Example test that just starts the weblayer shell.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class NavigationTest {
    @Rule
    public WebLayerShellActivityTestRule mActivityTestRule = new WebLayerShellActivityTestRule();

    // URL used for base tests.
    private static final String URL = "data:text";

    private static class Observer extends NavigationObserver {
        public static class NavigationCallbackHelper extends CallbackHelper {
            private Uri mUri;

            public void notifyCalled(Uri uri) {
                mUri = uri;
                notifyCalled();
            }

            public void assertCalledWith(int currentCallCount, String uri)
                    throws TimeoutException, InterruptedException {
                waitForCallback(currentCallCount);
                Assert.assertEquals(mUri.toString(), uri);
            }
        }

        public NavigationCallbackHelper onStartedCallback = new NavigationCallbackHelper();
        public NavigationCallbackHelper onCommittedCallback = new NavigationCallbackHelper();
        public NavigationCallbackHelper onCompletedCallback = new NavigationCallbackHelper();

        @Override
        public void navigationStarted(Navigation navigation) {
            onStartedCallback.notifyCalled(navigation.getUri());
        }

        @Override
        public void navigationCommitted(Navigation navigation) {
            onCommittedCallback.notifyCalled(navigation.getUri());
        }

        @Override
        public void navigationCompleted(Navigation navigation) {
            onCompletedCallback.notifyCalled(navigation.getUri());
        }
    }

    @Test
    @SmallTest
    public void testBaseStartup() throws Exception {
        WebLayerShellActivity activity = mActivityTestRule.launchShellWithUrl(URL);

        Assert.assertNotNull(activity);

        mActivityTestRule.waitForNavigation(URL);
    }

    @Test
    @SmallTest
    public void testNavigationEvents() throws Exception {
        WebLayerShellActivity activity = mActivityTestRule.launchShellWithUrl(URL);
        mActivityTestRule.waitForNavigation(URL);

        Observer observer = new Observer();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            activity.getBrowserController().getNavigationController().addObserver(observer);
        });
        int curStartedCount = observer.onStartedCallback.getCallCount();
        int curCommittedCount = observer.onCommittedCallback.getCallCount();
        int curCompletedCount = observer.onCompletedCallback.getCallCount();

        String url = "data:text,foo";
        mActivityTestRule.loadUrl(url);
        mActivityTestRule.waitForNavigation(url);

        observer.onStartedCallback.assertCalledWith(curStartedCount, url);
        observer.onCommittedCallback.assertCalledWith(curCommittedCount, url);
        observer.onCompletedCallback.assertCalledWith(curCompletedCount, url);
    }
}
