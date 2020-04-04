// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.ColorRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.LocaleUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.util.ColorUtils;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

/**
 * Service for state tracking and event delivery to classes that need to observe the state
 * of Assistant Voice Search.
 **/
public class AssistantVoiceSearchService implements TemplateUrlService.TemplateUrlServiceObserver {
    private static final String USER_ELIGIBILITY_HISTOGRAM =
            "Assistant.VoiceSearch.UserEligibility";

    /** Allows outside classes to listen for changes in this service. */
    public interface Observer {
        /**
         * Called when the service changes, use relevant getters to update your state. This
         * indicates that the state of AssistantVoiceSearchService has changed and you should
         * re-query the getters you're interested in.
         * - The microphone Drawable icon can change during runtime.
         **/
        void onAssistantVoiceSearchServiceChanged();
    }

    // Constants for Assistant Voice Search.
    // TODO(crbug.com/1041576): Update this placeholder to a the real min version once the code has
    //                          landed.
    @VisibleForTesting
    public static final String DEFAULT_ASSISTANT_AGSA_MIN_VERSION = "10.98";
    @VisibleForTesting
    public static final int DEFAULT_ASSISTANT_MIN_ANDROID_SDK_VERSION =
            Build.VERSION_CODES.LOLLIPOP;
    @VisibleForTesting
    public static final int DEFAULT_ASSISTANT_MIN_MEMORY_MB = 1024;
    @VisibleForTesting
    static final Set<Locale> DEFAULT_ASSISTANT_LOCALES = new HashSet<>(
            Arrays.asList(new Locale("en", "us"), new Locale("en", "gb"), new Locale("en", "in"),
                    new Locale("hi", "in"), new Locale("bn", "in"), new Locale("te", "in"),
                    new Locale("mr", "in"), new Locale("ta", "in"), new Locale("kn", "in"),
                    new Locale("ml", "in"), new Locale("gu", "in"), new Locale("ur", "in")));
    private static final boolean DEFAULT_ASSISTANT_COLORFUL_MIC_ENABLED = false;

    // TODO(wylieb): Convert this to an ObserverList and add #addObserver, #removeObserver.
    private final Observer mObserver;
    private final Context mContext;
    private final ExternalAuthUtils mExternalAuthUtils;
    private final TemplateUrlService mTemplateUrlService;
    private final GSAState mGsaState;

    private Set<Locale> mSupportedLocales;
    private boolean mIsDefaultSearchEngineGoogle;
    private boolean mIsAssistantVoiceSearchEnabled;
    private boolean mIsColorfulMicEnabled;
    private boolean mShouldShowColorfulMic;
    private String mMinAgsaVersion;
    private int mMinAndroidSdkVersion;
    private int mMinMemoryMb;

    public AssistantVoiceSearchService(@NonNull Context context,
            @NonNull ExternalAuthUtils externalAuthUtils,
            @NonNull TemplateUrlService templateUrlService, @NonNull GSAState gsaState,
            @Nullable Observer observer) {
        mContext = context;
        mExternalAuthUtils = externalAuthUtils;
        mTemplateUrlService = templateUrlService;
        mGsaState = gsaState;
        mObserver = observer;

        mTemplateUrlService.addObserver(this);
        initializeAssistantVoiceSearchState();
    }

    public void destroy() {
        mTemplateUrlService.removeObserver(this);
    }

    @Override
    public void onTemplateURLServiceChanged() {
        boolean searchEngineGoogle = mTemplateUrlService.isDefaultSearchEngineGoogle();
        if (mIsDefaultSearchEngineGoogle == searchEngineGoogle) return;

        mIsDefaultSearchEngineGoogle = searchEngineGoogle;
        mShouldShowColorfulMic = isColorfulMicEnabled();
        notifyObserver();
    }

    private void notifyObserver() {
        if (mObserver == null) return;
        mObserver.onAssistantVoiceSearchServiceChanged();
    }

    /** Cache Assistant voice search variable state. */
    @VisibleForTesting
    void initializeAssistantVoiceSearchState() {
        mIsAssistantVoiceSearchEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH);
        // If we're not using assistant, then don't bother initializing anything else.
        if (!mIsAssistantVoiceSearchEnabled) return;

