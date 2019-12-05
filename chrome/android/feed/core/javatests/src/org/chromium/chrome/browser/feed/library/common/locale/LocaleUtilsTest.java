// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.locale;

import static com.google.common.truth.Truth.assertThat;

import android.app.Activity;
import android.content.Context;
import android.os.Build.VERSION_CODES;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Locale;

/** Tests of the {@link LocaleUtils} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LocaleUtilsTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext = Robolectric.buildActivity(Activity.class).get();
    }

    @Test
    @Config(qualifiers = "fr-rCA", sdk = VERSION_CODES.O /*N is not available*/)
    public void getLocale_byContext_postN() {
        assertThat(LocaleUtils.getLocale(mContext)).isEqualTo(Locale.CANADA_FRENCH);
    }

    @Test
    @Config(qualifiers = "fr-rCA", sdk = VERSION_CODES.LOLLIPOP)
    public void getLoca_byContext_preN() {
        assertThat(LocaleUtils.getLocale(mContext)).isEqualTo(Locale.CANADA_FRENCH);
    }

    @Test
    @Config(qualifiers = "fr-rCA", sdk = VERSION_CODES.LOLLIPOP)
    public void getLanguageTag_byContext_postLollipop() {
        assertThat(LocaleUtils.getLanguageTag(mContext)).isEqualTo("fr-CA");
    }

    @Test
    @Config(qualifiers = "fr-rCA", sdk = VERSION_CODES.KITKAT)
    public void getLanguageTag_byContext_preLollipop() {
        assertThat(LocaleUtils.getLanguageTag(mContext)).isEqualTo("fr-CA");
    }

    @Test
    @Config(sdk = VERSION_CODES.LOLLIPOP)
    public void getLanguageTag_byLocale_postLollipop() {
        assertThat(LocaleUtils.getLanguageTag(Locale.CANADA_FRENCH)).isEqualTo("fr-CA");
    }

    @Test
    @Config(sdk = VERSION_CODES.KITKAT)
    public void getLanguageTag_byLocale_preLollipop() {
        assertThat(LocaleUtils.getLanguageTag(Locale.CANADA_FRENCH)).isEqualTo("fr-CA");
    }

    @Test
    @Config(sdk = VERSION_CODES.KITKAT)
    public void getLanguageTag_byLocale_preLollipop_badLanguage() {
        assertThat(LocaleUtils.getLanguageTag(new Locale("", "CA"))).isEqualTo("und-CA");
    }
}
