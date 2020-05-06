// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertNotNull;

import android.os.Build;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsFeatureList;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.location.LocationUtils;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.page_info.PageInfoView;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/**
 * Tests for PageInfoView. Uses pixel tests to ensure the UI handles different
 * configurations correctly.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
@Features.
EnableFeatures(ContentSettingsFeatureList.IMPROVED_COOKIE_CONTROLS_FOR_THIRD_PARTY_COOKIE_BLOCKING)
public class PageInfoViewTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule.SkiaGoldBuilder().setRevision(2).build();

    private boolean mIsSystemLocationSettingEnabled = true;

    private class TestLocationUtils extends LocationUtils {
        @Override
        public boolean isSystemLocationSettingEnabled() {
            return mIsSystemLocationSettingEnabled;
        }
    }

    @ClassRule
    public static DisableAnimationsTestRule disableAnimationsRule = new DisableAnimationsTestRule();

    private final String mPath = "/chrome/test/data/android/simple.html";

    private void loadUrlAndOpenPageInfo(String url) {
        mActivityTestRule.loadUrl(url);
        onView(withId(R.id.location_bar_status_icon)).perform(click());
    }

    private PageInfoView getPageInfoView() {
        PageInfoController controller = PageInfoController.getLastPageInfoControllerForTesting();
        assertNotNull(controller);
        PageInfoView view = controller.getPageInfoViewForTesting();
        assertNotNull(view);
        return view;
    }

    private void setThirdPartyCookieBlocking(boolean value) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefServiceBridge.getInstance().setBoolean(Pref.BLOCK_THIRD_PARTY_COOKIES, value);
        });
    }

    private void addSomePermissions(String url) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            WebsitePreferenceBridge.setContentSettingForPattern(Profile.getLastUsedRegularProfile(),
                    ContentSettingsType.GEOLOCATION, url, "*", ContentSettingValues.ALLOW);
            WebsitePreferenceBridge.setContentSettingForPattern(Profile.getLastUsedRegularProfile(),
                    ContentSettingsType.NOTIFICATIONS, url, "*", ContentSettingValues.ALLOW);
        });
    }

    @Before
    public void setUp() throws InterruptedException {
        // Some test devices have geolocation disabled. Override LocationUtils for a stable result.
        LocationUtils.setFactory(TestLocationUtils::new);

        // Choose a fixed, "random" port to create stable screenshots.
        mTestServerRule.setServerPort(424242);
        mTestServerRule.setServerUsesHttps(true);

        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() {
        LocationUtils.setFactory(null);
        // Notification channels don't get cleaned up automatically.
        // TODO(crbug.com/951402): Find a general solution to avoid leaking channels between tests.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                SiteChannelsManager manager = SiteChannelsManager.getInstance();
                manager.deleteAllSiteChannels();
            });
        }
    }

    /**
     * Tests PageInfo on an insecure website.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowOnInsecureHttpWebsite() throws IOException {
        mTestServerRule.setServerUsesHttps(false);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(mPath));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_HttpWebsite");
    }

    /**
     * Tests PageInfo on a secure website.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowOnSecureWebsite() throws IOException {
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(mPath));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_SecureWebsite");
    }

    /**
     * Tests PageInfo on a website with expired certificate.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowOnExpiredCertificateWebsite() throws IOException {
        mTestServerRule.setCertificateType(ServerCertificate.CERT_EXPIRED);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(mPath));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_ExpiredCertWebsite");
    }

    /**
     * Tests PageInfo on internal page.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testChromePage() throws IOException {
        loadUrlAndOpenPageInfo("chrome://version/");
        mRenderTestRule.render(getPageInfoView(), "PageInfo_InternalSite");
    }

    /**
     * Tests PageInfo on a website with permissions.
     * Geolocation is blocked system wide in this test.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowWithPermissions() throws IOException {
        mIsSystemLocationSettingEnabled = false;
        addSomePermissions(mTestServerRule.getServer().getURL("/"));
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(mPath));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_Permissions");
    }

    /**
     * Tests PageInfo on a website with cookie controls enabled.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowWithCookieBlocking() throws IOException {
        setThirdPartyCookieBlocking(true);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(mPath));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_CookieBlocking");
    }

    /**
     * Tests PageInfo on a website with cookie controls and permissions.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowWithPermissionsAndCookieBlocking() throws IOException {
        addSomePermissions(mTestServerRule.getServer().getURL("/"));
        setThirdPartyCookieBlocking(true);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(mPath));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_PermissionsAndCookieBlocking");
    }

    // TODO(1071762): Add tests for preview pages, offline pages, offline state and other states.
}
