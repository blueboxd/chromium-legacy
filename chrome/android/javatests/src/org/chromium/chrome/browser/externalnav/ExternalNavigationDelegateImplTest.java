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
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;

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

    class ExternalNavigationDelegateImplForTesting extends ExternalNavigationDelegateImpl {
        public ExternalNavigationDelegateImplForTesting() {
            super(mActivityTestRule.getActivity().getActivityTab());
        }

        @Override
        public boolean isSerpReferrer() {
            return mIsSerpReferrer;
        }

        public void setIsSerpReferrer(boolean value) {
            mIsSerpReferrer = value;
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

        private boolean mIsSerpReferrer;
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
        delegate.setIsSerpReferrer(true);

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
        delegate.setIsSerpReferrer(true);

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
        delegate.setIsSerpReferrer(false);

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
        delegate.setIsSerpReferrer(true);

        ExternalNavigationParams params =
                new ExternalNavigationParams
                        .Builder(AUTOFILL_ASSISTANT_INTENT_URL, /*isIncognito=*/false)
                        .build();

        Assert.assertFalse(delegate.handleWithAutofillAssistant(params));
        Assert.assertFalse(delegate.wasAutofillAssistantStarted());
    }
}
