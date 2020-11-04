// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import org.hamcrest.Matcher;

/**
 * Provides a means for validating whether some condition/criteria has been met.
 * <p>
 * See {@link CriteriaHelper} for usage guidelines.
 */
public final class Criteria {
    private Criteria() {}

    /**
     * Validates that a expected condition has been met, and throws an
     * {@link CriteriaNotSatisfiedException} if not.
     *
     * @param <T> The type of value whose being tested.
     * @param actual The actual value being tested.
     * @param matcher Determines if the current value matches the desired expectation.
     */
    public static <T> void checkThat(T actual, Matcher<T> matcher) {
        org.chromium.base.test.util.Criteria.checkThat(actual, matcher);
    }

    /**
     * Validates that a expected condition has been met, and throws an
     * {@link CriteriaNotSatisfiedException} if not.
     *
     * @param <T> The type of value whose being tested.
     * @param reason Additional reason description for the failure.
     * @param actual The actual value being tested.
     * @param matcher Determines if the current value matches the desired expectation.
     */
    public static <T> void checkThat(String reason, T actual, Matcher<T> matcher) {
        org.chromium.base.test.util.Criteria.checkThat(reason, actual, matcher);
    }
}
