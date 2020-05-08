// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.content.pm.Signature;
import android.os.Bundle;

import org.junit.Assert;
import org.mockito.ArgumentMatcher;
import org.mockito.ArgumentMatchers;
import org.mockito.Mockito;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.CalledByNativeJavaTest;
import org.chromium.components.payments.PaymentManifestDownloader;
import org.chromium.components.payments.PaymentManifestParser;
import org.chromium.components.payments.WebAppManifestSection;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.url.Origin;
import org.chromium.url.URI;

import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Tests for the native Android payment app finder. */
public class AndroidPaymentAppFinderUnitTest {
    private static final IntentArgumentMatcher sPayIntentArgumentMatcher =
            new IntentArgumentMatcher(new Intent("org.chromium.intent.action.PAY"));

    @CalledByNative
    private AndroidPaymentAppFinderUnitTest() {}

    /**
     * Argument matcher that matches Intents using |filterEquals| method.
     */
    private static class IntentArgumentMatcher implements ArgumentMatcher<Intent> {
        private final Intent mIntent;

        public IntentArgumentMatcher(Intent intent) {
            mIntent = intent;
        }

        @Override
        public boolean matches(Intent other) {
            return mIntent.filterEquals(other);
        }

        @Override
        public String toString() {
            return mIntent.toString();
        }
    }

    private PaymentAppFactoryDelegate findApps(String[] methodNames,
            PaymentManifestDownloader downloader, PaymentManifestParser parser,
            PackageManagerDelegate packageManagerDelegate) {
        Map<String, PaymentMethodData> methodData = new HashMap<>();
        for (String methodName : methodNames) {
            PaymentMethodData data = new PaymentMethodData();
            data.supportedMethod = methodName;
            data.stringifiedData = "{\"key\":\"value\"}";
            methodData.put(methodName, data);
        }
        PaymentAppFactoryParams params = Mockito.mock(PaymentAppFactoryParams.class);
        Mockito.when(params.getWebContents()).thenReturn(Mockito.mock(WebContents.class));
        Mockito.when(params.getId()).thenReturn("id");
        Mockito.when(params.getMethodData()).thenReturn(methodData);
        Mockito.when(params.getTopLevelOrigin()).thenReturn("https://chromium.org");
        Mockito.when(params.getPaymentRequestOrigin()).thenReturn("https://chromium.org");
        Mockito.when(params.getCertificateChain()).thenReturn(null);
        Mockito.when(params.getModifiers())
                .thenReturn(new HashMap<String, PaymentDetailsModifier>());
        Mockito.when(params.getMayCrawl()).thenReturn(false);
        PaymentAppFactoryDelegate delegate = Mockito.mock(PaymentAppFactoryDelegate.class);
        Mockito.when(delegate.getParams()).thenReturn(params);
        AndroidPaymentAppFinder finder =
                new AndroidPaymentAppFinder(Mockito.mock(PaymentManifestWebDataService.class),
                        downloader, parser, packageManagerDelegate, delegate,
                        /*factory=*/null);
        finder.bypassIsReadyToPayServiceInTest();
        finder.findAndroidPaymentApps();
        return delegate;
    }

    private void verifyNoAppsFound(PaymentAppFactoryDelegate delegate) {
        Mockito.verify(delegate, Mockito.never())
                .onPaymentAppCreated(Mockito.any(PaymentApp.class));
        Mockito.verify(delegate, Mockito.never())
                .onPaymentAppCreationError(Mockito.any(String.class));
        Mockito.verify(delegate, Mockito.never())
                .onAutofillPaymentAppCreatorAvailable(Mockito.any(AutofillPaymentAppCreator.class));
        Mockito.verify(delegate).onCanMakePaymentCalculated(false);
        Mockito.verify(delegate).onDoneCreatingPaymentApps(/*factory=*/null);
    }

    @CalledByNativeJavaTest
    public void testNoValidPaymentMethodNames() {
        verifyNoAppsFound(findApps(new String[] {"unknown-payment-method-name",
                                           "http://not.secure.payment.method.name.com", "https://"},
                Mockito.mock(PaymentManifestDownloader.class),
                Mockito.mock(PaymentManifestParser.class),
                Mockito.mock(PackageManagerDelegate.class)));
    }

