// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doReturn;

import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.graphics.drawable.Drawable;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.SysUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.search_engines.TemplateUrlService;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

/**
 * Tests for AssistantVoiceSearchService.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH)
public class AssistantVoiceSearchServiceUnitTest {
    AssistantVoiceSearchService mAssistantVoiceSearchService;

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    GSAState mGsaState;
    @Mock
    PackageManager mPackageManager;
    @Mock
    TemplateUrlService mTemplateUrlService;
    @Mock
    ExternalAuthUtils mExternalAuthUtils;

    PackageInfo mPackageInfo;
    Context mContext;

    @Before
    public void setUp() throws NameNotFoundException {
        MockitoAnnotations.initMocks(this);

        mContext = Mockito.spy(Robolectric.buildActivity(Activity.class).setup().get());

        doReturn(true).when(mExternalAuthUtils).isChromeGoogleSigned();
        doReturn(true).when(mExternalAuthUtils).isGoogleSigned(IntentHandler.PACKAGE_GSA);

        mPackageInfo = new PackageInfo();
        mPackageInfo.versionName = AssistantVoiceSearchService.DEFAULT_ASSISTANT_AGSA_MIN_VERSION;
        doReturn(mPackageInfo).when(mPackageManager).getPackageInfo(IntentHandler.PACKAGE_GSA, 0);
        doReturn(mPackageManager).when(mContext).getPackageManager();

        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        doReturn(false).when(mGsaState).isAgsaVersionBelowMinimum(anyString(), anyString());
        doReturn(true).when(mGsaState).canAgsaHandleIntent(any());

        SysUtils.setAmountOfPhysicalMemoryKBForTesting(
                AssistantVoiceSearchService.DEFAULT_ASSISTANT_MIN_MEMORY_MB
                * ConversionUtils.KILOBYTES_PER_MEGABYTE);

        mAssistantVoiceSearchService = new AssistantVoiceSearchService(
                mContext, mExternalAuthUtils, mTemplateUrlService, mGsaState, null);
    }

    @After
    public void tearDown() {
        SysUtils.resetForTesting();
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testStartVoiceRecognition_StartsAssistantVoiceSearch() {
        Assert.assertTrue(mAssistantVoiceSearchService.shouldRequestAssistantVoiceSearch());
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testStartVoiceRecognition_StartsAssistantVoiceSearch_ChromeNotSigned() {
        doReturn(false).when(mExternalAuthUtils).isChromeGoogleSigned();

        Assert.assertFalse(mAssistantVoiceSearchService.shouldRequestAssistantVoiceSearch());
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testStartVoiceRecognition_StartsAssistantVoiceSearch_AGSANotSigned() {
        doReturn(false).when(mExternalAuthUtils).isGoogleSigned(IntentHandler.PACKAGE_GSA);

        Assert.assertFalse(mAssistantVoiceSearchService.shouldRequestAssistantVoiceSearch());
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testAssistantEligibility_MimimumSpecs() {
        Assert.assertTrue(mAssistantVoiceSearchService.isDeviceEligibleForAssistant(
                AssistantVoiceSearchService.DEFAULT_ASSISTANT_MIN_MEMORY_MB,
                AssistantVoiceSearchService.DEFAULT_ASSISTANT_LOCALES.iterator().next()));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testAssistantEligibility_MemoryTooLow() {
        Assert.assertFalse(mAssistantVoiceSearchService.isDeviceEligibleForAssistant(
                AssistantVoiceSearchService.DEFAULT_ASSISTANT_MIN_MEMORY_MB - 1,
                AssistantVoiceSearchService.DEFAULT_ASSISTANT_LOCALES.iterator().next()));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testAssistantEligibility_UnsupportedLocale() {
        Assert.assertFalse(mAssistantVoiceSearchService.isDeviceEligibleForAssistant(
                AssistantVoiceSearchService.DEFAULT_ASSISTANT_MIN_MEMORY_MB, new Locale("br")));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void parseLocalesFromString_SingleValid() {
        String encodedLocales = "en-us";
        Set<Locale> locales = new HashSet<>(Arrays.asList(new Locale("en", "us")));

        Assert.assertEquals(
                locales, AssistantVoiceSearchService.parseLocalesFromString(encodedLocales));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void parseLocalesFromString_SingleInvalid() {
        String encodedLocales = "en)us";
        Assert.assertEquals(AssistantVoiceSearchService.DEFAULT_ASSISTANT_LOCALES,
                AssistantVoiceSearchService.parseLocalesFromString(encodedLocales));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void parseLocalesFromString_MultipleValid() {
        String encodedLocales = "en-us,es-us,hi-in";
        Set<Locale> locales = new HashSet<>(Arrays.asList(
                new Locale("en", "us"), new Locale("es", "us"), new Locale("hi", "in")));

        Assert.assertEquals(
                locales, AssistantVoiceSearchService.parseLocalesFromString(encodedLocales));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void parseLocalesFromString_MultipleLastInvalid() {
        String encodedLocales = "en-us,es-us,hi*in";
        Assert.assertEquals(AssistantVoiceSearchService.DEFAULT_ASSISTANT_LOCALES,
                AssistantVoiceSearchService.parseLocalesFromString(encodedLocales));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void getMicButtonColorStateList_ColorfulMicEnabled() {
        mAssistantVoiceSearchService.setColorfulMicEnabledForTesting(true);
        Assert.assertNull(mAssistantVoiceSearchService.getMicButtonColorStateList(0, mContext));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void getCurrentMicDrawable() {
        Drawable greyMic = mAssistantVoiceSearchService.getCurrentMicDrawable();
        mAssistantVoiceSearchService.setColorfulMicEnabledForTesting(true);
        Drawable colorfulMic = mAssistantVoiceSearchService.getCurrentMicDrawable();

        Assert.assertNotEquals(greyMic, colorfulMic);
    }
}