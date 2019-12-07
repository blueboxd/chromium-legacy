// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.conformance.storage;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Before;
import org.junit.Test;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentMutation;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentStorage;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.testing.RequiredConsumer;

import java.nio.charset.Charset;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;

/**
 * Conformance test for {@link ContentStorage}. Hosts who wish to test against this should extend
 * this class and set {@code storage} to the Host implementation.
 */
public abstract class ContentStorageConformanceTest {
    private static final String KEY = "key";
    private static final String KEY_0 = KEY + " 0";
    private static final String KEY_1 = KEY + " 1";
    private static final String OTHER_KEY = "other";
    private static final byte[] DATA_0 = "data 0".getBytes(Charset.forName("UTF-8"));
    private static final byte[] DATA_1 = "data 1".getBytes(Charset.forName("UTF-8"));
    private static final byte[] OTHER_DATA = "other data".getBytes(Charset.forName("UTF-8"));

    // Helper consumers to make tests cleaner
    private final Consumer<Result<Map<String, byte[]>>> mIsKey0Data0 = input -> {
        assertThat(input.isSuccessful()).isTrue();
        Map<String, byte[]> valueMap = input.getValue();
        assertThat(valueMap.get(KEY_0)).isEqualTo(DATA_0);
    };
    private final Consumer<Result<Map<String, byte[]>>> mIsKey0EmptyData = input -> {
        assertThat(input.isSuccessful()).isTrue();
        Map<String, byte[]> valueMap = input.getValue();
        assertThat(valueMap.get(KEY_0)).isNull();
    };
    private final Consumer<Result<Map<String, byte[]>>> mIsKey0Data1 = input -> {
        assertThat(input.isSuccessful()).isTrue();
        Map<String, byte[]> valueMap = input.getValue();
        assertThat(valueMap.get(KEY_0)).isEqualTo(DATA_1);
    };

    private final Consumer<Result<Map<String, byte[]>>> mIsKey0Data0Key1Data1 = result -> {
        assertThat(result.isSuccessful()).isTrue();
        Map<String, byte[]> input = result.getValue();
        assertThat(input.get(KEY_0)).isEqualTo(DATA_0);
        assertThat(input.get(KEY_1)).isEqualTo(DATA_1);
    };
    private final Consumer<Result<Map<String, byte[]>>> mIsKey0EmptyDataKey1EmptyData = input -> {
        assertThat(input.isSuccessful()).isTrue();
        Map<String, byte[]> valueMap = input.getValue();
        assertThat(valueMap.get(KEY_0)).isNull();
        assertThat(valueMap.get(KEY_1)).isNull();
    };
    private final Consumer<Result<Map<String, byte[]>>>
            mIsKey0EmptyDataKey1EmptyDataOtherKeyEmptyData = input -> {
        assertThat(input.isSuccessful()).isTrue();
        Map<String, byte[]> valueMap = input.getValue();
        assertThat(valueMap.get(KEY_0)).isNull();
        assertThat(valueMap.get(KEY_1)).isNull();
        assertThat(valueMap.get(OTHER_KEY)).isNull();
    };

    private final Consumer<Result<Map<String, byte[]>>> mIsKey0Data0Key1Data1OtherKeyOtherData =
            result -> {
        assertThat(result.isSuccessful()).isTrue();
        Map<String, byte[]> input = result.getValue();
        assertThat(input.get(KEY_0)).isEqualTo(DATA_0);
        assertThat(input.get(KEY_1)).isEqualTo(DATA_1);
        assertThat(input.get(OTHER_KEY)).isEqualTo(OTHER_DATA);
    };
    private final Consumer<Result<Map<String, byte[]>>>
            mIsKey0EmptyDataKey1EmptyDataOtherKeyOtherData = result -> {
        assertThat(result.isSuccessful()).isTrue();
        Map<String, byte[]> input = result.getValue();
        assertThat(input.get(KEY_0)).isNull();
        assertThat(input.get(KEY_1)).isNull();
        assertThat(input.get(OTHER_KEY)).isEqualTo(OTHER_DATA);
    };

    private final Consumer<CommitResult> mIsSuccess =
            input -> assertThat(input).isEqualTo(CommitResult.SUCCESS);

    private final Consumer<Result<List<String>>> mIsKey0Key1 = result -> {
        assertThat(result.isSuccessful()).isTrue();
        assertThat(result.getValue()).containsExactly(KEY_0, KEY_1);
    };

    protected ContentStorage mStorage;

    private RequiredConsumer<CommitResult> mAssertSuccessConsumer;