    @CalledByNativeJavaTest
    public void testQueryWithoutApps() {
        PackageManagerDelegate packageManagerDelegate = Mockito.mock(PackageManagerDelegate.class);
        Mockito.when(packageManagerDelegate.getActivitiesThatCanRespondToIntentWithMetaData(
                             ArgumentMatchers.argThat(sPayIntentArgumentMatcher)))
                .thenReturn(new ArrayList<ResolveInfo>());

        verifyNoAppsFound(
                findApps(new String[] {"basic-card"}, Mockito.mock(PaymentManifestDownloader.class),
                        Mockito.mock(PaymentManifestParser.class), packageManagerDelegate));

        Mockito.verify(packageManagerDelegate, Mockito.never())
                .getStringArrayResourceForApplication(
                        ArgumentMatchers.any(ApplicationInfo.class), ArgumentMatchers.anyInt());
    }

    @CalledByNativeJavaTest
    public void testQueryWithoutMetaData() {
        List<ResolveInfo> activities = new ArrayList<>();
        ResolveInfo alicePay = new ResolveInfo();
        alicePay.activityInfo = new ActivityInfo();
        alicePay.activityInfo.packageName = "com.alicepay.app";
        alicePay.activityInfo.name = "com.alicepay.app.WebPaymentActivity";
        activities.add(alicePay);

        PackageManagerDelegate packageManagerDelegate = Mockito.mock(PackageManagerDelegate.class);
        Mockito.when(packageManagerDelegate.getAppLabel(Mockito.any(ResolveInfo.class)))
                .thenReturn("A non-empty label");
        Mockito.when(packageManagerDelegate.getActivitiesThatCanRespondToIntentWithMetaData(
                             ArgumentMatchers.argThat(sPayIntentArgumentMatcher)))
                .thenReturn(activities);

        verifyNoAppsFound(
                findApps(new String[] {"basic-card"}, Mockito.mock(PaymentManifestDownloader.class),
                        Mockito.mock(PaymentManifestParser.class), packageManagerDelegate));

        Mockito.verify(packageManagerDelegate, Mockito.never())
                .getStringArrayResourceForApplication(
                        ArgumentMatchers.any(ApplicationInfo.class), ArgumentMatchers.anyInt());
    }

    @CalledByNativeJavaTest
    public void testQueryWithoutLabel() {
        List<ResolveInfo> activities = new ArrayList<>();
        ResolveInfo alicePay = new ResolveInfo();
        alicePay.activityInfo = new ActivityInfo();
        alicePay.activityInfo.packageName = "com.alicepay.app";
        alicePay.activityInfo.name = "com.alicepay.app.WebPaymentActivity";
        Bundle activityMetaData = new Bundle();
        activityMetaData.putString(
                AndroidPaymentAppFinder.META_DATA_NAME_OF_DEFAULT_PAYMENT_METHOD_NAME,
                "basic-card");
        alicePay.activityInfo.metaData = activityMetaData;
        activities.add(alicePay);

        PackageManagerDelegate packageManagerDelegate = Mockito.mock(PackageManagerDelegate.class);
        Mockito.when(packageManagerDelegate.getActivitiesThatCanRespondToIntentWithMetaData(
                             ArgumentMatchers.argThat(sPayIntentArgumentMatcher)))
                .thenReturn(activities);

        verifyNoAppsFound(
                findApps(new String[] {"basic-card"}, Mockito.mock(PaymentManifestDownloader.class),
                        Mockito.mock(PaymentManifestParser.class), packageManagerDelegate));

        Mockito.verify(packageManagerDelegate, Mockito.never())
                .getStringArrayResourceForApplication(
                        ArgumentMatchers.any(ApplicationInfo.class), ArgumentMatchers.anyInt());
    }

    @CalledByNativeJavaTest
    public void testQueryUnsupportedPaymentMethod() {
        PackageManagerDelegate packageManagerDelegate = installPaymentApps(
                new String[] {"com.alicepay.app"}, new String[] {"unsupported-payment-method"});

        verifyNoAppsFound(findApps(new String[] {"unsupported-payment-method"},
                Mockito.mock(PaymentManifestDownloader.class),
                Mockito.mock(PaymentManifestParser.class), packageManagerDelegate));

        Mockito.verify(packageManagerDelegate, Mockito.never())
                .getStringArrayResourceForApplication(
                        ArgumentMatchers.any(ApplicationInfo.class), ArgumentMatchers.anyInt());
    }

