// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.piet.host.AssetProvider;
import org.chromium.components.feed.core.proto.ui.piet.MediaQueriesProto.ComparisonCondition;
import org.chromium.components.feed.core.proto.ui.piet.MediaQueriesProto.DarkLightCondition;
import org.chromium.components.feed.core.proto.ui.piet.MediaQueriesProto.DarkLightCondition.DarkLightMode;
import org.chromium.components.feed.core.proto.ui.piet.MediaQueriesProto.FrameWidthCondition;
import org.chromium.components.feed.core.proto.ui.piet.MediaQueriesProto.MediaQueryCondition;
import org.chromium.components.feed.core.proto.ui.piet.MediaQueriesProto.OrientationCondition;
import org.chromium.components.feed.core.proto.ui.piet.MediaQueriesProto.OrientationCondition.Orientation;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Arrays;
import java.util.List;

/** Tests for the MediaQueryHelper. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MediaQueryHelperTest {
    private static final int DEFAULT_FRAME_WIDTH = 123;

    @Mock
    private AssetProvider mAssetProvider;

    private Context mContext;

    private MediaQueryHelper mMediaQueryHelper;

    @Before
    public void setUp() {
        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        mMediaQueryHelper = new MediaQueryHelper(DEFAULT_FRAME_WIDTH, mAssetProvider, mContext);
    }

    @Test
    public void testMediaQueryConditions_frameWidth() {
        int smallWidth = 1;
        int midWidth = 10;
        int bigWidth = 100;

        mMediaQueryHelper = new MediaQueryHelper(midWidth, mAssetProvider, mContext);
        assertThat(mMediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setFrameWidth(FrameWidthCondition.newBuilder()
                                                          .setWidth(midWidth)
                                                          .setCondition(ComparisonCondition.EQUALS))
                                   .build()))
                .isTrue();
        assertThat(mMediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setFrameWidth(FrameWidthCondition.newBuilder()
                                                          .setWidth(smallWidth)
                                                          .setCondition(ComparisonCondition.EQUALS))
                                   .build()))
                .isFalse();
        assertThat(mMediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setFrameWidth(
                                           FrameWidthCondition.newBuilder()
                                                   .setWidth(smallWidth)
                                                   .setCondition(ComparisonCondition.GREATER_THAN))
                                   .build()))
                .isTrue();
        assertThat(mMediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setFrameWidth(
                                           FrameWidthCondition.newBuilder()
                                                   .setWidth(bigWidth)
                                                   .setCondition(ComparisonCondition.GREATER_THAN))
                                   .build()))
                .isFalse();
        assertThat(
                mMediaQueryHelper.isMediaQueryMet(
                        MediaQueryCondition.newBuilder()
                                .setFrameWidth(FrameWidthCondition.newBuilder()
                                                       .setWidth(bigWidth)
                                                       .setCondition(ComparisonCondition.LESS_THAN))
                                .build()))
                .isTrue();
        assertThat(
                mMediaQueryHelper.isMediaQueryMet(
                        MediaQueryCondition.newBuilder()
                                .setFrameWidth(FrameWidthCondition.newBuilder()
                                                       .setWidth(smallWidth)
                                                       .setCondition(ComparisonCondition.LESS_THAN))
                                .build()))
                .isFalse();
        assertThat(mMediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setFrameWidth(
                                           FrameWidthCondition.newBuilder()
                                                   .setWidth(smallWidth)
                                                   .setCondition(ComparisonCondition.NOT_EQUALS))
                                   .build()))
                .isTrue();
        assertThat(mMediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setFrameWidth(
                                           FrameWidthCondition.newBuilder()
                                                   .setWidth(midWidth)
                                                   .setCondition(ComparisonCondition.NOT_EQUALS))
                                   .build()))
                .isFalse();

        assertThatRunnable(
                ()
                        -> mMediaQueryHelper.isMediaQueryMet(
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
        mContext.getResources().getConfiguration().orientation =
                Configuration.ORIENTATION_LANDSCAPE;
        mMediaQueryHelper = new MediaQueryHelper(DEFAULT_FRAME_WIDTH, mAssetProvider, mContext);
        assertThat(mMediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setOrientation(OrientationCondition.newBuilder().setOrientation(
                                           Orientation.LANDSCAPE))
                                   .build()))
                .isTrue();
        assertThat(mMediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setOrientation(OrientationCondition.newBuilder().setOrientation(
                                           Orientation.PORTRAIT))
                                   .build()))
                .isFalse();

        mContext.getResources().getConfiguration().orientation = Configuration.ORIENTATION_PORTRAIT;
        mMediaQueryHelper = new MediaQueryHelper(DEFAULT_FRAME_WIDTH, mAssetProvider, mContext);
        assertThat(mMediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setOrientation(OrientationCondition.newBuilder().setOrientation(
                                           Orientation.LANDSCAPE))
                                   .build()))
                .isFalse();
        assertThat(mMediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setOrientation(OrientationCondition.newBuilder().setOrientation(
                                           Orientation.PORTRAIT))
                                   .build()))
                .isTrue();
        assertThat(mMediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setOrientation(OrientationCondition.newBuilder().setOrientation(
                                           Orientation.UNSPECIFIED))
                                   .build()))
                .isTrue();

        mContext.getResources().getConfiguration().orientation =
                Configuration.ORIENTATION_UNDEFINED;
        mMediaQueryHelper = new MediaQueryHelper(DEFAULT_FRAME_WIDTH, mAssetProvider, mContext);
        assertThat(mMediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setOrientation(OrientationCondition.newBuilder().setOrientation(
                                           Orientation.LANDSCAPE))
                                   .build()))
                .isFalse();
        assertThat(mMediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setOrientation(OrientationCondition.newBuilder().setOrientation(
                                           Orientation.PORTRAIT))
                                   .build()))
                .isFalse();
        assertThat(mMediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setOrientation(OrientationCondition.newBuilder().setOrientation(
                                           Orientation.UNSPECIFIED))
                                   .build()))
                .isFalse();
    }

    @Test
    public void testMediaQueryConditions_darkLight() {
        when(mAssetProvider.isDarkTheme()).thenReturn(true);
        mMediaQueryHelper = new MediaQueryHelper(DEFAULT_FRAME_WIDTH, mAssetProvider, mContext);
        assertThat(mMediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setDarkLight(DarkLightCondition.newBuilder().setMode(
                                           DarkLightMode.DARK))
                                   .build()))
                .isTrue();
        assertThat(mMediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setDarkLight(DarkLightCondition.newBuilder().setMode(
                                           DarkLightMode.LIGHT))
                                   .build()))
                .isFalse();
        assertThat(mMediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setDarkLight(DarkLightCondition.newBuilder().setMode(
                                           DarkLightMode.UNSPECIFIED))
                                   .build()))
                .isFalse();

        when(mAssetProvider.isDarkTheme()).thenReturn(false);
        mMediaQueryHelper = new MediaQueryHelper(DEFAULT_FRAME_WIDTH, mAssetProvider, mContext);
        assertThat(mMediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setDarkLight(DarkLightCondition.newBuilder().setMode(
                                           DarkLightMode.DARK))
                                   .build()))
                .isFalse();
        assertThat(mMediaQueryHelper.isMediaQueryMet(
                           MediaQueryCondition.newBuilder()
                                   .setDarkLight(DarkLightCondition.newBuilder().setMode(
                                           DarkLightMode.LIGHT))
                                   .build()))
                .isTrue();
        assertThat(mMediaQueryHelper.isMediaQueryMet(
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

        mContext.getResources().getConfiguration().orientation =
                Configuration.ORIENTATION_LANDSCAPE;

        when(mAssetProvider.isDarkTheme()).thenReturn(true);
        mMediaQueryHelper = new MediaQueryHelper(DEFAULT_FRAME_WIDTH, mAssetProvider, mContext);
        assertThat(mMediaQueryHelper.areMediaQueriesMet(conditions)).isTrue();

        when(mAssetProvider.isDarkTheme()).thenReturn(false);
        mMediaQueryHelper = new MediaQueryHelper(DEFAULT_FRAME_WIDTH, mAssetProvider, mContext);
        assertThat(mMediaQueryHelper.areMediaQueriesMet(conditions)).isFalse();
    }
}
