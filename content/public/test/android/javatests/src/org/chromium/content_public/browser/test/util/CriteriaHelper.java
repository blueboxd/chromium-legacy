// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import java.util.concurrent.Callable;

/**
 * This class is in the process of being moved to base/. Please use that version instead.
 *
 * Helper methods for creating and managing criteria.
 *
 * <p>
 * If possible, use callbacks or testing delegates instead of criteria as they
 * do not introduce any polling delays.  Should only use criteria if no suitable
 * other approach exists.
 *
 * <p>
 * The Runnable variation of the CriteriaHelper methods allows a flexible way of verifying any
 * number of conditions are met prior to proceeding.
 *
 * <pre>
 * Example:
 * <code>
 * private void verifyMenuShown() {
 *     CriteriaHelper.pollUiThread(() -> {
 *         Criteria.checkThat("App menu was null", getActivity().getAppMenuHandler(),
 *                 Matchers.notNullValue());
 *         Criteria.checkThat("App menu was not shown",
 *                 getActivity().getAppMenuHandler().isAppMenuShowing(), Matchers.is(true));
 *     });
 * }
 * </code>
 * </pre>
 *
 * <p>
 * To verify simple conditions, the Callback variation can be less verbose.
 *
 * <pre>
 * Example:
 * <code>
 * private void assertMenuShown() {
 *     CriteriaHelper.pollUiThread(() -> getActivity().getAppMenuHandler().isAppMenuShowing(),
 *             "App menu was not shown");
 * }
 * </code>
 * </pre>
 */
public class CriteriaHelper {
    /** The default maximum time to wait for a criteria to become valid. */
    public static final long DEFAULT_MAX_TIME_TO_POLL = 3000L;
    /** The default polling interval to wait between checking for a satisfied criteria. */
    public static final long DEFAULT_POLLING_INTERVAL = 50;

    private static final long DEFAULT_JUNIT_MAX_TIME_TO_POLL = 1000;
    private static final long DEFAULT_JUNIT_POLLING_INTERVAL = 1;

    /**
     * Checks whether the given Runnable completes without exception at a given interval, until
     * either the Runnable successfully completes, or the maxTimeoutMs number of ms has elapsed.
     *
     * <p>
     * This evaluates the Criteria on the Instrumentation thread, which more often than not is not
     * correct in an InstrumentationTest. Use
     * {@link #pollUiThread(Runnable, long, long)} instead.
     *
     * @param criteria The Runnable that will be attempted.
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     *                     before timeout.
     * @param checkIntervalMs The number of ms between checks.
     */
    public static void pollInstrumentationThread(
            Runnable criteria, long maxTimeoutMs, long checkIntervalMs) {
        org.chromium.base.test.util.CriteriaHelper.pollInstrumentationThread(
                criteria, maxTimeoutMs, checkIntervalMs);
    }

