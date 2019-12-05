// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.testing;

import static com.google.common.truth.ExpectFailure.assertThat;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;
import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.runnables;

import com.google.common.truth.ExpectFailure;
import com.google.common.truth.Truth;

import org.junit.ComparisonFailure;
import org.junit.Ignore;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.ThrowingRunnable;

import java.io.IOException;
import java.util.concurrent.ExecutionException;

/** Unit tests for {@link RunnableSubject}. */
@RunWith(JUnit4.class)
public class RunnableSubjectTest {
    @Rule
    public final ExpectFailure expectFailure = new ExpectFailure();

    @Test
    public void runToCompletion() throws Throwable {
        ThrowingRunnable run = mock(ThrowingRunnable.class);

        expectFailure.whenTesting()
                .about(runnables())
                .that(run)
                .throwsAnExceptionOfType(RuntimeException.class);

        verify(run).run();
        AssertionError failure = expectFailure.getFailure();
        assertThat(failure).factValue("expected to throw").isEqualTo("java.lang.RuntimeException");
        assertThat(failure).factKeys().contains("but ran to completion");
        verifyNoMoreInteractions(run);
    }

    @Test
    public void nullSubjectFailsEvenWhenExpectingNullRefException() {
        expectFailure.whenTesting()
                .about(runnables())
                .that(null)
                .throwsAnExceptionOfType(NullPointerException.class);

        AssertionError failure = expectFailure.getFailure();
        assertThat(failure)
                .factValue("expected to throw")
                .isEqualTo("java.lang.NullPointerException");
        assertThat(failure).factKeys().contains("but didn't run because it's null");
    }

    @Test
    public void wrongException() {
        expectFailure.whenTesting()
                .about(runnables())
                .that(() -> { throw new IllegalArgumentException("a"); })
                .throwsAnExceptionOfType(NullPointerException.class);

        AssertionError failure = expectFailure.getFailure();
        assertThat(failure).factValue("value of").isEqualTo("runnable.thrownException()");
        assertThat(failure)
                .factValue("expected instance of")
                .isEqualTo("java.lang.NullPointerException");
        assertThat(failure)
                .factValue("but was instance of")
                .isEqualTo("java.lang.IllegalArgumentException");
        assertThat(failure)
                .factValue("with value")
                .isEqualTo("java.lang.IllegalArgumentException: a");
    }

    @Test
    public void correctException() {
        IllegalArgumentException ex = new IllegalArgumentException("b");

        Truth.assertAbout(runnables())
                .that(() -> { throw ex; })
                .throwsAnExceptionOfType(IllegalArgumentException.class)
                .that()
                .isSameInstanceAs(ex);
    }

    @Test
    @Ignore("crbug.com/1024945 java.lang.NoClassDefFoundError: difflib/DiffUtils")
    public void wrongMessage() {
        expectFailure.whenTesting()
                .about(runnables())
                .that(() -> { throw new IOException("Wrong message!"); })
                .throwsAnExceptionOfType(IOException.class)
                .that()
                .hasMessageThat()
                .isEqualTo("Expected message.");

        ComparisonFailure failure = (ComparisonFailure) expectFailure.getFailure();
        assertThat(failure.getExpected()).isEqualTo("Expected message.");
        assertThat(failure.getActual()).isEqualTo("Wrong message!");
    }

    @Test
    public void correctMessage() {
        String expectedMessage = "Expected message.";

        Truth.assertAbout(runnables())
                .that(() -> { throw new IOException(expectedMessage); })
                .throwsAnExceptionOfType(IOException.class)
                .that()
                .hasMessageThat()
                .isEqualTo(expectedMessage);
    }

    @Test
    public void getCaught() {
        IllegalArgumentException cause = new IllegalArgumentException("boo");
        IOException ex = new IOException(cause);

        IOException caught = RunnableSubject.assertThat(() -> { throw ex; })
                                     .throwsAnExceptionOfType(IOException.class)
                                     .getCaught();
        assertThat(caught).isSameInstanceAs(ex);
    }

    @Test
    public void wrongCauseType() {
        IllegalArgumentException cause = new IllegalArgumentException("boo");
        IOException ex = new IOException(cause);
        expectFailure.whenTesting()
                .about(runnables())
                .that(() -> { throw ex; })
                .throwsAnExceptionOfType(IOException.class)
                .causedByAnExceptionOfType(ExecutionException.class);
        AssertionError failure = expectFailure.getFailure();
        assertThat(failure)
                .factValue("value of")
                .isEqualTo("runnable.thrownException().getCause()");
        assertThat(failure)
                .factValue("expected instance of")
                .isEqualTo("java.util.concurrent.ExecutionException");
        assertThat(failure)
                .factValue("but was instance of")
                .isEqualTo("java.lang.IllegalArgumentException");
        assertThat(failure)
                .factValue("with value")
                .isEqualTo("java.lang.IllegalArgumentException: boo");
    }

    @Test
    public void correctCauseType() {
        IllegalArgumentException cause = new IllegalArgumentException("boo");
        IOException ex = new IOException(cause);

        Truth.assertAbout(runnables())
                .that(() -> { throw ex; })
                .throwsAnExceptionOfType(IOException.class)
                .causedByAnExceptionOfType(IllegalArgumentException.class)
                .that()
                .hasMessageThat()
                .isEqualTo("boo");
    }

    @Test
    public void causedByCorrect() {
        IOException cause = new IOException();
        IllegalArgumentException ex = new IllegalArgumentException(cause);

        Truth.assertAbout(runnables())
                .that(() -> { throw ex; })
                .throwsAnExceptionOfType(IllegalArgumentException.class)
                .causedBy(cause);
    }

    @Test
    public void causedByWrong() {
        IOException cause = new IOException();
        IllegalArgumentException ex = new IllegalArgumentException(cause);

        expectFailure.whenTesting()
                .about(runnables())
                .that(() -> { throw ex; })
                .throwsAnExceptionOfType(IllegalArgumentException.class)
                .causedBy(new IOException());
    }

    @Test
    public void exampleUsage() {
        assertThatRunnable(() -> { throw new IOException("boo!"); })
                .throwsAnExceptionOfType(IOException.class)
                .that()
                .hasMessageThat()
                .isEqualTo("boo!");

        RunnableSubject.assertThat(() -> { throw new IllegalArgumentException("oh no"); })
                .throwsAnExceptionOfType(IllegalArgumentException.class)
                .that()
                .hasMessageThat()
                .isEqualTo("oh no");

        RunnableSubject
                .assertThat(() -> {
                    throw new IllegalArgumentException(new RuntimeException("glitch"));
                })
                .throwsAnExceptionOfType(IllegalArgumentException.class)
                .causedByAnExceptionOfType(RuntimeException.class)
                .that()
                .hasMessageThat()
                .isEqualTo("glitch");

        // Recursive self-verification.
        RunnableSubject
                .assertThat(()
                                    -> RunnableSubject.assertThat(() -> {}).throwsAnExceptionOfType(
                                            RuntimeException.class))
                .throwsAnExceptionOfType(AssertionError.class);
    }
}
