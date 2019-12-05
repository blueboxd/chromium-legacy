// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.piet.host.AssetProvider;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.ParameterizedText;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.ParameterizedText.Parameter;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link ParameterizedTextEvaluator} */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
// TODO: Create a test of evaluteHtml
public class ParameterizedTextEvaluatorTest {
    private static final int MILLISECONDS_PER_SECOND = 1000;
    private static final int SECONDS_PER_MINUTE = 60;
    private static final int MILLISECONDS_PER_MINUTE = SECONDS_PER_MINUTE * MILLISECONDS_PER_SECOND;
    @Mock
    private AssetProvider mAssetProvider;

    private ParameterizedTextEvaluator mEvaluator;
    private FakeClock mClock = new FakeClock();

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        mEvaluator = new ParameterizedTextEvaluator(mClock);
    }

    @Test
    public void testEvaluate_noText() {
        ParameterizedText text = ParameterizedText.getDefaultInstance();
        assertThat(mEvaluator.evaluate(mAssetProvider, text).toString()).isEmpty();
    }

    @Test
    public void testEvaluate_noParameters() {
        String content = "content";
        ParameterizedText text = ParameterizedText.newBuilder().setText(content).build();
        assertThat(mEvaluator.evaluate(mAssetProvider, text).toString()).isEqualTo(content);
    }

    @Test
    public void testEvaluate_time() {
        String content = "content %s";
        String time = "10 minutes";
        when(mAssetProvider.getRelativeElapsedString(anyLong())).thenReturn(time);
        long initialTime = 10;
        mClock.set(initialTime * MILLISECONDS_PER_SECOND + 10 * MILLISECONDS_PER_MINUTE);
        ParameterizedText text =
                ParameterizedText.newBuilder()
                        .setText(content)
                        .addParameters(Parameter.newBuilder().setTimestampSeconds(initialTime))
                        .build();

        assertThat(mEvaluator.evaluate(mAssetProvider, text).toString())
                .isEqualTo(String.format(content, time));
        verify(mAssetProvider).getRelativeElapsedString(10 * MILLISECONDS_PER_MINUTE);
    }

    @Test
    public void testEvaluate_multipleParameters() {
        String content = "content %s - %s";
        String time1 = "10 minutes";
        String time2 = "20 minutes";
        when(mAssetProvider.getRelativeElapsedString(anyLong()))
                .thenReturn(time1)
                .thenReturn(time2);
        mClock.set(20 * MILLISECONDS_PER_MINUTE);

        ParameterizedText text =
                ParameterizedText.newBuilder()
                        .setText(content)
                        .addParameters(
                                Parameter.newBuilder().setTimestampSeconds(10 * SECONDS_PER_MINUTE))
                        .addParameters(Parameter.newBuilder().setTimestampSeconds(0))
                        .build();

        assertThat(mEvaluator.evaluate(mAssetProvider, text).toString())
                .isEqualTo(String.format(content, time1, time2));

        verify(mAssetProvider).getRelativeElapsedString(10 * MILLISECONDS_PER_MINUTE);
        verify(mAssetProvider).getRelativeElapsedString(20 * MILLISECONDS_PER_MINUTE);
    }

    @Test
    public void testEvaluate_html() {
        String content = "<h1>content</h1>";
        ParameterizedText text =
                ParameterizedText.newBuilder().setIsHtml(true).setText(content).build();
        assertThat(mEvaluator.evaluate(mAssetProvider, text).toString()).isEqualTo("content\n\n");
    }

    @Test
    public void testEvaluate_htmlAndParameters() {
        String content = "<h1>content %s</h1>";
        String time1 = "1 second";
        when(mAssetProvider.getRelativeElapsedString(anyLong())).thenReturn(time1);

        ParameterizedText text =
                ParameterizedText.newBuilder()
                        .setIsHtml(true)
                        .setText(content)
                        .addParameters(Parameter.newBuilder().setTimestampSeconds(1))
                        .build();
        assertThat(mEvaluator.evaluate(mAssetProvider, text).toString())
                .isEqualTo("content 1 second\n\n");
    }

    @Test
    public void testTimeConversion() {
        String content = "%s";
        String time = "10 minutes";
        when(mAssetProvider.getRelativeElapsedString(1000)).thenReturn(time);
        mClock.set(2000);
        ParameterizedText.Builder text = ParameterizedText.newBuilder();
        text.setText(content);
        text.addParameters(Parameter.newBuilder().setTimestampSeconds(1).build());

        assertThat(mEvaluator.evaluate(mAssetProvider, text.build()).toString())
                .isEqualTo(String.format(content, time));
    }
}
