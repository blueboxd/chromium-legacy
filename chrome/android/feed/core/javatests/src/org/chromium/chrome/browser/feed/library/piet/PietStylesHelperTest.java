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

import com.google.common.collect.ImmutableList;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.piet.PietStylesHelper.PietStylesHelperFactory;
import org.chromium.chrome.browser.feed.library.piet.PietStylesHelper.PietStylesHelperKey;
import org.chromium.chrome.browser.feed.library.piet.host.AssetProvider;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.StyleBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.GradientsProto.ColorStop;
import org.chromium.components.feed.core.proto.ui.piet.GradientsProto.Fill;
import org.chromium.components.feed.core.proto.ui.piet.GradientsProto.LinearGradient;
import org.chromium.components.feed.core.proto.ui.piet.MediaQueriesProto.DarkLightCondition;
import org.chromium.components.feed.core.proto.ui.piet.MediaQueriesProto.DarkLightCondition.DarkLightMode;
import org.chromium.components.feed.core.proto.ui.piet.MediaQueriesProto.MediaQueryCondition;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.PietSharedState;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Stylesheet;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Stylesheets;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Template;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.BoundStyle;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Font;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Style;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.StyleIdsStack;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Tests of the {@link PietStylesHelper}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PietStylesHelperTest {
    private static final String STYLESHEET_ID_1 = "ss1";
    private static final String STYLESHEET_ID_2 = "ss2";
    private static final String STYLE_ID_1 = "s1";
    private static final String STYLE_ID_2 = "s2";
    private static final Style STYLE_1 = Style.newBuilder().setStyleId(STYLE_ID_1).build();
    private static final Style STYLE_2 = Style.newBuilder().setStyleId(STYLE_ID_2).build();
    private static final String TEMPLATE_ID_1 = "t1";
    private static final String TEMPLATE_ID_2 = "t2";
    private static final Template TEMPLATE_1 =
            Template.newBuilder().setTemplateId(TEMPLATE_ID_1).build();
    private static final Template TEMPLATE_2 =
            Template.newBuilder().setTemplateId(TEMPLATE_ID_2).build();

    private static final MediaQueryCondition DARK_CONDITION =
            MediaQueryCondition.newBuilder()
                    .setDarkLight(DarkLightCondition.newBuilder().setMode(DarkLightMode.DARK))
                    .build();
    private static final MediaQueryCondition LIGHT_CONDITION =
            MediaQueryCondition.newBuilder()
                    .setDarkLight(DarkLightCondition.newBuilder().setMode(DarkLightMode.LIGHT))
                    .build();

    private static final int FRAME_WIDTH_PX = 480;

    private List<PietSharedState> mSharedStates = new ArrayList<>();
    @Mock
    FrameContext mFrameContext;
    @Mock
    AssetProvider mAssetProvider;
    DebugLogger mDebugLogger;

    private Context mContext;
    private PietStylesHelperFactory mHelperFactory;
    private PietStylesHelper mHelper;
    private PietSharedState mSharedState1;

    @Before
    public void setUp() throws Exception {
        mContext = Robolectric.buildActivity(Activity.class).get();
        initMocks(this);
        Stylesheet.Builder stylesheet1 =
                Stylesheet.newBuilder().setStylesheetId(STYLESHEET_ID_1).addStyles(STYLE_1);

        Stylesheet.Builder stylesheet2 =
                Stylesheet.newBuilder().setStylesheetId(STYLESHEET_ID_2).addStyles(STYLE_2);

        mSharedState1 = PietSharedState.newBuilder()
                                .addStylesheets(stylesheet1.build())
                                .addTemplates(TEMPLATE_1)
                                .build();

        PietSharedState sharedState2 = PietSharedState.newBuilder()
                                               .addStylesheets(stylesheet2.build())
                                               .addTemplates(TEMPLATE_2)
                                               .build();
        mSharedStates.add(mSharedState1);
        mSharedStates.add(sharedState2);

        mDebugLogger = new DebugLogger();

        mHelperFactory = new PietStylesHelperFactory();
        mHelper = mHelperFactory.get(mSharedStates, newMediaQueryHelper());
    }

    @Test
    public void testConstructorRespectsMediaQueryConditions_stylesheets() {
        Stylesheet darkStylesheet = Stylesheet.newBuilder()
                                            .setStylesheetId("DARK")
                                            .addConditions(DARK_CONDITION)
                                            .addStyles(STYLE_1)
                                            .build();
        Stylesheet lightStylesheet = Stylesheet.newBuilder()
                                             .setStylesheetId("LIGHT")
                                             .addConditions(LIGHT_CONDITION)
                                             .addStyles(STYLE_2)
                                             .build();

        Style darkStyle = Style.newBuilder().setStyleId("dark").build();
        Style lightStyle = Style.newBuilder().setStyleId("light").build();
        Stylesheet switchStylesheetDark = Stylesheet.newBuilder()
                                                  .setStylesheetId("SWITCH")
                                                  .addConditions(DARK_CONDITION)
                                                  .addStyles(darkStyle)
                                                  .build();
        Stylesheet switchStylesheetLight = Stylesheet.newBuilder()
                                                   .setStylesheetId("SWITCH")
                                                   .addConditions(LIGHT_CONDITION)
                                                   .addStyles(lightStyle)
                                                   .build();

        PietSharedState sharedState = PietSharedState.newBuilder()
                                              .addStylesheets(darkStylesheet)
                                              .addStylesheets(lightStylesheet)
                                              .addStylesheets(switchStylesheetDark)
                                              .addStylesheets(switchStylesheetLight)
                                              .build();

        when(mAssetProvider.isDarkTheme()).thenReturn(true);
        mHelper = mHelperFactory.get(Collections.singletonList(sharedState), newMediaQueryHelper());

        assertThat(mHelper.getStylesheetMap(
                           Stylesheets.newBuilder().addStylesheetIds("DARK").build(), mDebugLogger))
                .containsExactly(STYLE_ID_1, STYLE_1);
        assertThat(
                mHelper.getStylesheetMap(
                        Stylesheets.newBuilder().addStylesheetIds("LIGHT").build(), mDebugLogger))
                .isEmpty();
        assertThat(
                mHelper.getStylesheetMap(
                        Stylesheets.newBuilder().addStylesheetIds("SWITCH").build(), mDebugLogger))
                .containsExactly("dark", darkStyle);

        when(mAssetProvider.isDarkTheme()).thenReturn(false);
        mHelper = mHelperFactory.get(Collections.singletonList(sharedState), newMediaQueryHelper());

        assertThat(mHelper.getStylesheetMap(
                           Stylesheets.newBuilder().addStylesheetIds("DARK").build(), mDebugLogger))
                .isEmpty();
        assertThat(
                mHelper.getStylesheetMap(
                        Stylesheets.newBuilder().addStylesheetIds("LIGHT").build(), mDebugLogger))
                .containsExactly(STYLE_ID_2, STYLE_2);
        assertThat(
                mHelper.getStylesheetMap(
                        Stylesheets.newBuilder().addStylesheetIds("SWITCH").build(), mDebugLogger))
                .containsExactly("light", lightStyle);
    }

    @Test
    public void testConstructorRespectsMediaQueryConditions_templates() {
        Template darkTemplate =
                Template.newBuilder().setTemplateId("DARK").addConditions(DARK_CONDITION).build();
        Template lightTemplate =
                Template.newBuilder().setTemplateId("LIGHT").addConditions(LIGHT_CONDITION).build();
        Template switchTemplateDark =
                Template.newBuilder().setTemplateId("SWITCH").addConditions(DARK_CONDITION).build();
        Template switchTemplateLight = Template.newBuilder()
                                               .setTemplateId("SWITCH")
                                               .addConditions(LIGHT_CONDITION)
                                               .build();

        PietSharedState sharedState = PietSharedState.newBuilder()
                                              .addTemplates(darkTemplate)
                                              .addTemplates(lightTemplate)
                                              .addTemplates(switchTemplateDark)
                                              .addTemplates(switchTemplateLight)
                                              .build();

        when(mAssetProvider.isDarkTheme()).thenReturn(true);
        mHelper = mHelperFactory.get(Collections.singletonList(sharedState), newMediaQueryHelper());

        assertThat(mHelper.getTemplate("DARK")).isSameInstanceAs(darkTemplate);
        assertThat(mHelper.getTemplate("LIGHT")).isNull();
        assertThat(mHelper.getTemplate("SWITCH")).isSameInstanceAs(switchTemplateDark);

        when(mAssetProvider.isDarkTheme()).thenReturn(false);
        mHelper = mHelperFactory.get(Collections.singletonList(sharedState), newMediaQueryHelper());

        assertThat(mHelper.getTemplate("DARK")).isNull();
        assertThat(mHelper.getTemplate("LIGHT")).isSameInstanceAs(lightTemplate);
        assertThat(mHelper.getTemplate("SWITCH")).isSameInstanceAs(switchTemplateLight);
    }

    @Test
    public void testGetStylesheet() {
        Map<String, Style> resultMap1 = mHelper.getStylesheetMap(
                Stylesheets.newBuilder().addStylesheetIds(STYLESHEET_ID_1).build(), mDebugLogger);
        assertThat(resultMap1).containsExactly(STYLE_ID_1, STYLE_1);

        // Retrieve map again to test caching
        assertThat(mHelper.getStylesheetMap(
                           Stylesheets.newBuilder().addStylesheetIds(STYLESHEET_ID_1).build(),
                           mDebugLogger))
                .isSameInstanceAs(resultMap1);

        Map<String, Style> resultMap2 = mHelper.getStylesheetMap(
                Stylesheets.newBuilder().addStylesheetIds(STYLESHEET_ID_2).build(), mDebugLogger);
        assertThat(resultMap2).containsExactly(STYLE_ID_2, STYLE_2);
        assertThat(mHelper.getStylesheetMap(
                           Stylesheets.newBuilder().addStylesheetIds(STYLESHEET_ID_2).build(),
                           mDebugLogger))
                .isSameInstanceAs(resultMap2);
    }

    @Test
    public void testGetStylesheet_withSameStylesheetId() {
        List<PietSharedState> sharedStates = new ArrayList<>();
        Stylesheet.Builder stylesheet =
                Stylesheet.newBuilder().setStylesheetId(STYLESHEET_ID_1).addStyles(STYLE_2);
        PietSharedState sharedState2 = PietSharedState.newBuilder()
                                               .addStylesheets(stylesheet.build())
                                               .addTemplates(TEMPLATE_2)
                                               .build();
        sharedStates.add(mSharedState1);
        sharedStates.add(sharedState2);

        assertThatRunnable(() -> mHelperFactory.get(sharedStates, newMediaQueryHelper()))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Stylesheet key 'ss1' already defined");
    }

    @Test
    public void testGetStylesheet_notExist() {
        assertThat(mHelper.getStylesheetMap(
                           Stylesheets.newBuilder().addStylesheetIds("NOT EXIST").build(),
                           mDebugLogger))
                .isEmpty();
    }

    @Test
    public void testCreateMapFromStylesheet() {
        Stylesheet.Builder myStyles = Stylesheet.newBuilder();
        Style sixties = Style.newBuilder().setStyleId("60s").build();
        myStyles.addStyles(sixties);
        Style eighties = Style.newBuilder().setStyleId("80s").build();
        myStyles.addStyles(eighties);

        assertThat(mHelper.getStylesheetMap(
                           Stylesheets.newBuilder().addStylesheets(myStyles.build()).build(),
                           mDebugLogger))
                .containsExactly("60s", sixties, "80s", eighties);
    }

    @Test
    public void testCreateMapFromStylesheet_throwsWhenDuplicateStyleId() {
        Stylesheet.Builder myStyles = Stylesheet.newBuilder();
        Style sixties = Style.newBuilder().setStyleId("60s").build();
        myStyles.addStyles(sixties);
        Style sixtiesAgain = Style.newBuilder().setStyleId("60s").build();
        myStyles.addStyles(sixtiesAgain);

        assertThatRunnable(
                ()
                        -> mHelper.getStylesheetMap(
                                Stylesheets.newBuilder().addStylesheets(myStyles.build()).build(),
                                mDebugLogger))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Style key '60s' already defined");
    }

    @Test
    public void testCreateMapFromStylesheet_filtersByMediaQuery() {
        Stylesheet.Builder myStyles = Stylesheet.newBuilder();
        Style lightStyle =
                Style.newBuilder()
                        .setStyleId("theme")
                        .addConditions(MediaQueryCondition.newBuilder().setDarkLight(
                                DarkLightCondition.newBuilder().setMode(DarkLightMode.LIGHT)))
                        .build();
        myStyles.addStyles(lightStyle);
        Style darkStyle =
                Style.newBuilder()
                        .setStyleId("theme")
                        .addConditions(MediaQueryCondition.newBuilder().setDarkLight(
                                DarkLightCondition.newBuilder().setMode(DarkLightMode.DARK)))
                        .build();
        myStyles.addStyles(darkStyle);

        when(mAssetProvider.isDarkTheme()).thenReturn(true);
        mHelper = mHelperFactory.get(mSharedStates, newMediaQueryHelper());
        assertThat(mHelper.getStylesheetMap(
                           Stylesheets.newBuilder().addStylesheets(myStyles.build()).build(),
                           mDebugLogger))
                .containsExactly("theme", darkStyle);

        when(mAssetProvider.isDarkTheme()).thenReturn(false);
        mHelper = mHelperFactory.get(mSharedStates, newMediaQueryHelper());
        assertThat(mHelper.getStylesheetMap(
                           Stylesheets.newBuilder().addStylesheets(myStyles.build()).build(),
                           mDebugLogger))
                .containsExactly("theme", lightStyle);
    }

    @Test
    public void testGetTemplate() {
        assertThat(mHelper.getTemplate(TEMPLATE_ID_1)).isEqualTo(TEMPLATE_1);
        assertThat(mHelper.getTemplate(TEMPLATE_ID_2)).isEqualTo(TEMPLATE_2);
    }

    @Test
    public void testGetTemplate_notExist() {
        assertThat(mHelper.getTemplate("NOT EXIST")).isNull();
    }

    @Test
    public void testMergeStyleIdsStack() {
        // These constants are in the final output and are not overridden
        int baseWidth = 999;
        int style1Color = 12345;
        boolean style1Italic = true;
        int style2MaxLines = 22222;
        int style2MinHeight = 33333;
        int style2FontSize = 13;
        int style1GradientColor = 1234;
        int style2GradientDirection = 321;
        int boundStyleColor = 5050;
        Fill boundStyleBackground =
                Fill.newBuilder()
                        .setLinearGradient(
                                LinearGradient.newBuilder().setDirectionDeg(boundStyleColor))
                        .build();

        Style baseStyle = Style.newBuilder()
                                  .setWidth(baseWidth) // Not overridden
                                  .setColor(888) // Overridden
                                  .build();
        String styleId1 = "STYLE1";
        Style style1 =
                Style.newBuilder()
                        .setColor(style1Color) // Not overridden
                        .setMaxLines(54321) // Overridden
                        .setFont(Font.newBuilder().setSize(11).setItalic(style1Italic))
                        .setBackground(Fill.newBuilder().setLinearGradient(
                                LinearGradient.newBuilder().addStops(
                                        ColorStop.newBuilder().setColor(style1GradientColor))))
                        .build();
        String styleId2 = "STYLE2";
        Style style2 =
                Style.newBuilder()
                        .setMaxLines(style2MaxLines) // Overrides
                        .setMinHeight(style2MinHeight) // Not an override
                        .setFont(Font.newBuilder().setSize(style2FontSize)) // Just override size
                        .setBackground(Fill.newBuilder().setLinearGradient(
                                LinearGradient.newBuilder().setDirectionDeg(
                                        style2GradientDirection)))
                        .build();
        Map<String, Style> stylesheetMap = new HashMap<>();
        stylesheetMap.put(styleId1, style1);
        stylesheetMap.put(styleId2, style2);

        // Test merging a style IDs stack
        Style mergedStyle = PietStylesHelper.mergeStyleIdsStack(baseStyle,
                StyleIdsStack.newBuilder().addStyleIds(styleId1).addStyleIds(styleId2).build(),
                stylesheetMap, mFrameContext);

        assertThat(mergedStyle)
                .isEqualTo(Style.newBuilder()
                                   .setColor(style1Color)
                                   .setMaxLines(style2MaxLines)
                                   .setMinHeight(style2MinHeight)
                                   .setFont(Font.newBuilder()
                                                    .setSize(style2FontSize)
                                                    .setItalic(style1Italic))
                                   .setBackground(Fill.newBuilder().setLinearGradient(
                                           LinearGradient.newBuilder()
                                                   .addStops(ColorStop.newBuilder().setColor(
                                                           style1GradientColor))
                                                   .setDirectionDeg(style2GradientDirection)))
                                   .setWidth(baseWidth)
                                   .build());

        // Test merging a style IDs stack with a bound style
        String styleBindingId = "BOUND_STYLE";
        BoundStyle boundStyle = BoundStyle.newBuilder()
                                        .setColor(boundStyleColor)
                                        .setBackground(boundStyleBackground)
                                        .build();
        StyleBindingRef styleBindingRef =
                StyleBindingRef.newBuilder().setBindingId(styleBindingId).build();
        when(mFrameContext.getStyleFromBinding(styleBindingRef)).thenReturn(boundStyle);

        mergedStyle = PietStylesHelper.mergeStyleIdsStack(baseStyle,
                StyleIdsStack.newBuilder()
                        .addStyleIds(styleId1)
                        .addStyleIds(styleId2)
                        .setStyleBinding(styleBindingRef)
                        .build(),
                stylesheetMap, mFrameContext);
        assertThat(mergedStyle)
                .isEqualTo(Style.newBuilder()
                                   .setColor(boundStyleColor)
                                   .setMaxLines(style2MaxLines)
                                   .setMinHeight(style2MinHeight)
                                   .setFont(Font.newBuilder()
                                                    .setSize(style2FontSize)
                                                    .setItalic(style1Italic))
                                   .setBackground(Fill.newBuilder().setLinearGradient(
                                           LinearGradient.newBuilder()
                                                   .addStops(ColorStop.newBuilder().setColor(
                                                           style1GradientColor))
                                                   .setDirectionDeg(boundStyleColor)))
                                   .setWidth(baseWidth)
                                   .build());

        // Binding styles fails when frameContext is null.
        assertThatRunnable(() -> {
            PietStylesHelper.mergeStyleIdsStack(Style.getDefaultInstance(),
                    StyleIdsStack.newBuilder()
                            .addStyleIds(styleId1)
                            .addStyleIds(styleId2)
                            .setStyleBinding(styleBindingRef)
                            .build(),
                    stylesheetMap, null);
        })
                .throwsAnExceptionOfType(NullPointerException.class)
                .that()
                .hasMessageThat()
                .contains("Binding styles not supported when frameContext is null");
    }

    @Test
    public void testHashCodeAndEquals() {
        PietSharedState sharedState = PietSharedState.newBuilder()
                                              .addStylesheets(Stylesheet.newBuilder().addStyles(
                                                      Style.newBuilder().setStyleId("style")))
                                              .build();
        PietSharedState equalSharedState =
                PietSharedState.newBuilder()
                        .addStylesheets(Stylesheet.newBuilder().addStyles(
                                Style.newBuilder().setStyleId("style")))
                        .build();
        PietSharedState differentSharedState =
                PietSharedState.newBuilder()
                        .addStylesheets(Stylesheet.newBuilder().addStyles(
                                Style.newBuilder().setStyleId("NOT_SAME")))
                        .build();

        MediaQueryHelper mediaQueryHelper = new MediaQueryHelper(123, 4, false, mContext);
        MediaQueryHelper equalMediaQueryHelper = new MediaQueryHelper(123, 4, false, mContext);
        MediaQueryHelper differentMediaQueryHelper = new MediaQueryHelper(456, 7, false, mContext);

        // Objects that should be equal
        assertThat(ImmutableList.of(sharedState).hashCode())
                .isEqualTo(ImmutableList.of(sharedState).hashCode());
        assertThat(ImmutableList.of(sharedState).hashCode())
                .isEqualTo(ImmutableList.of(equalSharedState).hashCode());
        assertThat(new PietStylesHelperKey(ImmutableList.of(sharedState), mediaQueryHelper))
                .isEqualTo(
                        new PietStylesHelperKey(ImmutableList.of(sharedState), mediaQueryHelper));
        assertThat(new PietStylesHelperKey(ImmutableList.of(sharedState), mediaQueryHelper))
                .isEqualTo(new PietStylesHelperKey(
                        ImmutableList.of(equalSharedState), equalMediaQueryHelper));
        assertThat(new PietStylesHelperKey(
                           ImmutableList.of(sharedState, differentSharedState), mediaQueryHelper))
                .isEqualTo(
                        new PietStylesHelperKey(ImmutableList.of(sharedState, differentSharedState),
                                equalMediaQueryHelper));
        assertThat(new PietStylesHelperKey(
                           ImmutableList.of(sharedState, differentSharedState), mediaQueryHelper))
                .isEqualTo(new PietStylesHelperKey(
                        ImmutableList.of(differentSharedState, equalSharedState),
                        equalMediaQueryHelper));
        assertThat(new PietStylesHelperKey(
                           ImmutableList.of(sharedState, differentSharedState), mediaQueryHelper))
                .isEqualTo(new PietStylesHelperKey(
                        Arrays.asList(sharedState, differentSharedState), equalMediaQueryHelper));

        assertThat(
                new PietStylesHelperKey(ImmutableList.of(sharedState), mediaQueryHelper).hashCode())
                .isEqualTo(new PietStylesHelperKey(ImmutableList.of(sharedState), mediaQueryHelper)
                                   .hashCode());
        assertThat(
                new PietStylesHelperKey(ImmutableList.of(sharedState), mediaQueryHelper).hashCode())
                .isEqualTo(new PietStylesHelperKey(
                        ImmutableList.of(equalSharedState), equalMediaQueryHelper)
                                   .hashCode());
        assertThat(new PietStylesHelperKey(
                           ImmutableList.of(sharedState, differentSharedState), mediaQueryHelper)
                           .hashCode())
                .isEqualTo(new PietStylesHelperKey(
                        ImmutableList.of(sharedState, differentSharedState), equalMediaQueryHelper)
                                   .hashCode());

        // Objects that should be different
        assertThat(new PietStylesHelperKey(ImmutableList.of(sharedState), mediaQueryHelper))
                .isNotEqualTo(new PietStylesHelperKey(
                        ImmutableList.of(differentSharedState), mediaQueryHelper));
        assertThat(new PietStylesHelperKey(ImmutableList.of(sharedState), mediaQueryHelper))
                .isNotEqualTo(new PietStylesHelperKey(
                        ImmutableList.of(equalSharedState), differentMediaQueryHelper));
        assertThat(new PietStylesHelperKey(ImmutableList.of(sharedState), mediaQueryHelper))
                .isNotEqualTo(new PietStylesHelperKey(
                        ImmutableList.of(sharedState, differentSharedState), mediaQueryHelper));

        assertThat(
                new PietStylesHelperKey(ImmutableList.of(sharedState), mediaQueryHelper).hashCode())
                .isNotEqualTo(new PietStylesHelperKey(
                        ImmutableList.of(differentSharedState), mediaQueryHelper)
                                      .hashCode());
        assertThat(
                new PietStylesHelperKey(ImmutableList.of(sharedState), mediaQueryHelper).hashCode())
                .isNotEqualTo(new PietStylesHelperKey(
                        ImmutableList.of(equalSharedState), differentMediaQueryHelper)
                                      .hashCode());
        assertThat(
                new PietStylesHelperKey(ImmutableList.of(sharedState), mediaQueryHelper).hashCode())
                .isNotEqualTo(new PietStylesHelperKey(
                        ImmutableList.of(sharedState, differentSharedState), mediaQueryHelper)
                                      .hashCode());
    }

    private MediaQueryHelper newMediaQueryHelper() {
        return new MediaQueryHelper(FRAME_WIDTH_PX, mAssetProvider, mContext);
    }
}
