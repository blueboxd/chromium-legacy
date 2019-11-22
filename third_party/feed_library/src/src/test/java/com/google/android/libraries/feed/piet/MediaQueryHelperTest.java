// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.android.libraries.feed.common.testing.RunnableSubject.assertThatRunnable;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;

import com.google.android.libraries.feed.piet.host.AssetProvider;
import com.google.search.now.ui.piet.MediaQueriesProto.ComparisonCondition;
import com.google.search.now.ui.piet.MediaQueriesProto.DarkLightCondition;
import com.google.search.now.ui.piet.MediaQueriesProto.DarkLightCondition.DarkLightMode;
import com.google.search.now.ui.piet.MediaQueriesProto.FrameWidthCondition;
import com.google.search.now.ui.piet.MediaQueriesProto.MediaQueryCondition;
import com.google.search.now.ui.piet.MediaQueriesProto.OrientationCondition;
import com.google.search.now.ui.piet.MediaQueriesProto.OrientationCondition.Orientation;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

import java.util.Arrays;
import java.util.List;

/** Tests for the MediaQueryHelper. */
@RunWith(RobolectricTestRunner.class)
public class MediaQueryHelperTest {
    private static final int DEFAULT_FRAME_WIDTH = 123;

    @Mock
    private AssetProvider assetProvider;

    private Context context;

    private MediaQueryHelper mediaQueryHelper;

    @Before
    public void setUp() {
        initMocks(this);
        context = Robolectric.buildActivity(Activity.class).get();
        mediaQueryHelper = new MediaQueryHelper(DEFAULT_FRAME_WIDTH, assetProvider, context);
    }