    private static PackageManagerDelegate installPaymentApps(
            String[] packageNames, String[] methodNames) {
        assert packageNames.length == methodNames.length;
        List<ResolveInfo> activities = new ArrayList<>();
        for (int i = 0; i < packageNames.length; i++) {
            ResolveInfo alicePay = new ResolveInfo();
            alicePay.activityInfo = new ActivityInfo();
            alicePay.activityInfo.packageName = packageNames[i];
            alicePay.activityInfo.name = packageNames[i] + ".WebPaymentActivity";
            Bundle activityMetaData = new Bundle();
            activityMetaData.putString(
                    AndroidPaymentAppFinder.META_DATA_NAME_OF_DEFAULT_PAYMENT_METHOD_NAME,
                    methodNames[i]);
            alicePay.activityInfo.metaData = activityMetaData;
            activities.add(alicePay);
        }

        PackageManagerDelegate packageManagerDelegate = Mockito.mock(PackageManagerDelegate.class);
        Mockito.when(packageManagerDelegate.getAppLabel(Mockito.any(ResolveInfo.class)))
                .thenReturn("A non-empty label");
        Mockito.when(packageManagerDelegate.getActivitiesThatCanRespondToIntentWithMetaData(
                             ArgumentMatchers.argThat(sPayIntentArgumentMatcher)))
                .thenReturn(activities);
        return packageManagerDelegate;
    }

    @CalledByNativeJavaTest
    public void testQueryDifferentPaymentMethod() {
        PackageManagerDelegate packageManagerDelegate =
                installPaymentApps(new String[] {"com.alicepay.app"}, new String[] {"basic-card"});

        verifyNoAppsFound(findApps(new String[] {"interledger"},
                Mockito.mock(PaymentManifestDownloader.class),
                Mockito.mock(PaymentManifestParser.class), packageManagerDelegate));

        Mockito.verify(packageManagerDelegate, Mockito.never())
                .getStringArrayResourceForApplication(
                        ArgumentMatchers.any(ApplicationInfo.class), ArgumentMatchers.anyInt());
    }

    @CalledByNativeJavaTest
    public void testQueryNoPaymentMethod() {
        PackageManagerDelegate packageManagerDelegate =
                installPaymentApps(new String[] {"com.alicepay.app"}, new String[] {"basic-card"});

        verifyNoAppsFound(findApps(new String[0], Mockito.mock(PaymentManifestDownloader.class),
                Mockito.mock(PaymentManifestParser.class), packageManagerDelegate));

        Mockito.verify(packageManagerDelegate, Mockito.never())
                .getStringArrayResourceForApplication(
                        ArgumentMatchers.any(ApplicationInfo.class), ArgumentMatchers.anyInt());
    }

