// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.net.Uri;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.CookieManager;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that CookieManager works as expected.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class CookieManagerTest {
    private CookieManager mCookieManager;

    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    @Before
    public void setUp() {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        mCookieManager = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> { return activity.getBrowser().getProfile().getCookieManager(); });
    }

    @Test
    @SmallTest
    public void testSetCookie() throws Exception {
        Assert.assertTrue(setCookie("foo=bar"));

        mActivityTestRule.navigateAndWait(
                mActivityTestRule.getTestServer().getURL("/echoheader?Cookie"));
        Assert.assertEquals("foo=bar",
                mActivityTestRule.executeScriptAndExtractString("document.body.textContent"));
    }

    @Test
    @SmallTest
    public void testSetCookieInvalid() throws Exception {
        String baseUrl = mActivityTestRule.getTestServer().getURL("/");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                mCookieManager.setCookie(Uri.parse(baseUrl), "", null);
                Assert.fail("Exception not thrown.");
            } catch (IllegalArgumentException e) {
                Assert.assertEquals(e.getMessage(), "Invalid cookie: ");
            }
        });
    }

    @Test
    @SmallTest
    public void testSetCookieNotSet() throws Exception {
        Assert.assertFalse(setCookie("foo=bar; Secure"));
    }

    @Test
    @SmallTest
    public void testSetCookieNullCallback() throws Exception {
        String baseUrl = mActivityTestRule.getTestServer().getURL("/");
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mCookieManager.setCookie(Uri.parse(baseUrl), "foo=bar", null); });

        // Do a navigation to make sure the cookie gets set.
        mActivityTestRule.navigateAndWait(mActivityTestRule.getTestServer().getURL("/echo"));

        mActivityTestRule.navigateAndWait(
                mActivityTestRule.getTestServer().getURL("/echoheader?Cookie"));
        Assert.assertEquals("foo=bar",
                mActivityTestRule.executeScriptAndExtractString("document.body.textContent"));
    }

    @Test
    @SmallTest
    public void testGetCookie() throws Exception {
        Assert.assertEquals(getCookie(), "");
        Assert.assertTrue(setCookie("foo="));
        Assert.assertEquals(getCookie(), "foo=");
        Assert.assertTrue(setCookie("foo=bar"));
        Assert.assertEquals(getCookie(), "foo=bar");
    }

    private boolean setCookie(String value) throws Exception {
        Boolean[] resultHolder = new Boolean[1];
        CallbackHelper callbackHelper = new CallbackHelper();
        String baseUrl = mActivityTestRule.getTestServer().getURL("/");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mCookieManager.setCookie(Uri.parse(baseUrl), value, (Boolean result) -> {
                resultHolder[0] = result;
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForFirst();
        return resultHolder[0];
    }

    private String getCookie() throws Exception {
        String[] resultHolder = new String[1];
        CallbackHelper callbackHelper = new CallbackHelper();
        String baseUrl = mActivityTestRule.getTestServer().getURL("/");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mCookieManager.getCookie(Uri.parse(baseUrl), (String result) -> {
                resultHolder[0] = result;
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForFirst();
        return resultHolder[0];
    }
}
