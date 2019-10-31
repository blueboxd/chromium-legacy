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
import org.chromium.weblayer.BrowserCallback;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Tests that BrowserCallback methods are invoked as expected.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class BrowserCallbackTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private static class Callback extends BrowserCallback {
        public static class BrowserCallbackValueRecorder {
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

        public BrowserCallbackValueRecorder visibleUrlChangedCallback =
                new BrowserCallbackValueRecorder();

        @Override
        public void visibleUrlChanged(Uri url) {
            visibleUrlChangedCallback.recordValue(url.toString());
        }
    }

    @Test
    @SmallTest
    public void testLoadEvents() {
        String startupUrl = "about:blank";
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(startupUrl);

        Callback calllback = new Callback();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { activity.getBrowserController().registerBrowserCallback(calllback); });

        String url = "data:text,foo";
        mActivityTestRule.navigateAndWait(url);

        /* Verify that the visible URL changes to the target. */
        calllback.visibleUrlChangedCallback.waitUntilValueObserved(url);
    }
}
