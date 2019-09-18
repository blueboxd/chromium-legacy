// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

import java.util.LinkedHashSet;

/**
 * Bridge class that lets Android code access native code to execute translate on a tab.
 */
public class TranslateBridge {
    /**
     * Translates the given tab when the necessary state has been computed (e.g. source language).
     */
    public static void translateTabWhenReady(Tab tab) {
        TranslateBridgeJni.get().manualTranslateWhenReady(tab.getWebContents());
    }

    /**
     * Returns true iff the current tab can be manually translated.
     */
    public static boolean canManuallyTranslate(Tab tab) {
        return TranslateBridgeJni.get().canManuallyTranslate(tab.getWebContents());
    }

    /**
     * Returns true iff we're in a state where the manual translate IPH could be shown.
     */
    public static boolean shouldShowManualTranslateIPH(Tab tab) {
        return TranslateBridgeJni.get().shouldShowManualTranslateIPH(tab.getWebContents());
    }

    /**
     * Sets the language that the contents of the tab needs to be translated to.
     * No-op in case target language is invalid or not supported.
     *
     * @param targetLanguage language code in ISO 639 format.
     */
    public static void setPredefinedTargetLanguage(Tab tab, String targetLanguage) {
        TranslateBridgeJni.get().setPredefinedTargetLanguage(tab.getWebContents(), targetLanguage);
    }

    /**
     * @return The best target language based on what the Translate Service knows about the user.
     */
    public static String getTargetLanguage() {
        return TranslateBridgeJni.get().getTargetLanguage();
    }

    /** @return whether the given string is blocked for translation. */
    public static boolean isBlockedLanguage(String language) {
        return TranslateBridgeJni.get().isBlockedLanguage(language);
    }

    /**
     * @return The ordered set of all languages that the user's knows, ordered by how well they know
     *         them with the most familiar listed first.
     */
    public static LinkedHashSet<String> getModelLanguages() {
        LinkedHashSet<String> set = new LinkedHashSet<String>();
        // Calls back through addModelLanguageToSet repeatedly.
        TranslateBridgeJni.get().getModelLanguages(set);
        return set;
    }

    /**
     * Called by {@link #TranslateBridgeJni.get().getModelLanguages} with the set to add to and the
     * language to add.
     */
    @CalledByNative
    private static void addModelLanguageToSet(
            LinkedHashSet<String> languages, String languageCode) {
        languages.add(languageCode);
    }

    @NativeMethods
    interface Natives {
        void manualTranslateWhenReady(WebContents webContents);
        boolean canManuallyTranslate(WebContents webContents);
        boolean shouldShowManualTranslateIPH(WebContents webContents);
        void setPredefinedTargetLanguage(WebContents webContents, String targetLanguage);
        String getTargetLanguage();
        boolean isBlockedLanguage(String language);
        void getModelLanguages(LinkedHashSet<String> set);
    }
}
