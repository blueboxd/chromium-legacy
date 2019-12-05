// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;

import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link SingleKeyRecyclerPool} */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SingleKeyRecyclerPoolTest {
    public static final TestRecyclerKey DEFAULT_KEY = new TestRecyclerKey("HappyKey");
    public static final TestRecyclerKey INVALID_KEY = new TestRecyclerKey("INVALID");
    public static final int DEFAULT_CAPACITY = 2;

    @Mock
    ElementAdapter<View, Object> mAdapter;
    @Mock
    ElementAdapter<View, Object> mAdapter2;
    @Mock
    ElementAdapter<View, Object> mAdapter3;

    SingleKeyRecyclerPool<ElementAdapter<View, Object>> mPool;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        mPool = new SingleKeyRecyclerPool<>(DEFAULT_KEY, DEFAULT_CAPACITY);
    }

    @Test
    public void testPutAndGetOneElement() {
        mPool.put(DEFAULT_KEY, mAdapter);
        assertThat(mPool.get(DEFAULT_KEY)).isEqualTo(mAdapter);
    }

    @Test
    public void testGetOnEmptyPoolReturnsNull() {
        assertThat(mPool.get(DEFAULT_KEY)).isNull();
    }

    @Test
    public void testGetWithDifferentKeyFails() {
        assertThatRunnable(() -> mPool.get(INVALID_KEY))
                .throwsAnExceptionOfType(IllegalArgumentException.class)
                .that()
                .hasMessageThat()
                .contains("does not match singleton key");
    }

    @Test
    public void testPutWithDifferentKeyFails() {
        assertThatRunnable(() -> mPool.put(INVALID_KEY, mAdapter))
                .throwsAnExceptionOfType(IllegalArgumentException.class)
                .that()
                .hasMessageThat()
                .contains("does not match singleton key");
    }

    @Test
    public void testGetWithNullKeyFails() {
        assertThatRunnable(() -> mPool.get(null))
                .throwsAnExceptionOfType(IllegalArgumentException.class)
                .that()
                .hasMessageThat()
                .contains("does not match singleton key");
    }

    @Test
    public void testPutWithNullKeyFails() {
        assertThatRunnable(() -> mPool.put(null, mAdapter))
                .throwsAnExceptionOfType(NullPointerException.class)
                .that()
                .hasMessageThat()
                .contains("null key for mAdapter");
    }

    @Test
    public void testPutSameElementTwiceFails() {
        mPool.put(DEFAULT_KEY, mAdapter);
        assertThatRunnable(() -> mPool.put(DEFAULT_KEY, mAdapter))
                .throwsAnExceptionOfType(IllegalStateException.class)
                .that()
                .hasMessageThat()
                .contains("Already in the pool!");
    }

    @Test
    public void testPutAndGetTwoElements() {
        mPool.put(DEFAULT_KEY, mAdapter);
        mPool.put(DEFAULT_KEY, mAdapter2);
        assertThat(mPool.get(DEFAULT_KEY)).isNotNull();
        assertThat(mPool.get(DEFAULT_KEY)).isNotNull();
        assertThat(mPool.get(DEFAULT_KEY)).isNull();
    }

    @Test
    public void testPoolOverflowIgnoresLastElement() {
        mPool.put(DEFAULT_KEY, mAdapter);
        mPool.put(DEFAULT_KEY, mAdapter2);
        mPool.put(DEFAULT_KEY, mAdapter3);
        assertThat(mPool.get(DEFAULT_KEY)).isNotNull();
        assertThat(mPool.get(DEFAULT_KEY)).isNotNull();
        assertThat(mPool.get(DEFAULT_KEY)).isNull();
    }

    @Test
    public void testClear() {
        mPool.put(DEFAULT_KEY, mAdapter);
        mPool.put(DEFAULT_KEY, mAdapter2);
        mPool.clear();
        assertThat(mPool.get(DEFAULT_KEY)).isNull();
    }

    private static class TestRecyclerKey extends RecyclerKey {
        private final String mKey;

        TestRecyclerKey(String key) {
            this.mKey = key;
        }

        @Override
        public int hashCode() {
            return mKey.hashCode();
        }

        @Override
        public boolean equals(/*@Nullable*/ Object obj) {
            if (obj == this) {
                return true;
            }

            if (obj == null) {
                return false;
            }

            if (!(obj instanceof TestRecyclerKey)) {
                return false;
            }

            TestRecyclerKey otherKey = (TestRecyclerKey) obj;
            return otherKey.mKey.equals(this.mKey);
        }
    }
}