    @CalledByNativeJavaTest
    public void testQueryBasicCardsWithTwoApps() {
        List<ResolveInfo> activities = new ArrayList<>();
        ResolveInfo alicePay = new ResolveInfo();
        alicePay.activityInfo = new ActivityInfo();
        alicePay.activityInfo.packageName = "com.alicepay.app";
        alicePay.activityInfo.name = "com.alicepay.app.WebPaymentActivity";
        alicePay.activityInfo.applicationInfo = new ApplicationInfo();
        Bundle alicePayMetaData = new Bundle();
        alicePayMetaData.putString(
                AndroidPaymentAppFinder.META_DATA_NAME_OF_DEFAULT_PAYMENT_METHOD_NAME,
                "basic-card");
        alicePayMetaData.putInt(AndroidPaymentAppFinder.META_DATA_NAME_OF_PAYMENT_METHOD_NAMES, 1);
        alicePay.activityInfo.metaData = alicePayMetaData;
        activities.add(alicePay);

        ResolveInfo bobPay = new ResolveInfo();
        bobPay.activityInfo = new ActivityInfo();
        bobPay.activityInfo.packageName = "com.bobpay.app";
        bobPay.activityInfo.name = "com.bobpay.app.WebPaymentActivity";
        bobPay.activityInfo.applicationInfo = new ApplicationInfo();
        Bundle bobPayMetaData = new Bundle();
        bobPayMetaData.putString(
                AndroidPaymentAppFinder.META_DATA_NAME_OF_DEFAULT_PAYMENT_METHOD_NAME,
                "basic-card");
        bobPayMetaData.putInt(AndroidPaymentAppFinder.META_DATA_NAME_OF_PAYMENT_METHOD_NAMES, 2);
        bobPay.activityInfo.metaData = bobPayMetaData;
        activities.add(bobPay);

        PackageManagerDelegate packageManagerDelegate = Mockito.mock(PackageManagerDelegate.class);
        Mockito.when(packageManagerDelegate.getAppLabel(Mockito.any(ResolveInfo.class)))
                .thenReturn("A non-empty label");
        Mockito.when(packageManagerDelegate.getActivitiesThatCanRespondToIntentWithMetaData(
                             ArgumentMatchers.argThat(sPayIntentArgumentMatcher)))
                .thenReturn(activities);
        Mockito.when(packageManagerDelegate.getServicesThatCanRespondToIntent(
                             ArgumentMatchers.argThat(new IntentArgumentMatcher(
                                     new Intent(AndroidPaymentAppFinder.ACTION_IS_READY_TO_PAY)))))
                .thenReturn(new ArrayList<ResolveInfo>());

        Mockito.when(packageManagerDelegate.getStringArrayResourceForApplication(
                             ArgumentMatchers.eq(alicePay.activityInfo.applicationInfo),
                             ArgumentMatchers.eq(1)))
                .thenReturn(new String[] {"https://alicepay.com"});
        Mockito.when(packageManagerDelegate.getStringArrayResourceForApplication(
                             ArgumentMatchers.eq(bobPay.activityInfo.applicationInfo),
                             ArgumentMatchers.eq(2)))
                .thenReturn(new String[] {"https://bobpay.com"});

        PaymentAppFactoryDelegate delegate =
                findApps(new String[] {"basic-card"}, Mockito.mock(PaymentManifestDownloader.class),
                        Mockito.mock(PaymentManifestParser.class), packageManagerDelegate);

        Mockito.verify(delegate).onCanMakePaymentCalculated(true);
        Mockito.verify(delegate).onPaymentAppCreated(
                ArgumentMatchers.argThat(Matches.paymentAppIdentifier("com.alicepay.app")));
        Mockito.verify(delegate).onPaymentAppCreated(
                ArgumentMatchers.argThat(Matches.paymentAppIdentifier("com.bobpay.app")));
        Mockito.verify(delegate).onDoneCreatingPaymentApps(/*factory=*/null);
    }