        mIsColorfulMicEnabled = ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH, "colorful_mic",
                DEFAULT_ASSISTANT_COLORFUL_MIC_ENABLED);

        mMinAgsaVersion = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH, "min_agsa_version");

        mMinAndroidSdkVersion = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH, "min_android_sdk",
                DEFAULT_ASSISTANT_MIN_ANDROID_SDK_VERSION);

        mMinMemoryMb = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH, "min_memory_mb",
                DEFAULT_ASSISTANT_MIN_MEMORY_MB);

        mSupportedLocales = parseLocalesFromString(ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH, "enabled_locales"));

        mIsDefaultSearchEngineGoogle = mTemplateUrlService.isDefaultSearchEngineGoogle();

        mShouldShowColorfulMic = isColorfulMicEnabled();
    }

    /**
     * Checks to see if the call to this method can be resolved with assistant voice search and
     * resolves it if possible.
     *
     * @return Whether the startVoiceRecognition call has been resolved.
     */
    public boolean shouldRequestAssistantVoiceSearch() {
        if (!mIsAssistantVoiceSearchEnabled || !mIsDefaultSearchEngineGoogle
                || !isDeviceEligibleForAssistant(
                        ConversionUtils.kilobytesToMegabytes(SysUtils.amountOfPhysicalMemoryKB()),
                        Locale.getDefault())) {
            return false;
        }

        return true;
    }

    /**
     * @return Gets the current mic drawable, this will create the drawable if it doesn't already
     *         exist or reuse the existing Drawable's ConstantState if it does already exist.
     **/
    public Drawable getCurrentMicDrawable() {
        return AppCompatResources.getDrawable(
                mContext, mShouldShowColorfulMic ? R.drawable.ic_colorful_mic : R.drawable.btn_mic);
    }

    /** @return The correct ColorStateList for the current theme. */
    public @Nullable ColorStateList getMicButtonColorStateList(
            @ColorRes int primaryColor, Context context) {
        if (mShouldShowColorfulMic) return null;

        final boolean useLightColors =
                ColorUtils.shouldUseLightForegroundOnBackground(primaryColor);
        int id = ChromeColors.getPrimaryIconTintRes(useLightColors);
        return AppCompatResources.getColorStateList(context, id);
    }

    /** @return Whether the colorful mic is enabled. */
    private boolean isColorfulMicEnabled() {
        return mContext.getPackageManager() != null && mIsColorfulMicEnabled
                && shouldRequestAssistantVoiceSearch();
    }

    /**
     * @return The min Agsa version required for assistant voice search. This method checks the
     *         chrome feature list for this value before using the default.
     */
    private String getAgsaMinVersion() {
        if (TextUtils.isEmpty(mMinAgsaVersion)) return DEFAULT_ASSISTANT_AGSA_MIN_VERSION;
        return mMinAgsaVersion;
    }

    /** @return Whether the device is eligible to use assistant. */
    @VisibleForTesting
    protected boolean isDeviceEligibleForAssistant(long availableMemoryMb, Locale currentLocale) {
        if (!mGsaState.canAgsaHandleIntent(getAssistantVoiceSearchIntent())) {
            return false;
        }

        if (mGsaState.isAgsaVersionBelowMinimum(
                    mGsaState.getAgsaVersionName(), getAgsaMinVersion())) {
            return false;
        }

        // AGSA will throw an exception if Chrome isn't Google signed.
        if (!mExternalAuthUtils.isChromeGoogleSigned()) return false;
        if (!mExternalAuthUtils.isGoogleSigned(IntentHandler.PACKAGE_GSA)) return false;

        return mMinMemoryMb <= availableMemoryMb && mMinAndroidSdkVersion <= Build.VERSION.SDK_INT
                && mSupportedLocales.contains(currentLocale);
    }

    /** @return The Intent for Assistant voice search. */
    public Intent getAssistantVoiceSearchIntent() {
        Intent intent = new Intent(Intent.ACTION_SEARCH);
        intent.setPackage(IntentHandler.PACKAGE_GSA);
        return intent;
    }

    /**
     * @return List of locales parsed from the given input string, or a default list if the parsing
     * fails.
     */
    @VisibleForTesting
    static Set<Locale> parseLocalesFromString(String encodedEnabledLocalesList) {
        if (TextUtils.isEmpty(encodedEnabledLocalesList)) return DEFAULT_ASSISTANT_LOCALES;

        String[] encodedEnabledLocales = encodedEnabledLocalesList.split(",");
        HashSet<Locale> enabledLocales = new HashSet<>(encodedEnabledLocales.length);
        for (int i = 0; i < encodedEnabledLocales.length; i++) {
            Locale locale = LocaleUtils.forLanguageTag(encodedEnabledLocales[i]);
            if (TextUtils.isEmpty(locale.getCountry()) || TextUtils.isEmpty(locale.getLanguage())) {
                // Error with the locale encoding, fallback to the default locales.
                return DEFAULT_ASSISTANT_LOCALES;
            }
            enabledLocales.add(locale);
        }

        return enabledLocales;
    }

    /** Records whether the user is eligible. */
    public static void reportUserEligibility(boolean eligible) {
        RecordHistogram.recordBooleanHistogram(USER_ELIGIBILITY_HISTOGRAM, eligible);
    }

    /** Enable the colorful mic for testing purposes. */
    void setColorfulMicEnabledForTesting(boolean enabled) {
        mIsColorfulMicEnabled = enabled;
        mShouldShowColorfulMic = enabled;
    }
}
