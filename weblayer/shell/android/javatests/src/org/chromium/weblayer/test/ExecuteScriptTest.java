// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.support.test.filters.SmallTest;

import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.BrowserController;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that script execution works as expected.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ExecuteScriptTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private static final String DATA_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><script>var bar = 10;</script></head><body>foo</body></html>");

    @Test
    @SmallTest
    public void testBasicScript() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(DATA_URL);
        JSONObject result = mActivityTestRule.executeScriptSync("document.body.innerHTML");
        Assert.assertEquals(result.getString(BrowserController.SCRIPT_RESULT_KEY), "foo");
    }

    @Test
    @SmallTest
    public void testScriptIsolatedFromPage() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(DATA_URL);
        JSONObject result = mActivityTestRule.executeScriptSync("bar");
        Assert.assertTrue(result.isNull(BrowserController.SCRIPT_RESULT_KEY));
    }

    @Test
    @SmallTest
    public void testScriptNotIsolatedFromOtherScript() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(DATA_URL);
        mActivityTestRule.executeScriptSync("var foo = 20;");
        JSONObject result = mActivityTestRule.executeScriptSync("foo");
        Assert.assertEquals(result.getInt(BrowserController.SCRIPT_RESULT_KEY), 20);
    }

    @Test
    @SmallTest
    public void testClearedOnNavigate() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(DATA_URL);
        mActivityTestRule.executeScriptSync("var foo = 20;");

        String newUrl = UrlUtils.encodeHtmlDataUri("<html></html>");
        mActivityTestRule.navigateAndWait(newUrl);
        JSONObject result = mActivityTestRule.executeScriptSync("foo");
        Assert.assertTrue(result.isNull(BrowserController.SCRIPT_RESULT_KEY));
    }

    @Test
    @SmallTest
    public void testNullCallback() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(DATA_URL);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Null callback should not crash.
            activity.getBrowserController().executeScript("null", null);
        });
        // Execute a sync script to make sure the other script finishes.
        mActivityTestRule.executeScriptSync("null");
    }
}
