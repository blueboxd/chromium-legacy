// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.net.Uri;
import android.support.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.BrowserObserver;
import org.chromium.weblayer.shell.WebLayerShellActivity;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Tests that BrowserObserver methods are invoked as expected.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class BrowserObserverTest {
    @Rule
    public WebLayerShellActivityTestRule mActivityTestRule = new WebLayerShellActivityTestRule();

    private static class Observer extends BrowserObserver {
        public static class BrowserObserverValueRecorder {
            private List<String> mObservedValues =
                    Collections.synchronizedList(new ArrayList<String>());

            public void recordValue(String parameter) {
                mObservedValues.add(parameter);
            }

            public List<String> getObservedValues() {
                return mObservedValues;
            }

            public void waitUntilValueObserved(String expectation) {
                CriteriaHelper.pollInstrumentationThread(
                        new Criteria() {
                            @Override
                            public boolean isSatisfied() {
                                return mObservedValues.contains(expectation);
                            }
                        },
                        CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL,
                        CriteriaHelper.DEFAULT_POLLING_INTERVAL);
            }
        }

        public BrowserObserverValueRecorder visibleUrlChangedCallback =
                new BrowserObserverValueRecorder();

        @Override
        public void visibleUrlChanged(Uri url) {
            visibleUrlChangedCallback.recordValue(url.toString());
        }
    }

    @Test
    @SmallTest
    public void testLoadEvents() {
        String startupUrl = "about:blank";
        WebLayerShellActivity activity = mActivityTestRule.launchShellWithUrl(startupUrl);

        Observer observer = new Observer();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { activity.getBrowserController().addObserver(observer); });

        String url = "data:text,foo";
        mActivityTestRule.navigateAndWait(url);

        /* Verify that the visible URL changes to the target. */
        observer.visibleUrlChangedCallback.waitUntilValueObserved(url);
    }
}