    @Test
    public void testMediaQueryConditions_frameWidth() {
        int smallWidth = 1;
        int midWidth = 10;
        int bigWidth = 100;

        mediaQueryHelper = new MediaQueryHelper(midWidth, assetProvider, context);
        assertThat(mediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setFrameWidth(FrameWidthCondition.newBuilder()
                                                          .setWidth(midWidth)
                                                          .setCondition(ComparisonCondition.EQUALS))
                                   .build()))
                .isTrue();
        assertThat(mediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setFrameWidth(FrameWidthCondition.newBuilder()
                                                          .setWidth(smallWidth)
                                                          .setCondition(ComparisonCondition.EQUALS))
                                   .build()))
                .isFalse();
        assertThat(mediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setFrameWidth(
                                           FrameWidthCondition.newBuilder()
                                                   .setWidth(smallWidth)
                                                   .setCondition(ComparisonCondition.GREATER_THAN))
                                   .build()))
                .isTrue();
        assertThat(mediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setFrameWidth(
                                           FrameWidthCondition.newBuilder()
                                                   .setWidth(bigWidth)
                                                   .setCondition(ComparisonCondition.GREATER_THAN))
                                   .build()))
                .isFalse();
        assertThat(
                mediaQueryHelper.isMediaQueryMet(
                        MediaQueryCondition.newBuilder()
                                .setFrameWidth(FrameWidthCondition.newBuilder()
                                                       .setWidth(bigWidth)
                                                       .setCondition(ComparisonCondition.LESS_THAN))
                                .build()))
                .isTrue();
        assertThat(
                mediaQueryHelper.isMediaQueryMet(
                        MediaQueryCondition.newBuilder()
                                .setFrameWidth(FrameWidthCondition.newBuilder()
                                                       .setWidth(smallWidth)
                                                       .setCondition(ComparisonCondition.LESS_THAN))
                                .build()))
                .isFalse();
        assertThat(mediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setFrameWidth(
                                           FrameWidthCondition.newBuilder()
                                                   .setWidth(smallWidth)
                                                   .setCondition(ComparisonCondition.NOT_EQUALS))
                                   .build()))
                .isTrue();
        assertThat(mediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setFrameWidth(
                                           FrameWidthCondition.newBuilder()
                                                   .setWidth(midWidth)
                                                   .setCondition(ComparisonCondition.NOT_EQUALS))
                                   .build()))
                .isFalse();

        assertThatRunnable(
                ()
                        -> mediaQueryHelper.isMediaQueryMet(
                                MediaQueryCondition.newBuilder()
                                        .setFrameWidth(
                                                FrameWidthCondition.newBuilder()
                                                        .setWidth(midWidth)
                                                        .setCondition(
                                                                ComparisonCondition.UNSPECIFIED))
                                        .build()))
                .throwsAnExceptionOfType(IllegalArgumentException.class)
                .that()
                .hasMessageThat()
                .contains("Unhandled ComparisonCondition: UNSPECIFIED");
    }

    @Test
    public void testMediaQueryConditions_orientation() {
        context.getResources().getConfiguration().orientation = Configuration.ORIENTATION_LANDSCAPE;
        mediaQueryHelper = new MediaQueryHelper(DEFAULT_FRAME_WIDTH, assetProvider, context);
        assertThat(mediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setOrientation(OrientationCondition.newBuilder().setOrientation(
                                           Orientation.LANDSCAPE))
                                   .build()))
                .isTrue();
        assertThat(mediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setOrientation(OrientationCondition.newBuilder().setOrientation(
                                           Orientation.PORTRAIT))
                                   .build()))
                .isFalse();

        context.getResources().getConfiguration().orientation = Configuration.ORIENTATION_PORTRAIT;
        mediaQueryHelper = new MediaQueryHelper(DEFAULT_FRAME_WIDTH, assetProvider, context);
        assertThat(mediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setOrientation(OrientationCondition.newBuilder().setOrientation(
                                           Orientation.LANDSCAPE))
                                   .build()))
                .isFalse();
        assertThat(mediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setOrientation(OrientationCondition.newBuilder().setOrientation(
                                           Orientation.PORTRAIT))
                                   .build()))
                .isTrue();
        assertThat(mediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setOrientation(OrientationCondition.newBuilder().setOrientation(
                                           Orientation.UNSPECIFIED))
                                   .build()))
                .isTrue();

        context.getResources().getConfiguration().orientation = Configuration.ORIENTATION_UNDEFINED;
        mediaQueryHelper = new MediaQueryHelper(DEFAULT_FRAME_WIDTH, assetProvider, context);
        assertThat(mediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setOrientation(OrientationCondition.newBuilder().setOrientation(
                                           Orientation.LANDSCAPE))
                                   .build()))
                .isFalse();
        assertThat(mediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setOrientation(OrientationCondition.newBuilder().setOrientation(
                                           Orientation.PORTRAIT))
                                   .build()))
                .isFalse();
        assertThat(mediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setOrientation(OrientationCondition.newBuilder().setOrientation(
                                           Orientation.UNSPECIFIED))
                                   .build()))
                .isFalse();
    }

    @Test
    public void testMediaQueryConditions_darkLight() {
        when(assetProvider.isDarkTheme()).thenReturn(true);
        mediaQueryHelper = new MediaQueryHelper(DEFAULT_FRAME_WIDTH, assetProvider, context);
        assertThat(mediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setDarkLight(DarkLightCondition.newBuilder().setMode(
                                           DarkLightMode.DARK))
                                   .build()))
                .isTrue();
        assertThat(mediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setDarkLight(DarkLightCondition.newBuilder().setMode(
                                           DarkLightMode.LIGHT))
                                   .build()))
                .isFalse();
        assertThat(mediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setDarkLight(DarkLightCondition.newBuilder().setMode(
                                           DarkLightMode.UNSPECIFIED))
                                   .build()))
                .isFalse();

        when(assetProvider.isDarkTheme()).thenReturn(false);
        mediaQueryHelper = new MediaQueryHelper(DEFAULT_FRAME_WIDTH, assetProvider, context);
        assertThat(mediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setDarkLight(DarkLightCondition.newBuilder().setMode(
                                           DarkLightMode.DARK))
                                   .build()))
                .isFalse();
        assertThat(mediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setDarkLight(DarkLightCondition.newBuilder().setMode(
                                           DarkLightMode.LIGHT))
                                   .build()))
                .isTrue();
        assertThat(mediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setDarkLight(DarkLightCondition.newBuilder().setMode(
                                           DarkLightMode.UNSPECIFIED))
                                   .build()))
                .isTrue();
    }

    @Test
    public void testMediaQueryConditions_allConditionsTrue() {
        MediaQueryCondition landscapeCondition =
                MediaQueryCondition.newBuilder()
                        .setOrientation(OrientationCondition.newBuilder().setOrientation(
                                Orientation.LANDSCAPE))
                        .build();
        MediaQueryCondition darkThemeCondition =
                MediaQueryCondition.newBuilder()
                        .setDarkLight(DarkLightCondition.newBuilder().setMode(DarkLightMode.DARK))
                        .build();
        MediaQueryCondition frameWidthCondition =
                MediaQueryCondition.newBuilder()
                        .setFrameWidth(FrameWidthCondition.newBuilder().setWidth(100).setCondition(
                                ComparisonCondition.GREATER_THAN))
                        .build();

        List<MediaQueryCondition> conditions =
                Arrays.asList(landscapeCondition, darkThemeCondition, frameWidthCondition);

        context.getResources().getConfiguration().orientation = Configuration.ORIENTATION_LANDSCAPE;

        when(assetProvider.isDarkTheme()).thenReturn(true);
        mediaQueryHelper = new MediaQueryHelper(DEFAULT_FRAME_WIDTH, assetProvider, context);
        assertThat(mediaQueryHelper.areMediaQueriesMet(conditions)).isTrue();

        when(assetProvider.isDarkTheme()).thenReturn(false);
        mediaQueryHelper = new MediaQueryHelper(DEFAULT_FRAME_WIDTH, assetProvider, context);
        assertThat(mediaQueryHelper.areMediaQueriesMet(conditions)).isFalse();
    }
}
