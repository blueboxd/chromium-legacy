// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.support.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.regex.Pattern;

/**
 * Test class that verifies that {@link ChromePreferenceKeys} conforms to its constraints:
 * - No keys are both in [keys in use] and in [deprecated keys].
 * - All keys follow the format "Chrome.[Feature].[Key]"
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromePreferenceKeysTest {
    /**
     * The important test: verify that keys in {@link ChromePreferenceKeys} are not reused.
     *
     * If a key was used in the past but is not used anymore, it should be in [deprecated keys].
     * Adding the same key to [keys in use] will break this test to warn the developer.
     */
    @Test
    @SmallTest
    public void testKeysAreNotReused() {
        doTestKeysAreNotReused(ChromePreferenceKeys.createKeysInUse(),
                ChromePreferenceKeys.createDeprecatedKeysForTesting());
    }

    private void doTestKeysAreNotReused(List<String> usedList, List<String> deprecatedList) {
        // Check for duplicate keys in [keys in use].
        Set<String> usedSet = new HashSet<>(usedList);
        assertEquals(usedList.size(), usedSet.size());

        // Check for duplicate keys in [deprecated keys].
        Set<String> deprecatedSet = new HashSet<>(deprecatedList);
        assertEquals(deprecatedList.size(), deprecatedSet.size());

        // Check for keys in [deprecated keys] that are now also [keys in use]. This ensures no
        // deprecated keys are reused.
        Set<String> intersection = new HashSet<>(usedSet);
        intersection.retainAll(deprecatedSet);
        if (!intersection.isEmpty()) {
            fail("\"" + intersection.iterator().next()
                    + "\" is both in ChromePreferenceKeys' [keys in use] and in "
                    + "[deprecated keys]");
        }
    }

    // Below are tests to ensure that testKeysAreNotReused() works.

    @Test
    @SmallTest
    public void testReuseCheck_emptyLists() {
        doTestKeysAreNotReused(Collections.EMPTY_LIST, Collections.EMPTY_LIST);
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testReuseCheck_duplicateKey_used() {
        doTestKeysAreNotReused(Arrays.asList("UsedKey1", "UsedKey1"),
                Arrays.asList("DeprecatedKey1", "DeprecatedKey2"));
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testReuseCheck_duplicateKey_deprecated() {
        doTestKeysAreNotReused(Arrays.asList("UsedKey1", "UsedKey2"),
                Arrays.asList("DeprecatedKey1", "DeprecatedKey1"));
    }

    @Test
    @SmallTest
    public void testReuseCheck_noIntersection() {
        doTestKeysAreNotReused(Arrays.asList("UsedKey1", "UsedKey2"),
                Arrays.asList("DeprecatedKey1", "DeprecatedKey2"));
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testReuseCheck_intersection() {
        doTestKeysAreNotReused(Arrays.asList("UsedKey1", "ReusedKey"),
                Arrays.asList("ReusedKey", "DeprecatedKey1"));
    }

    /**
     * Test that the keys in use conform to the format:
     * "Chrome.[Feature].[Key]"
     *
     * Old constants are grandfathered in.
     */
    @Test
    @SmallTest
    public void testKeysConformToFormat() {
        doTestKeysConformToFormat(ChromePreferenceKeys.createKeysInUse(),
                ChromePreferenceKeys.createGrandfatheredFormatKeysForTesting());
    }

    private void doTestKeysConformToFormat(List<String> usedList, List<String> grandfathered) {
        Set<String> grandfatheredSet = new HashSet<>(grandfathered);
        String term = "([A-Z][a-z0-9]*)+";
        Pattern regex = Pattern.compile("Chrome\\." + term + "\\." + term + "(\\.\\*)?");

        for (String keyInUse : usedList) {
            if (grandfatheredSet.contains(keyInUse)) {
                continue;
            }

            assertTrue("\"" + keyInUse + "\" does not conform to format \"Chrome.[Feature].[Key]\"",
                    regex.matcher(keyInUse).matches());
        }
    }

    // Below are tests to ensure that doTestKeysConformToFormat() works.

    private static class TestFormatConstantsClass {
        static final String GRANDFATHERED_IN = "grandfathered_in";
        static final String NEW1 = "Chrome.FeatureOne.Key1";
        static final String NEW2 = "Chrome.Foo.Key";
        static final String BROKEN_PREFIX = "Chrom.Foo.Key";
        static final String MISSING_FEATURE = "Chrome..Key";
        static final String LOWERCASE_KEY = "Chrome.Foo.key";

        static final KeyPrefix PREFIX = new KeyPrefix("Chrome.FeatureOne.KeyPrefix1.*");
        static final KeyPrefix PREFIX_EXTRA_LEVEL =
                new KeyPrefix("Chrome.FeatureOne.KeyPrefix1.ExtraLevel.*");
        static final KeyPrefix PREFIX_MISSING_LEVEL = new KeyPrefix("Chrome.FeatureOne.*");
    }

    @Test
    @SmallTest
    public void testFormatCheck_correct() {
        doTestKeysConformToFormat(
                Arrays.asList(TestFormatConstantsClass.GRANDFATHERED_IN,
                        TestFormatConstantsClass.NEW1, TestFormatConstantsClass.NEW2),
                Arrays.asList(TestFormatConstantsClass.GRANDFATHERED_IN));
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testFormatCheck_invalidFormat() {
        doTestKeysConformToFormat(
                Arrays.asList(TestFormatConstantsClass.GRANDFATHERED_IN,
                        TestFormatConstantsClass.NEW1, TestFormatConstantsClass.NEW2),
                Collections.emptyList());
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testFormatCheck_brokenPrefix() {
        doTestKeysConformToFormat(
                Arrays.asList(TestFormatConstantsClass.GRANDFATHERED_IN,
                        TestFormatConstantsClass.NEW1, TestFormatConstantsClass.BROKEN_PREFIX),
                Arrays.asList(TestFormatConstantsClass.GRANDFATHERED_IN));
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testFormatCheck_MissingFeature() {
        doTestKeysConformToFormat(
                Arrays.asList(TestFormatConstantsClass.GRANDFATHERED_IN,
                        TestFormatConstantsClass.NEW1, TestFormatConstantsClass.MISSING_FEATURE),
                Arrays.asList(TestFormatConstantsClass.GRANDFATHERED_IN));
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testFormatCheck_LowercaseKey() {
        doTestKeysConformToFormat(
                Arrays.asList(TestFormatConstantsClass.GRANDFATHERED_IN,
                        TestFormatConstantsClass.NEW1, TestFormatConstantsClass.LOWERCASE_KEY),
                Arrays.asList(TestFormatConstantsClass.GRANDFATHERED_IN));
    }

    @Test
    @SmallTest
    public void testFormatCheck_prefixCorrect() {
        doTestKeysConformToFormat(
                Arrays.asList(TestFormatConstantsClass.PREFIX.pattern()), Collections.emptyList());
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testFormatCheck_prefixExtralevel() {
        doTestKeysConformToFormat(
                Arrays.asList(TestFormatConstantsClass.PREFIX_EXTRA_LEVEL.pattern()),
                Collections.emptyList());
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testFormatCheck_prefixMissingLevel() {
        doTestKeysConformToFormat(
                Arrays.asList(TestFormatConstantsClass.PREFIX_MISSING_LEVEL.pattern()),
                Collections.emptyList());
    }
}
