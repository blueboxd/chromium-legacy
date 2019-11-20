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

package com.google.android.libraries.feed.common.testing;

import static com.google.common.truth.Fact.fact;
import static com.google.common.truth.Fact.simpleFact;
import static com.google.common.truth.Truth.assertAbout;

import com.google.android.libraries.feed.common.testing.RunnableSubject.ThrowingRunnable;
import com.google.common.truth.FailureMetadata;
import com.google.common.truth.Subject;
import com.google.common.truth.ThrowableSubject;

/** A Truth subject for testing exceptions. */
@SuppressWarnings("nullness")
public final class RunnableSubject extends Subject {
  private static final Subject.Factory<RunnableSubject, ThrowingRunnable> RUNNABLE_SUBJECT_FACTORY =
      RunnableSubject::new;

  /** Gets the subject factory of {@link RunnableSubject}. */
  public static Subject.Factory<RunnableSubject, ThrowingRunnable> runnables() {
    return RUNNABLE_SUBJECT_FACTORY;
  }

  private final ThrowingRunnable actual;

  public RunnableSubject(FailureMetadata failureMetadata, ThrowingRunnable runnable) {
    super(failureMetadata, runnable);
    this.actual = runnable;
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
   * <p>Note: This disambiguated method only exists because just 'assertThat' doesn't work when both
   * RunnableSubject.assertThat and Truth.assertThat are imported statically. There's a type
   * inference clash.
   */
  public static RunnableSubject assertThatRunnable(ThrowingRunnable runnable) {
    return assertAbout(RUNNABLE_SUBJECT_FACTORY).that(runnable);
  }

  @SuppressWarnings("unchecked")
  private <E extends Throwable> E runAndCaptureOrFail(Class<E> clazz) {
    if (actual == null) {
      failWithoutActual(
          fact("expected to throw", clazz.getName()),
          simpleFact("but didn't run because it's null"));
      return null;
    }

    try {
      actual.run();
    } catch (Throwable ex) {
      if (!clazz.isInstance(ex)) {
        check("thrownException()").that(ex).isInstanceOf(clazz); // fails
        return null;
      }
      return (E) ex;
    }

    failWithoutActual(
        fact("expected to throw", clazz.getName()),
        simpleFact("but ran to completion"),
        fact("runnable was", actual));
    return null;
  }

  public <E extends Throwable> ThrowsThenClause<E> throwsAnExceptionOfType(Class<E> clazz) {
    return new ThrowsThenClause<>(runAndCaptureOrFail(clazz), "thrownException()");
  }

  /**
   * Just a fluent class prompting you to type ".that" before asserting things about the exception.
   */
  public class ThrowsThenClause<E extends Throwable> {
    private final E throwable;
    private final String description;

    private ThrowsThenClause(E throwable, String description) {
      this.throwable = throwable;
      this.description = description;
    }

    public ThrowableSubject that() {
      return check(description).that(throwable);
    }

    public <C extends Throwable> ThrowsThenClause<C> causedByAnExceptionOfType(Class<C> clazz) {
      if (!clazz.isInstance(throwable.getCause())) {
        that().hasCauseThat().isInstanceOf(clazz); // fails
        return null;
      }

      @SuppressWarnings("unchecked")
      C cause = (C) throwable.getCause();

      return new ThrowsThenClause<>(cause, description + ".getCause()");
    }

    public void causedBy(Throwable cause) {
      that().hasCauseThat().isSameInstanceAs(cause);
    }

    public E getCaught() {
      return throwable;
    }
  }

  /**
   * Runnable which is able to throw a {@link Throwable}. Used for subject in order to test that a
   * block of code actually throws an appropriate exception.
   */
  public interface ThrowingRunnable {
    void run() throws Throwable;
  }
}