    @Before
    public final void testSetup() {
        mAssertSuccessConsumer = new RequiredConsumer<>(mIsSuccess);
    }

    @Test
    public void missingKey() {
        RequiredConsumer<Result<Map<String, byte[]>>> consumer =
                new RequiredConsumer<>(mIsKey0EmptyData);
        mStorage.get(Collections.singletonList(KEY_0), consumer);
        assertThat(consumer.isCalled()).isTrue();
    }

    @Test
    public void missingKey_multipleKeys() {
        RequiredConsumer<Result<Map<String, byte[]>>> consumer =
                new RequiredConsumer<>(mIsKey0EmptyDataKey1EmptyData);
        mStorage.get(Arrays.asList(KEY_0, KEY_1), consumer);
        assertThat(consumer.isCalled()).isTrue();
    }

    @Test
    public void storeAndRetrieve() {
        mStorage.commit(new ContentMutation.Builder().upsert(KEY_0, DATA_0).build(),
                mAssertSuccessConsumer);
        assertThat(mAssertSuccessConsumer.isCalled()).isTrue();
        RequiredConsumer<Result<Map<String, byte[]>>> byteConsumer =
                new RequiredConsumer<>(mIsKey0Data0);
        mStorage.get(Collections.singletonList(KEY_0), byteConsumer);
        assertThat(byteConsumer.isCalled()).isTrue();
    }

    @Test
    public void storeAndRetrieve_multipleKeys() {
        mStorage.commit(
                new ContentMutation.Builder().upsert(KEY_0, DATA_0).upsert(KEY_1, DATA_1).build(),
                mAssertSuccessConsumer);
        assertThat(mAssertSuccessConsumer.isCalled()).isTrue();
        RequiredConsumer<Result<Map<String, byte[]>>> byteConsumer =
                new RequiredConsumer<>(mIsKey0Data0Key1Data1);
        mStorage.get(Arrays.asList(KEY_0, KEY_1), byteConsumer);
        assertThat(byteConsumer.isCalled()).isTrue();
    }

    @Test
    public void storeAndOverwrite_chained() {
        mStorage.commit(
                new ContentMutation.Builder().upsert(KEY_0, DATA_0).upsert(KEY_0, DATA_1).build(),
                mAssertSuccessConsumer);
        assertThat(mAssertSuccessConsumer.isCalled()).isTrue();
        RequiredConsumer<Result<Map<String, byte[]>>> byteConsumer =
                new RequiredConsumer<>(mIsKey0Data1);
        mStorage.get(Collections.singletonList(KEY_0), byteConsumer);
        assertThat(byteConsumer.isCalled()).isTrue();
    }

    @Test
    public void storeAndOverwrite_separate() {
        mStorage.commit(new ContentMutation.Builder().upsert(KEY_0, DATA_0).build(),
                mAssertSuccessConsumer);
        assertThat(mAssertSuccessConsumer.isCalled()).isTrue();

        RequiredConsumer<Result<Map<String, byte[]>>> byteConsumer =
                new RequiredConsumer<>(mIsKey0Data0);
        mStorage.get(Collections.singletonList(KEY_0), byteConsumer);
        assertThat(byteConsumer.isCalled()).isTrue();

        // Reset assertSuccessConsumer
        mAssertSuccessConsumer = new RequiredConsumer<>(
                input -> { assertThat(input).isEqualTo(CommitResult.SUCCESS); });
        mStorage.commit(new ContentMutation.Builder().upsert(KEY_0, DATA_1).build(),
                mAssertSuccessConsumer);
        assertThat(mAssertSuccessConsumer.isCalled()).isTrue();

        RequiredConsumer<Result<Map<String, byte[]>>> byteConsumer2 =
                new RequiredConsumer<>(mIsKey0Data1);
        mStorage.get(Collections.singletonList(KEY_0), byteConsumer2);
        assertThat(byteConsumer2.isCalled()).isTrue();
    }

    @Test
    public void storeAndDelete() {
        mStorage.commit(
                new ContentMutation.Builder().upsert(KEY_0, DATA_0).upsert(KEY_1, DATA_1).build(),
                mAssertSuccessConsumer);
        assertThat(mAssertSuccessConsumer.isCalled()).isTrue();

        // Confirm Key 0 and 1 are present
        RequiredConsumer<Result<Map<String, byte[]>>> byteConsumer =
                new RequiredConsumer<>(mIsKey0Data0Key1Data1);
        mStorage.get(Arrays.asList(KEY_0, KEY_1), byteConsumer);
        assertThat(byteConsumer.isCalled()).isTrue();

        // Delete Key 0
        RequiredConsumer<CommitResult> deleteConsumer = new RequiredConsumer<>(mIsSuccess);
        mStorage.commit(new ContentMutation.Builder().delete(KEY_0).build(), deleteConsumer);
        assertThat(deleteConsumer.isCalled()).isTrue();

        // Confirm that Key 0 is deleted and Key 1 is present
        byteConsumer = new RequiredConsumer<>(mIsKey0EmptyData);
        mStorage.get(Arrays.asList(KEY_0, KEY_1), byteConsumer);
        assertThat(byteConsumer.isCalled()).isTrue();
    }

