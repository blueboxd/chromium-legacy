// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.payments.PaymentManifestDownloader;
import org.chromium.components.payments.PaymentManifestParser;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.url.Origin;
import org.chromium.url.URI;

import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** An integration test for the Android payment app finder. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@MediumTest
public class AndroidPaymentAppFinderTest
        implements PaymentAppFactoryDelegate, PaymentAppFactoryParams {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    /** Simulates a package manager in memory. */
    private final MockPackageManagerDelegate mPackageManager = new MockPackageManagerDelegate();

    /** Downloads from the test server. */
    private static class TestServerDownloader extends PaymentManifestDownloader {
        private URI mTestServerUri;

        /**
         * @param uri The URI of the test server.
         */
        /* package */ void setTestServerUri(URI uri) {
            assert mTestServerUri == null : "Test server URI should be set only once";
            mTestServerUri = uri;
        }

        @Override
        public void downloadPaymentMethodManifest(
                Origin merchantOrigin, URI methodName, ManifestDownloadCallback callback) {
            super.downloadPaymentMethodManifest(
                    merchantOrigin, substituteTestServerUri(methodName), callback);
        }

        @Override
        public void downloadWebAppManifest(Origin paymentMethodManifestOrigin,
                URI webAppManifestUri, ManifestDownloadCallback callback) {
            super.downloadWebAppManifest(paymentMethodManifestOrigin,
                    substituteTestServerUri(webAppManifestUri), callback);
        }

        private URI substituteTestServerUri(URI uri) {
            try {
                return new URI(uri.toString().replaceAll("https://", mTestServerUri.toString()));
            } catch (URISyntaxException e) {
                assert false;
                return null;
            }
        }
    }

    private final TestServerDownloader mDownloader = new TestServerDownloader();

    private EmbeddedTestServer mServer;
    private List<PaymentApp> mPaymentApps;
    private boolean mAllPaymentAppsCreated;
    private Map<String, PaymentMethodData> mMethodData;

    // PaymentAppFactoryDelegate implementation.
    @Override
    public PaymentAppFactoryParams getParams() {
        return this;
    }

    // PaymentAppFactoryDelegate implementation.
    @Override
    public void onPaymentAppCreated(PaymentApp paymentApp) {
        mPaymentApps.add(paymentApp);
    }

    // PaymentAppFactoryDelegate implementation.
    @Override
    public void onPaymentAppCreationError(String errorMessage) {}

    // PaymentAppFactoryDelegate implementation.
    @Override
    public void onDoneCreatingPaymentApps(PaymentAppFactoryInterface unusedFactory) {
        mAllPaymentAppsCreated = true;
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public WebContents getWebContents() {
        return mRule.getActivity().getCurrentWebContents();
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public RenderFrameHost getRenderFrameHost() {
        return getWebContents().getMainFrame();
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public Map<String, PaymentDetailsModifier> getModifiers() {
        return Collections.unmodifiableMap(new HashMap<String, PaymentDetailsModifier>());
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public Origin getPaymentRequestSecurityOrigin() {
        return PaymentManifestDownloader.createOpaqueOriginForTest();
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public String getTopLevelOrigin() {
        return "https://top.level.origin";
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public String getPaymentRequestOrigin() {
        return "https://payment.request.origin";
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public Map<String, PaymentMethodData> getMethodData() {
        return mMethodData;
    }

    @Before
    public void setUp() throws Throwable {
        mRule.startMainActivityOnBlankPage();
        mPackageManager.reset();
        mServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mDownloader.setTestServerUri(new URI(mServer.getURL("/components/test/data/payments/")));
        mPaymentApps = new ArrayList<>();
        mAllPaymentAppsCreated = false;
    }

    @After
    public void tearDown() {
        if (mServer != null) mServer.stopAndDestroyServer();
    }

    /** Absence of installed apps should result in no payment apps. */
    @Test
    @Feature({"Payments"})
    public void testNoApps() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("basic-card");
        methods.add("https://alicepay.com/webpay");
        methods.add("https://bobpay.com/webpay");
        methods.add("https://charliepay.com/webpay");
        methods.add("https://davepay.com/webpay");

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());
    }

    /** Payment apps without metadata should be filtered out. */
    @Test
    @Feature({"Payments"})
    public void testNoMetadata() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("basic-card");
        mPackageManager.installPaymentApp(
                "BobPay", "com.bobpay", null /* no metadata */, /*signature=*/"01");

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertTrue("No apps should still match the query", mPaymentApps.isEmpty());
    }

    /**
     * Payment apps without default payment method name in metadata should still be able to use
     * non-URL payment method names.
     */
    @Test
    @Feature({"Payments"})
    public void testNoDefaultPaymentMethodNameWithNonUrlPaymentMethodName() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("basic-card");
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay",
                "" /* no default payment method name in metadata */, /*signature=*/"AA");
        mPackageManager.setStringArrayMetaData("com.alicepay", new String[] {"basic-card"});

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.alicepay", mPaymentApps.get(0).getIdentifier());
        Assert.assertEquals(1, mPaymentApps.get(0).getInstrumentMethodNames().size());
        Assert.assertEquals(
                "basic-card", mPaymentApps.get(0).getInstrumentMethodNames().iterator().next());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("1 app should still match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.alicepay", mPaymentApps.get(0).getIdentifier());
        Assert.assertEquals(1, mPaymentApps.get(0).getInstrumentMethodNames().size());
        Assert.assertEquals(
                "basic-card", mPaymentApps.get(0).getInstrumentMethodNames().iterator().next());
    }

    /**
     * Payment apps without default payment method name in metadata should still be able to use
     * URL payment method names that support all origins.
     */
    @Test
    @Feature({"Payments"})
    public void testNoDefaultPaymentMethodNameWithUrlPaymentMethodNameThatSupportsAllOrigins()
            throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://frankpay.com/webpay");
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay",
                "" /* no default payment method name in metadata */, /*signature=*/"AA");
        mPackageManager.setStringArrayMetaData(
                "com.alicepay", new String[] {"https://frankpay.com/webpay"});

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.alicepay", mPaymentApps.get(0).getIdentifier());
        Assert.assertEquals(1, mPaymentApps.get(0).getInstrumentMethodNames().size());
        Assert.assertEquals("https://frankpay.com/webpay",
                mPaymentApps.get(0).getInstrumentMethodNames().iterator().next());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("1 app should still match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.alicepay", mPaymentApps.get(0).getIdentifier());
        Assert.assertEquals(1, mPaymentApps.get(0).getInstrumentMethodNames().size());
        Assert.assertEquals("https://frankpay.com/webpay",
                mPaymentApps.get(0).getInstrumentMethodNames().iterator().next());
    }

    /** Payment apps without a human-readable name should be filtered out. */
    @Test
    @Feature({"Payments"})
    public void testEmptyLabel() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("basic-card");
        mPackageManager.installPaymentApp("" /* empty label */, "com.bobpay", "basic-card",
                /*signature=*/"01020304050607080900");

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());
    }

    /** Invalid and relative URIs cannot be used as payment method names. */
    @Test
    @Feature({"Payments"})
    public void testInvalidPaymentMethodNames() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://"); // Invalid URI.
        methods.add("../index.html"); // Relative URI.
        mPackageManager.installPaymentApp(
                "BobPay", "com.bobpay", "https://", /*signature=*/"01020304050607080900");
        mPackageManager.installPaymentApp(
                "AlicePay", "com.alicepay", "../index.html", /*signature=*/"ABCDEFABCDEFABCDEFAB");

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());
    }

    /** Non-URL payment method names are hard-coded to those defined in W3C. */
    @Test
    @Feature({"Payments"})
    public void testTwoAppsWithIncorrectMethodNames() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("basic-card");
        methods.add("incorrect-method-name"); // Even if merchant supports it, Chrome filters out
        // unknown non-URL method names.
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "incorrect-method-name",
                /*signature=*/"01020304050607080900");
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay", "incorrect-method-name",
                /*signature=*/"ABCDEFABCDEFABCDEFAB");

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());
    }

    /**
     * Test "basic-card" payment method with a payment app that supports IS_READY_TO_PAY service.
     * Another non-payment app also supports IS_READY_TO_PAY service, but it should be filtered
     * out, because it's not a payment app.
     */
    @Test
    @Feature({"Payments"})
    public void testOneBasicCardAppWithAFewIsReadyToPayServices() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("basic-card");
        mPackageManager.installPaymentApp(
                "BobPay", "com.bobpay", "basic-card", /*signature=*/"01020304050607080900");
        mPackageManager.addIsReadyToPayService("com.bobpay");
        mPackageManager.addIsReadyToPayService("com.alicepay");

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.bobpay", mPaymentApps.get(0).getIdentifier());
    }

    /**
     * Test BobPay with https://bobpay.com/webpay payment method name, which the payment app
     * supports through the "default_applications" directive in the
     * https://bobpay.com/payment-manifest.json file. BobPay has the correct signature that
     * matches the fingerprint in https://bobpay.com/app.json.
     */
    @Test
    @Feature({"Payments"})
    public void testOneUrlMethodNameApp() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://bobpay.com/webpay");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.bobpay", mPaymentApps.get(0).getIdentifier());
    }

    /** Ignored payment methods should be filtered out. */
    @Test
    @Feature({"Payments"})
    public void testIgnoredPaymentMethodIdentifier() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://bobpay.com/webpay");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");

        ignorePaymentMethodIdentifierAndFindApps(
                /*ignoredPaymentMethodIdentifier=*/"https://bobpay.com/webpay", methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());
    }

    /**
     * Test BobPay with an incorrect signature and https://bobpay.com/webpay payment method name.
     */
    @Test
    @Feature({"Payments"})
    public void testOneUrlMethodNameAppWithWrongSignature() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://bobpay.com/webpay");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                "AA" /* incorrect signature */);

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());
    }

    /** A payment app whose package info cannot be retrieved should be filtered out. */
    @Test
    @Feature({"Payments"})
    public void testOneAppWithoutPackageInfo() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://bobpay.com/webpay");
        mPackageManager.installPaymentApp(
                "BobPay", "com.bobpay", "https://bobpay.com/webpay", null /* no package info*/);

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());
    }

    /** Unsigned payment app should be filtered out. */
    @Test
    @Feature({"Payments"})
    public void testOneAppWithoutSignatures() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://bobpay.com/webpay");
        mPackageManager.installPaymentApp(
                "BobPay", "com.bobpay", "https://bobpay.com/webpay", "" /* no signatures */);

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());
    }

    /**
     * If two payment apps both support "basic-card" payment method name, then they both should be
     * found.
     */
    @Test
    @Feature({"Payments"})
    public void testTwoBasicCardApps() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("basic-card");
        mPackageManager.installPaymentApp(
                "BobPay", "com.bobpay", "basic-card", /*signature=*/"01020304050607080900");
        mPackageManager.installPaymentApp(
                "AlicePay", "com.alicepay", "basic-card", /*signature=*/"ABCDEFABCDEFABCDEFAB");

        findApps(methods);

        Assert.assertEquals("2 apps should match the query", 2, mPaymentApps.size());
        Set<String> appIdentifiers = new HashSet<>();
        appIdentifiers.add(mPaymentApps.get(0).getIdentifier());
        appIdentifiers.add(mPaymentApps.get(1).getIdentifier());
        Assert.assertTrue(appIdentifiers.contains("com.bobpay"));
        Assert.assertTrue(appIdentifiers.contains("com.alicepay"));
    }

    /**
     * Test https://davepay.com/webpay payment method, the "default_applications" of which
     * supports two different package names: one for production and one for development version
     * of the payment app. Both of these apps should be found. Repeated lookups should continue
     * finding the two apps.
     */
    @Test
    @Feature({"Payments"})
    public void testTwoUrlMethodNameAppsWithSameMethodName() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://davepay.com/webpay");
        mPackageManager.installPaymentApp("DavePay", "com.davepay.prod",
                "https://davepay.com/webpay", /*signature=*/"44444444442222222222");
        mPackageManager.installPaymentApp("DavePay Dev", "com.davepay.dev",
                "https://davepay.com/webpay", /*signature=*/"44444444441111111111");

        findApps(methods);

        Assert.assertEquals("2 apps should match the query", 2, mPaymentApps.size());
        Set<String> appIdentifiers = new HashSet<>();
        appIdentifiers.add(mPaymentApps.get(0).getIdentifier());
        appIdentifiers.add(mPaymentApps.get(1).getIdentifier());
        Assert.assertTrue(appIdentifiers.contains("com.davepay.prod"));
        Assert.assertTrue(appIdentifiers.contains("com.davepay.dev"));

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("2 apps should match the query again", 2, mPaymentApps.size());
        appIdentifiers.clear();
        appIdentifiers.add(mPaymentApps.get(0).getIdentifier());
        appIdentifiers.add(mPaymentApps.get(1).getIdentifier());
        Assert.assertTrue(appIdentifiers.contains("com.davepay.prod"));
        Assert.assertTrue(appIdentifiers.contains("com.davepay.dev"));
    }

    /**
     * If the merchant supports https://bobpay.com/webpay and https://alicepay.com/webpay payment
     * method names and the user has an app for each of those, then both apps should be found.
     * Repeated lookups should succeed.
     */
    @Test
    @Feature({"Payments"})
    public void testTwoUrlMethodNameAppsWithDifferentMethodNames() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://bobpay.com/webpay");
        methods.add("https://alicepay.com/webpay");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay", "https://alicepay.com/webpay",
                /*signature=*/"ABCDEFABCDEFABCDEFAB");

        findApps(methods);

        Assert.assertEquals("2 apps should match the query", 2, mPaymentApps.size());
        Set<String> appIdentifiers = new HashSet<>();
        appIdentifiers.add(mPaymentApps.get(0).getIdentifier());
        appIdentifiers.add(mPaymentApps.get(1).getIdentifier());
        Assert.assertTrue(appIdentifiers.contains("com.bobpay"));
        Assert.assertTrue(appIdentifiers.contains("com.alicepay"));

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("2 apps should match the query again", 2, mPaymentApps.size());
        appIdentifiers.clear();
        appIdentifiers.add(mPaymentApps.get(0).getIdentifier());
        appIdentifiers.add(mPaymentApps.get(1).getIdentifier());
        Assert.assertTrue(appIdentifiers.contains("com.bobpay"));
        Assert.assertTrue(appIdentifiers.contains("com.alicepay"));
    }

    /**
     * If the merchant supports a couple of payment methods, one of which does not have a valid
     * manifest, then all apps that support the invalid manifest should be filtered out. Repeated
     * calls should continue finding only the payment app for the valid manifest.
     */
    @Test
    @Feature({"Payments"})
    public void testOneValidManifestAndOneInvalidManifestWithPaymentAppsForBoth() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://bobpay.com/webpay");
        methods.add("https://not-valid.com/webpay");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.installPaymentApp("NotValid", "com.not-valid",
                "https://not-valid.com/webpay", /*signature=*/"ABCDEFABCDEFABCDEFAB");

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.bobpay", mPaymentApps.get(0).getIdentifier());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("1 app should match the query again", 1, mPaymentApps.size());
        Assert.assertEquals("com.bobpay", mPaymentApps.get(0).getIdentifier());
    }

    /**
     * Repeated lookups of payment apps for URL method names should continue finding the same
     * payment apps.
     */
    @Test
    @Feature({"Payments"})
    public void testTwoUrlMethodNameAppsTwice() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://bobpay.com/webpay");
        methods.add("https://alicepay.com/webpay");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay", "https://alicepay.com/webpay",
                /*signature=*/"ABCDEFABCDEFABCDEFAB");

        findApps(methods);

        Assert.assertEquals("2 apps should match the query", 2, mPaymentApps.size());
        Set<String> appIdentifiers = new HashSet<>();
        appIdentifiers.add(mPaymentApps.get(0).getIdentifier());
        appIdentifiers.add(mPaymentApps.get(1).getIdentifier());
        Assert.assertTrue(appIdentifiers.contains("com.bobpay"));
        Assert.assertTrue(appIdentifiers.contains("com.alicepay"));

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("2 apps should still match the query", 2, mPaymentApps.size());
        appIdentifiers.clear();
        appIdentifiers.add(mPaymentApps.get(0).getIdentifier());
        appIdentifiers.add(mPaymentApps.get(1).getIdentifier());
        Assert.assertTrue(appIdentifiers.contains("com.bobpay"));
        Assert.assertTrue(appIdentifiers.contains("com.alicepay"));
    }

    /**
     * Test CharliePay Dev with https://charliepay.com/webpay payment method, which supports both
     * dev and prod versions of the app through multiple web app manifests. Repeated app look ups
     * should be successful.
     */
    @Test
    @Feature({"Payments"})
    public void testCharliePayDev() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://charliepay.com/webpay");
        mPackageManager.installPaymentApp("CharliePay", "com.charliepay.dev",
                "https://charliepay.com/webpay", /*signature=*/"33333333333111111111");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay", "https://alicepay.com/webpay",
                /*signature=*/"ABCDEFABCDEFABCDEFAB");

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.charliepay.dev", mPaymentApps.get(0).getIdentifier());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("1 app should match the query again", 1, mPaymentApps.size());
        Assert.assertEquals("com.charliepay.dev", mPaymentApps.get(0).getIdentifier());
    }

    /**
     * Test DavePay Dev with https://davepay.com/webpay payment method, which supports both
     * dev and prod versions of the app through multiple sections of "related_applications" entry
     * in the same web app manifest. Repeated app look ups should be successful.
     */
    @Test
    @Feature({"Payments"})
    public void testDavePayDev() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://davepay.com/webpay");
        mPackageManager.installPaymentApp("DavePay", "com.davepay.dev",
                "https://davepay.com/webpay", /*signature=*/"44444444441111111111");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay", "https://alicepay.com/webpay",
                /*signature=*/"ABCDEFABCDEFABCDEFAB");

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.davepay.dev", mPaymentApps.get(0).getIdentifier());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("1 app should match the query again", 1, mPaymentApps.size());
        Assert.assertEquals("com.davepay.dev", mPaymentApps.get(0).getIdentifier());
    }

    /**
     * Test a valid installation of EvePay with 55555555551111111111 signature and
     * https://evepay.com/webpay payment method, which supports a couple of different signatures
     * (with the same package name) through different web app manifests. Repeated app look ups
     * should be successful.
     */
    @Test
    @Feature({"Payments"})
    public void testValidEvePay1() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://evepay.com/webpay");
        mPackageManager.installPaymentApp("EvePay", "com.evepay", "https://evepay.com/webpay",
                /*signature=*/"55555555551111111111");

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.evepay", mPaymentApps.get(0).getIdentifier());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("1 app should match the query again", 1, mPaymentApps.size());
        Assert.assertEquals("com.evepay", mPaymentApps.get(0).getIdentifier());
    }

    /**
     * Test a valid installation of EvePay with 55555555552222222222 signature and
     * https://evepay.com/webpay payment method, which supports a couple of different signatures
     * (with the same package name) through different web app manifests. Repeated app look ups
     * should be successful.
     */
    @Test
    @Feature({"Payments"})
    public void testValidEvePay2() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://evepay.com/webpay");
        mPackageManager.installPaymentApp("EvePay", "com.evepay", "https://evepay.com/webpay",
                /*signature=*/"55555555552222222222");

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.evepay", mPaymentApps.get(0).getIdentifier());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("1 app should match the query again", 1, mPaymentApps.size());
        Assert.assertEquals("com.evepay", mPaymentApps.get(0).getIdentifier());
    }

    /**
     * Test an invalid installation of EvePay with https://evepay.com/webpay payment method, which
     * supports several different signatures (with the same package name) through different web app
     * manifests. Repeated app look ups should find no payment apps.
     */
    @Test
    @Feature({"Payments"})
    public void testInvalidEvePay() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://evepay.com/webpay");
        mPackageManager.installPaymentApp(
                "EvePay", "com.evepay", "https://evepay.com/webpay", /*signature=*/"55");

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());

        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertTrue("No apps should match the query again", mPaymentApps.isEmpty());
    }

    /**
     * Test https://frankpay.com/webpay payment method, which supports all apps without signature
     * verification because https://frankpay.com/payment-manifest.json contains
     * {"supported_origins": "*"}. Repeated app look ups should be successful.
     */
    @Test
    @Feature({"Payments"})
    public void testFrankPaySupportsPaymentAppsFromAllOrigins() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://frankpay.com/webpay");
        mPackageManager.installPaymentApp(
                "AlicePay", "com.alicepay", "https://alicepay.com/webpay", /*signature=*/"00");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "basic-card", /*signature=*/"11");
        mPackageManager.installPaymentApp(
                "AlicePay", "com.charliepay", "invalid-payment-method-name", /*signature=*/"22");
        mPackageManager.setStringArrayMetaData(
                "com.alicepay", new String[] {"https://frankpay.com/webpay"});
        mPackageManager.setStringArrayMetaData(
                "com.bobpay", new String[] {"https://frankpay.com/webpay"});
        mPackageManager.setStringArrayMetaData(
                "com.charliepay", new String[] {"https://frankpay.com/webpay"});

        findApps(methods);

        Assert.assertEquals("3 apps should match the query", 3, mPaymentApps.size());
        Set<String> appIdentifiers = new HashSet<>();
        appIdentifiers.add(mPaymentApps.get(0).getIdentifier());
        appIdentifiers.add(mPaymentApps.get(1).getIdentifier());
        appIdentifiers.add(mPaymentApps.get(2).getIdentifier());
        Assert.assertTrue(appIdentifiers.contains("com.alicepay"));
        Assert.assertTrue(appIdentifiers.contains("com.bobpay"));
        Assert.assertTrue(appIdentifiers.contains("com.charliepay"));

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("3 apps should still match the query", 3, mPaymentApps.size());
        appIdentifiers.clear();
        appIdentifiers.add(mPaymentApps.get(0).getIdentifier());
        appIdentifiers.add(mPaymentApps.get(1).getIdentifier());
        appIdentifiers.add(mPaymentApps.get(2).getIdentifier());
        Assert.assertTrue(appIdentifiers.contains("com.alicepay"));
        Assert.assertTrue(appIdentifiers.contains("com.bobpay"));
        Assert.assertTrue(appIdentifiers.contains("com.charliepay"));
    }

    /**
     * Verify that a payment app with an incorrect signature can support only
     * {"supported_origins": "*"} payment methods, e.g., https://frankpay.com/webpay. Repeated app
     * look ups should be successful.
     */
    @Test
    @Feature({"Payments"})
    public void testMatchOnlyValidPaymentMethods() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://frankpay.com/webpay");
        methods.add("https://bobpay.com/webpay");
        methods.add("invalid-payment-method-name");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                "00" /* Invalid signature for https://bobpay.com/webpay. */);
        mPackageManager.setStringArrayMetaData("com.bobpay",
                new String[] {"invalid-payment-method-name", "https://frankpay.com/webpay"});

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.bobpay", mPaymentApps.get(0).getIdentifier());
        Assert.assertEquals("1 payment method should be enabled", 1,
                mPaymentApps.get(0).getInstrumentMethodNames().size());
        Assert.assertEquals("https://frankpay.com/webpay",
                mPaymentApps.get(0).getInstrumentMethodNames().iterator().next());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("1 app should still match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.bobpay", mPaymentApps.get(0).getIdentifier());
        Assert.assertEquals("1 payment method should still be enabled", 1,
                mPaymentApps.get(0).getInstrumentMethodNames().size());
        Assert.assertEquals("https://frankpay.com/webpay",
                mPaymentApps.get(0).getInstrumentMethodNames().iterator().next());
    }

    /**
     * Verify that only a valid AlicePay app can use https://georgepay.com/webpay payment method
     * name, because https://georgepay.com/payment-manifest.json contains {"supported_origins":
     * ["https://alicepay.com"]}. Repeated app look ups should be successful.
     */
    @Test
    @Feature({"Payments"})
    public void testGeorgePaySupportsPaymentAppsFromAlicePayOrigin() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://georgepay.com/webpay");
        // Valid AlicePay:
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay", "https://alicepay.com/webpay",
                /*signature=*/"ABCDEFABCDEFABCDEFAB");
        mPackageManager.setStringArrayMetaData(
                "com.alicepay", new String[] {"https://georgepay.com/webpay"});
        // Invalid AlicePay:
        mPackageManager.installPaymentApp("AlicePay", "com.fake-alicepay" /* invalid package name*/,
                "https://alicepay.com/webpay", "00" /* invalid signature */);
        mPackageManager.setStringArrayMetaData(
                "com.fake-alicepay", new String[] {"https://georgepay.com/webpay"});
        // Valid BobPay:
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.setStringArrayMetaData(
                "com.bobpay", new String[] {"https://georgepay.com/webpay"});
        // A "basic-card" app.
        mPackageManager.installPaymentApp("CharliePay", "com.charliepay.dev", "basic-card",
                /*signature=*/"33333333333111111111");
        mPackageManager.setStringArrayMetaData(
                "com.charliepay.dev", new String[] {"https://georgepay.com/webpay"});

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.alicepay", mPaymentApps.get(0).getIdentifier());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("1 app should still match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.alicepay", mPaymentApps.get(0).getIdentifier());
    }

    /**
     * Verify that AlicePay app with incorrect signature cannot use https://georgepay.com/webpay
     * payment method, which contains {"supported_origins": ["https://alicepay.com"]} in
     * https://georgepay.com/payment-manifest.json file. Repeated app look ups should find no apps.
     */
    @Test
    @Feature({"Payments"})
    public void testInvalidSignatureAlicePayAppCannotUseGeorgePayMethodName() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://georgepay.com/webpay");
        // AlicePay with invalid signature:
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay", "https://alicepay.com/webpay",
                "00" /* invalid signature */);
        mPackageManager.setStringArrayMetaData(
                "com.alicepay", new String[] {"https://georgepay.com/webpay"});

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());

        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertTrue("No apps should match the query again", mPaymentApps.isEmpty());
    }

    /**
     * Verify that BobPay app cannot use https://georgepay.com/webpay payment method, because
     * https://georgepay.com/payment-manifest.json contains
     * {"supported_origins": ["https://alicepay.com"]} and no "https://bobpay.com". BobPay can still
     * use its own payment method name, however. Repeated app look ups should succeed.
     */
    @Test
    @Feature({"Payments"})
    public void testValidBobPayCannotUseGeorgePayMethod() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://bobpay.com/webpay");
        methods.add("https://georgepay.com/webpay");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.setStringArrayMetaData(
                "com.bobpay", new String[] {"https://georgepay.com/webpay"});

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.bobpay", mPaymentApps.get(0).getIdentifier());
        Assert.assertEquals("1 payment method should be enabled", 1,
                mPaymentApps.get(0).getInstrumentMethodNames().size());
        Assert.assertEquals("https://bobpay.com/webpay",
                mPaymentApps.get(0).getInstrumentMethodNames().iterator().next());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("1 app should still match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.bobpay", mPaymentApps.get(0).getIdentifier());
        Assert.assertEquals("1 payment method should still be enabled", 1,
                mPaymentApps.get(0).getInstrumentMethodNames().size());
        Assert.assertEquals("https://bobpay.com/webpay",
                mPaymentApps.get(0).getInstrumentMethodNames().iterator().next());
    }

    /**
     * Verify that HenryPay can use https://henrypay.com/webpay payment method name because it's
     * a default application and BobPay can use it because
     * https://henrypay.com/payment-manifest.json contains "supported_origins": "*". Repeated app
     * look ups should succeed.
     */
    @Test
    @Feature({"Payments"})
    public void testUrlPaymentMethodWithDefaultApplicationAndAllSupportedOrigins()
            throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://henrypay.com/webpay");
        mPackageManager.installPaymentApp("HenryPay", "com.henrypay", "https://henrypay.com/webpay",
                /*signature=*/"55555555551111111111");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.setStringArrayMetaData(
                "com.bobpay", new String[] {"https://henrypay.com/webpay"});

        findApps(methods);

        Assert.assertEquals("2 apps should match the query", 2, mPaymentApps.size());
        Set<String> appIdentifiers = new HashSet<>();
        appIdentifiers.add(mPaymentApps.get(0).getIdentifier());
        appIdentifiers.add(mPaymentApps.get(1).getIdentifier());
        Assert.assertTrue(appIdentifiers.contains("com.henrypay"));
        Assert.assertTrue(appIdentifiers.contains("com.bobpay"));

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("2 apps should still match the query", 2, mPaymentApps.size());
        appIdentifiers.clear();
        appIdentifiers.add(mPaymentApps.get(0).getIdentifier());
        appIdentifiers.add(mPaymentApps.get(1).getIdentifier());
        Assert.assertTrue(appIdentifiers.contains("com.henrypay"));
        Assert.assertTrue(appIdentifiers.contains("com.bobpay"));
    }

    /**
     * Verify that a payment app with a non-URI payment method name can use
     * https://henrypay.com/webpay payment method name because
     * https://henrypay.com/payment-manifest.json contains "supported_origins": "*". Repeated app
     * look ups should succeed.
     */
    @Test
    @Feature({"Payments"})
    public void testNonUriDefaultPaymentMethodAppCanUseMethodThatSupportsAllOrigins()
            throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://henrypay.com/webpay");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "basic-card", /*signature=*/"AA");
        mPackageManager.setStringArrayMetaData(
                "com.bobpay", new String[] {"https://henrypay.com/webpay"});

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.bobpay", mPaymentApps.get(0).getIdentifier());
        Assert.assertEquals("1 payment method should be enabled", 1,
                mPaymentApps.get(0).getInstrumentMethodNames().size());
        Assert.assertEquals("https://henrypay.com/webpay",
                mPaymentApps.get(0).getInstrumentMethodNames().iterator().next());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("1 app should still match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.bobpay", mPaymentApps.get(0).getIdentifier());
        Assert.assertEquals("1 payment method should still be enabled", 1,
                mPaymentApps.get(0).getInstrumentMethodNames().size());
        Assert.assertEquals("https://henrypay.com/webpay",
                mPaymentApps.get(0).getInstrumentMethodNames().iterator().next());
    }

    /**
     * Verify that IkePay can use https://ikepay.com/webpay payment method name because it's
     * a default application and AlicePay can use it because
     * https://ikepay.com/payment-manifest.json contains
     * "supported_origins": ["https://alicepay.com"]. BobPay cannot use this payment method.
     * Repeated app look ups should succeed.
     */
    @Test
    @Feature({"Payments"})
    public void testUrlPaymentMethodWithDefaultApplicationAndOneSupportedOrigin() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://ikepay.com/webpay");
        mPackageManager.installPaymentApp("IkePay", "com.ikepay", "https://ikepay.com/webpay",
                /*signature=*/"66666666661111111111");
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay", "https://alicepay.com/webpay",
                /*signature=*/"ABCDEFABCDEFABCDEFAB");
        mPackageManager.setStringArrayMetaData(
                "com.alicepay", new String[] {"https://ikepay.com/webpay"});
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.setStringArrayMetaData(
                "com.bobpay", new String[] {"https://ikepay.com/webpay"});

        findApps(methods);

        Assert.assertEquals("2 apps should match the query", 2, mPaymentApps.size());
        Set<String> appIdentifiers = new HashSet<>();
        appIdentifiers.add(mPaymentApps.get(0).getIdentifier());
        appIdentifiers.add(mPaymentApps.get(1).getIdentifier());
        Assert.assertTrue(appIdentifiers.contains("com.ikepay"));
        Assert.assertTrue(appIdentifiers.contains("com.alicepay"));

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("2 apps should still match the query", 2, mPaymentApps.size());
        appIdentifiers.clear();
        appIdentifiers.add(mPaymentApps.get(0).getIdentifier());
        appIdentifiers.add(mPaymentApps.get(1).getIdentifier());
        Assert.assertTrue(appIdentifiers.contains("com.ikepay"));
        Assert.assertTrue(appIdentifiers.contains("com.alicepay"));
    }

    /**
     * Verify that a payment app with duplicate default and supported method can use its own
     * default payment method, which supports all origins.
     * Repeated app look ups should succeed.
     */
    @Test
    @Feature({"Payments"})
    public void testDuplicateDefaultAndSupportedMethodAndAllOriginsSupported() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://henrypay.com/webpay");
        mPackageManager.installPaymentApp("HenryPay", "com.henrypay", "https://henrypay.com/webpay",
                /*signature=*/"55555555551111111111");
        mPackageManager.setStringArrayMetaData(
                "com.henrypay", new String[] {"https://henrypay.com/webpay"});

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.henrypay", mPaymentApps.get(0).getIdentifier());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("1 app should still match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.henrypay", mPaymentApps.get(0).getIdentifier());
    }

    /**
     * If a payment method supports two apps from different origins, both apps should be found.
     * Repeated app look ups should succeed.
     */
    @Test
    @Feature({"Payments"})
    public void testTwoAppsFromDifferentOriginsWithTheSamePaymentMethod() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://jonpay.com/webpay");
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay", "https://alicepay.com/webpay",
                /*signature=*/"ABCDEFABCDEFABCDEFAB");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.setStringArrayMetaData(
                "com.alicepay", new String[] {"https://jonpay.com/webpay"});
        mPackageManager.setStringArrayMetaData(
                "com.bobpay", new String[] {"https://jonpay.com/webpay"});

        findApps(methods);

        Assert.assertEquals("2 apps should match the query", 2, mPaymentApps.size());
        Set<String> appIdentifiers = new HashSet<>();
        appIdentifiers.add(mPaymentApps.get(0).getIdentifier());
        appIdentifiers.add(mPaymentApps.get(1).getIdentifier());
        Assert.assertTrue(appIdentifiers.contains("com.alicepay"));
        Assert.assertTrue(appIdentifiers.contains("com.bobpay"));

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("2 apps should still match the query", 2, mPaymentApps.size());
        appIdentifiers.clear();
        appIdentifiers.add(mPaymentApps.get(0).getIdentifier());
        appIdentifiers.add(mPaymentApps.get(1).getIdentifier());
        Assert.assertTrue(appIdentifiers.contains("com.alicepay"));
        Assert.assertTrue(appIdentifiers.contains("com.bobpay"));
    }

    /**
     * All known payment method names are valid.
     */
    @Test
    @Feature({"Payments"})
    public void testAllKnownPaymentMethodNames() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("basic-card");
        methods.add("interledger");
        methods.add("payee-credit-transfer");
        methods.add("payer-credit-transfer");
        methods.add("tokenized-card");
        methods.add("not-supported");
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay",
                "" /* no default payment method name in metadata */, /*signature=*/"AA");
        mPackageManager.setStringArrayMetaData("com.alicepay",
                new String[] {"basic-card", "interledger", "payee-credit-transfer",
                        "payer-credit-transfer", "tokenized-card", "not-supported"});

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.alicepay", mPaymentApps.get(0).getIdentifier());
        Assert.assertEquals(5, mPaymentApps.get(0).getInstrumentMethodNames().size());
        Assert.assertTrue(mPaymentApps.get(0).getInstrumentMethodNames().contains("basic-card"));
        Assert.assertTrue(mPaymentApps.get(0).getInstrumentMethodNames().contains("interledger"));
        Assert.assertTrue(
                mPaymentApps.get(0).getInstrumentMethodNames().contains("payee-credit-transfer"));
        Assert.assertTrue(
                mPaymentApps.get(0).getInstrumentMethodNames().contains("payer-credit-transfer"));
        Assert.assertTrue(
                mPaymentApps.get(0).getInstrumentMethodNames().contains("tokenized-card"));

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("1 app should still match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.alicepay", mPaymentApps.get(0).getIdentifier());
        Assert.assertEquals(5, mPaymentApps.get(0).getInstrumentMethodNames().size());
        Assert.assertTrue(mPaymentApps.get(0).getInstrumentMethodNames().contains("basic-card"));
        Assert.assertTrue(mPaymentApps.get(0).getInstrumentMethodNames().contains("interledger"));
        Assert.assertTrue(
                mPaymentApps.get(0).getInstrumentMethodNames().contains("payee-credit-transfer"));
        Assert.assertTrue(
                mPaymentApps.get(0).getInstrumentMethodNames().contains("payer-credit-transfer"));
        Assert.assertTrue(
                mPaymentApps.get(0).getInstrumentMethodNames().contains("tokenized-card"));
    }

    private void findApps(Set<String> methodNames) throws Throwable {
        ignorePaymentMethodIdentifierAndFindApps(
                /*ignoredPaymentMethodIdentifier=*/null, methodNames);
    }

    private void ignorePaymentMethodIdentifierAndFindApps(
            String ignoredPaymentMethodIdentifier, Set<String> methodNames) throws Throwable {
        mMethodData = buildMethodData(methodNames);
        mRule.runOnUiThread(() -> {
            AndroidPaymentAppFinder finder =
                    new AndroidPaymentAppFinder(new PaymentManifestWebDataService(), mDownloader,
                            new PaymentManifestParser(), mPackageManager,
                            /*delegate=*/AndroidPaymentAppFinderTest.this, /*factory=*/null);
            finder.bypassIsReadyToPayServiceInTest();
            if (ignoredPaymentMethodIdentifier != null) {
                finder.ignorePaymentMethodForTest(ignoredPaymentMethodIdentifier);
            }
            finder.findAndroidPaymentApps();
        });
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mAllPaymentAppsCreated;
            }
        });
    }

    private static Map<String, PaymentMethodData> buildMethodData(Set<String> methodNames) {
        Map<String, PaymentMethodData> result = new HashMap<>();
        for (String methodName : methodNames) {
            PaymentMethodData methodData = new PaymentMethodData();
            methodData.supportedMethod = methodName;
            result.put(methodName, methodData);
        }
        return result;
    }
}
