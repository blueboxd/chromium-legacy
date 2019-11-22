// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.piet.host.AssetProvider;
import com.google.search.now.ui.piet.TextProto.ParameterizedText;
import com.google.search.now.ui.piet.TextProto.ParameterizedText.Parameter;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests of the {@link ParameterizedTextEvaluator} */
@RunWith(RobolectricTestRunner.class)
// TODO: Create a test of evaluteHtml
public class ParameterizedTextEvaluatorTest {
    private static final int MILLISECONDS_PER_SECOND = 1000;
    private static final int SECONDS_PER_MINUTE = 60;
    private static final int MILLISECONDS_PER_MINUTE = SECONDS_PER_MINUTE * MILLISECONDS_PER_SECOND;
    @Mock
    private AssetProvider assetProvider;

    private ParameterizedTextEvaluator evaluator;
    private FakeClock clock = new FakeClock();

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        evaluator = new ParameterizedTextEvaluator(clock);
    }

    @Test
    public void testEvaluate_noText() {
        ParameterizedText text = ParameterizedText.getDefaultInstance();
        assertThat(evaluator.evaluate(assetProvider, text).toString()).isEmpty();
    }

    @Test
    public void testEvaluate_noParameters() {
        String content = "content";
        ParameterizedText text = ParameterizedText.newBuilder().setText(content).build();
        assertThat(evaluator.evaluate(assetProvider, text).toString()).isEqualTo(content);
    }

    @Test
    public void testEvaluate_time() {
        String content = "content %s";
        String time = "10 minutes";
        when(assetProvider.getRelativeElapsedString(anyLong())).thenReturn(time);
        long initialTime = 10;
        clock.set(initialTime * MILLISECONDS_PER_SECOND + 10 * MILLISECONDS_PER_MINUTE);
        ParameterizedText text =
                ParameterizedText.newBuilder()
                        .setText(content)
                        .addParameters(Parameter.newBuilder().setTimestampSeconds(initialTime))
                        .build();

        assertThat(evaluator.evaluate(assetProvider, text).toString())
                .isEqualTo(String.format(content, time));
        verify(assetProvider).getRelativeElapsedString(10 * MILLISECONDS_PER_MINUTE);
    }

    @Test
    public void testEvaluate_multipleParameters() {
        String content = "content %s - %s";
        String time1 = "10 minutes";
        String time2 = "20 minutes";
        when(assetProvider.getRelativeElapsedString(anyLong())).thenReturn(time1).thenReturn(time2);
        clock.set(20 * MILLISECONDS_PER_MINUTE);

        ParameterizedText text =
                ParameterizedText.newBuilder()
                        .setText(content)
                        .addParameters(
                                Parameter.newBuilder().setTimestampSeconds(10 * SECONDS_PER_MINUTE))
                        .addParameters(Parameter.newBuilder().setTimestampSeconds(0))
                        .build();

        assertThat(evaluator.evaluate(assetProvider, text).toString())
                .isEqualTo(String.format(content, time1, time2));

        verify(assetProvider).getRelativeElapsedString(10 * MILLISECONDS_PER_MINUTE);
        verify(assetProvider).getRelativeElapsedString(20 * MILLISECONDS_PER_MINUTE);
    }

    @Test
    public void testEvaluate_html() {
        String content = "<h1>content</h1>";
        ParameterizedText text =
                ParameterizedText.newBuilder().setIsHtml(true).setText(content).build();
        assertThat(evaluator.evaluate(assetProvider, text).toString()).isEqualTo("content\n\n");
    }

    @Test
    public void testEvaluate_htmlAndParameters() {
        String content = "<h1>content %s</h1>";
        String time1 = "1 second";
        when(assetProvider.getRelativeElapsedString(anyLong())).thenReturn(time1);

        ParameterizedText text =
                ParameterizedText.newBuilder()
                        .setIsHtml(true)
                        .setText(content)
                        .addParameters(Parameter.newBuilder().setTimestampSeconds(1))
                        .build();
        assertThat(evaluator.evaluate(assetProvider, text).toString())
                .isEqualTo("content 1 second\n\n");
    }

    @Test
    public void testTimeConversion() {
        String content = "%s";
        String time = "10 minutes";
        when(assetProvider.getRelativeElapsedString(1000)).thenReturn(time);
        clock.set(2000);
        ParameterizedText.Builder text = ParameterizedText.newBuilder();
        text.setText(content);
        text.addParameters(Parameter.newBuilder().setTimestampSeconds(1).build());

        assertThat(evaluator.evaluate(assetProvider, text.build()).toString())
                .isEqualTo(String.format(content, time));
    }
}
