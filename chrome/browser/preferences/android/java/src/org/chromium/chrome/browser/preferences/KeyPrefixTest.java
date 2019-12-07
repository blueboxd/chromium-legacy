// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import static org.junit.Assert.assertEquals;

import android.support.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for {@link KeyPrefix}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class KeyPrefixTest {
    @Test
    @SmallTest
    public void testSuccess_validPattern() {
        KeyPrefix prefix = new KeyPrefix("Chrome.Feature.KP.*");

        assertEquals(prefix.pattern(), "Chrome.Feature.KP.*");
        assertEquals(prefix.createKey("DynamicKey"), "Chrome.Feature.KP.DynamicKey");
        assertEquals(prefix.createKey("Level.DynamicKey"), "Chrome.Feature.KP.Level.DynamicKey");
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testError_missingPeriod() {
        new KeyPrefix("Chrome.Feature.KP");
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testError_missingStar() {
        new KeyPrefix("Chrome.Feature.KP.");
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testError_extraStar() {
        new KeyPrefix("Chrome.Feature.KP.**");
    }
}
