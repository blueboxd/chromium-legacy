// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.hamcrest.CoreMatchers.containsString;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.task.TaskTraits.THREAD_POOL;
import static org.chromium.content_public.browser.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.io.PrintWriter;
import java.io.StringWriter;

/**
 * Tests for {@link CriteriaHelper}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class CriteriaHelperTest {
    private static final String ERROR_MESSAGE = "my special error message";

    @Rule
    public ExpectedException thrown = ExpectedException.none();

    @Test
    @MediumTest
    public void testUiThread() {
        // Robolectric runs the test on UI thread.
        assertTrue(ThreadUtils.runningOnUiThread());
        // In Instrumented tests, the tests would be on instrumentation thread instead.
        // Emulate that behavior by posting the body of the test.
        PostTask.postTask(THREAD_POOL, () -> {
            assertFalse(ThreadUtils.runningOnUiThread());
            CriteriaHelper.pollUiThread(() -> assertTrue(ThreadUtils.runningOnUiThread()));
        });
    }

    @Test
    @MediumTest
    public void testInstrumentationThread() {
        // Robolectric runs the test on UI thread.
        assertTrue(ThreadUtils.runningOnUiThread());
        // In Instrumented tests, the tests would be on instrumentation thread instead.
        // Emulate that behavior by posting the body of the test.
        PostTask.postTask(THREAD_POOL, () -> {
            assertFalse(ThreadUtils.runningOnUiThread());
            CriteriaHelper.pollInstrumentationThread(
                    () -> assertFalse(ThreadUtils.runningOnUiThread()));
        });
    }

    @Test
    @MediumTest
    public void testPass_Runnable_UiThread() {
        CriteriaHelper.pollUiThread(() -> {});
    }

    @Test
    @MediumTest
    public void testPass_Runnable_InstrumentationThread() {
        CriteriaHelper.pollInstrumentationThread(() -> {});
    }

    @Test
    @MediumTest
    public void testPass_Callable_UiThread() {
        CriteriaHelper.pollUiThread(() -> true);
    }

    @Test
    @MediumTest
    public void testPass_Callable_InstrumentationThread() {
        CriteriaHelper.pollInstrumentationThread(() -> true);
    }

    @Test
    @MediumTest
    public void testPass_Criteria_UiThread() {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return true;
            }
        });
    }

    @Test
    @MediumTest
    public void testPass_Criteria_InstrumentationThread() {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return true;
            }
        });
    }

    @Test
    @MediumTest
    public void testThrow_Runnable_UiThread() {
        thrown.expect(AssertionError.class);
        CriteriaHelper.pollUiThread((Runnable) Assert::fail, 0, DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testThrow_Runnable_InstrumentationThread() {
        thrown.expect(AssertionError.class);
        CriteriaHelper.pollInstrumentationThread(
                (Runnable) Assert::fail, 0, DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testThrow_Callable_UiThread() {
        thrown.expect(AssertionError.class);
        CriteriaHelper.pollUiThread(() -> false, 0, DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testThrow_Callable_InstrumentationThread() {
        thrown.expect(AssertionError.class);
        CriteriaHelper.pollInstrumentationThread(() -> false, 0, DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testThrow_Criteria_UiThread() {
        thrown.expect(AssertionError.class);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return false;
            }
        }, 0, DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testThrow_Criteria_InstrumentationThread() {
        thrown.expect(AssertionError.class);
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return false;
            }
        }, 0, DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testMessage_Runnable_UiThread() {
        thrown.expectMessage(ERROR_MESSAGE);
        CriteriaHelper.pollUiThread(() -> Assert.fail(ERROR_MESSAGE), 0, DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testMessage_Runnable_InstrumentationThread() {
        thrown.expectMessage(ERROR_MESSAGE);
        CriteriaHelper.pollInstrumentationThread(
                () -> Assert.fail(ERROR_MESSAGE), 0, DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testMessage_Callback_UiThread() {
        thrown.expectMessage(ERROR_MESSAGE);
        CriteriaHelper.pollUiThread(() -> false, ERROR_MESSAGE, 0, DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testMessage_Callback_InstrumentationThread() {
        thrown.expectMessage(ERROR_MESSAGE);
        CriteriaHelper.pollInstrumentationThread(
                () -> false, ERROR_MESSAGE, 0, DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testMessage_Criteria_UiThread() {
        thrown.expectMessage(ERROR_MESSAGE);
        CriteriaHelper.pollUiThread(new Criteria(ERROR_MESSAGE) {
            @Override
            public boolean isSatisfied() {
                return false;
            }
        }, 0, DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testMessage_Criteria_InstrumentationThread() {
        thrown.expectMessage(ERROR_MESSAGE);
        CriteriaHelper.pollInstrumentationThread(new Criteria(ERROR_MESSAGE) {
            @Override
            public boolean isSatisfied() {
                return false;
            }
        }, 0, DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testMessage_Criteria_dynamic_UiThread() {
        thrown.expectMessage(ERROR_MESSAGE);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                updateFailureReason(ERROR_MESSAGE);
                return false;
            }
        }, 0, DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testMessage_Criteria_dynamic_InstrumentationThread() {
        thrown.expectMessage(ERROR_MESSAGE);
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                updateFailureReason(ERROR_MESSAGE);
                return false;
            }
        }, 0, DEFAULT_POLLING_INTERVAL);
    }

    private String getStackTrace(AssertionError e) {
        StringWriter sw = new StringWriter();
        e.printStackTrace(new PrintWriter(sw));
        return sw.toString();
    }

    @Test
    @MediumTest
    public void testStack_Runnable_UiThread() {
        try {
            CriteriaHelper.pollUiThread((Runnable) Assert::fail, 0, DEFAULT_POLLING_INTERVAL);
        } catch (AssertionError e) {
            assertThat(getStackTrace(e),
                    containsString("CriteriaHelperTest.testStack_Runnable_UiThread("));
            return;
        }
        Assert.fail();
    }

    @Test
    @MediumTest
    public void testStack_Runnable_InstrumentationThread() {
        try {
            CriteriaHelper.pollInstrumentationThread(
                    (Runnable) Assert::fail, 0, DEFAULT_POLLING_INTERVAL);
        } catch (AssertionError e) {
            assertThat(getStackTrace(e),
                    containsString("CriteriaHelperTest.testStack_Runnable_InstrumentationThread("));
            return;
        }
        Assert.fail();
    }

    @Test
    @MediumTest
    public void testStack_Callable_UiThread() {
        try {
            CriteriaHelper.pollUiThread(() -> false, 0, DEFAULT_POLLING_INTERVAL);
        } catch (AssertionError e) {
            assertThat(getStackTrace(e),
                    containsString("CriteriaHelperTest.testStack_Callable_UiThread("));
            return;
        }
        Assert.fail();
    }

    @Test
    @MediumTest
    public void testStack_Callable_InstrumentationThread() {
        try {
            CriteriaHelper.pollInstrumentationThread(() -> false, 0, DEFAULT_POLLING_INTERVAL);
        } catch (AssertionError e) {
            assertThat(getStackTrace(e),
                    containsString("CriteriaHelperTest.testStack_Callable_InstrumentationThread("));
            return;
        }
        Assert.fail();
    }

    @Test
    @MediumTest
    public void testStack_Criteria_UiThread() {
        try {
            CriteriaHelper.pollUiThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return false;
                }
            }, 0, DEFAULT_POLLING_INTERVAL);
        } catch (AssertionError e) {
            assertThat(getStackTrace(e),
                    containsString("CriteriaHelperTest.testStack_Criteria_UiThread("));
            return;
        }
        Assert.fail();
    }
    @Test
    @MediumTest
    public void testStack_Criteria_InstrumentationThread() {
        try {
            CriteriaHelper.pollInstrumentationThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return false;
                }
            }, 0, DEFAULT_POLLING_INTERVAL);
        } catch (AssertionError e) {
            assertThat(getStackTrace(e),
                    containsString("CriteriaHelperTest.testStack_Criteria_InstrumentationThread("));
            return;
        }
        Assert.fail();
    }
}
