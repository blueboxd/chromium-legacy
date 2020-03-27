// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.StringDescription;

import java.util.concurrent.Callable;

/**
 * Criteria that supports the Matcher assertion patterns.
 *
 * @param <T> The type of value being tested.
 */
public class MatcherCriteria<T> extends Criteria {
    private final Matcher<? super T> mMatcher;
    private final Callable<T> mActualValueCallable;

    /**
     * Construct the MatcherCriteria with a specific matcher and a means of fetching the current
     * actual value.
     * @param actualValueCallable Provides access to the current value.
     * @param matcher Determines if the current value matches the desired expectation.
     */
    public MatcherCriteria(Callable<T> actualValueCallable, Matcher<? super T> matcher) {
        mActualValueCallable = actualValueCallable;
        mMatcher = matcher;
    }

    @Override
    public final boolean isSatisfied() {
        T actualValue;
        try {
            actualValue = mActualValueCallable.call();
        } catch (Exception ex) {
            updateFailureReason("Exception occurred: " + ex.getMessage());
            ex.printStackTrace();
            return false;
        }

        if (mMatcher.matches(actualValue)) return true;

        Description description = new StringDescription();
        description.appendText("Expected: ")
                .appendDescriptionOf(mMatcher)
                .appendText(System.lineSeparator())
                .appendText("     but: ");
        mMatcher.describeMismatch(actualValue, description);
        updateFailureReason(description.toString());
        return false;
    }
}
