// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;

import android.app.Activity;
import android.content.Context;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.config.DebugBehavior;
import org.chromium.chrome.browser.feed.library.common.ui.LayoutUtils;
import org.chromium.chrome.browser.feed.library.piet.DebugLogger.MessageType;
import org.chromium.chrome.browser.feed.library.piet.PietStylesHelper.PietStylesHelperFactory;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler;
import org.chromium.chrome.browser.feed.library.piet.host.AssetProvider;
import org.chromium.chrome.browser.feed.library.piet.host.CustomElementProvider;
import org.chromium.chrome.browser.feed.library.piet.host.HostBindingProvider;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.Actions;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ActionsBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ChunkedTextBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.CustomBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ElementBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.GridCellWidthBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ImageBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.LogDataBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ParameterizedTextBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.StyleBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.TemplateBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.VisibilityBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingContext;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingValue;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.CustomElementData;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.GridCellWidth;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.HostBindingData;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.TemplateInvocation;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.TextElement;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Visibility;
import org.chromium.components.feed.core.proto.ui.piet.GradientsProto.ColorStop;
import org.chromium.components.feed.core.proto.ui.piet.GradientsProto.Fill;
import org.chromium.components.feed.core.proto.ui.piet.GradientsProto.LinearGradient;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.Image;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.ImageSource;
import org.chromium.components.feed.core.proto.ui.piet.LogDataProto.LogData;
import org.chromium.components.feed.core.proto.ui.piet.MediaQueriesProto.ComparisonCondition;
import org.chromium.components.feed.core.proto.ui.piet.MediaQueriesProto.DarkLightCondition;
import org.chromium.components.feed.core.proto.ui.piet.MediaQueriesProto.DarkLightCondition.DarkLightMode;
import org.chromium.components.feed.core.proto.ui.piet.MediaQueriesProto.FrameWidthCondition;
import org.chromium.components.feed.core.proto.ui.piet.MediaQueriesProto.MediaQueryCondition;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Frame;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.PietSharedState;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Stylesheet;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Stylesheets;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Template;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.BoundStyle;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Font;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Style;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.StyleIdsStack;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.Chunk;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.ChunkedText;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.ParameterizedText;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.StyledTextChunk;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Tests of the {@link FrameContext}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FrameContextTest {
    private static final String DEFAULT_TEMPLATE_ID = "TEMPLATE_ID";
    private static final Template DEFAULT_TEMPLATE =
            Template.newBuilder().setTemplateId(DEFAULT_TEMPLATE_ID).build();
    private static final Frame DEFAULT_FRAME =
            Frame.newBuilder().addTemplates(DEFAULT_TEMPLATE).build();
    private static final String SAMPLE_STYLE_ID = "STYLE_ID";
    private static final int BASE_STYLE_COLOR = 111111;
    private static final int SAMPLE_STYLE_COLOR = 888888;
    private static final Style SAMPLE_STYLE =
            Style.newBuilder().setStyleId(SAMPLE_STYLE_ID).setColor(SAMPLE_STYLE_COLOR).build();
    private static final String STYLESHEET_ID = "STYLESHEET_ID";
    private static final Style BASE_STYLE = Style.newBuilder().setColor(BASE_STYLE_COLOR).build();
    private static final StyleIdsStack SAMPLE_STYLE_IDS =
            StyleIdsStack.newBuilder().addStyleIds(SAMPLE_STYLE_ID).build();
    private static final String BINDING_ID = "BINDING_ID";
    private static final String INVALID_BINDING_ID = "NOT_A_REAL_BINDING_ID";
    private static final Map<String, Template> DEFAULT_TEMPLATES = new HashMap<>();
    private static final int FRAME_WIDTH_PX = 999999;

    static {
        DEFAULT_TEMPLATES.put(DEFAULT_TEMPLATE_ID, DEFAULT_TEMPLATE);
    }

    @Mock
    private ActionHandler mActionHandler;
    @Mock
    private AssetProvider mAssetProvider;

    private final Map<String, Style> mDefaultStylesheet = new HashMap<>();
    private final DebugLogger mDebugLogger = new DebugLogger();

    private HostBindingProvider mHostBindingProvider;
    private HostProviders mHostProviders;
    private FrameContext mFrameContext;
    private List<PietSharedState> mPietSharedStates;
    private PietStylesHelper mPietStylesHelper;
    private StyleProvider mDefaultStyleProvider;
    private Context mContext;
    private FrameLayout mFrameView;

    @Before
    public void setUp() throws Exception {
        initMocks(this);

        mContext = Robolectric.buildActivity(Activity.class).get();
        mHostBindingProvider = new HostBindingProvider();
        mHostProviders = new HostProviders(
                mAssetProvider, mock(CustomElementProvider.class), mHostBindingProvider, null);
        mDefaultStyleProvider = new StyleProvider(mAssetProvider);

        mDefaultStylesheet.put(SAMPLE_STYLE_ID, SAMPLE_STYLE);
        mPietSharedStates = new ArrayList<>();
        mPietStylesHelper = newPietStylesHelper();

        mFrameView = new FrameLayout(mContext);
    }

    @Test
    public void testSharedStateTemplatesAddedToFrame() {
        String sharedStateTemplateId = "SHARED_STATE_TEMPLATE_ID";
        Template sharedStateTemplate =
                Template.newBuilder().setTemplateId(sharedStateTemplateId).build();
        mPietSharedStates.add(
                PietSharedState.newBuilder().addTemplates(sharedStateTemplate).build());
        mFrameContext = defaultFrameContext();

        assertThat(mFrameContext.getTemplate(sharedStateTemplateId))
                .isSameInstanceAs(sharedStateTemplate);
        assertThat(mFrameContext.getTemplate(DEFAULT_TEMPLATE_ID))
                .isSameInstanceAs(DEFAULT_TEMPLATE);
    }

    @Test
    public void testThrowsIfSharedStateTemplatesConflictWithFrameTemplates() {
        Template sharedStateTemplate =
                Template.newBuilder().setTemplateId(DEFAULT_TEMPLATE_ID).build();
        mPietSharedStates.add(
                PietSharedState.newBuilder().addTemplates(sharedStateTemplate).build());
        assertThatRunnable(this::defaultFrameContext)
                .throwsAnExceptionOfType(IllegalArgumentException.class)
                .that()
                .hasMessageThat()
                .contains("Template key 'TEMPLATE_ID' already defined");
    }

    @Test
    public void testThrowsIfDuplicateBindingValueId() {
        mFrameContext = defaultFrameContext();
        BindingContext bindingContextWithDuplicateIds =
                BindingContext.newBuilder()
                        .addBindingValues(BindingValue.newBuilder().setBindingId(BINDING_ID))
                        .addBindingValues(BindingValue.newBuilder().setBindingId(BINDING_ID))
                        .build();
        assertThatRunnable(()
                                   -> mFrameContext.createTemplateContext(
                                           DEFAULT_TEMPLATE, bindingContextWithDuplicateIds))
                .throwsAnExceptionOfType(IllegalArgumentException.class)
                .that()
                .hasMessageThat()
                .contains("BindingValue key 'BINDING_ID' already defined");
    }

    @Test
    public void testGetters() {
        String template2BindingId = "TEMPLATE_2";
        Template template2 = Template.newBuilder().setTemplateId(template2BindingId).build();
        PietSharedState sharedState = PietSharedState.newBuilder().addTemplates(template2).build();
        mPietSharedStates.add(sharedState);
        mFrameContext = new FrameContext(DEFAULT_FRAME, mDefaultStylesheet, mPietSharedStates,
                newPietStylesHelper(), DebugBehavior.VERBOSE, mDebugLogger, mActionHandler,
                mHostProviders, mFrameView);

        assertThat(mFrameContext.getFrame()).isEqualTo(DEFAULT_FRAME);
        assertThat(mFrameContext.getActionHandler()).isEqualTo(mActionHandler);

        assertThat(mFrameContext.getTemplate(DEFAULT_TEMPLATE_ID)).isEqualTo(DEFAULT_TEMPLATE);
        assertThat(mFrameContext.getTemplate(template2BindingId)).isEqualTo(template2);
    }

    @Test
    public void testThrowsWithNoBindingContext() {
        mFrameContext = makeFrameContextForDefaultFrame();

        assertThatRunnable(
                () -> mFrameContext.getActionsFromBinding(ActionsBindingRef.getDefaultInstance()))
                .throwsAnExceptionOfType(IllegalArgumentException.class)
                .that()
                .hasMessageThat()
                .contains("no BindingValues defined");
    }

    @Test
    public void testCreateTemplateContext_lotsOfStuff() {
        mFrameContext = defaultFrameContext();

        // createTemplateContext should add new BindingValues
        ParameterizedText text = ParameterizedText.newBuilder().setText("Calico").build();
        BindingValue textBinding = BindingValue.newBuilder()
                                           .setBindingId(BINDING_ID)
                                           .setParameterizedText(text)
                                           .build();
        FrameContext frameContextWithBindings =
                mFrameContext.createTemplateContext(DEFAULT_TEMPLATE,
                        BindingContext.newBuilder().addBindingValues(textBinding).build());
        assertThat(
                frameContextWithBindings
                        .getParameterizedTextBindingValue(ParameterizedTextBindingRef.newBuilder()
                                                                  .setBindingId(BINDING_ID)
                                                                  .build())
                        .getParameterizedText())
                .isEqualTo(text);

        // and clear out all the previous styles
        assertThat(frameContextWithBindings.makeStyleFor(StyleIdsStack.getDefaultInstance()))
                .isEqualTo(mDefaultStyleProvider);
        assertThat(frameContextWithBindings.makeStyleFor(SAMPLE_STYLE_IDS).getColor())
                .isEqualTo(mDefaultStyleProvider.getColor());
    }

    @Test
    public void testCreateTemplateContext_stylesheetId() {
        String styleId = "cotton";
        int width = 343;
        Style style = Style.newBuilder().setStyleId(styleId).setWidth(width).build();
        String stylesheetId = "linen";
        Stylesheet stylesheet =
                Stylesheet.newBuilder().setStylesheetId(stylesheetId).addStyles(style).build();
        mPietSharedStates.add(PietSharedState.newBuilder().addStylesheets(stylesheet).build());
        mFrameContext = defaultFrameContext();

        Template template =
                Template.newBuilder()
                        .setTemplateId("kingSize")
                        .setStylesheets(Stylesheets.newBuilder().addStylesheetIds(stylesheetId))
                        .build();

        FrameContext templateContext =
                mFrameContext.createTemplateContext(template, BindingContext.getDefaultInstance());

        StyleIdsStack styleRef = StyleIdsStack.newBuilder().addStyleIds(styleId).build();
        assertThat(templateContext.makeStyleFor(styleRef).getWidthSpecPx(mContext))
                .isEqualTo((int) LayoutUtils.dpToPx(width, mContext));
    }

    @Test
    public void testCreateTemplateContext_multipleStylesheets() {
        String styleId = "cotton";
        int width = 343;
        Style style = Style.newBuilder().setStyleId(styleId).setWidth(width).build();
        String stylesheetId = "linen";
        Stylesheet stylesheet =
                Stylesheet.newBuilder().setStylesheetId(stylesheetId).addStyles(style).build();
        mPietSharedStates.add(PietSharedState.newBuilder().addStylesheets(stylesheet).build());
        mFrameContext = defaultFrameContext();
        int templateStyleColor = 343434;
        String templateStyleId = "templateStyle";

        Template template =
                Template.newBuilder()
                        .setTemplateId("kingSize")
                        .setStylesheets(Stylesheets.newBuilder()
                                                .addStylesheetIds(stylesheetId)
                                                .addStylesheets(Stylesheet.newBuilder().addStyles(
                                                        Style.newBuilder()
                                                                .setStyleId(templateStyleId)
                                                                .setColor(templateStyleColor))))
                        .build();

        FrameContext templateContext =
                mFrameContext.createTemplateContext(template, BindingContext.getDefaultInstance());

        StyleIdsStack styleRef = StyleIdsStack.newBuilder().addStyleIds(styleId).build();
        assertThat(templateContext.makeStyleFor(styleRef).getWidthSpecPx(mContext))
                .isEqualTo((int) LayoutUtils.dpToPx(width, mContext));
        styleRef = StyleIdsStack.newBuilder().addStyleIds(templateStyleId).build();
        assertThat(templateContext.makeStyleFor(styleRef).getColor()).isEqualTo(templateStyleColor);
    }

    @Test
    public void testCreateTemplateContext_transcludingBinding() {
        String parentBindingId = "PARENT";
        String childBindingId = "CHILD";
        ParameterizedText parentBoundText =
                ParameterizedText.newBuilder().setText("parent_text").build();
        BindingValue parentBindingValue = BindingValue.newBuilder()
                                                  .setBindingId(parentBindingId)
                                                  .setParameterizedText(parentBoundText)
                                                  .build();
        ParameterizedTextBindingRef childTextBindingRef =
                ParameterizedTextBindingRef.newBuilder().setBindingId(childBindingId).build();
        mFrameContext = makeFrameContextWithBinding(parentBindingValue);
        FrameContext childContext = mFrameContext.createTemplateContext(
                Template.newBuilder()
                        .setElement(Element.newBuilder().setTextElement(
                                TextElement.newBuilder().setParameterizedTextBinding(
                                        childTextBindingRef)))
                        .build(),
                BindingContext.newBuilder()
                        .addBindingValues(
                                BindingValue.newBuilder()
                                        .setBindingId(childBindingId)
                                        .setBindingIdFromTranscludingTemplate(parentBindingId))
                        .build());

        assertThat(childContext.getParameterizedTextBindingValue(childTextBindingRef))
                .isEqualTo(BindingValue.newBuilder()
                                   .setBindingId(childBindingId)
                                   .setParameterizedText(parentBoundText)
                                   .build());
    }

    @Test
    public void testCreateTemplateContext_transcludingBindingNotFound() {
        String invalidParentBindingId = "NOT_FOUND";
        String childBindingId = "CHILD";
        ParameterizedTextBindingRef childTextBindingRef =
                ParameterizedTextBindingRef.newBuilder().setBindingId(childBindingId).build();
        mFrameContext = makeFrameContextWithNoBindings();
        FrameContext childContext = mFrameContext.createTemplateContext(
                Template.newBuilder()
                        .setElement(Element.newBuilder().setTextElement(
                                TextElement.newBuilder().setParameterizedTextBinding(
                                        childTextBindingRef)))
                        .build(),
                BindingContext.newBuilder()
                        .addBindingValues(BindingValue.newBuilder()
                                                  .setBindingId(childBindingId)
                                                  .setBindingIdFromTranscludingTemplate(
                                                          invalidParentBindingId))
                        .build());

        assertThat(mDebugLogger.getMessages(MessageType.ERROR)).isEmpty();
        assertThatRunnable(() -> childContext.getParameterizedTextBindingValue(childTextBindingRef))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Parameterized text binding not found for CHILD");
    }

    @Test
    public void testCreateTemplateContext_optionalTranscludingBindingNotFound() {
        String invalidParentBindingId = "NOT_FOUND";
        String childBindingId = "CHILD";
        ParameterizedTextBindingRef childTextBindingRef = ParameterizedTextBindingRef.newBuilder()
                                                                  .setBindingId(childBindingId)
                                                                  .setIsOptional(true)
                                                                  .build();
        mFrameContext = makeFrameContextWithNoBindings();
        FrameContext childContext = mFrameContext.createTemplateContext(
                Template.newBuilder()
                        .setElement(Element.newBuilder().setTextElement(
                                TextElement.newBuilder().setParameterizedTextBinding(
                                        childTextBindingRef)))
                        .build(),
                BindingContext.newBuilder()
                        .addBindingValues(BindingValue.newBuilder()
                                                  .setBindingId(childBindingId)
                                                  .setBindingIdFromTranscludingTemplate(
                                                          invalidParentBindingId))
                        .build());

        assertThat(mDebugLogger.getMessages(MessageType.ERROR)).isEmpty();
        assertThat(childContext.getParameterizedTextBindingValue(childTextBindingRef))
                .isEqualTo(BindingValue.getDefaultInstance());
    }

    @Test
    public void testMakeStyleFor() {
        mFrameContext = defaultFrameContext();

        // Returns base style provider if there are no styles defined
        StyleProvider noStyle = mFrameContext.makeStyleFor(StyleIdsStack.getDefaultInstance());
        assertThat(noStyle).isEqualTo(mDefaultStyleProvider);

        // Successful lookup results in a new style provider
        StyleProvider defaultStyle = mFrameContext.makeStyleFor(SAMPLE_STYLE_IDS);
        assertThat(defaultStyle.getColor()).isEqualTo(SAMPLE_STYLE_COLOR);

        // Failed lookup returns the current style provider
        StyleProvider notFoundStyle = mFrameContext.makeStyleFor(
                StyleIdsStack.newBuilder().addStyleIds(INVALID_BINDING_ID).build());
        assertThat(notFoundStyle).isEqualTo(mDefaultStyleProvider);
    }

    @Test
    public void testGetText() {
        ParameterizedText text = ParameterizedText.newBuilder().setText("tabby").build();
        BindingValue textBindingValue = defaultBinding().setParameterizedText(text).build();
        ParameterizedTextBindingRef textBindingRef =
                ParameterizedTextBindingRef.newBuilder().setBindingId(BINDING_ID).build();

        mFrameContext = makeFrameContextWithBinding(textBindingValue);

        // Succeed in looking up binding
        assertThat(mFrameContext.getParameterizedTextBindingValue(textBindingRef))
                .isEqualTo(textBindingValue);

        // Can't look up binding
        assertThatRunnable(()
                                   -> mFrameContext.getParameterizedTextBindingValue(
                                           ParameterizedTextBindingRef.newBuilder()
                                                   .setBindingId(INVALID_BINDING_ID)
                                                   .build()))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Parameterized text binding not found");

        // Binding has no content
        assertThatRunnable(
                ()
                        -> makeFrameContextWithNoBindings().getParameterizedTextBindingValue(
                                textBindingRef))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Parameterized text binding not found");

        // Binding is invalid but is optional
        ParameterizedTextBindingRef textBindingRefInvalidOptional =
                ParameterizedTextBindingRef.newBuilder()
                        .setBindingId(INVALID_BINDING_ID)
                        .setIsOptional(true)
                        .build();
        assertThat(makeFrameContextWithNoBindings().getParameterizedTextBindingValue(
                           textBindingRefInvalidOptional))
                .isEqualTo(BindingValue.getDefaultInstance());

        // Binding has no content but is optional
        ParameterizedTextBindingRef textBindingRefOptional =
                ParameterizedTextBindingRef.newBuilder()
                        .setBindingId(BINDING_ID)
                        .setIsOptional(true)
                        .build();
        assertThat(makeFrameContextWithNoBindings().getParameterizedTextBindingValue(
                           textBindingRefOptional))
                .isEqualTo(BindingValue.getDefaultInstance());
    }

    @Test
    public void testGetText_hostBinding() {
        ParameterizedText text = ParameterizedText.newBuilder().setText("tabby").build();
        BindingValue textBindingValue = defaultBinding().setParameterizedText(text).build();
        BindingValue hostTextBindingValue =
                defaultBinding()
                        .setHostBindingData(HostBindingData.newBuilder())
                        .setParameterizedText(text)
                        .build();
        ParameterizedTextBindingRef textBindingRef =
                ParameterizedTextBindingRef.newBuilder().setBindingId(BINDING_ID).build();

        mFrameContext = makeFrameContextWithBinding(hostTextBindingValue);

        // Succeed in looking up binding
        assertThat(mFrameContext.getParameterizedTextBindingValue(textBindingRef))
                .isEqualTo(textBindingValue);
    }

    @Test
    public void testGetImage() {
        Image image =
                Image.newBuilder().addSources(ImageSource.newBuilder().setUrl("myUrl")).build();
        BindingValue imageBindingValue = defaultBinding().setImage(image).build();
        ImageBindingRef imageBindingRef =
                ImageBindingRef.newBuilder().setBindingId(BINDING_ID).build();

        mFrameContext = makeFrameContextWithBinding(imageBindingValue);

        // Succeed in looking up binding
        assertThat(mFrameContext.getImageBindingValue(imageBindingRef))
                .isEqualTo(imageBindingValue);

        // Can't look up binding
        assertThatRunnable(()
                                   -> mFrameContext.getImageBindingValue(
                                           ImageBindingRef.newBuilder()
                                                   .setBindingId(INVALID_BINDING_ID)
                                                   .build()))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Image binding not found");

        // Binding has no content
        assertThatRunnable(
                () -> makeFrameContextWithNoBindings().getImageBindingValue(imageBindingRef))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Image binding not found");

        // Binding is invalid but is optional
        ImageBindingRef imageBindingRefInvalidOptional = ImageBindingRef.newBuilder()
                                                                 .setBindingId(INVALID_BINDING_ID)
                                                                 .setIsOptional(true)
                                                                 .build();
        assertThat(makeFrameContextWithNoBindings().getImageBindingValue(
                           imageBindingRefInvalidOptional))
                .isEqualTo(BindingValue.getDefaultInstance());

        // Binding has no content but is optional
        ImageBindingRef imageBindingRefOptional =
                ImageBindingRef.newBuilder().setBindingId(BINDING_ID).setIsOptional(true).build();
        assertThat(makeFrameContextWithNoBindings().getImageBindingValue(imageBindingRefOptional))
                .isEqualTo(BindingValue.getDefaultInstance());
    }

    @Test
    public void testGetImage_hostBinding() {
        Image image =
                Image.newBuilder().addSources(ImageSource.newBuilder().setUrl("myUrl")).build();
        BindingValue imageBindingValue = defaultBinding().setImage(image).build();
        BindingValue hostImageBindingValue =
                defaultBinding()
                        .setHostBindingData(HostBindingData.newBuilder())
                        .setImage(image)
                        .build();
        ImageBindingRef imageBindingRef =
                ImageBindingRef.newBuilder().setBindingId(BINDING_ID).build();

        mFrameContext = makeFrameContextWithBinding(hostImageBindingValue);

        // Succeed in looking up binding
        assertThat(mFrameContext.getImageBindingValue(imageBindingRef))
                .isEqualTo(imageBindingValue);
    }

    @Test
    public void testGetElement() {
        Element element = Element.newBuilder()
                                  .setStyleReferences(StyleIdsStack.newBuilder().addStyleIds("el"))
                                  .build();
        BindingValue elementBindingValue = defaultBinding().setElement(element).build();
        ElementBindingRef elementBindingRef =
                ElementBindingRef.newBuilder().setBindingId(BINDING_ID).build();

        mFrameContext = makeFrameContextWithBinding(elementBindingValue);

        // Succeed in looking up binding
        assertThat(mFrameContext.getElementBindingValue(elementBindingRef))
                .isEqualTo(elementBindingValue);

        // Can't look up binding
        assertThatRunnable(()
                                   -> mFrameContext.getElementBindingValue(
                                           ElementBindingRef.newBuilder()
                                                   .setBindingId(INVALID_BINDING_ID)
                                                   .build()))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Element binding not found");

        // Binding has no content
        assertThatRunnable(
                () -> makeFrameContextWithNoBindings().getElementBindingValue(elementBindingRef))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Element binding not found");

        // Binding is missing but is optional
        ElementBindingRef elementBindingRefInvalidOptional =
                ElementBindingRef.newBuilder()
                        .setBindingId(INVALID_BINDING_ID)
                        .setIsOptional(true)
                        .build();
        assertThat(makeFrameContextWithNoBindings().getElementBindingValue(
                           elementBindingRefInvalidOptional))
                .isEqualTo(BindingValue.getDefaultInstance());

        // Binding has no content but is optional
        ElementBindingRef elementBindingRefOptional =
                ElementBindingRef.newBuilder().setBindingId(BINDING_ID).setIsOptional(true).build();
        assertThat(
                makeFrameContextWithNoBindings().getElementBindingValue(elementBindingRefOptional))
                .isEqualTo(BindingValue.getDefaultInstance());
    }

    @Test
    public void testGetElement_hostBinding() {
        Element element = Element.newBuilder()
                                  .setStyleReferences(StyleIdsStack.newBuilder().addStyleIds("el"))
                                  .build();
        BindingValue elementBindingValue = defaultBinding().setElement(element).build();
        BindingValue hostListBindingValue =
                defaultBinding()
                        .setHostBindingData(HostBindingData.newBuilder())
                        .setElement(element)
                        .build();
        ElementBindingRef elementBindingRef =
                ElementBindingRef.newBuilder().setBindingId(BINDING_ID).build();

        mFrameContext = makeFrameContextWithBinding(hostListBindingValue);

        // Succeed in looking up binding
        assertThat(mFrameContext.getElementBindingValue(elementBindingRef))
                .isEqualTo(elementBindingValue);
    }

    @Test
    public void testGetVisibility() {
        Visibility visibility = Visibility.INVISIBLE;
        BindingValue visibilityBindingValue = defaultBinding().setVisibility(visibility).build();
        VisibilityBindingRef visibilityBindingRef =
                VisibilityBindingRef.newBuilder().setBindingId(BINDING_ID).build();

        mFrameContext = makeFrameContextWithBinding(visibilityBindingValue);

        // Succeed in looking up binding
        assertThat(mFrameContext.getVisibilityFromBinding(visibilityBindingRef))
                .isEqualTo(visibility);

        // Can't look up binding
        assertThat(
                mFrameContext.getVisibilityFromBinding(
                        VisibilityBindingRef.newBuilder().setBindingId(INVALID_BINDING_ID).build()))
                .isNull();

        // Binding has no content
        assertThat(makeFrameContextWithNoBindings().getVisibilityFromBinding(visibilityBindingRef))
                .isNull();
    }

    @Test
    public void testGetVisibility_hostBinding() {
        Visibility visibility = Visibility.INVISIBLE;
        BindingValue hostListBindingValue =
                defaultBinding()
                        .setHostBindingData(HostBindingData.newBuilder())
                        .setVisibility(visibility)
                        .build();
        VisibilityBindingRef visibilityBindingRef =
                VisibilityBindingRef.newBuilder().setBindingId(BINDING_ID).build();

        mFrameContext = makeFrameContextWithBinding(hostListBindingValue);

        // Succeed in looking up binding
        assertThat(mFrameContext.getVisibilityFromBinding(visibilityBindingRef))
                .isEqualTo(visibility);
    }

    @Test
    public void testGetGridCellWidthFromBinding() {
        GridCellWidth cellWidth = GridCellWidth.newBuilder().setWeight(123).build();
        mFrameContext =
                makeFrameContextWithBinding(defaultBinding().setCellWidth(cellWidth).build());
        assertThat(mFrameContext.getGridCellWidthFromBinding(
                           GridCellWidthBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isEqualTo(cellWidth);
        assertThat(
                mFrameContext.getGridCellWidthFromBinding(GridCellWidthBindingRef.newBuilder()
                                                                  .setBindingId(INVALID_BINDING_ID)
                                                                  .build()))
                .isNull();

        mFrameContext = makeFrameContextWithNoBindings();
        assertThat(mFrameContext.getGridCellWidthFromBinding(
                           GridCellWidthBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isNull();
    }

    @Test
    public void testGetGridCellWidthFromBinding_hostBinding() {
        GridCellWidth cellWidth = GridCellWidth.newBuilder().setWeight(123).build();
        mFrameContext = makeFrameContextWithBinding(
                defaultBinding()
                        .setHostBindingData(HostBindingData.newBuilder())
                        .setCellWidth(cellWidth)
                        .build());
        assertThat(mFrameContext.getGridCellWidthFromBinding(
                           GridCellWidthBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isEqualTo(cellWidth);
    }

    @Test
    public void testGetActionsFromBinding() {
        Actions actions = Actions.newBuilder().build();
        mFrameContext = makeFrameContextWithBinding(defaultBinding().setActions(actions).build());
        assertThat(mFrameContext.getActionsFromBinding(
                           ActionsBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isSameInstanceAs(actions);
        assertThat(mFrameContext.getActionsFromBinding(
                           ActionsBindingRef.newBuilder().setBindingId(INVALID_BINDING_ID).build()))
                .isSameInstanceAs(Actions.getDefaultInstance());

        mFrameContext = makeFrameContextWithNoBindings();
        assertThat(mFrameContext.getActionsFromBinding(
                           ActionsBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isSameInstanceAs(Actions.getDefaultInstance());
    }

    @Test
    public void testGetActionsFromBinding_hostBinding() {
        mFrameContext = makeFrameContextWithBinding(
                defaultBinding()
                        .setHostBindingData(HostBindingData.newBuilder())
                        .setActions(Actions.getDefaultInstance())
                        .build());
        assertThat(mFrameContext.getActionsFromBinding(
                           ActionsBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isEqualTo(Actions.getDefaultInstance());
    }

    @Test
    public void testGetLogDataFromBinding() {
        LogData logData = LogData.newBuilder().build();
        mFrameContext = makeFrameContextWithBinding(defaultBinding().setLogData(logData).build());
        assertThat(mFrameContext.getLogDataFromBinding(
                           LogDataBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isSameInstanceAs(logData);

        assertThat(mFrameContext.getLogDataFromBinding(
                           LogDataBindingRef.newBuilder().setBindingId(INVALID_BINDING_ID).build()))
                .isNull();

        mFrameContext = makeFrameContextWithNoBindings();
        assertThat(mFrameContext.getLogDataFromBinding(
                           LogDataBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isNull();
    }

    @Test
    public void testGetLogDataFromBinding_hostBinding() {
        mFrameContext = makeFrameContextWithBinding(
                defaultBinding()
                        .setHostBindingData(HostBindingData.newBuilder())
                        .setLogData(LogData.getDefaultInstance())
                        .build());
        assertThat(mFrameContext.getLogDataFromBinding(
                           LogDataBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isEqualTo(LogData.getDefaultInstance());
    }

    @Test
    public void testGetStyleFromBinding() {
        BoundStyle boundStyle = BoundStyle.newBuilder().setColor(12345).build();
        mFrameContext =
                makeFrameContextWithBinding(defaultBinding().setBoundStyle(boundStyle).build());
        assertThat(mFrameContext.getStyleFromBinding(
                           StyleBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isEqualTo(boundStyle);
        assertThat(mFrameContext.getStyleFromBinding(
                           StyleBindingRef.newBuilder().setBindingId(INVALID_BINDING_ID).build()))
                .isEqualTo(BoundStyle.getDefaultInstance());

        mFrameContext = makeFrameContextWithNoBindings();
        assertThat(mFrameContext.getStyleFromBinding(
                           StyleBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isEqualTo(BoundStyle.getDefaultInstance());
    }

    @Test
    public void testGetStyleFromBinding_hostBinding() {
        BoundStyle boundStyle = BoundStyle.newBuilder().setColor(12345).build();
        mFrameContext = makeFrameContextWithBinding(
                defaultBinding()
                        .setHostBindingData(HostBindingData.newBuilder())
                        .setBoundStyle(boundStyle)
                        .build());
        assertThat(mFrameContext.getStyleFromBinding(
                           StyleBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isEqualTo(boundStyle);
    }

    @Test
    public void testGetTemplateInvocationFromBinding() {
        TemplateInvocation templateInvocation =
                TemplateInvocation.newBuilder().setTemplateId("carboncopy").build();
        BindingValue templateBindingValue =
                defaultBinding().setTemplateInvocation(templateInvocation).build();
        mFrameContext = makeFrameContextWithBinding(templateBindingValue);
        assertThat(mFrameContext.getTemplateInvocationBindingValue(
                           TemplateBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isEqualTo(templateBindingValue);
        assertThat(mFrameContext.getTemplateInvocationBindingValue(
                           TemplateBindingRef.newBuilder()
                                   .setIsOptional(true)
                                   .setBindingId(INVALID_BINDING_ID)
                                   .build()))
                .isEqualTo(BindingValue.getDefaultInstance());
        assertThatRunnable(()
                                   -> mFrameContext.getTemplateInvocationBindingValue(
                                           TemplateBindingRef.newBuilder()
                                                   .setBindingId(INVALID_BINDING_ID)
                                                   .build()))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Template binding not found for NOT_A_REAL_BINDING_ID");

        mFrameContext = makeFrameContextWithNoBindings();
        assertThatRunnable(
                ()
                        -> mFrameContext.getTemplateInvocationBindingValue(
                                TemplateBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Template binding not found for BINDING_ID");
    }

    @Test
    public void testGetTemplateInvocationFromBinding_hostBinding() {
        TemplateInvocation templateInvocation =
                TemplateInvocation.newBuilder().setTemplateId("carboncopy").build();
        mFrameContext = makeFrameContextWithBinding(
                defaultBinding()
                        .setHostBindingData(HostBindingData.newBuilder())
                        .setTemplateInvocation(templateInvocation)
                        .build());
    }

    @Test
    public void testGetCustomElementBindingValue() {
        CustomElementData customElement = CustomElementData.getDefaultInstance();
        BindingValue customElementBindingValue =
                defaultBinding().setCustomElementData(customElement).build();
        CustomBindingRef customBindingRef =
                CustomBindingRef.newBuilder().setBindingId(BINDING_ID).build();

        mFrameContext = makeFrameContextWithBinding(customElementBindingValue);

        // Succeed in looking up binding
        assertThat(mFrameContext.getCustomElementBindingValue(customBindingRef))
                .isEqualTo(customElementBindingValue);

        // Can't look up binding
        assertThatRunnable(()
                                   -> mFrameContext.getCustomElementBindingValue(
                                           CustomBindingRef.newBuilder()
                                                   .setBindingId(INVALID_BINDING_ID)
                                                   .build()))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Custom element binding not found");

        // Binding has no content
        assertThatRunnable(()
                                   -> makeFrameContextWithNoBindings().getCustomElementBindingValue(
                                           customBindingRef))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Custom element binding not found");

        // Binding is missing but is optional
        CustomBindingRef customBindingRefInvalidOptional = CustomBindingRef.newBuilder()
                                                                   .setBindingId(INVALID_BINDING_ID)
                                                                   .setIsOptional(true)
                                                                   .build();
        assertThat(makeFrameContextWithNoBindings().getCustomElementBindingValue(
                           customBindingRefInvalidOptional))
                .isEqualTo(BindingValue.getDefaultInstance());

        // Binding has no content but is optional
        CustomBindingRef customBindingRefOptional =
                CustomBindingRef.newBuilder().setBindingId(BINDING_ID).setIsOptional(true).build();
        assertThat(makeFrameContextWithNoBindings().getCustomElementBindingValue(
                           customBindingRefOptional))
                .isEqualTo(BindingValue.getDefaultInstance());
    }

    @Test
    public void testGetCustomElementBindingValue_hostBinding() {
        CustomElementData customElement = CustomElementData.getDefaultInstance();
        BindingValue customElementBindingValue =
                defaultBinding().setCustomElementData(customElement).build();
        BindingValue hostCustomElementBindingValue =
                defaultBinding()
                        .setHostBindingData(HostBindingData.newBuilder())
                        .setCustomElementData(customElement)
                        .build();
        CustomBindingRef customBindingRef =
                CustomBindingRef.newBuilder().setBindingId(BINDING_ID).build();

        mFrameContext = makeFrameContextWithBinding(hostCustomElementBindingValue);

        // Succeed in looking up binding
        assertThat(mFrameContext.getCustomElementBindingValue(customBindingRef))
                .isEqualTo(customElementBindingValue);
    }

    @Test
    public void testGetChunkedTextBindingValue() {
        ChunkedText text = ChunkedText.newBuilder()
                                   .addChunks(Chunk.newBuilder().setTextChunk(
                                           StyledTextChunk.newBuilder().setParameterizedText(
                                                   ParameterizedText.newBuilder().setText("text"))))
                                   .build();
        BindingValue textBindingValue = defaultBinding().setChunkedText(text).build();
        ChunkedTextBindingRef textBindingRef =
                ChunkedTextBindingRef.newBuilder().setBindingId(BINDING_ID).build();

        mFrameContext = makeFrameContextWithBinding(textBindingValue);

        // Succeed in looking up binding
        assertThat(mFrameContext.getChunkedTextBindingValue(textBindingRef))
                .isEqualTo(textBindingValue);

        // Can't look up binding
        assertThatRunnable(()
                                   -> mFrameContext.getChunkedTextBindingValue(
                                           ChunkedTextBindingRef.newBuilder()
                                                   .setBindingId(INVALID_BINDING_ID)
                                                   .build()))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Chunked text binding not found");

        // Binding has no content
        assertThatRunnable(
                () -> makeFrameContextWithNoBindings().getChunkedTextBindingValue(textBindingRef))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Chunked text binding not found");

        // Binding is missing but is optional
        ChunkedTextBindingRef textBindingRefInvalidOptional =
                ChunkedTextBindingRef.newBuilder()
                        .setBindingId(INVALID_BINDING_ID)
                        .setIsOptional(true)
                        .build();
        assertThat(makeFrameContextWithNoBindings().getChunkedTextBindingValue(
                           textBindingRefInvalidOptional))
                .isEqualTo(BindingValue.getDefaultInstance());

        // Binding has no content but is optional
        ChunkedTextBindingRef textBindingRefOptional = ChunkedTextBindingRef.newBuilder()
                                                               .setBindingId(BINDING_ID)
                                                               .setIsOptional(true)
                                                               .build();
        assertThat(
                makeFrameContextWithNoBindings().getChunkedTextBindingValue(textBindingRefOptional))
                .isEqualTo(BindingValue.getDefaultInstance());
    }

    @Test
    public void testGetChunkedTextBindingValue_hostBinding() {
        ChunkedText text = ChunkedText.newBuilder()
                                   .addChunks(Chunk.newBuilder().setTextChunk(
                                           StyledTextChunk.newBuilder().setParameterizedText(
                                                   ParameterizedText.newBuilder().setText("text"))))
                                   .build();
        BindingValue textBindingValue = defaultBinding().setChunkedText(text).build();
        BindingValue hostTextBindingValue =
                defaultBinding()
                        .setHostBindingData(HostBindingData.newBuilder())
                        .setChunkedText(text)
                        .build();
        ChunkedTextBindingRef textBindingRef =
                ChunkedTextBindingRef.newBuilder().setBindingId(BINDING_ID).build();

        mFrameContext = makeFrameContextWithBinding(hostTextBindingValue);

        // Succeed in looking up binding
        assertThat(mFrameContext.getChunkedTextBindingValue(textBindingRef))
                .isEqualTo(textBindingValue);
    }

    @Test
    public void testCreateWithoutError() {
        Frame frame = getWorkingFrame();
        FrameContext frameContext = makeFrameContextFromFrame(frame);
        assertThat(frameContext).isNotNull();
        assertThat(frameContext.getDebugLogger().getMessages(MessageType.ERROR)).isEmpty();
    }

    @Test
    public void testBindFrame_withStylesheetId() {
        Frame frame =
                Frame.newBuilder()
                        .setStylesheets(Stylesheets.newBuilder().addStylesheetIds(STYLESHEET_ID))
                        .build();
        setUpPietSharedStates();
        FrameContext frameContext = makeFrameContextFromFrame(frame);

        // The style is not currently bound, but available from the stylesheet.
        assertThat(frameContext.makeStyleFor(StyleIdsStack.getDefaultInstance()).getColor())
                .isNotEqualTo(SAMPLE_STYLE_COLOR);
        assertThat(frameContext.makeStyleFor(SAMPLE_STYLE_IDS).getColor())
                .isEqualTo(SAMPLE_STYLE_COLOR);
        assertThat(frameContext.getDebugLogger().getMessages(MessageType.ERROR)).isEmpty();
    }

    @Test
    public void testBindFrame_withStylesheet() {
        Frame frame = Frame.newBuilder()
                              .setStylesheets(Stylesheets.newBuilder().addStylesheets(
                                      Stylesheet.newBuilder().addStyles(SAMPLE_STYLE)))
                              .build();

        FrameContext frameContext = makeFrameContextFromFrame(frame);

        // The style is not currently bound, but available from the stylesheet.
        assertThat(frameContext.makeStyleFor(StyleIdsStack.getDefaultInstance()).getColor())
                .isNotEqualTo(SAMPLE_STYLE_COLOR);
        assertThat(frameContext.makeStyleFor(SAMPLE_STYLE_IDS).getColor())
                .isEqualTo(SAMPLE_STYLE_COLOR);
        assertThat(frameContext.getDebugLogger().getMessages(MessageType.ERROR)).isEmpty();
    }

    @Test
    public void testBindFrame_withMultipleStylesheets() {
        Frame frame = Frame.newBuilder()
                              .setStylesheets(Stylesheets.newBuilder()
                                                      .addStylesheetIds(STYLESHEET_ID)
                                                      .addStylesheets(Stylesheet.newBuilder()
                                                                              .addStyles(BASE_STYLE)
                                                                              .build()))
                              .build();

        setUpPietSharedStates();

        FrameContext frameContext = makeFrameContextFromFrame(frame);

        assertThat(frameContext.makeStyleFor(StyleIdsStack.getDefaultInstance()).getColor())
                .isNotEqualTo(SAMPLE_STYLE_COLOR);
        assertThat(frameContext.makeStyleFor(SAMPLE_STYLE_IDS).getColor())
                .isEqualTo(SAMPLE_STYLE_COLOR);
        assertThat(frameContext
                           .makeStyleFor(StyleIdsStack.newBuilder()
                                                 .addStyleIds(BASE_STYLE.getStyleId())
                                                 .build())
                           .getColor())
                .isEqualTo(BASE_STYLE_COLOR);
        assertThat(frameContext.getDebugLogger().getMessages(MessageType.ERROR)).isEmpty();
    }

    @Test
    public void testBindFrame_withFrameStyle() {
        Frame frame = Frame.newBuilder()
                              .setStylesheets(Stylesheets.newBuilder().addStylesheets(
                                      Stylesheet.newBuilder().addStyles(SAMPLE_STYLE)))
                              .setStyleReferences(SAMPLE_STYLE_IDS)
                              .build();

        FrameContext frameContext = makeFrameContextFromFrame(frame);

        assertThat(frameContext.makeStyleFor(StyleIdsStack.getDefaultInstance()))
                .isEqualTo(mDefaultStyleProvider);
        assertThat(frameContext.getDebugLogger().getMessages(MessageType.ERROR)).isEmpty();
    }

    @Test
    public void testBindFrame_withoutFrameStyle() {
        Frame frame = Frame.newBuilder()
                              .setStylesheets(Stylesheets.newBuilder().addStylesheets(
                                      Stylesheet.newBuilder().addStyles(SAMPLE_STYLE)))
                              .build();

        FrameContext frameContext = makeFrameContextFromFrame(frame);

        assertThat(frameContext.makeStyleFor(StyleIdsStack.getDefaultInstance()))
                .isEqualTo(mDefaultStyleProvider);
        assertThat(frameContext.getDebugLogger().getMessages(MessageType.ERROR)).isEmpty();
    }

    @Test
    public void testBindFrame_baseStyle() {
        int styleHeight = 747;
        String heightStyleId = "JUMBO";
        Frame frame = Frame.newBuilder()
                              .setStylesheets(Stylesheets.newBuilder().addStylesheets(
                                      Stylesheet.newBuilder()
                                              .addStyles(SAMPLE_STYLE)
                                              .addStyles(Style.newBuilder()
                                                                 .setStyleId(heightStyleId)
                                                                 .setHeight(styleHeight))))
                              .setStyleReferences(SAMPLE_STYLE_IDS)
                              .build();

        // Set up a frame with a color applied to the frame.
        FrameContext frameContext = makeFrameContextFromFrame(frame);
        StyleProvider baseStyleWithHeight = frameContext.makeStyleFor(
                StyleIdsStack.newBuilder().addStyleIds(heightStyleId).build());

        // Make a style for something that doesn't override color, and check that the base color is
        // a default, not the frame color.
        assertThat(baseStyleWithHeight.getColor()).isEqualTo(Style.getDefaultInstance().getColor());
        assertThat(baseStyleWithHeight.getHeightSpecPx(mContext))
                .isEqualTo((int) LayoutUtils.dpToPx(styleHeight, mContext));
        assertThat(frameContext.getDebugLogger().getMessages(MessageType.ERROR)).isEmpty();
    }

    @Test
    public void testMergeStyleIdsStack() {
        String styleId1 = "STYLE1";
        Style style1 = Style.newBuilder()
                               .setColor(12345) // Not overridden
                               .setMaxLines(54321) // Overridden
                               .setFont(Font.newBuilder().setSize(11).setItalic(true))
                               .setBackground(Fill.newBuilder().setLinearGradient(
                                       LinearGradient.newBuilder().addStops(
                                               ColorStop.newBuilder().setColor(1234))))
                               .setStyleId(styleId1)
                               .build();
        String styleId2 = "STYLE2";
        Style style2 = Style.newBuilder()
                               .setMaxLines(22222) // Overrides
                               .setMinHeight(33333) // Not an override
                               .setFont(Font.newBuilder().setSize(13))
                               .setBackground(Fill.newBuilder().setLinearGradient(
                                       LinearGradient.newBuilder().setDirectionDeg(321)))
                               .setStyleId(styleId2)
                               .build();
        StyleIdsStack twoStyles =
                StyleIdsStack.newBuilder().addStyleIds(styleId1).addStyleIds(styleId2).build();
        Frame frame =
                Frame.newBuilder()
                        .setStylesheets(Stylesheets.newBuilder().addStylesheetIds(STYLESHEET_ID))
                        .setStyleReferences(twoStyles)
                        .build();
        mPietSharedStates.add(PietSharedState.newBuilder()
                                      .addStylesheets(Stylesheet.newBuilder()
                                                              .setStylesheetId(STYLESHEET_ID)
                                                              .addStyles(style1)
                                                              .addStyles(style2)
                                                              .build())
                                      .build());

        FrameContext frameContext = makeFrameContextFromFrame(frame);

        StyleProvider defaultFrameStyle = frameContext.makeStyleFor(twoStyles);
        assertThat(defaultFrameStyle.getColor()).isEqualTo(12345);
        assertThat(defaultFrameStyle.getMaxLines()).isEqualTo(22222);
        assertThat(defaultFrameStyle.getMinHeight()).isEqualTo(33333);
        assertThat(defaultFrameStyle.getFont())
                .isEqualTo(Font.newBuilder().setSize(13).setItalic(true).build());
        assertThat(defaultFrameStyle.getBackground())
                .isEqualTo(Fill.newBuilder()
                                   .setLinearGradient(
                                           LinearGradient.newBuilder()
                                                   .addStops(ColorStop.newBuilder().setColor(1234))
                                                   .setDirectionDeg(321))
                                   .build());
        assertThat(frameContext.getDebugLogger().getMessages(MessageType.ERROR)).isEmpty();

        // Frame's styles don't affect the childrens' styles.
        assertThat(frameContext.makeStyleFor(StyleIdsStack.getDefaultInstance()))
                .isEqualTo(mDefaultStyleProvider);
        assertThat(frameContext.getDebugLogger().getMessages(MessageType.ERROR)).isEmpty();
    }

    @Test
    public void testGetMediaQueryStylesheets() {
        String noMediaQueryStylesheetId = "noMediaQueries";
        Stylesheet noMediaQueryStylesheet =
                Stylesheet.newBuilder().setStylesheetId(noMediaQueryStylesheetId).build();

        String mediaQueryStylesheetId = "mediaQueries";
        Stylesheet mediaQueryStylesheet =
                Stylesheet.newBuilder()
                        .setStylesheetId(mediaQueryStylesheetId)
                        .addConditions(MediaQueryCondition.newBuilder().setFrameWidth(
                                FrameWidthCondition.newBuilder().setWidth(0).setCondition(
                                        ComparisonCondition.GREATER_THAN)))
                        .build();

        mPietSharedStates.add(PietSharedState.newBuilder()
                                      .addStylesheets(mediaQueryStylesheet)
                                      .addStylesheets(noMediaQueryStylesheet)
                                      .build());

        FrameContext frameContext = defaultFrameContext();

        Template inlineStylesheetTemplate =
                Template.newBuilder()
                        .setStylesheets(Stylesheets.newBuilder().addStylesheets(
                                Stylesheet.newBuilder().setStylesheetId("inline")))
                        .build();
        assertThat(frameContext.getMediaQueryStylesheets(inlineStylesheetTemplate)).isEmpty();

        Template notFoundStylesheetTemplate =
                Template.newBuilder()
                        .setStylesheets(Stylesheets.newBuilder().addStylesheetIds("NotFound"))
                        .build();
        assertThat(frameContext.getMediaQueryStylesheets(notFoundStylesheetTemplate)).isEmpty();

        Template noConditionsTemplate =
                Template.newBuilder()
                        .setStylesheets(
                                Stylesheets.newBuilder().addStylesheetIds(noMediaQueryStylesheetId))
                        .build();
        assertThat(frameContext.getMediaQueryStylesheets(noConditionsTemplate)).isEmpty();

        Template mediaQueryTemplate =
                Template.newBuilder()
                        .setStylesheets(
                                Stylesheets.newBuilder().addStylesheetIds(mediaQueryStylesheetId))
                        .build();
        assertThat(frameContext.getMediaQueryStylesheets(mediaQueryTemplate))
                .containsExactly(mediaQueryStylesheet);
    }

    @Test
    public void testFilterImageSourcesByMediaQueryCondition() {
        ImageSource activeSource =
                ImageSource.newBuilder()
                        .addConditions(MediaQueryCondition.newBuilder().setDarkLight(
                                DarkLightCondition.newBuilder().setMode(DarkLightMode.DARK)))
                        .build();
        ImageSource inactiveSource =
                ImageSource.newBuilder()
                        .addConditions(MediaQueryCondition.newBuilder().setDarkLight(
                                DarkLightCondition.newBuilder().setMode(DarkLightMode.LIGHT)))
                        .build();
        ImageSource sourceWithNoConditions = ImageSource.getDefaultInstance();
        Image image = Image.newBuilder()
                              .addSources(activeSource)
                              .addSources(inactiveSource)
                              .addSources(sourceWithNoConditions)
                              .build();
        when(mAssetProvider.isDarkTheme()).thenReturn(true);
        mFrameContext = defaultFrameContext();

        Image resultImage = mFrameContext.filterImageSourcesByMediaQueryCondition(image);
        assertThat(resultImage.getSourcesList())
                .containsExactly(activeSource, sourceWithNoConditions);
    }

    private FrameContext defaultFrameContext() {
        return makeFrameContextForDefaultFrame();
    }

    private FrameContext makeFrameContextForDefaultFrame() {
        return new FrameContext(DEFAULT_FRAME, mDefaultStylesheet, mPietSharedStates,
                newPietStylesHelper(), DebugBehavior.VERBOSE, mDebugLogger, mActionHandler,
                mHostProviders, mFrameView);
    }

    private FrameContext makeFrameContextWithBinding(BindingValue bindingValue) {
        Map<String, BindingValue> bindingValueMap = new HashMap<>();
        bindingValueMap.put(bindingValue.getBindingId(), bindingValue);
        return new FrameContext(DEFAULT_FRAME, mDefaultStylesheet, bindingValueMap,
                mPietSharedStates, mPietStylesHelper, DebugBehavior.VERBOSE, mDebugLogger,
                mActionHandler, mHostProviders, DEFAULT_TEMPLATES, mFrameView);
    }

    private FrameContext makeFrameContextWithNoBindings() {
        Map<String, BindingValue> bindingValueMap = new HashMap<>();
        bindingValueMap.put(BINDING_ID, BindingValue.getDefaultInstance());
        return new FrameContext(DEFAULT_FRAME, mDefaultStylesheet, bindingValueMap,
                mPietSharedStates, mPietStylesHelper, DebugBehavior.VERBOSE, mDebugLogger,
                mActionHandler, mHostProviders, DEFAULT_TEMPLATES, mFrameView);
    }

    private FrameContext makeFrameContextFromFrame(Frame frame) {
        return FrameContext.createFrameContext(frame, mPietSharedStates, newPietStylesHelper(),
                DebugBehavior.VERBOSE, mDebugLogger, mActionHandler, mHostProviders, mFrameView);
    }

    private void setUpPietSharedStates() {
        mPietSharedStates.add(PietSharedState.newBuilder()
                                      .addStylesheets(Stylesheet.newBuilder()
                                                              .setStylesheetId(STYLESHEET_ID)
                                                              .addStyles(SAMPLE_STYLE))
                                      .build());
    }

    private Frame getWorkingFrame() {
        setUpPietSharedStates();
        return getBaseFrame();
    }

    private Frame getBaseFrame() {
        return Frame.newBuilder()
                .setStylesheets(Stylesheets.newBuilder().addStylesheetIds(STYLESHEET_ID))
                .setStyleReferences(SAMPLE_STYLE_IDS)
                .build();
    }

    private BindingValue.Builder defaultBinding() {
        return BindingValue.newBuilder().setBindingId(BINDING_ID);
    }

    private PietStylesHelper newPietStylesHelper() {
        return new PietStylesHelperFactory().get(
                mPietSharedStates, new MediaQueryHelper(FRAME_WIDTH_PX, mAssetProvider, mContext));
    }
}