    @CalledByNativeJavaTest
    public void testQueryBobPayWithOneAppThatHasIsReadyToPayService() {
        List<ResolveInfo> activities = new ArrayList<>();
        ResolveInfo bobPay = new ResolveInfo();
        bobPay.activityInfo = new ActivityInfo();
        bobPay.activityInfo.packageName = "com.bobpay.app";
        bobPay.activityInfo.name = "com.bobpay.app.WebPaymentActivity";
        bobPay.activityInfo.applicationInfo = new ApplicationInfo();
        Bundle bobPayMetaData = new Bundle();
        bobPayMetaData.putString(
                AndroidPaymentAppFinder.META_DATA_NAME_OF_DEFAULT_PAYMENT_METHOD_NAME,
                "https://bobpay.com");
        bobPayMetaData.putInt(AndroidPaymentAppFinder.META_DATA_NAME_OF_PAYMENT_METHOD_NAMES, 1);
        bobPay.activityInfo.metaData = bobPayMetaData;
        activities.add(bobPay);

        PackageManagerDelegate packageManagerDelegate = Mockito.mock(PackageManagerDelegate.class);
        Mockito.when(packageManagerDelegate.getAppLabel(Mockito.any(ResolveInfo.class)))
                .thenReturn("A non-empty label");
        Mockito.when(packageManagerDelegate.getActivitiesThatCanRespondToIntentWithMetaData(
                             ArgumentMatchers.argThat(sPayIntentArgumentMatcher)))
                .thenReturn(activities);

        Mockito.when(packageManagerDelegate.getStringArrayResourceForApplication(
                             ArgumentMatchers.eq(bobPay.activityInfo.applicationInfo),
                             ArgumentMatchers.eq(1)))
                .thenReturn(new String[] {"https://bobpay.com", "basic-card"});

        List<ResolveInfo> services = new ArrayList<>();
        ResolveInfo isBobPayReadyToPay = new ResolveInfo();
        isBobPayReadyToPay.serviceInfo = new ServiceInfo();
        isBobPayReadyToPay.serviceInfo.packageName = "com.bobpay.app";
        isBobPayReadyToPay.serviceInfo.name = "com.bobpay.app.IsReadyToWebPay";
        services.add(isBobPayReadyToPay);
        Intent isReadyToPayIntent = new Intent(AndroidPaymentAppFinder.ACTION_IS_READY_TO_PAY);
        Mockito
                .when(packageManagerDelegate.getServicesThatCanRespondToIntent(
                        ArgumentMatchers.argThat(new IntentArgumentMatcher(isReadyToPayIntent))))
                .thenReturn(services);

        PackageInfo bobPayPackageInfo = new PackageInfo();
        bobPayPackageInfo.versionCode = 10;
        bobPayPackageInfo.signatures = new Signature[1];
        bobPayPackageInfo.signatures[0] = PaymentManifestVerifierTest.BOB_PAY_SIGNATURE;
        Mockito.when(packageManagerDelegate.getPackageInfoWithSignatures("com.bobpay.app"))
                .thenReturn(bobPayPackageInfo);

        PaymentManifestDownloader downloader = new PaymentManifestDownloader() {
            @Override
            public void initialize(WebContents webContents) {}

            @Override
            public void downloadPaymentMethodManifest(
                    Origin merchantOrigin, URI uri, ManifestDownloadCallback callback) {
                callback.onPaymentMethodManifestDownloadSuccess(uri,
                        PaymentManifestDownloader.createOpaqueOriginForTest(), "some content here");
            }

            @Override
            public void downloadWebAppManifest(Origin paynentMethodManifestOrigin, URI uri,
                    ManifestDownloadCallback callback) {
                callback.onWebAppManifestDownloadSuccess("some content here");
            }

            @Override
            public void destroy() {}
        };

        PaymentManifestParser parser = new PaymentManifestParser() {
            @Override
            public void parsePaymentMethodManifest(
                    URI paymentMethodManifestUrl, String content, ManifestParseCallback callback) {
                try {
                    callback.onPaymentMethodManifestParseSuccess(
                            new URI[] {new URI("https://bobpay.com/app.json")}, new URI[0]);
                } catch (URISyntaxException e) {
                    Assert.assertTrue(false);
                }
            }

            @Override
            public void parseWebAppManifest(String content, ManifestParseCallback callback) {
                WebAppManifestSection[] manifest = new WebAppManifestSection[1];
                int minVersion = 10;
                manifest[0] = new WebAppManifestSection("com.bobpay.app", minVersion,
                        PaymentManifestVerifierTest.BOB_PAY_SIGNATURE_FINGERPRINTS);
                callback.onWebAppManifestParseSuccess(manifest);
            }

            @Override
            public void createNative(WebContents webContents) {}

            @Override
            public void destroyNative() {}
        };

        PaymentAppFactoryDelegate delegate = findApps(
                new String[] {"https://bobpay.com"}, downloader, parser, packageManagerDelegate);

        Mockito.verify(delegate).onPaymentAppCreated(
                ArgumentMatchers.argThat(Matches.paymentAppIdentifier("com.bobpay.app")));
        Mockito.verify(delegate).onDoneCreatingPaymentApps(/*factory=*/null);
    }

    private static final class Matches implements ArgumentMatcher<PaymentApp> {
        private final String mExpectedAppIdentifier;

        private Matches(String expectedAppIdentifier) {
            mExpectedAppIdentifier = expectedAppIdentifier;
        }

        /**
         * Builds a matcher based on payment app identifier.
         *
         * @param expectedAppIdentifier The expected app identifier to match.
         * @return A matcher to use in a mock expectation.
         */
        public static ArgumentMatcher<PaymentApp> paymentAppIdentifier(
                String expectedAppIdentifier) {
            return new Matches(expectedAppIdentifier);
        }

        @Override
        public boolean matches(PaymentApp app) {
            return app.getIdentifier().equals(mExpectedAppIdentifier);
        }
    }
}
