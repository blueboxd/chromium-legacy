// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.config;

import static com.google.common.truth.Truth.assertThat;

import static junit.framework.Assert.fail;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link Configuration}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ConfigurationTest {
    @ConfigKey
    private static final String CONFIG_KEY_WITH_DEFAULT = "config-test";

    private static final boolean CONFIG_KEY_WITH_DEFAULT_VALUE = false;

    @Test
    public void defaultConfig_hasDefaultValues() {
        Configuration configuration =
                new Configuration.Builder()
                        .put(CONFIG_KEY_WITH_DEFAULT, CONFIG_KEY_WITH_DEFAULT_VALUE)
                        .build();

        boolean valueOrDefault = configuration.getValueOrDefault(
                CONFIG_KEY_WITH_DEFAULT, !CONFIG_KEY_WITH_DEFAULT_VALUE);

        assertThat(valueOrDefault).isEqualTo(CONFIG_KEY_WITH_DEFAULT_VALUE);
    }

    @Test
    public void hasValue_forValueThatDoesNotExist_returnsFalse() {
        Configuration configuration = new Configuration.Builder().build();

        assertThat(configuration.hasValue(ConfigKey.FEED_SERVER_ENDPOINT)).isFalse();
    }

    @Test
    public void getValueOrDefault_forValueThatDoesNotExist_returnsSpecifiedDefault() {
        final String defaultString = "defaultString";
        Configuration configuration = new Configuration.Builder().build();

        assertThat(configuration.getValueOrDefault(ConfigKey.FEED_SERVER_ENDPOINT, defaultString))
                .isEqualTo(defaultString);
    }

    @Test
    public void getValueOrDefault_forValueThatExists_returnsValue() {
        final String someValue = "someValue";
        Configuration configuration =
                new Configuration.Builder().put(ConfigKey.FEED_SERVER_ENDPOINT, someValue).build();

        assertThat(configuration.getValueOrDefault(ConfigKey.FEED_SERVER_ENDPOINT, ""))
                .isEqualTo(someValue);
    }

    @Test
    public void getValueOrDefaultWithWrongType_throwsClassCastException() {
        Configuration configuration = new Configuration.Builder()
                                              .put(ConfigKey.FEED_SERVER_ENDPOINT, "someString")
                                              .build();

        try {
            @SuppressWarnings("unused") // Used for type inference
            Boolean ignored =
                    configuration.getValueOrDefault(ConfigKey.FEED_SERVER_ENDPOINT, false);
            fail();
        } catch (ClassCastException ignored) {
            // expected
        }
    }

    @Test
    public void getValue_forValueThatWasOverridden_ReturnsOverriddenValue() {
        Configuration configuration =
                new Configuration.Builder()
                        .put(CONFIG_KEY_WITH_DEFAULT, !CONFIG_KEY_WITH_DEFAULT_VALUE)
                        .build();

        boolean valueOrDefault = configuration.getValueOrDefault(
                CONFIG_KEY_WITH_DEFAULT, !CONFIG_KEY_WITH_DEFAULT_VALUE);

        assertThat(valueOrDefault).isEqualTo(!CONFIG_KEY_WITH_DEFAULT_VALUE);
    }
}
