// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.android.libraries.feed.common.testing.RunnableSubject.assertThatRunnable;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.widget.FrameLayout;

import com.google.android.libraries.feed.api.host.config.DebugBehavior;
import com.google.android.libraries.feed.common.ui.LayoutUtils;
import com.google.android.libraries.feed.piet.DebugLogger.MessageType;
import com.google.android.libraries.feed.piet.PietStylesHelper.PietStylesHelperFactory;
import com.google.android.libraries.feed.piet.host.ActionHandler;
import com.google.android.libraries.feed.piet.host.AssetProvider;
import com.google.android.libraries.feed.piet.host.CustomElementProvider;
import com.google.android.libraries.feed.piet.host.HostBindingProvider;
import com.google.search.now.ui.piet.ActionsProto.Actions;
import com.google.search.now.ui.piet.BindingRefsProto.ActionsBindingRef;
import com.google.search.now.ui.piet.BindingRefsProto.ChunkedTextBindingRef;
import com.google.search.now.ui.piet.BindingRefsProto.CustomBindingRef;
import com.google.search.now.ui.piet.BindingRefsProto.ElementBindingRef;
import com.google.search.now.ui.piet.BindingRefsProto.GridCellWidthBindingRef;
import com.google.search.now.ui.piet.BindingRefsProto.ImageBindingRef;
import com.google.search.now.ui.piet.BindingRefsProto.LogDataBindingRef;
import com.google.search.now.ui.piet.BindingRefsProto.ParameterizedTextBindingRef;
import com.google.search.now.ui.piet.BindingRefsProto.StyleBindingRef;
import com.google.search.now.ui.piet.BindingRefsProto.TemplateBindingRef;
import com.google.search.now.ui.piet.BindingRefsProto.VisibilityBindingRef;
import com.google.search.now.ui.piet.ElementsProto.BindingContext;
import com.google.search.now.ui.piet.ElementsProto.BindingValue;
import com.google.search.now.ui.piet.ElementsProto.CustomElementData;
import com.google.search.now.ui.piet.ElementsProto.Element;
import com.google.search.now.ui.piet.ElementsProto.GridCellWidth;
import com.google.search.now.ui.piet.ElementsProto.HostBindingData;
import com.google.search.now.ui.piet.ElementsProto.TemplateInvocation;
import com.google.search.now.ui.piet.ElementsProto.TextElement;
import com.google.search.now.ui.piet.ElementsProto.Visibility;
import com.google.search.now.ui.piet.GradientsProto.ColorStop;
import com.google.search.now.ui.piet.GradientsProto.Fill;
import com.google.search.now.ui.piet.GradientsProto.LinearGradient;
import com.google.search.now.ui.piet.ImagesProto.Image;
import com.google.search.now.ui.piet.ImagesProto.ImageSource;
import com.google.search.now.ui.piet.LogDataProto.LogData;
import com.google.search.now.ui.piet.MediaQueriesProto.ComparisonCondition;
import com.google.search.now.ui.piet.MediaQueriesProto.DarkLightCondition;
import com.google.search.now.ui.piet.MediaQueriesProto.DarkLightCondition.DarkLightMode;
import com.google.search.now.ui.piet.MediaQueriesProto.FrameWidthCondition;
import com.google.search.now.ui.piet.MediaQueriesProto.MediaQueryCondition;
import com.google.search.now.ui.piet.PietProto.Frame;
import com.google.search.now.ui.piet.PietProto.PietSharedState;
import com.google.search.now.ui.piet.PietProto.Stylesheet;
import com.google.search.now.ui.piet.PietProto.Stylesheets;
import com.google.search.now.ui.piet.PietProto.Template;
import com.google.search.now.ui.piet.StylesProto.BoundStyle;
import com.google.search.now.ui.piet.StylesProto.Font;
import com.google.search.now.ui.piet.StylesProto.Style;
import com.google.search.now.ui.piet.StylesProto.StyleIdsStack;
import com.google.search.now.ui.piet.TextProto.Chunk;
import com.google.search.now.ui.piet.TextProto.ChunkedText;
import com.google.search.now.ui.piet.TextProto.ParameterizedText;
import com.google.search.now.ui.piet.TextProto.StyledTextChunk;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Tests of the {@link FrameContext}. */
@RunWith(RobolectricTestRunner.class)
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
    private ActionHandler actionHandler;
    @Mock
    private AssetProvider assetProvider;

    private final Map<String, Style> defaultStylesheet = new HashMap<>();
    private final DebugLogger debugLogger = new DebugLogger();

    private HostBindingProvider hostBindingProvider;
    private HostProviders hostProviders;
    private FrameContext frameContext;
    private List<PietSharedState> pietSharedStates;
    private PietStylesHelper pietStylesHelper;
    private StyleProvider defaultStyleProvider;
    private Context context;
    private FrameLayout frameView;

    @Before
    public void setUp() throws Exception {
        initMocks(this);

        context = Robolectric.buildActivity(Activity.class).get();
        hostBindingProvider = new HostBindingProvider();
        hostProviders = new HostProviders(
                assetProvider, mock(CustomElementProvider.class), hostBindingProvider, null);
        defaultStyleProvider = new StyleProvider(assetProvider);

        defaultStylesheet.put(SAMPLE_STYLE_ID, SAMPLE_STYLE);
        pietSharedStates = new ArrayList<>();
        pietStylesHelper = newPietStylesHelper();

        frameView = new FrameLayout(context);
    }

    @Test
    public void testSharedStateTemplatesAddedToFrame() {
        String sharedStateTemplateId = "SHARED_STATE_TEMPLATE_ID";
        Template sharedStateTemplate =
                Template.newBuilder().setTemplateId(sharedStateTemplateId).build();
        pietSharedStates.add(
                PietSharedState.newBuilder().addTemplates(sharedStateTemplate).build());
        frameContext = defaultFrameContext();

        assertThat(frameContext.getTemplate(sharedStateTemplateId))
                .isSameInstanceAs(sharedStateTemplate);
        assertThat(frameContext.getTemplate(DEFAULT_TEMPLATE_ID))
                .isSameInstanceAs(DEFAULT_TEMPLATE);
    }

    @Test
    public void testThrowsIfSharedStateTemplatesConflictWithFrameTemplates() {
        Template sharedStateTemplate =
                Template.newBuilder().setTemplateId(DEFAULT_TEMPLATE_ID).build();
        pietSharedStates.add(
                PietSharedState.newBuilder().addTemplates(sharedStateTemplate).build());
        assertThatRunnable(this::defaultFrameContext)
                .throwsAnExceptionOfType(IllegalArgumentException.class)
                .that()
                .hasMessageThat()
                .contains("Template key 'TEMPLATE_ID' already defined");
    }

    @Test
    public void testThrowsIfDuplicateBindingValueId() {
        frameContext = defaultFrameContext();
        BindingContext bindingContextWithDuplicateIds =
                BindingContext.newBuilder()
                        .addBindingValues(BindingValue.newBuilder().setBindingId(BINDING_ID))
                        .addBindingValues(BindingValue.newBuilder().setBindingId(BINDING_ID))
                        .build();
        assertThatRunnable(()
                                   -> frameContext.createTemplateContext(
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
        pietSharedStates.add(sharedState);
        frameContext = new FrameContext(DEFAULT_FRAME, defaultStylesheet, pietSharedStates,
                newPietStylesHelper(), DebugBehavior.VERBOSE, debugLogger, actionHandler,
                hostProviders, frameView);

        assertThat(frameContext.getFrame()).isEqualTo(DEFAULT_FRAME);
        assertThat(frameContext.getActionHandler()).isEqualTo(actionHandler);

        assertThat(frameContext.getTemplate(DEFAULT_TEMPLATE_ID)).isEqualTo(DEFAULT_TEMPLATE);
        assertThat(frameContext.getTemplate(template2BindingId)).isEqualTo(template2);
    }

    @Test
    public void testThrowsWithNoBindingContext() {
        frameContext = makeFrameContextForDefaultFrame();

        assertThatRunnable(
                () -> frameContext.getActionsFromBinding(ActionsBindingRef.getDefaultInstance()))
                .throwsAnExceptionOfType(IllegalArgumentException.class)
                .that()
                .hasMessageThat()
                .contains("no BindingValues defined");
    }

    @Test
    public void testCreateTemplateContext_lotsOfStuff() {
        frameContext = defaultFrameContext();

        // createTemplateContext should add new BindingValues
        ParameterizedText text = ParameterizedText.newBuilder().setText("Calico").build();
        BindingValue textBinding = BindingValue.newBuilder()
                                           .setBindingId(BINDING_ID)
                                           .setParameterizedText(text)
                                           .build();
        FrameContext frameContextWithBindings = frameContext.createTemplateContext(DEFAULT_TEMPLATE,
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
                .isEqualTo(defaultStyleProvider);
        assertThat(frameContextWithBindings.makeStyleFor(SAMPLE_STYLE_IDS).getColor())
                .isEqualTo(defaultStyleProvider.getColor());
    }

    @Test
    public void testCreateTemplateContext_stylesheetId() {
        String styleId = "cotton";
        int width = 343;
        Style style = Style.newBuilder().setStyleId(styleId).setWidth(width).build();
        String stylesheetId = "linen";
        Stylesheet stylesheet =
                Stylesheet.newBuilder().setStylesheetId(stylesheetId).addStyles(style).build();
        pietSharedStates.add(PietSharedState.newBuilder().addStylesheets(stylesheet).build());
        frameContext = defaultFrameContext();

        Template template =
                Template.newBuilder()
                        .setTemplateId("kingSize")
                        .setStylesheets(Stylesheets.newBuilder().addStylesheetIds(stylesheetId))
                        .build();

        FrameContext templateContext =
                frameContext.createTemplateContext(template, BindingContext.getDefaultInstance());

        StyleIdsStack styleRef = StyleIdsStack.newBuilder().addStyleIds(styleId).build();
        assertThat(templateContext.makeStyleFor(styleRef).getWidthSpecPx(context))
                .isEqualTo((int) LayoutUtils.dpToPx(width, context));
    }

    @Test
    public void testCreateTemplateContext_multipleStylesheets() {
        String styleId = "cotton";
        int width = 343;
        Style style = Style.newBuilder().setStyleId(styleId).setWidth(width).build();
        String stylesheetId = "linen";
        Stylesheet stylesheet =
                Stylesheet.newBuilder().setStylesheetId(stylesheetId).addStyles(style).build();
        pietSharedStates.add(PietSharedState.newBuilder().addStylesheets(stylesheet).build());
        frameContext = defaultFrameContext();
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
                frameContext.createTemplateContext(template, BindingContext.getDefaultInstance());

        StyleIdsStack styleRef = StyleIdsStack.newBuilder().addStyleIds(styleId).build();
        assertThat(templateContext.makeStyleFor(styleRef).getWidthSpecPx(context))
                .isEqualTo((int) LayoutUtils.dpToPx(width, context));
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
        frameContext = makeFrameContextWithBinding(parentBindingValue);
        FrameContext childContext = frameContext.createTemplateContext(
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
        frameContext = makeFrameContextWithNoBindings();
        FrameContext childContext = frameContext.createTemplateContext(
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

        assertThat(debugLogger.getMessages(MessageType.ERROR)).isEmpty();
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
        frameContext = makeFrameContextWithNoBindings();
        FrameContext childContext = frameContext.createTemplateContext(
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

        assertThat(debugLogger.getMessages(MessageType.ERROR)).isEmpty();
        assertThat(childContext.getParameterizedTextBindingValue(childTextBindingRef))
                .isEqualTo(BindingValue.getDefaultInstance());
    }

    @Test
    public void testMakeStyleFor() {
        frameContext = defaultFrameContext();

        // Returns base style provider if there are no styles defined
        StyleProvider noStyle = frameContext.makeStyleFor(StyleIdsStack.getDefaultInstance());
        assertThat(noStyle).isEqualTo(defaultStyleProvider);

        // Successful lookup results in a new style provider
        StyleProvider defaultStyle = frameContext.makeStyleFor(SAMPLE_STYLE_IDS);
        assertThat(defaultStyle.getColor()).isEqualTo(SAMPLE_STYLE_COLOR);

        // Failed lookup returns the current style provider
        StyleProvider notFoundStyle = frameContext.makeStyleFor(
                StyleIdsStack.newBuilder().addStyleIds(INVALID_BINDING_ID).build());
        assertThat(notFoundStyle).isEqualTo(defaultStyleProvider);
    }

    @Test
    public void testGetText() {
        ParameterizedText text = ParameterizedText.newBuilder().setText("tabby").build();
        BindingValue textBindingValue = defaultBinding().setParameterizedText(text).build();
        ParameterizedTextBindingRef textBindingRef =
                ParameterizedTextBindingRef.newBuilder().setBindingId(BINDING_ID).build();

        frameContext = makeFrameContextWithBinding(textBindingValue);

        // Succeed in looking up binding
        assertThat(frameContext.getParameterizedTextBindingValue(textBindingRef))
                .isEqualTo(textBindingValue);

        // Can't look up binding
        assertThatRunnable(()
                                   -> frameContext.getParameterizedTextBindingValue(
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

        frameContext = makeFrameContextWithBinding(hostTextBindingValue);

        // Succeed in looking up binding
        assertThat(frameContext.getParameterizedTextBindingValue(textBindingRef))
                .isEqualTo(textBindingValue);
    }

    @Test
    public void testGetImage() {
        Image image =
                Image.newBuilder().addSources(ImageSource.newBuilder().setUrl("myUrl")).build();
        BindingValue imageBindingValue = defaultBinding().setImage(image).build();
        ImageBindingRef imageBindingRef =
                ImageBindingRef.newBuilder().setBindingId(BINDING_ID).build();

        frameContext = makeFrameContextWithBinding(imageBindingValue);

        // Succeed in looking up binding
        assertThat(frameContext.getImageBindingValue(imageBindingRef)).isEqualTo(imageBindingValue);

        // Can't look up binding
        assertThatRunnable(()
                                   -> frameContext.getImageBindingValue(
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

        frameContext = makeFrameContextWithBinding(hostImageBindingValue);

        // Succeed in looking up binding
        assertThat(frameContext.getImageBindingValue(imageBindingRef)).isEqualTo(imageBindingValue);
    }

    @Test
    public void testGetElement() {
        Element element = Element.newBuilder()
                                  .setStyleReferences(StyleIdsStack.newBuilder().addStyleIds("el"))
                                  .build();
        BindingValue elementBindingValue = defaultBinding().setElement(element).build();
        ElementBindingRef elementBindingRef =
                ElementBindingRef.newBuilder().setBindingId(BINDING_ID).build();

        frameContext = makeFrameContextWithBinding(elementBindingValue);

        // Succeed in looking up binding
        assertThat(frameContext.getElementBindingValue(elementBindingRef))
                .isEqualTo(elementBindingValue);

        // Can't look up binding
        assertThatRunnable(()
                                   -> frameContext.getElementBindingValue(
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

        frameContext = makeFrameContextWithBinding(hostListBindingValue);

        // Succeed in looking up binding
        assertThat(frameContext.getElementBindingValue(elementBindingRef))
                .isEqualTo(elementBindingValue);
    }

    @Test
    public void testGetVisibility() {
        Visibility visibility = Visibility.INVISIBLE;
        BindingValue visibilityBindingValue = defaultBinding().setVisibility(visibility).build();
        VisibilityBindingRef visibilityBindingRef =
                VisibilityBindingRef.newBuilder().setBindingId(BINDING_ID).build();

        frameContext = makeFrameContextWithBinding(visibilityBindingValue);

        // Succeed in looking up binding
        assertThat(frameContext.getVisibilityFromBinding(visibilityBindingRef))
                .isEqualTo(visibility);

        // Can't look up binding
        assertThat(
                frameContext.getVisibilityFromBinding(
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

        frameContext = makeFrameContextWithBinding(hostListBindingValue);

        // Succeed in looking up binding
        assertThat(frameContext.getVisibilityFromBinding(visibilityBindingRef))
                .isEqualTo(visibility);
    }

    @Test
    public void testGetGridCellWidthFromBinding() {
        GridCellWidth cellWidth = GridCellWidth.newBuilder().setWeight(123).build();
        frameContext =
                makeFrameContextWithBinding(defaultBinding().setCellWidth(cellWidth).build());
        assertThat(frameContext.getGridCellWidthFromBinding(
                           GridCellWidthBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isEqualTo(cellWidth);
        assertThat(
                frameContext.getGridCellWidthFromBinding(GridCellWidthBindingRef.newBuilder()
                                                                 .setBindingId(INVALID_BINDING_ID)
                                                                 .build()))
                .isNull();

        frameContext = makeFrameContextWithNoBindings();
        assertThat(frameContext.getGridCellWidthFromBinding(
                           GridCellWidthBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isNull();
    }

    @Test
    public void testGetGridCellWidthFromBinding_hostBinding() {
        GridCellWidth cellWidth = GridCellWidth.newBuilder().setWeight(123).build();
        frameContext = makeFrameContextWithBinding(
                defaultBinding()
                        .setHostBindingData(HostBindingData.newBuilder())
                        .setCellWidth(cellWidth)
                        .build());
        assertThat(frameContext.getGridCellWidthFromBinding(
                           GridCellWidthBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isEqualTo(cellWidth);
    }

    @Test
    public void testGetActionsFromBinding() {
        Actions actions = Actions.newBuilder().build();
        frameContext = makeFrameContextWithBinding(defaultBinding().setActions(actions).build());
        assertThat(frameContext.getActionsFromBinding(
                           ActionsBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isSameInstanceAs(actions);
        assertThat(frameContext.getActionsFromBinding(
                           ActionsBindingRef.newBuilder().setBindingId(INVALID_BINDING_ID).build()))
                .isSameInstanceAs(Actions.getDefaultInstance());

        frameContext = makeFrameContextWithNoBindings();
        assertThat(frameContext.getActionsFromBinding(
                           ActionsBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isSameInstanceAs(Actions.getDefaultInstance());
    }

    @Test
    public void testGetActionsFromBinding_hostBinding() {
        frameContext = makeFrameContextWithBinding(
                defaultBinding()
                        .setHostBindingData(HostBindingData.newBuilder())
                        .setActions(Actions.getDefaultInstance())
                        .build());
        assertThat(frameContext.getActionsFromBinding(
                           ActionsBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isEqualTo(Actions.getDefaultInstance());
    }

    @Test
    public void testGetLogDataFromBinding() {
        LogData logData = LogData.newBuilder().build();
        frameContext = makeFrameContextWithBinding(defaultBinding().setLogData(logData).build());
        assertThat(frameContext.getLogDataFromBinding(
                           LogDataBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isSameInstanceAs(logData);

        assertThat(frameContext.getLogDataFromBinding(
                           LogDataBindingRef.newBuilder().setBindingId(INVALID_BINDING_ID).build()))
                .isNull();

        frameContext = makeFrameContextWithNoBindings();
        assertThat(frameContext.getLogDataFromBinding(
                           LogDataBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isNull();
    }

    @Test
    public void testGetLogDataFromBinding_hostBinding() {
        frameContext = makeFrameContextWithBinding(
                defaultBinding()
                        .setHostBindingData(HostBindingData.newBuilder())
                        .setLogData(LogData.getDefaultInstance())
                        .build());
        assertThat(frameContext.getLogDataFromBinding(
                           LogDataBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isEqualTo(LogData.getDefaultInstance());
    }

    @Test
    public void testGetStyleFromBinding() {
        BoundStyle boundStyle = BoundStyle.newBuilder().setColor(12345).build();
        frameContext =
                makeFrameContextWithBinding(defaultBinding().setBoundStyle(boundStyle).build());
        assertThat(frameContext.getStyleFromBinding(
                           StyleBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isEqualTo(boundStyle);
        assertThat(frameContext.getStyleFromBinding(
                           StyleBindingRef.newBuilder().setBindingId(INVALID_BINDING_ID).build()))
                .isEqualTo(BoundStyle.getDefaultInstance());

        frameContext = makeFrameContextWithNoBindings();
        assertThat(frameContext.getStyleFromBinding(
                           StyleBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isEqualTo(BoundStyle.getDefaultInstance());
    }

    @Test
    public void testGetStyleFromBinding_hostBinding() {
        BoundStyle boundStyle = BoundStyle.newBuilder().setColor(12345).build();
        frameContext = makeFrameContextWithBinding(
                defaultBinding()
                        .setHostBindingData(HostBindingData.newBuilder())
                        .setBoundStyle(boundStyle)
                        .build());
        assertThat(frameContext.getStyleFromBinding(
                           StyleBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isEqualTo(boundStyle);
    }

    @Test
    public void testGetTemplateInvocationFromBinding() {
        TemplateInvocation templateInvocation =
                TemplateInvocation.newBuilder().setTemplateId("carboncopy").build();
        BindingValue templateBindingValue =
                defaultBinding().setTemplateInvocation(templateInvocation).build();
        frameContext = makeFrameContextWithBinding(templateBindingValue);
        assertThat(frameContext.getTemplateInvocationBindingValue(
                           TemplateBindingRef.newBuilder().setBindingId(BINDING_ID).build()))
                .isEqualTo(templateBindingValue);
        assertThat(frameContext.getTemplateInvocationBindingValue(
                           TemplateBindingRef.newBuilder()
                                   .setIsOptional(true)
                                   .setBindingId(INVALID_BINDING_ID)
                                   .build()))
                .isEqualTo(BindingValue.getDefaultInstance());
        assertThatRunnable(()
                                   -> frameContext.getTemplateInvocationBindingValue(
                                           TemplateBindingRef.newBuilder()
                                                   .setBindingId(INVALID_BINDING_ID)
                                                   .build()))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Template binding not found for NOT_A_REAL_BINDING_ID");

        frameContext = makeFrameContextWithNoBindings();
        assertThatRunnable(
                ()
                        -> frameContext.getTemplateInvocationBindingValue(
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
        frameContext = makeFrameContextWithBinding(
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

        frameContext = makeFrameContextWithBinding(customElementBindingValue);

        // Succeed in looking up binding
        assertThat(frameContext.getCustomElementBindingValue(customBindingRef))
                .isEqualTo(customElementBindingValue);

        // Can't look up binding
        assertThatRunnable(()
                                   -> frameContext.getCustomElementBindingValue(
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

        frameContext = makeFrameContextWithBinding(hostCustomElementBindingValue);

        // Succeed in looking up binding
        assertThat(frameContext.getCustomElementBindingValue(customBindingRef))
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

        frameContext = makeFrameContextWithBinding(textBindingValue);

        // Succeed in looking up binding
        assertThat(frameContext.getChunkedTextBindingValue(textBindingRef))
                .isEqualTo(textBindingValue);

        // Can't look up binding
        assertThatRunnable(()
                                   -> frameContext.getChunkedTextBindingValue(
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

        frameContext = makeFrameContextWithBinding(hostTextBindingValue);

        // Succeed in looking up binding
        assertThat(frameContext.getChunkedTextBindingValue(textBindingRef))
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
                .isEqualTo(defaultStyleProvider);
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
                .isEqualTo(defaultStyleProvider);
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
        assertThat(baseStyleWithHeight.getHeightSpecPx(context))
                .isEqualTo((int) LayoutUtils.dpToPx(styleHeight, context));
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
        pietSharedStates.add(PietSharedState.newBuilder()
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
                .isEqualTo(defaultStyleProvider);
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

        pietSharedStates.add(PietSharedState.newBuilder()
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
        when(assetProvider.isDarkTheme()).thenReturn(true);
        frameContext = defaultFrameContext();

        Image resultImage = frameContext.filterImageSourcesByMediaQueryCondition(image);
        assertThat(resultImage.getSourcesList())
                .containsExactly(activeSource, sourceWithNoConditions);
    }

    private FrameContext defaultFrameContext() {
        return makeFrameContextForDefaultFrame();
    }

    private FrameContext makeFrameContextForDefaultFrame() {
        return new FrameContext(DEFAULT_FRAME, defaultStylesheet, pietSharedStates,
                newPietStylesHelper(), DebugBehavior.VERBOSE, debugLogger, actionHandler,
                hostProviders, frameView);
    }

    private FrameContext makeFrameContextWithBinding(BindingValue bindingValue) {
        Map<String, BindingValue> bindingValueMap = new HashMap<>();
        bindingValueMap.put(bindingValue.getBindingId(), bindingValue);
        return new FrameContext(DEFAULT_FRAME, defaultStylesheet, bindingValueMap, pietSharedStates,
                pietStylesHelper, DebugBehavior.VERBOSE, debugLogger, actionHandler, hostProviders,
                DEFAULT_TEMPLATES, frameView);
    }

    private FrameContext makeFrameContextWithNoBindings() {
        Map<String, BindingValue> bindingValueMap = new HashMap<>();
        bindingValueMap.put(BINDING_ID, BindingValue.getDefaultInstance());
        return new FrameContext(DEFAULT_FRAME, defaultStylesheet, bindingValueMap, pietSharedStates,
                pietStylesHelper, DebugBehavior.VERBOSE, debugLogger, actionHandler, hostProviders,
                DEFAULT_TEMPLATES, frameView);
    }

    private FrameContext makeFrameContextFromFrame(Frame frame) {
        return FrameContext.createFrameContext(frame, pietSharedStates, newPietStylesHelper(),
                DebugBehavior.VERBOSE, debugLogger, actionHandler, hostProviders, frameView);
    }

    private void setUpPietSharedStates() {
        pietSharedStates.add(PietSharedState.newBuilder()
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
                pietSharedStates, new MediaQueryHelper(FRAME_WIDTH_PX, assetProvider, context));
    }
}
