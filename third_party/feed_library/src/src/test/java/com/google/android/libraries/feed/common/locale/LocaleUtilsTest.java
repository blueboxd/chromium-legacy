// Copyright 2018 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.android.libraries.feed.common.locale;

import static com.google.common.truth.Truth.assertThat;

import android.app.Activity;
import android.content.Context;
import android.os.Build.VERSION_CODES;
import java.util.Locale;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Tests of the {@link LocaleUtils} class. */
@RunWith(RobolectricTestRunner.class)
public class LocaleUtilsTest {

  private Context context;

  @Before
  public void setUp() {
    context = Robolectric.buildActivity(Activity.class).get();
  }

  @Test
  @Config(qualifiers = "fr-rCA", sdk = VERSION_CODES.N)
  public void getLocale_byContext_postN() {
    assertThat(LocaleUtils.getLocale(context)).isEqualTo(Locale.CANADA_FRENCH);
  }

  @Test
  @Config(qualifiers = "fr-rCA", sdk = VERSION_CODES.LOLLIPOP)
  public void getLoca_byContext_preN() {
    assertThat(LocaleUtils.getLocale(context)).isEqualTo(Locale.CANADA_FRENCH);
  }

  @Test
  @Config(qualifiers = "fr-rCA", sdk = VERSION_CODES.LOLLIPOP)
  public void getLanguageTag_byContext_postLollipop() {
    assertThat(LocaleUtils.getLanguageTag(context)).isEqualTo("fr-CA");
  }

  @Test
  @Config(qualifiers = "fr-rCA", sdk = VERSION_CODES.KITKAT)
  public void getLanguageTag_byContext_preLollipop() {
    assertThat(LocaleUtils.getLanguageTag(context)).isEqualTo("fr-CA");
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
