// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import android.text.TextUtils;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.StringDescription;

/**
 * Provides a means for validating whether some condition/criteria has been met.
 * <p>
 * See {@link CriteriaHelper} for usage guidelines.
 */
public abstract class Criteria {
    private String mFailureReason;

    /**
     * Constructs a Criteria with a default failure message.
     */
    public Criteria() {
        this("Criteria not met in allotted time.");
    }

    /**
     * Constructs a Criteria with an explicit message to be shown on failure.
     * @param failureReason The failure reason to be shown.
     */
    public Criteria(String failureReason) {
        if (failureReason != null) mFailureReason = failureReason;
    }

    /**
     * @return Whether the criteria this is testing has been satisfied.
     */
    public abstract boolean isSatisfied();

    /**
     * @return The failure message that will be logged if the criteria is not satisfied within
     *         the specified time range.
     */
    public String getFailureReason() {
        return mFailureReason;
    }

    /**
     * Updates the message to displayed if this criteria does not succeed in the allotted time.  For
     * correctness, you should be updating this in {@link #isSatisfied()} to ensure the error state
     * is the same that you last checked.
     *
     * @param reason The failure reason to be shown.
     */
    public void updateFailureReason(String reason) {
        mFailureReason = reason;
    }

    /**
     * Validates that a expected condition has been met, and throws an
     * {@link CriteriaNotSatisfiedException} if not.
     *
     * @param <T> The type of value whose being tested.
     * @param actual The actual value being tested.
     * @param matcher Determines if the current value matches the desired expectation.
     */
    public static <T> void checkThat(T actual, Matcher<T> matcher) {
        checkThat("", actual, matcher);
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
        if (matcher.matches(actual)) return;
        Description description = new StringDescription();
        if (!TextUtils.isEmpty(reason)) {
            description.appendText(reason).appendText(System.lineSeparator());
        }
        description.appendText("Expected: ")
                .appendDescriptionOf(matcher)
                .appendText(System.lineSeparator())
                .appendText("     but: ");
        matcher.describeMismatch(actual, description);
        throw new CriteriaNotSatisfiedException(description.toString());
    }
}