    /**
     * Checks whether the given Runnable completes without exception at the default interval.
     *
     * <p>
     * This evaluates the Runnable on the test thread, which more often than not is not correct
     * in an InstrumentationTest.  Use {@link #pollUiThread(Runnable)} instead.
     *
     * @param criteria The Runnable that will be attempted.
     *
     * @see #pollInstrumentationThread(Criteria, long, long)
     */
    public static void pollInstrumentationThread(Runnable criteria) {
        org.chromium.base.test.util.CriteriaHelper.pollInstrumentationThread(criteria);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied at a given interval, until either
     * the criteria is satisfied, or the specified maxTimeoutMs number of ms has elapsed.
     *
     * <p>
     * This evaluates the Callable<Boolean> on the test thread, which more often than not is not
     * correct in an InstrumentationTest.  Use {@link #pollUiThread(Callable)} instead.
     *
     * @param criteria The Callable<Boolean> that will be checked.
     * @param failureReason The static failure reason
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     *                     before timeout.
     * @param checkIntervalMs The number of ms between checks.
     */
    public static void pollInstrumentationThread(final Callable<Boolean> criteria,
            String failureReason, long maxTimeoutMs, long checkIntervalMs) {
        org.chromium.base.test.util.CriteriaHelper.pollInstrumentationThread(
                criteria, failureReason, maxTimeoutMs, checkIntervalMs);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied at a given interval, until either
     * the criteria is satisfied, or the specified maxTimeoutMs number of ms has elapsed.
     *
     * <p>
     * This evaluates the Callable<Boolean> on the test thread, which more often than not is not
     * correct in an InstrumentationTest.  Use {@link #pollUiThread(Callable)} instead.
     *
     * @param criteria The Callable<Boolean> that will be checked.
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     *                     before timeout.
     * @param checkIntervalMs The number of ms between checks.
     */
    public static void pollInstrumentationThread(
            final Callable<Boolean> criteria, long maxTimeoutMs, long checkIntervalMs) {
        org.chromium.base.test.util.CriteriaHelper.pollInstrumentationThread(
                criteria, maxTimeoutMs, checkIntervalMs);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied polling at a default interval.
     *
     * <p>
     * This evaluates the Callable<Boolean> on the test thread, which more often than not is not
     * correct in an InstrumentationTest.  Use {@link #pollUiThread(Callable)} instead.
     *
     * @param criteria The Callable<Boolean> that will be checked.
     * @param failureReason The static failure reason
     */
    public static void pollInstrumentationThread(Callable<Boolean> criteria, String failureReason) {
        org.chromium.base.test.util.CriteriaHelper.pollInstrumentationThread(
                criteria, failureReason);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied polling at a default interval.
     *
     * <p>
     * This evaluates the Callable<Boolean> on the test thread, which more often than not is not
     * correct in an InstrumentationTest.  Use {@link #pollUiThread(Callable)} instead.
     *
     * @param criteria The Callable<Boolean> that will be checked.
     */
    public static void pollInstrumentationThread(Callable<Boolean> criteria) {
        org.chromium.base.test.util.CriteriaHelper.pollInstrumentationThread(criteria);
    }

    /**
     * Checks whether the given Runnable completes without exception at a given interval on the UI
     * thread, until either the Runnable successfully completes, or the maxTimeoutMs number of ms
     * has elapsed.
     *
     * @param criteria The Runnable that will be attempted.
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     *                     before timeout.
     * @param checkIntervalMs The number of ms between checks.
     *
     * @see #pollInstrumentationThread(Runnable)
     */
    public static void pollUiThread(
            final Runnable criteria, long maxTimeoutMs, long checkIntervalMs) {
        org.chromium.base.test.util.CriteriaHelper.pollUiThread(
                criteria, maxTimeoutMs, checkIntervalMs);
    }

    /**
     * Checks whether the given Runnable completes without exception at the default interval on
     * the UI thread.
     * @param criteria The Runnable that will be attempted.
     *
     * @see #pollInstrumentationThread(Runnable)
     */
    public static void pollUiThread(final Runnable criteria) {
        org.chromium.base.test.util.CriteriaHelper.pollUiThread(criteria);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied polling at a given interval on the UI
     * thread, until either the criteria is satisfied, or the maxTimeoutMs number of ms has elapsed.
     *
     * @param criteria The Callable<Boolean> that will be checked.
     * @param failureReason The static failure reason
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     *                     before timeout.
     * @param checkIntervalMs The number of ms between checks.
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThread(final Callable<Boolean> criteria, String failureReason,
            long maxTimeoutMs, long checkIntervalMs) {
        org.chromium.base.test.util.CriteriaHelper.pollUiThread(
                criteria, failureReason, maxTimeoutMs, checkIntervalMs);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied polling at a given interval on the UI
     * thread, until either the criteria is satisfied, or the maxTimeoutMs number of ms has elapsed.
     *
     * @param criteria The Callable<Boolean> that will be checked.
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     *                     before timeout.
     * @param checkIntervalMs The number of ms between checks.
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThread(
            final Callable<Boolean> criteria, long maxTimeoutMs, long checkIntervalMs) {
        org.chromium.base.test.util.CriteriaHelper.pollUiThread(
                criteria, maxTimeoutMs, checkIntervalMs);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied polling at a default interval on the
     * UI thread. A static failure reason is given.
     * @param criteria The Callable<Boolean> that will be checked.
     * @param failureReason The static failure reason
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThread(final Callable<Boolean> criteria, String failureReason) {
        org.chromium.base.test.util.CriteriaHelper.pollUiThread(criteria, failureReason);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied polling at a default interval on the
     * UI thread.
     * @param criteria The Callable<Boolean> that will be checked.
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThread(final Callable<Boolean> criteria) {
        org.chromium.base.test.util.CriteriaHelper.pollUiThread(criteria);
    }

    /**
     * Checks whether the given Runnable completes without exception at a given interval on the UI
     * thread, until either the Runnable successfully completes, or the maxTimeoutMs number of ms
     * has elapsed.
     * This call will nest the Looper in order to wait for the Runnable to complete.
     *
     * @param criteria The Runnable that will be attempted.
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     *                     before timeout.
     * @param checkIntervalMs The number of ms between checks.
     *
     * @see #pollInstrumentationThread(Runnable)
     */
    public static void pollUiThreadNested(
            Runnable criteria, long maxTimeoutMs, long checkIntervalMs) {
        org.chromium.base.test.util.CriteriaHelper.pollUiThreadNested(
                criteria, maxTimeoutMs, checkIntervalMs);
    }

    /**
     * Checks whether the given Runnable is satisfied polling at a given interval on the UI
     * thread, until either the criteria is satisfied, or the maxTimeoutMs number of ms has elapsed.
     * This call will nest the Looper in order to wait for the Criteria to be satisfied.
     *
     * @param criteria The Callable<Boolean> that will be checked.
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     *                     before timeout.
     * @param checkIntervalMs The number of ms between checks.
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThreadNested(
            final Callable<Boolean> criteria, long maxTimeoutMs, long checkIntervalMs) {
        org.chromium.base.test.util.CriteriaHelper.pollUiThreadNested(
                criteria, maxTimeoutMs, checkIntervalMs);
    }

    /**
     * Checks whether the given Runnable completes without exception at the default interval on
     * the UI thread. This call will nest the Looper in order to wait for the Runnable to complete.
     * @param criteria The Runnable that will be attempted.
     *
     * @see #pollInstrumentationThread(Runnable)
     */
    public static void pollUiThreadNested(final Runnable criteria) {
        org.chromium.base.test.util.CriteriaHelper.pollUiThreadNested(criteria);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied polling at a default interval on the
     * UI thread. This call will nest the Looper in order to wait for the Criteria to be satisfied.
     * @param criteria The Callable<Boolean> that will be checked.
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThreadNested(final Callable<Boolean> criteria) {
        org.chromium.base.test.util.CriteriaHelper.pollUiThreadNested(criteria);
    }

    /**
     * Sleeps the JUnit UI thread to wait on the condition. The condition must be met by a
     * background thread that does not block on the UI thread.
     *
     * @param criteria The Callable<Boolean> that will be checked.
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThreadForJUnit(final Callable<Boolean> criteria) {
        org.chromium.base.test.util.CriteriaHelper.pollUiThreadForJUnit(criteria);
    }

    /**
     * Sleeps the JUnit UI thread to wait on the criteria. The criteria must be met by a
     * background thread that does not block on the UI thread.
     *
     * @param criteria The Runnable that will be attempted.
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThreadForJUnit(final Runnable criteria) {
        org.chromium.base.test.util.CriteriaHelper.pollUiThreadForJUnit(criteria);
    }
}
