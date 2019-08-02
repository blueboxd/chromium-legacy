// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.CommandLine;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.test.MockCertVerifierRuleAndroid;
import org.chromium.chrome.browser.touchless.TouchlessDelegate;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.webapk.lib.client.WebApkValidator;
import org.chromium.webapk.lib.common.WebApkConstants;

/** Integration tests for WebAPK feature. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebApkIntegrationTest {
    public final ChromeActivityTestRule<WebApkActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(WebApkActivity.class);

    public final NativeLibraryTestRule mNativeLibraryTestRule = new NativeLibraryTestRule();

    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    public MockCertVerifierRuleAndroid mCertVerifierRule =
            new MockCertVerifierRuleAndroid(mNativeLibraryTestRule, 0 /* net::OK */);

    @Rule
    public RuleChain mRuleChain = RuleChain.emptyRuleChain()
                                          .around(mActivityTestRule)
                                          .around(mNativeLibraryTestRule)
                                          .around(mCertVerifierRule)
                                          .around(mTestServerRule);

    private static final long STARTUP_TIMEOUT = ScalableTimeout.scaleTimeout(10000);

    public void startWebApkActivity(String webApkPackageName, final String startUrl)
            throws InterruptedException {
        Intent intent =
                new Intent(InstrumentationRegistry.getTargetContext(), WebApkActivity0.class);
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, webApkPackageName);
        intent.putExtra(ShortcutHelper.EXTRA_URL, startUrl);
        intent.addFlags(
                Intent.FLAG_ACTIVITY_NEW_TASK | ApiCompatibilityUtils.getActivityNewDocumentFlag());

        WebApkActivity webApkActivity =
                (WebApkActivity) InstrumentationRegistry.getInstrumentation().startActivitySync(
                        intent);
        mActivityTestRule.setActivity(webApkActivity);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mActivityTestRule.getActivity().getActivityTab() != null;
            }
        }, STARTUP_TIMEOUT, CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        ChromeTabUtils.waitForTabPageLoaded(
                mActivityTestRule.getActivity().getActivityTab(), startUrl);
    }

    /** Waits for the splash screen to be hidden. */
    public void waitUntilSplashscreenHides() {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mActivityTestRule.getActivity().getSplashScreenForTests() == null;
            }
        });
    }

    /** Register WebAPK with WebappDataStorage */
    private WebappDataStorage registerWithStorage(final String webappId) throws Exception {
        TestFetchStorageCallback callback = new TestFetchStorageCallback();
        WebappRegistry.getInstance().register(webappId, callback);
        callback.waitForCallback(0);
        return WebappRegistry.getInstance().getWebappDataStorage(webappId);
    }

    /** Returns URL for the passed-in host which maps to a page on the EmbeddedTestServer. */
    private String getUrlForHost(String host) {
        return "https://" + host + "/defaultresponse";
    }

    @Before
    public void setUp() throws Exception {
        WebApkUpdateManager.setUpdatesEnabledForTesting(false);
        mTestServerRule.setServerUsesHttps(true);
        Uri mapToUri = Uri.parse(mTestServerRule.getServer().getURL("/"));
        CommandLine.getInstance().appendSwitchWithValue(
                ContentSwitches.HOST_RESOLVER_RULES, "MAP * " + mapToUri.getAuthority());
        WebApkValidator.disableValidationForTesting();
    }

    /**
     * Tests that WebApkActivities are started properly by WebappLauncherActivity.
     */
    @Test
    @LargeTest
    @Feature({"Webapps"})
    public void testWebApkLaunchesByLauncherActivity() {
        String pwaRocksUrl = getUrlForHost("pwa.rocks");

        Intent intent = new Intent();
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setPackage(InstrumentationRegistry.getTargetContext().getPackageName());
        intent.setAction(WebappLauncherActivity.ACTION_START_WEBAPP);
        intent.putExtra(WebApkConstants.EXTRA_URL, pwaRocksUrl)
                .putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, "org.chromium.webapk.test");

        mActivityTestRule.startActivityCompletely(intent);

        WebApkActivity lastActivity = mActivityTestRule.getActivity();
        Assert.assertEquals(pwaRocksUrl, lastActivity.getWebappInfo().uri().toString());
    }

    /**
     * Test launching a WebAPK. Test that loading the start page works and that the splashscreen
     * eventually hides.
     */
    @Test
    @LargeTest
    @Feature({"WebApk"})
    public void testLaunchAndNavigateOffOrigin() throws Exception {
        startWebApkActivity("org.chromium.webapk.test", getUrlForHost("pwa.rocks"));
        waitUntilSplashscreenHides();
        WebApkActivity webApkActivity = mActivityTestRule.getActivity();
        WebappActivityTestRule.assertToolbarShowState(webApkActivity, false);

        // We navigate outside origin and expect CCT toolbar to show on top of WebApkActivity.
        String googleUrl = getUrlForHost("www.google.com");
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                "window.top.location = '" + googleUrl + "'");

        ChromeTabUtils.waitForTabPageLoaded(webApkActivity.getActivityTab(), googleUrl);
        WebappActivityTestRule.assertToolbarShowState(webApkActivity, true);
    }

    /**
     * Test launching a WebAPK. Test that open a url within scope through window.open() will open a
     * CCT.
     */
    @Test
    @LargeTest
    @Feature({"WebApk"})
    public void testLaunchAndOpenNewWindowInOrigin() throws Exception {
        String pwaRocksUrl = getUrlForHost("pwa.rocks");
        startWebApkActivity("org.chromium.webapk.test", pwaRocksUrl);
        waitUntilSplashscreenHides();

        WebappActivityTestRule.jsWindowOpen(mActivityTestRule.getActivity(), pwaRocksUrl);

        CustomTabActivity customTabActivity =
                ChromeActivityTestRule.waitFor(CustomTabActivity.class);
        ChromeTabUtils.waitForTabPageLoaded(customTabActivity.getActivityTab(), pwaRocksUrl);
        Assert.assertTrue(
                "Sending to external handlers needs to be enabled for redirect back (e.g. OAuth).",
                IntentUtils.safeGetBooleanExtra(customTabActivity.getIntent(),
                        CustomTabIntentDataProvider.EXTRA_SEND_TO_EXTERNAL_DEFAULT_HANDLER, false));
    }

    /**
     * Test launching a WebAPK. Test that open a url off scope through window.open() will open a
     * CCT, and in scope urls will stay in the CCT.
     */
    @Test
    @LargeTest
    @Feature({"WebApk"})
    public void testLaunchAndNavigationInNewWindowOffandInOrigin() throws Exception {
        String pwaRocksUrl = getUrlForHost("pwa.rocks");
        String googleUrl = getUrlForHost("www.google.com");
        startWebApkActivity("org.chromium.webapk.test", pwaRocksUrl);
        waitUntilSplashscreenHides();

        WebappActivityTestRule.jsWindowOpen(mActivityTestRule.getActivity(), googleUrl);
        CustomTabActivity customTabActivity =
                ChromeActivityTestRule.waitFor(CustomTabActivity.class);
        ChromeTabUtils.waitForTabPageLoaded(customTabActivity.getActivityTab(), googleUrl);

        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                customTabActivity.getActivityTab().getWebContents(),
                String.format("window.location.href='%s'", pwaRocksUrl));
        ChromeTabUtils.waitForTabPageLoaded(customTabActivity.getActivityTab(), pwaRocksUrl);
    }

    /**
     * Test that on first launch:
     * - the "WebApk.LaunchInterval" histogram is not recorded (because there is no prevous launch
     *   to compute the interval from).
     * - the "last used" time is updated (to compute future "launch intervals").
     */
    @Test
    @LargeTest
    @Feature({"WebApk"})
    public void testLaunchIntervalHistogramNotRecordedOnFirstLaunch() throws Exception {
        final String histogramName = "WebApk.LaunchInterval";
        final String packageName = "org.chromium.webapk.test";
        startWebApkActivity(packageName, getUrlForHost("pwa.rocks"));

        CriteriaHelper.pollUiThread(new Criteria("Deferred startup never completed") {
            @Override
            public boolean isSatisfied() {
                return DeferredStartupHandler.getInstance().isDeferredStartupCompleteForApp();
            }
        });
        Assert.assertEquals(0, RecordHistogram.getHistogramTotalCountForTesting(histogramName));
        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage(
                WebApkConstants.WEBAPK_ID_PREFIX + packageName);
        Assert.assertNotEquals(WebappDataStorage.TIMESTAMP_INVALID, storage.getLastUsedTimeMs());
    }

    /** Test that the "WebApk.LaunchInterval" histogram is recorded on susbequent launches. */
    @Test
    @LargeTest
    @Feature({"WebApk"})
    public void testLaunchIntervalHistogramRecordedOnSecondLaunch() throws Exception {
        mNativeLibraryTestRule.loadNativeLibraryNoBrowserProcess();

        final String histogramName = "WebApk.LaunchInterval2";
        final String packageName = "org.chromium.webapk.test";

        WebappDataStorage storage =
                registerWithStorage(WebApkConstants.WEBAPK_ID_PREFIX + packageName);
        storage.setHasBeenLaunched();
        storage.updateLastUsedTime();
        Assert.assertEquals(0, RecordHistogram.getHistogramTotalCountForTesting(histogramName));

        startWebApkActivity(packageName, getUrlForHost("pwa.rocks"));

        CriteriaHelper.pollUiThread(new Criteria("Deferred startup never completed") {
            @Override
            public boolean isSatisfied() {
                return DeferredStartupHandler.getInstance().isDeferredStartupCompleteForApp();
            }
        });

        Assert.assertEquals(1, RecordHistogram.getHistogramTotalCountForTesting(histogramName));
    }

    /**
     * Test that {@link TabWebContentsDelegateAndroid#activateContents} brings a WebAPK to the
     * foreground.
     */
    @LargeTest
    @Test
    public void testActivateWebApk() throws Exception {
        // Launch WebAPK.
        startWebApkActivity("org.chromium.webapk.test", getUrlForHost("pwa.rocks"));
        waitUntilSplashscreenHides();

        Class<? extends ChromeActivity> mainClass = FeatureUtilities.isNoTouchModeEnabled()
                ? TouchlessDelegate.getNoTouchActivityClass()
                : ChromeTabbedActivity.class;

        // Move WebAPK to the background by launching Chrome.
        Intent intent = new Intent(InstrumentationRegistry.getTargetContext(), mainClass);
        intent.setFlags(
                Intent.FLAG_ACTIVITY_NEW_TASK | ApiCompatibilityUtils.getActivityNewDocumentFlag());
        InstrumentationRegistry.getTargetContext().startActivity(intent);
        ChromeActivityTestRule.waitFor(mainClass);

        WebApkActivity webApkActivity = mActivityTestRule.getActivity();
        TabWebContentsDelegateAndroid tabDelegate =
                webApkActivity.getActivityTab().getTabWebContentsDelegateAndroid();
        tabDelegate.activateContents();

        // WebApkActivity should have been brought back to the foreground.
        ChromeActivityTestRule.waitFor(WebApkActivity.class);
    }
}
