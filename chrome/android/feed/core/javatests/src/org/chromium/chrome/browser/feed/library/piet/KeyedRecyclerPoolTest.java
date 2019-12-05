// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;

import android.app.Activity;
import android.content.Context;
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link KeyedRecyclerPool} */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class KeyedRecyclerPoolTest {
    private static final RecyclerKey KEY1 = new TestRecyclerKey("KEY1");
    private static final RecyclerKey KEY2 = new TestRecyclerKey("KEY2");
    private static final RecyclerKey KEY3 = new TestRecyclerKey("KEY3");
    private static final int MAX_KEYS = 10;
    private static final int CAPACITY = 11;

    @Mock
    TestElementAdapter mAdapter;
    @Mock
    TestElementAdapter mAdapter2;
    @Mock
    TestElementAdapter mAdapter3;
    @Mock
    TestElementAdapter mAdapter4;

    private Context mTestContext;

    @Before
    public void setUp() {
        mTestContext = Robolectric.buildActivity(Activity.class).get();
        initMocks(this);
    }

    @Test
    public void testPutAndGetOneElement() {
        KeyedRecyclerPool<TestElementAdapter> pool = new KeyedRecyclerPool<>(MAX_KEYS, CAPACITY);
        pool.put(KEY1, mAdapter);
        assertThat(pool.get(KEY1)).isEqualTo(mAdapter);
    }

    @Test
    public void testGetFromEmptyPoolReturnsNull() {
        KeyedRecyclerPool<TestElementAdapter> pool = new KeyedRecyclerPool<>(MAX_KEYS, CAPACITY);
        assertThat(pool.get(KEY1)).isNull();
    }

    @Test
    public void testGetFromEmptyPoolWithDifferentKeyReturnsNull() {
        KeyedRecyclerPool<TestElementAdapter> pool = new KeyedRecyclerPool<>(MAX_KEYS, CAPACITY);
        pool.put(KEY1, mAdapter);
        assertThat(pool.get(KEY2)).isNull();
    }

    @Test
    public void testPutAndGetElementsWithDifferentKeys() {
        KeyedRecyclerPool<TestElementAdapter> pool = new KeyedRecyclerPool<>(MAX_KEYS, CAPACITY);
        pool.put(KEY1, mAdapter);
        pool.put(KEY2, mAdapter2);
        assertThat(pool.get(KEY2)).isEqualTo(mAdapter2);
        assertThat(pool.get(KEY1)).isEqualTo(mAdapter);
    }

    @Test
    public void testPutNullElementFails() {
        KeyedRecyclerPool<TestElementAdapter> pool = new KeyedRecyclerPool<>(MAX_KEYS, CAPACITY);
        pool.put(KEY1, null);
        assertThat(pool.get(KEY1)).isNull();
    }

    @Test
    public void testPutNullKeyFails() {
        KeyedRecyclerPool<TestElementAdapter> pool = new KeyedRecyclerPool<>(MAX_KEYS, CAPACITY);
        assertThatRunnable(() -> pool.put(null, mAdapter))
                .throwsAnExceptionOfType(NullPointerException.class)
                .that()
                .hasMessageThat()
                .contains("null key for mAdapter");
    }

    @Test
    public void testGetNullKeyReturnsNull() {
        KeyedRecyclerPool<TestElementAdapter> pool = new KeyedRecyclerPool<>(MAX_KEYS, CAPACITY);
        assertThat(pool.get(null)).isNull();
    }

    @Test
    public void testCacheOverflowEjectsPool() {
        KeyedRecyclerPool<TestElementAdapter> pool = new KeyedRecyclerPool<>(2, CAPACITY);
        pool.put(KEY1, mAdapter);
        pool.put(KEY2, mAdapter2);
        pool.put(KEY3, mAdapter3);
        assertThat(pool.get(KEY1)).isNull();
        assertThat(pool.get(KEY2)).isEqualTo(mAdapter2);
        assertThat(pool.get(KEY3)).isEqualTo(mAdapter3);
    }

    @Test
    public void testOverflowSinglePoolIgnoresLastElement() {
        KeyedRecyclerPool<TestElementAdapter> pool = new KeyedRecyclerPool<>(MAX_KEYS, 2);
        pool.put(KEY1, mAdapter);
        pool.put(KEY1, mAdapter2);
        pool.put(KEY1, mAdapter3);
        assertThat(pool.get(KEY1)).isNotNull();
        assertThat(pool.get(KEY1)).isNotNull();
        assertThat(pool.get(KEY1)).isNull();
    }

    @Test
    public void testFillAllPools() {
        KeyedRecyclerPool<TestElementAdapter> pool = new KeyedRecyclerPool<>(2, 2);
        pool.put(KEY1, mAdapter);
        pool.put(KEY1, mAdapter2);
        pool.put(KEY2, mAdapter3);
        pool.put(KEY2, mAdapter4);
        assertThat(pool.get(KEY1)).isNotNull();
        assertThat(pool.get(KEY1)).isNotNull();
        assertThat(pool.get(KEY1)).isNull();
        assertThat(pool.get(KEY2)).isNotNull();
        assertThat(pool.get(KEY2)).isNotNull();
        assertThat(pool.get(KEY2)).isNull();
    }

    @Test
    public void testClear() {
        KeyedRecyclerPool<TestElementAdapter> pool = new KeyedRecyclerPool<>(MAX_KEYS, MAX_KEYS);
        pool.put(KEY1, mAdapter);
        pool.put(KEY1, mAdapter2);
        pool.put(KEY2, mAdapter3);
        pool.put(KEY2, mAdapter4);
        pool.clear();
        assertThat(pool.get(KEY1)).isNull();
        assertThat(pool.get(KEY2)).isNull();
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

    private class TestElementAdapter extends ElementAdapter<View, Object> {
        TestElementAdapter() {
            super(mTestContext, null, new View(mTestContext));
        }

        @Override
        protected Object getModelFromElement(Element baseElement) {
            return null;
        }
    }
}
