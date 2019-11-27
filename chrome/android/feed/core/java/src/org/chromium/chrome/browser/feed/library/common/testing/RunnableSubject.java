// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.testing;

import static com.google.common.truth.Fact.fact;
import static com.google.common.truth.Fact.simpleFact;
import static com.google.common.truth.Truth.assertAbout;

import com.google.common.truth.FailureMetadata;
import com.google.common.truth.Subject;
import com.google.common.truth.ThrowableSubject;

import org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.ThrowingRunnable;

/** A Truth subject for testing exceptions. */
@SuppressWarnings("nullness")
public final class RunnableSubject extends Subject {
    private static final Subject
            .Factory<RunnableSubject, ThrowingRunnable> RUNNABLE_SUBJECT_FACTORY =
            RunnableSubject::new;

    /** Gets the subject factory of {@link RunnableSubject}. */
    public static Subject.Factory<RunnableSubject, ThrowingRunnable> runnables() {
        return RUNNABLE_SUBJECT_FACTORY;
    }

    private final ThrowingRunnable mActual;

    public RunnableSubject(FailureMetadata failureMetadata, ThrowingRunnable runnable) {
        super(failureMetadata, runnable);
        this.mActual = runnable;
    }

    /**
     * Note: Doesn't work when both this method, and Truth.assertThat, are imported statically.
     * There's a type inference clash.
     */
    public static RunnableSubject assertThat(ThrowingRunnable runnable) {
        return assertAbout(RUNNABLE_SUBJECT_FACTORY).that(runnable);
    }

    /**
     * Wraps a RunnableSubject around the given runnable.
     *
     * <p>Note: This disambiguated method only exists because just 'assertThat' doesn't work when
     * both RunnableSubject.assertThat and Truth.assertThat are imported statically. There's a type
     * inference clash.
     */
    public static RunnableSubject assertThatRunnable(ThrowingRunnable runnable) {
        return assertAbout(RUNNABLE_SUBJECT_FACTORY).that(runnable);
    }

    @SuppressWarnings("unchecked")
    private <E extends Throwable> E runAndCaptureOrFail(Class<E> clazz) {
        if (mActual == null) {
            failWithoutActual(fact("expected to throw", clazz.getName()),
                    simpleFact("but didn't run because it's null"));
            return null;
        }

        try {
            mActual.run();
        } catch (Throwable ex) {
            if (!clazz.isInstance(ex)) {
                check("thrownException()").that(ex).isInstanceOf(clazz); // fails
                return null;
            }
            return (E) ex;
        }

        failWithoutActual(fact("expected to throw", clazz.getName()),
                simpleFact("but ran to completion"), fact("runnable was", mActual));
        return null;
    }

    public <E extends Throwable> ThrowsThenClause<E> throwsAnExceptionOfType(Class<E> clazz) {
        return new ThrowsThenClause<>(runAndCaptureOrFail(clazz), "thrownException()");
    }

    /**
     * Just a fluent class prompting you to type ".that" before asserting things about the
     * exception.
     */
    public class ThrowsThenClause<E extends Throwable> {
        private final E mThrowable;
        private final String mDescription;

        private ThrowsThenClause(E throwable, String description) {
            this.mThrowable = throwable;
            this.mDescription = description;
        }

        public ThrowableSubject that() {
            return check(mDescription).that(mThrowable);
        }

        public <C extends Throwable> ThrowsThenClause<C> causedByAnExceptionOfType(Class<C> clazz) {
            if (!clazz.isInstance(mThrowable.getCause())) {
                that().hasCauseThat().isInstanceOf(clazz); // fails
                return null;
            }

            @SuppressWarnings("unchecked")
            C cause = (C) mThrowable.getCause();

            return new ThrowsThenClause<>(cause, mDescription + ".getCause()");
        }

        public void causedBy(Throwable cause) {
            that().hasCauseThat().isSameInstanceAs(cause);
        }

        public E getCaught() {
            return mThrowable;
        }
    }

    /**
     * Runnable which is able to throw a {@link Throwable}. Used for subject in order to test that a
     * block of code actually throws an appropriate exception.
     */
    public interface ThrowingRunnable { void run() throws Throwable; }
}
