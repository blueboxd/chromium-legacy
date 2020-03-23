// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Build;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.ExternalNavigationParams;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Instrumentation tests for {@link ExternalNavigationHandler}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags
        .Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
        @Features.DisableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT,
                ChromeFeatureList.AUTOFILL_ASSISTANT_CHROME_ENTRY})
        public class ExternalNavigationDelegateImplTest {
    private static final String AUTOFILL_ASSISTANT_INTENT_URL =
            "intent://www.example.com#Intent;scheme=https;"
            + "B.org.chromium.chrome.browser.autofill_assistant.ENABLED=true;"
            + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
            + Uri.encode("https://www.example.com") + ";end";
    private static final String[] SUPERVISOR_START_ACTIONS = {
            "com.google.android.instantapps.START", "com.google.android.instantapps.nmr1.INSTALL",
            "com.google.android.instantapps.nmr1.VIEW"};

    class ExternalNavigationDelegateImplForTesting extends ExternalNavigationDelegateImpl {
        public ExternalNavigationDelegateImplForTesting() {
            super(mActivityTestRule.getActivity().getActivityTab());
        }

        @Override
        public boolean isGoogleReferrer() {
            return mIsGoogleReferrer;
        }

        public void setIsGoogleReferrer(boolean value) {
            mIsGoogleReferrer = value;
        }

        @Override
        protected void startAutofillAssistantWithIntent(
                Intent targetIntent, String browserFallbackUrl) {
            mWasAutofillAssistantStarted = true;
        }

        public boolean wasAutofillAssistantStarted() {
            return mWasAutofillAssistantStarted;
        }

        // Convenience for testing that reduces boilerplate in constructing arguments to the
        // production method that are common across tests.
        public boolean handleWithAutofillAssistant(ExternalNavigationParams params) {
            Intent intent;
            try {
                intent = Intent.parseUri(AUTOFILL_ASSISTANT_INTENT_URL, Intent.URI_INTENT_SCHEME);
            } catch (Exception ex) {
                Assert.assertTrue(false);
                return false;
            }

            String fallbackUrl = "https://www.example.com";

            return handleWithAutofillAssistant(params, intent, fallbackUrl);
        }

        private boolean mIsGoogleReferrer;
        private boolean mWasAutofillAssistantStarted;
    }

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static List<ResolveInfo> makeResolveInfos(ResolveInfo... infos) {
        return Arrays.asList(infos);
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializedHandler_NoResolveInfo() {
        String packageName = "";
        List<ResolveInfo> resolveInfos = new ArrayList<ResolveInfo>();
        Assert.assertEquals(0,
                ExternalNavigationDelegateImpl
                        .getSpecializedHandlersWithFilter(resolveInfos, packageName)
                        .size());
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializedHandler_NoPathOrAuthority() {
        String packageName = "";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        Assert.assertEquals(0,
                ExternalNavigationDelegateImpl
                        .getSpecializedHandlersWithFilter(resolveInfos, packageName)
                        .size());
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializedHandler_WithPath() {
        String packageName = "";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        info.filter.addDataPath("somepath", 2);
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        Assert.assertEquals(1,
                ExternalNavigationDelegateImpl
                        .getSpecializedHandlersWithFilter(resolveInfos, packageName)
                        .size());
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializedHandler_WithAuthority() {
        String packageName = "";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        info.filter.addDataAuthority("http://www.google.com", "80");
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        Assert.assertEquals(1,
                ExternalNavigationDelegateImpl
                        .getSpecializedHandlersWithFilter(resolveInfos, packageName)
                        .size());
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializedHandler_WithAuthority_Wildcard_Host() {
        String packageName = "";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        info.filter.addDataAuthority("*", null);
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        Assert.assertEquals(0,
                ExternalNavigationDelegateImpl
                        .getSpecializedHandlersWithFilter(resolveInfos, packageName)
                        .size());

        ResolveInfo infoWildcardSubDomain = new ResolveInfo();
        infoWildcardSubDomain.filter = new IntentFilter();
        infoWildcardSubDomain.filter.addDataAuthority("http://*.google.com", "80");
        List<ResolveInfo> resolveInfosWildcardSubDomain = makeResolveInfos(infoWildcardSubDomain);
        Assert.assertEquals(1,
                ExternalNavigationDelegateImpl
                        .getSpecializedHandlersWithFilter(
                                resolveInfosWildcardSubDomain, packageName)
                        .size());
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializedHandler_WithTargetPackage_Matching() {
        String packageName = "com.android.chrome";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        info.filter.addDataAuthority("http://www.google.com", "80");
        info.activityInfo = new ActivityInfo();
        info.activityInfo.packageName = packageName;
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        Assert.assertEquals(1,
                ExternalNavigationDelegateImpl
                        .getSpecializedHandlersWithFilter(resolveInfos, packageName)
                        .size());
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializedHandler_WithTargetPackage_NotMatching() {
        String packageName = "com.android.chrome";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        info.filter.addDataAuthority("http://www.google.com", "80");
        info.activityInfo = new ActivityInfo();
        info.activityInfo.packageName = "com.foo.bar";
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        Assert.assertEquals(0,
                ExternalNavigationDelegateImpl
                        .getSpecializedHandlersWithFilter(resolveInfos, packageName)
                        .size());
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializeHandler_withEphemeralResolver() {
        String packageName = "";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        info.filter.addDataPath("somepath", 2);
        info.activityInfo = new ActivityInfo();

        // See InstantAppsHandler.isInstantAppResolveInfo
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            info.isInstantAppAvailable = true;
        } else {
            info.activityInfo.name = InstantAppsHandler.EPHEMERAL_INSTALLER_CLASS;
        }
        info.activityInfo.packageName = "com.google.android.gms";
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        // Ephemeral resolver is not counted as a specialized handler.
        Assert.assertEquals(0,
                ExternalNavigationDelegateImpl
                        .getSpecializedHandlersWithFilter(resolveInfos, packageName)
                        .size());
    }

    @Test
    @SmallTest
    public void testIsDownload_noSystemDownloadManager() {
        ExternalNavigationDelegateImpl delegate = new ExternalNavigationDelegateImpl(
                mActivityTestRule.getActivity().getActivityTab());
        Assert.assertTrue("pdf should be a download, no viewer in Android Chrome",
                delegate.isPdfDownload("http://somesampeleurldne.com/file.pdf"));
        Assert.assertFalse("URL is not a file, but web page",
                delegate.isPdfDownload("http://somesampleurldne.com/index.html"));
        Assert.assertFalse("URL is not a file url",
                delegate.isPdfDownload("http://somesampeleurldne.com/not.a.real.extension"));
        Assert.assertFalse("URL is an image, can be viewed in Chrome",
                delegate.isPdfDownload("http://somesampleurldne.com/image.jpg"));
        Assert.assertFalse("URL is a text file can be viewed in Chrome",
                delegate.isPdfDownload("http://somesampleurldne.com/copy.txt"));
    }

    @Test
    @SmallTest
    public void testMaybeSetPendingIncognitoUrl() {
        ExternalNavigationDelegateImpl delegate = new ExternalNavigationDelegateImpl(
                mActivityTestRule.getActivity().getActivityTab());

        String url = "http://www.example.com";
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));

        delegate.maybeSetPendingIncognitoUrl(intent);

        Assert.assertTrue(
                intent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false));
        Assert.assertEquals(url, IntentHandler.getPendingIncognitoUrl());
    }

    @Test
    @SmallTest
    public void testIsIntentToInstantApp() {
        ExternalNavigationDelegateImpl delegate = new ExternalNavigationDelegateImpl(
                mActivityTestRule.getActivity().getActivityTab());

        // Check that the delegate correctly distinguishes instant app intents from others.
        String vanillaUrl = "http://www.example.com";
        Intent vanillaIntent = new Intent(Intent.ACTION_VIEW);
        vanillaIntent.setData(Uri.parse(vanillaUrl));

        String instantAppIntentUrl = "intent://buzzfeed.com/tasty#Intent;scheme=http;"
                + "package=com.google.android.instantapps.supervisor;"
                + "action=com.google.android.instantapps.START;"
                + "S.com.google.android.instantapps.FALLBACK_PACKAGE="
                + "com.android.chrome;S.com.google.android.instantapps.INSTANT_APP_PACKAGE="
                + "com.yelp.android;S.android.intent.extra.REFERRER_NAME="
                + "https%3A%2F%2Fwww.google.com;end";
        Intent instantAppIntent;
        try {
            instantAppIntent = Intent.parseUri(instantAppIntentUrl, Intent.URI_INTENT_SCHEME);
        } catch (Exception ex) {
            Assert.assertTrue(false);
            return;
        }

        Assert.assertFalse(delegate.isIntentToInstantApp(vanillaIntent));
        Assert.assertTrue(delegate.isIntentToInstantApp(instantAppIntent));

        // Check that Supervisor is detected by action even without package.
        for (String action : SUPERVISOR_START_ACTIONS) {
            String intentWithoutPackageUrl = "intent://buzzfeed.com/tasty#Intent;scheme=http;"
                    + "action=" + action + ";"
                    + "S.com.google.android.instantapps.FALLBACK_PACKAGE="
                    + "com.android.chrome;S.com.google.android.instantapps.INSTANT_APP_PACKAGE="
                    + "com.yelp.android;S.android.intent.extra.REFERRER_NAME="
                    + "https%3A%2F%2Fwww.google.com;end";
            try {
                instantAppIntent =
                        Intent.parseUri(intentWithoutPackageUrl, Intent.URI_INTENT_SCHEME);
            } catch (Exception ex) {
                Assert.assertTrue(false);
                return;
            }
            Assert.assertTrue(delegate.isIntentToInstantApp(instantAppIntent));
        }
    }

    @Test
    @SmallTest
    public void testMaybeAdjustInstantAppExtras() {
        ExternalNavigationDelegateImpl delegate = new ExternalNavigationDelegateImpl(
                mActivityTestRule.getActivity().getActivityTab());

        String url = "http://www.example.com";
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));

        delegate.maybeAdjustInstantAppExtras(intent, /*isIntentToInstantApp=*/true);
        Assert.assertTrue(intent.hasExtra(InstantAppsHandler.IS_GOOGLE_SEARCH_REFERRER));

        delegate.maybeAdjustInstantAppExtras(intent, /*isIntentToInstantApp=*/false);
        Assert.assertFalse(intent.hasExtra(InstantAppsHandler.IS_GOOGLE_SEARCH_REFERRER));
    }

    @Test
    @SmallTest
    public void testMaybeSetUserGesture() {
        ExternalNavigationDelegateImpl delegate = new ExternalNavigationDelegateImpl(
                mActivityTestRule.getActivity().getActivityTab());

        String url = "http://www.example.com";
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));

        delegate.maybeSetUserGesture(intent);
        Assert.assertTrue(IntentWithGesturesHandler.getInstance().getUserGestureAndClear(intent));
    }

    @Test
    @SmallTest
    public void testMaybeSetPendingReferrer() {
        ExternalNavigationDelegateImpl delegate = new ExternalNavigationDelegateImpl(
                mActivityTestRule.getActivity().getActivityTab());

        String url = "http://www.example.com";
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));

        String referrerUrl = "http://www.example-referrer.com";
        delegate.maybeSetPendingReferrer(intent, referrerUrl);

        Assert.assertEquals(
                Uri.parse(referrerUrl), intent.getParcelableExtra(Intent.EXTRA_REFERRER));
        Assert.assertEquals(1, intent.getIntExtra(IntentHandler.EXTRA_REFERRER_ID, 0));
        Assert.assertEquals(referrerUrl, IntentHandler.getPendingReferrerUrl(1));
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT,
            ChromeFeatureList.AUTOFILL_ASSISTANT_CHROME_ENTRY})
    public void
    testHandleWithAutofillAssistant_TriggersFromSearch() {
        ExternalNavigationDelegateImplForTesting delegate =
                new ExternalNavigationDelegateImplForTesting();
        delegate.setIsGoogleReferrer(true);

        ExternalNavigationParams params =
                new ExternalNavigationParams
                        .Builder(AUTOFILL_ASSISTANT_INTENT_URL, /*isIncognito=*/false)
                        .build();

        Assert.assertTrue(delegate.handleWithAutofillAssistant(params));
        Assert.assertTrue(delegate.wasAutofillAssistantStarted());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT,
            ChromeFeatureList.AUTOFILL_ASSISTANT_CHROME_ENTRY})
    public void
    testHandleWithAutofillAssistant_DoesNotTriggerFromSearchInIncognito() {
        ExternalNavigationDelegateImplForTesting delegate =
                new ExternalNavigationDelegateImplForTesting();
        delegate.setIsGoogleReferrer(true);

        ExternalNavigationParams params =
                new ExternalNavigationParams
                        .Builder(AUTOFILL_ASSISTANT_INTENT_URL, /*isIncognito=*/true)
                        .build();

        Assert.assertFalse(delegate.handleWithAutofillAssistant(params));
        Assert.assertFalse(delegate.wasAutofillAssistantStarted());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT,
            ChromeFeatureList.AUTOFILL_ASSISTANT_CHROME_ENTRY})
    public void
    testHandleWithAutofillAssistant_DoesNotTriggerFromDifferentOrigin() {
        ExternalNavigationDelegateImplForTesting delegate =
                new ExternalNavigationDelegateImplForTesting();
        delegate.setIsGoogleReferrer(false);

        ExternalNavigationParams params =
                new ExternalNavigationParams
                        .Builder(AUTOFILL_ASSISTANT_INTENT_URL, /*isIncognito=*/false)
                        .build();

        Assert.assertFalse(delegate.handleWithAutofillAssistant(params));
        Assert.assertFalse(delegate.wasAutofillAssistantStarted());
    }

    @Test
    @SmallTest
    @Features.DisableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT,
            ChromeFeatureList.AUTOFILL_ASSISTANT_CHROME_ENTRY})
    public void
    testHandleWithAutofillAssistant_DoesNotTriggerWhenFeatureDisabled() {
        ExternalNavigationDelegateImplForTesting delegate =
                new ExternalNavigationDelegateImplForTesting();
        delegate.setIsGoogleReferrer(true);

        ExternalNavigationParams params =
                new ExternalNavigationParams
                        .Builder(AUTOFILL_ASSISTANT_INTENT_URL, /*isIncognito=*/false)
                        .build();

        Assert.assertFalse(delegate.handleWithAutofillAssistant(params));
        Assert.assertFalse(delegate.wasAutofillAssistantStarted());
    }
}
