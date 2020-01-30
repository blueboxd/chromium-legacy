// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.LocationBarVoiceRecognitionHandler.Delegate;
import org.chromium.chrome.browser.omnibox.LocationBarVoiceRecognitionHandler.VoiceInteractionSource;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.base.WindowAndroid;

import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/**
 * Tests for LocationBarVoiceRecognitionHandler.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LocationBarVoiceRecognitionHandlerUnitTest {
    LocationBarVoiceRecognitionHandler mLocationBarVoiceRecognitionHandler;

    @Mock
    PackageManager mPackageManager;
    @Mock
    TemplateUrlService mTemplateUrlService;
    @Mock
    ExternalAuthUtils mExternalAuthUtils;

    PackageInfo mPackageInfo;

    @Before
    public void setUp() throws NameNotFoundException {
        MockitoAnnotations.initMocks(this);

        doReturn(true).when(mExternalAuthUtils).isGoogleSigned(IntentHandler.PACKAGE_GSA);

        mLocationBarVoiceRecognitionHandler = new LocationBarVoiceRecognitionHandler(
                Mockito.mock(Delegate.class), mExternalAuthUtils);

        mPackageInfo = new PackageInfo();
        mPackageInfo.versionCode =
                LocationBarVoiceRecognitionHandler.DEFAULT_ASSISTANT_AGSA_MIN_VERSION;
        doReturn(mPackageInfo).when(mPackageManager).getPackageInfo(IntentHandler.PACKAGE_GSA, 0);

        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        enableAssistantVoiceSearchFeature();
    }

    private void enableAssistantVoiceSearchFeature() {
        Map<String, Boolean> featureMap = new HashMap<>();
        featureMap.put(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH, true);
        ChromeFeatureList.setTestFeatures(featureMap);
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testStartVoiceRecognition_StartsAssistantVoiceSearch() {
        mPackageInfo.versionCode =
                LocationBarVoiceRecognitionHandler.DEFAULT_ASSISTANT_AGSA_MIN_VERSION;

        Assert.assertTrue(
                mLocationBarVoiceRecognitionHandler.requestAssistantVoiceSearchIfConditionsMet(
                        Mockito.mock(WindowAndroid.class), mPackageManager,
                        VoiceInteractionSource.OMNIBOX));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testStartVoiceRecognition_StartsAssistantVoiceSearch_AGSAVersionTooLow() {
        mPackageInfo.versionCode =
                LocationBarVoiceRecognitionHandler.DEFAULT_ASSISTANT_AGSA_MIN_VERSION - 1;

        Assert.assertFalse(
                mLocationBarVoiceRecognitionHandler.requestAssistantVoiceSearchIfConditionsMet(
                        Mockito.mock(WindowAndroid.class), mPackageManager,
                        VoiceInteractionSource.OMNIBOX));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testStartVoiceRecognition_StartsAssistantVoiceSearch_NameNotFound()
            throws NameNotFoundException {
        NameNotFoundException exception = new NameNotFoundException();
        doThrow(exception).when(mPackageManager).getPackageInfo(IntentHandler.PACKAGE_GSA, 0);

        Assert.assertFalse(
                mLocationBarVoiceRecognitionHandler.requestAssistantVoiceSearchIfConditionsMet(
                        Mockito.mock(WindowAndroid.class), mPackageManager,
                        VoiceInteractionSource.OMNIBOX));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testStartVoiceRecognition_StartsAssistantVoiceSearch_AGSANotSigned() {
        mPackageInfo.versionCode =
                LocationBarVoiceRecognitionHandler.DEFAULT_ASSISTANT_AGSA_MIN_VERSION;

        doReturn(false).when(mExternalAuthUtils).isGoogleSigned(IntentHandler.PACKAGE_GSA);

        Assert.assertFalse(
                mLocationBarVoiceRecognitionHandler.requestAssistantVoiceSearchIfConditionsMet(
                        Mockito.mock(WindowAndroid.class), mPackageManager,
                        VoiceInteractionSource.OMNIBOX));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testAssistantEligibility_MimimumSpecs() {
        Assert.assertTrue(mLocationBarVoiceRecognitionHandler.isDeviceEligibleForAssistant(
                mPackageManager, LocationBarVoiceRecognitionHandler.DEFAULT_ASSISTANT_MIN_MEMORY_MB,
                LocationBarVoiceRecognitionHandler.DEFAULT_ASSISTANT_LOCALES.iterator().next()));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testAssistantEligibility_MemoryTooLow() {
        Assert.assertFalse(mLocationBarVoiceRecognitionHandler.isDeviceEligibleForAssistant(
                mPackageManager,
                LocationBarVoiceRecognitionHandler.DEFAULT_ASSISTANT_MIN_MEMORY_MB - 1,
                LocationBarVoiceRecognitionHandler.DEFAULT_ASSISTANT_LOCALES.iterator().next()));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testAssistantEligibility_UnsupportedLocale() {
        Assert.assertFalse(mLocationBarVoiceRecognitionHandler.isDeviceEligibleForAssistant(
                mPackageManager, LocationBarVoiceRecognitionHandler.DEFAULT_ASSISTANT_MIN_MEMORY_MB,
                new Locale("br")));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void parseLocalesFromString_SingleValid() {
        String encodedLocales = "en-us";
        Set<Locale> locales = new HashSet<>(Arrays.asList(new Locale("en", "us")));

        Assert.assertEquals(locales,
                mLocationBarVoiceRecognitionHandler.parseLocalesFromString(encodedLocales));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void parseLocalesFromString_SingleInvalid() {
        String encodedLocales = "en)us";
        Assert.assertEquals(LocationBarVoiceRecognitionHandler.DEFAULT_ASSISTANT_LOCALES,
                mLocationBarVoiceRecognitionHandler.parseLocalesFromString(encodedLocales));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void parseLocalesFromString_MultipleValid() {
        String encodedLocales = "en-us,es-us,hi-in";
        Set<Locale> locales = new HashSet<>(Arrays.asList(
                new Locale("en", "us"), new Locale("es", "us"), new Locale("hi", "in")));

        Assert.assertEquals(locales,
                mLocationBarVoiceRecognitionHandler.parseLocalesFromString(encodedLocales));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void parseLocalesFromString_MultipleLastInvalid() {
        String encodedLocales = "en-us,es-us,hi*in";
        Assert.assertEquals(LocationBarVoiceRecognitionHandler.DEFAULT_ASSISTANT_LOCALES,
                mLocationBarVoiceRecognitionHandler.parseLocalesFromString(encodedLocales));
    }
}