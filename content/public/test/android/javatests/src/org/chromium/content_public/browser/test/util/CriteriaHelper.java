// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.TimeoutTimer;

import java.util.concurrent.Callable;
import java.util.concurrent.atomic.AtomicReference;

/**
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
 *         Assert.assertNotNull("App menu was null", getActivity().getAppMenuHandler());
 *         Assert.assertTrue("App menu was not shown",
 *                 getActivity().getAppMenuHandler().isAppMenuShowing());
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
 *
 * <p>
 * The Criteria variation is deprecated and should be avoided in favor of using a Runnable.
 */
public class CriteriaHelper {
    /** The default maximum time to wait for a criteria to become valid. */
    public static final long DEFAULT_MAX_TIME_TO_POLL = 3000L;
    /** The default polling interval to wait between checking for a satisfied criteria. */
    public static final long DEFAULT_POLLING_INTERVAL = 50;

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
        AssertionError assertionError;
        try {
            criteria.run();
            return;
        } catch (AssertionError ae) {
            assertionError = ae;
        }
        TimeoutTimer timer = new TimeoutTimer(maxTimeoutMs);
        while (!timer.isTimedOut()) {
            try {
                Thread.sleep(checkIntervalMs);
            } catch (InterruptedException e) {
                // Catch the InterruptedException. If the exception occurs before maxTimeoutMs
                // and the criteria is not satisfied, the while loop will run again.
            }
            try {
                criteria.run();
                return;
            } catch (AssertionError ae) {
                assertionError = ae;
            }
        }
        throw assertionError;
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
        pollInstrumentationThread(criteria, DEFAULT_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Deprecated, use {@link #pollInstrumentationThread(Runnable, long, long)}.
     */
    public static void pollInstrumentationThread(
            Criteria criteria, long maxTimeoutMs, long checkIntervalMs) {
        pollInstrumentationThread(toAssertionRunnable(criteria), maxTimeoutMs, checkIntervalMs);
    }

    /**
     * Deprecated, use {@link #pollInstrumentationThread(Runnable)}.
     */
    public static void pollInstrumentationThread(Criteria criteria) {
        pollInstrumentationThread(criteria, DEFAULT_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);
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
        pollInstrumentationThread(
                toAssertionRunnable(criteria, failureReason), maxTimeoutMs, checkIntervalMs);
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
        pollInstrumentationThread(criteria, null, maxTimeoutMs, checkIntervalMs);
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
        pollInstrumentationThread(
                criteria, failureReason, DEFAULT_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);
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
        pollInstrumentationThread(criteria, null);
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
        pollInstrumentationThread(() -> {
            AtomicReference<Throwable> throwableRef = new AtomicReference<>();
            ThreadUtils.runOnUiThreadBlocking(() -> {
                try {
                    criteria.run();
                } catch (Throwable t) {
                    throwableRef.set(t);
                }
            });
            Throwable throwable = throwableRef.get();
            if (throwable != null) {
                if (throwable instanceof AssertionError) {
                    throw new AssertionError(throwable);
                } else if (throwable instanceof RuntimeException) {
                    throw (RuntimeException) throwable;
                } else {
                    throw new RuntimeException(throwable);
                }
            }
        }, maxTimeoutMs, checkIntervalMs);
    }

    /**
     * Checks whether the given Runnable completes without exception at the default interval on
     * the UI thread.
     * @param criteria The Runnable that will be attempted.
     *
     * @see #pollInstrumentationThread(Runnable)
     */
    public static void pollUiThread(final Runnable criteria) {
        pollUiThread(criteria, DEFAULT_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Deprecated, use {@link #pollUiThread(Runnable, long, long)}.
     */
    public static void pollUiThread(
            final Criteria criteria, long maxTimeoutMs, long checkIntervalMs) {
        pollUiThread(toAssertionRunnable(criteria), maxTimeoutMs, checkIntervalMs);
    }

    /**
     * Deprecated, use {@link #pollUiThread(Runnable)}.
     */
    public static void pollUiThread(final Criteria criteria) {
        pollUiThread(criteria, DEFAULT_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);
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
        pollUiThread(toAssertionRunnable(criteria, failureReason), maxTimeoutMs, checkIntervalMs);
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
        pollUiThread(criteria, null, maxTimeoutMs, checkIntervalMs);
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
        pollUiThread(criteria, failureReason, DEFAULT_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Checks whether the given Callable<Boolean> is satisfied polling at a default interval on the
     * UI thread.
     * @param criteria The Callable<Boolean> that will be checked.
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThread(final Callable<Boolean> criteria) {
        pollUiThread(criteria, null);
    }

    private static Runnable toAssertionRunnable(Callable<Boolean> criteria, String failureReason) {
        return () -> {
            boolean isSatisfied;
            try {
                isSatisfied = criteria.call();
            } catch (RuntimeException re) {
                throw re;
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
            Assert.assertTrue(failureReason, isSatisfied);
        };
    }

    private static Runnable toAssertionRunnable(Criteria criteria) {
        return () -> {
            boolean satisfied = criteria.isSatisfied();
            Assert.assertTrue(criteria.getFailureReason(), satisfied);
        };
    }
}