    @Test
    public void storeAndDeleteByPrefix() {
        mStorage.commit(new ContentMutation.Builder()
                                .upsert(KEY_0, DATA_0)
                                .upsert(KEY_1, DATA_1)
                                .upsert(OTHER_KEY, OTHER_DATA)
                                .build(),
                mAssertSuccessConsumer);
        assertThat(mAssertSuccessConsumer.isCalled()).isTrue();

        // Confirm Key 0, Key 1, and Other are present
        RequiredConsumer<Result<Map<String, byte[]>>> byteConsumer =
                new RequiredConsumer<>(mIsKey0Data0Key1Data1OtherKeyOtherData);
        mStorage.get(Arrays.asList(KEY_0, KEY_1, OTHER_KEY), byteConsumer);
        assertThat(byteConsumer.isCalled()).isTrue();

        // Delete by prefix Key
        RequiredConsumer<CommitResult> deleteConsumer = new RequiredConsumer<>(mIsSuccess);
        mStorage.commit(new ContentMutation.Builder().deleteByPrefix(KEY).build(), deleteConsumer);

        // Confirm Key 0 and Key 1 are deleted, and Other is present
        byteConsumer = new RequiredConsumer<>(mIsKey0EmptyDataKey1EmptyDataOtherKeyOtherData);
        mStorage.get(Arrays.asList(KEY_0, KEY_1, OTHER_KEY), byteConsumer);
        assertThat(byteConsumer.isCalled()).isTrue();
    }

    @Test
    public void storeAndDeleteAll() {
        mStorage.commit(new ContentMutation.Builder()
                                .upsert(KEY_0, DATA_0)
                                .upsert(KEY_1, DATA_1)
                                .upsert(OTHER_KEY, OTHER_DATA)
                                .build(),
                mAssertSuccessConsumer);
        assertThat(mAssertSuccessConsumer.isCalled()).isTrue();

        // Confirm Key 0, Key 1, and Other are present
        RequiredConsumer<Result<Map<String, byte[]>>> byteConsumer =
                new RequiredConsumer<>(mIsKey0Data0Key1Data1OtherKeyOtherData);
        mStorage.get(Arrays.asList(KEY_0, KEY_1, OTHER_KEY), byteConsumer);
        assertThat(byteConsumer.isCalled()).isTrue();

        // Delete all
        RequiredConsumer<CommitResult> deleteConsumer = new RequiredConsumer<>(mIsSuccess);
        mStorage.commit(new ContentMutation.Builder().deleteAll().build(), deleteConsumer);

        // Confirm all keys are deleted
        byteConsumer = new RequiredConsumer<>(mIsKey0EmptyDataKey1EmptyDataOtherKeyEmptyData);
        mStorage.get(Arrays.asList(KEY_0, KEY_1, OTHER_KEY), byteConsumer);
        assertThat(byteConsumer.isCalled()).isTrue();
    }

    @Test
    public void multipleValues_getAll() {
        mStorage.commit(
                new ContentMutation.Builder().upsert(KEY_0, DATA_0).upsert(KEY_1, DATA_1).build(),
                mAssertSuccessConsumer);
        assertThat(mAssertSuccessConsumer.isCalled()).isTrue();

        RequiredConsumer<Result<Map<String, byte[]>>> mapConsumer =
                new RequiredConsumer<>(mIsKey0Data0Key1Data1);
        mStorage.getAll(KEY, mapConsumer);
        assertThat(mapConsumer.isCalled()).isTrue();
    }

    @Test
    public void multipleValues_getAllKeys() {
        mStorage.commit(
                new ContentMutation.Builder().upsert(KEY_0, DATA_0).upsert(KEY_1, DATA_1).build(),
                mAssertSuccessConsumer);
        assertThat(mAssertSuccessConsumer.isCalled()).isTrue();

        RequiredConsumer<Result<List<String>>> listConsumer = new RequiredConsumer<>(mIsKey0Key1);
        mStorage.getAllKeys(listConsumer);
        assertThat(listConsumer.isCalled()).isTrue();
    }
}
