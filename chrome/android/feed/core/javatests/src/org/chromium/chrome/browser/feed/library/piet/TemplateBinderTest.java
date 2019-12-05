// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.config.DebugBehavior;
import org.chromium.chrome.browser.feed.library.common.functional.Suppliers;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.piet.PietStylesHelper.PietStylesHelperFactory;
import org.chromium.chrome.browser.feed.library.piet.TemplateBinder.TemplateAdapterModel;
import org.chromium.chrome.browser.feed.library.piet.TemplateBinder.TemplateKey;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler;
import org.chromium.chrome.browser.feed.library.piet.host.AssetProvider;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ParameterizedTextBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingContext;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingValue;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Content;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ElementList;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.GridRow;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.TextElement;
import org.chromium.components.feed.core.proto.ui.piet.GradientsProto.Fill;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Frame;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.PietSharedState;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Stylesheet;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Stylesheets;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Template;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Style;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.StyleIdsStack;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.ParameterizedText;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Tests for TemplateBinder methods. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TemplateBinderTest {
    private static final int FRAME_COLOR = 12345;
    private static final String FRAME_STYLESHEET_ID = "coolcat";
    private static final String TEXT_STYLE_ID = "catalog";
    private static final String TEMPLATE_ID = "duplicat";
    private static final String OTHER_TEMPLATE_ID = "alleycat";
    private static final String TEXT_BINDING_ID = "ofmiceandmen";
    private static final String TEXT_CONTENTS = "afewmilessouth";

    private static final int FRAME_WIDTH_PX = 321;

    private static final Template MODEL_TEMPLATE =
            Template.newBuilder()
                    .setTemplateId(TEMPLATE_ID)
                    .setElement(
                            Element.newBuilder()
                                    .setStyleReferences(
                                            StyleIdsStack.newBuilder().addStyleIds(TEXT_STYLE_ID))
                                    .setTextElement(
                                            TextElement.newBuilder().setParameterizedTextBinding(
                                                    ParameterizedTextBindingRef.newBuilder()
                                                            .setBindingId(TEXT_BINDING_ID))))
                    .build();
    private static final Template OTHER_TEMPLATE =
            Template.newBuilder()
                    .setTemplateId(OTHER_TEMPLATE_ID)
                    .setElement(
                            Element.newBuilder().setElementList(ElementList.getDefaultInstance()))
                    .build();

    private static final Frame DEFAULT_FRAME =
            Frame.newBuilder()
                    .setStylesheets(Stylesheets.newBuilder().addStylesheets(
                            Stylesheet.newBuilder()
                                    .setStylesheetId(FRAME_STYLESHEET_ID)
                                    .addStyles(Style.newBuilder()
                                                       .setStyleId(TEXT_STYLE_ID)
                                                       .setColor(FRAME_COLOR))))
                    .addTemplates(MODEL_TEMPLATE)
                    .addTemplates(OTHER_TEMPLATE)
                    .build();

    private static final BindingContext MODEL_BINDING_CONTEXT =
            BindingContext.newBuilder()
                    .addBindingValues(
                            BindingValue.newBuilder()
                                    .setBindingId(TEXT_BINDING_ID)
                                    .setParameterizedText(
                                            ParameterizedText.newBuilder().setText(TEXT_CONTENTS)))
                    .build();

    private static final Element DEFAULT_TEMPLATE_ELEMENT =
            Element.newBuilder().setElementList(ElementList.getDefaultInstance()).build();

    @Mock
    private ElementListAdapter mElementListAdapter;
    @Mock
    private ElementAdapter<? extends View, ?> mTemplateAdapter;
    @Mock
    private FrameContext mFrameContext;
    @Mock
    private ElementAdapterFactory mAdapterFactory;
    @Mock
    private ActionHandler mActionHandler;

    private KeyedRecyclerPool<ElementAdapter<? extends View, ?>> mTemplateRecyclerPool;

    private TemplateBinder mTemplateBinder;

    @Before
    public void setUp() throws Exception {
        initMocks(this);

        mTemplateRecyclerPool = new KeyedRecyclerPool<>(100, 100);
        mTemplateBinder = new TemplateBinder(mTemplateRecyclerPool, mAdapterFactory);
    }

    @Test
    public void testCreateTemplateAdapter() {
        // Set up data for test: template, shared states, binding context, template adapter model
        String templateId = "papa";
        Template template = Template.newBuilder()
                                    .setTemplateId(templateId)
                                    .setElement(DEFAULT_TEMPLATE_ELEMENT)
                                    .build();
        when(mFrameContext.getTemplate(templateId)).thenReturn(template);
        PietSharedState sharedState = PietSharedState.newBuilder().addTemplates(template).build();
        List<PietSharedState> sharedStates = Collections.singletonList(sharedState);
        when(mFrameContext.getPietSharedStates()).thenReturn(sharedStates);
        BindingContext bindingContext =
                BindingContext.newBuilder()
                        .addBindingValues(BindingValue.newBuilder().setBindingId("potato"))
                        .build();
        TemplateAdapterModel model = new TemplateAdapterModel(template, bindingContext);

        // Set frameContext to return a new frameContext when a template is bound
        FrameContext templateContext = mock(FrameContext.class);
        when(mFrameContext.createTemplateContext(template, bindingContext))
                .thenReturn(templateContext);
        doReturn(mElementListAdapter)
                .when(mAdapterFactory)
                .createAdapterForElement(DEFAULT_TEMPLATE_ELEMENT, templateContext);

        ElementAdapter<? extends View, ?> adapter =
                mTemplateBinder.createTemplateAdapter(model, mFrameContext);

        assertThat(adapter).isSameInstanceAs(mElementListAdapter);
        verify(mElementListAdapter)
                .setKey(new TemplateKey(template, sharedStates, new ArrayList<>()));
    }

    @Test
    public void testCreateTemplateAdapter_recycled() {
        Template template = Template.newBuilder()
                                    .setTemplateId("template")
                                    .setElement(DEFAULT_TEMPLATE_ELEMENT)
                                    .build();
        List<PietSharedState> sharedStates = Collections.emptyList();
        TemplateKey templateKey = new TemplateKey(template, sharedStates, new ArrayList<>());
        when(mTemplateAdapter.getKey()).thenReturn(templateKey);
        when(mFrameContext.getPietSharedStates()).thenReturn(sharedStates);

        // Release adapter to populate the recycler pool
        mTemplateRecyclerPool.put(templateKey, mTemplateAdapter);

        // Get a new adapter from the pool.
        TemplateAdapterModel model =
                new TemplateAdapterModel(template, BindingContext.getDefaultInstance());
        ElementAdapter<? extends View, ?> adapter =
                mTemplateBinder.createTemplateAdapter(model, mFrameContext);

        assertThat(adapter).isSameInstanceAs(mTemplateAdapter);
        // We don't need to re-create the adapter; it has already been created.
        verify(mTemplateAdapter, never()).createAdapter(any(Element.class), any());
        verify(mTemplateAdapter, never()).createAdapter(any(), any(), any());
    }

    /**
     * Set up a "real" environment, and ensure that styles are set from the template, not from the
     * frame.
     */
    @Test
    public void testCreateTemplateAdapter_checkStyleSources() {
        // Set up 3 styles: one on the template, one on the frame, and one on the shared state.
        String templateStyleId = "templateStyle";
        String frameStyleId = "frameStyle";
        String globalStyleId = "globalStyle";

        int templateColor = Color.GREEN;
        Style templateStyle = Style.newBuilder()
                                      .setStyleId(templateStyleId)
                                      .setBackground(Fill.newBuilder().setColor(templateColor))
                                      .build();
        int frameColor = Color.RED;
        Style frameStyle = Style.newBuilder()
                                   .setStyleId(frameStyleId)
                                   .setBackground(Fill.newBuilder().setColor(frameColor))
                                   .build();
        int globalColor = Color.BLUE;
        Style globalStyle = Style.newBuilder()
                                    .setStyleId(globalStyleId)
                                    .setBackground(Fill.newBuilder().setColor(globalColor))
                                    .build();

        // Style on the frame with the same ID as the template's style.
        int frameTemplateColor = Color.MAGENTA;
        Style frameTemplateStyle =
                Style.newBuilder()
                        .setStyleId(templateStyleId)
                        .setBackground(Fill.newBuilder().setColor(frameTemplateColor))
                        .build();

        // Template: A list of 3 elements with template style, frame style, and global style,
        // respectively.
        Template template =
                Template.newBuilder()
                        .setStylesheets(Stylesheets.newBuilder().addStylesheets(
                                Stylesheet.newBuilder().addStyles(templateStyle)))
                        .setElement(Element.newBuilder().setElementList(
                                ElementList.newBuilder()
                                        .addContents(Content.newBuilder().setElement(
                                                Element.newBuilder()
                                                        .setStyleReferences(
                                                                StyleIdsStack.newBuilder()
                                                                        .addStyleIds(
                                                                                templateStyleId))
                                                        .setElementList(
                                                                ElementList.getDefaultInstance())))
                                        .addContents(Content.newBuilder().setElement(
                                                Element.newBuilder()
                                                        .setStyleReferences(
                                                                StyleIdsStack.newBuilder()
                                                                        .addStyleIds(frameStyleId))
                                                        .setElementList(
                                                                ElementList.getDefaultInstance())))
                                        .addContents(Content.newBuilder().setElement(
                                                Element.newBuilder()
                                                        .setStyleReferences(
                                                                StyleIdsStack.newBuilder()
                                                                        .addStyleIds(globalStyleId))
                                                        .setElementList(
                                                                ElementList
                                                                        .getDefaultInstance())))))
                        .build();

        PietSharedState pietSharedState =
                PietSharedState.newBuilder()
                        .addTemplates(template)
                        .addStylesheets(Stylesheet.newBuilder().addStyles(globalStyle))
                        .build();

        // Frame defines style IDs that are also defined in the template
        Frame frame = Frame.newBuilder()
                              .setStylesheets(Stylesheets.newBuilder().addStylesheets(
                                      Stylesheet.newBuilder()
                                              .addStyles(frameStyle)
                                              .addStyles(frameTemplateStyle)))
                              .build();

        // Set up a "real" frameContext, adapterParameters, factory
        Context context = Robolectric.buildActivity(Activity.class).get();
        List<PietSharedState> pietSharedStates = Collections.singletonList(pietSharedState);
        HostProviders mockHostProviders = mock(HostProviders.class);
        AssetProvider mockAssetProvider = mock(AssetProvider.class);
        when(mockHostProviders.getAssetProvider()).thenReturn(mockAssetProvider);
        MediaQueryHelper mediaQueryHelper =
                new MediaQueryHelper(FRAME_WIDTH_PX, mockAssetProvider, context);
        PietStylesHelper pietStylesHelper =
                new PietStylesHelperFactory().get(pietSharedStates, mediaQueryHelper);
        mFrameContext = FrameContext.createFrameContext(frame, pietSharedStates, pietStylesHelper,
                DebugBehavior.VERBOSE, new DebugLogger(), mActionHandler, mockHostProviders,
                new FrameLayout(context));
        AdapterParameters adapterParameters = new AdapterParameters(
                context, Suppliers.of(null), mockHostProviders, new FakeClock(), false, false);
        TemplateBinder templateBinder = adapterParameters.mTemplateBinder;

        // Create and bind adapter
        TemplateAdapterModel templateModel =
                new TemplateAdapterModel(template, BindingContext.getDefaultInstance());
        ElementAdapter<? extends View, ?> adapter =
                templateBinder.createTemplateAdapter(templateModel, mFrameContext);
        templateBinder.bindTemplateAdapter(adapter, templateModel, mFrameContext);

        // Check views to ensure that template styles get set, but not frame or global styles.
        LinearLayout templateList = (LinearLayout) adapter.getView();
        assertThat(templateList.getChildCount()).isEqualTo(3);

        // Template style element gets background from template
        assertThat(((ColorDrawable) templateList.getChildAt(0).getBackground()).getColor())
                .isEqualTo(templateColor);

        // Frame style element gets no background - frame styles are not in scope for template.
        assertThat(templateList.getChildAt(1).getBackground()).isNull();

        // Global style element gets no background - global styles are not in scope for template.
        assertThat(templateList.getChildAt(2).getBackground()).isNull();
    }

    @Test
    public void testBindTemplateAdapter_success() {
        // Set up data for test: template, shared states, binding context, template adapter model
        String templateId = "papa";
        Template template = Template.newBuilder()
                                    .setTemplateId(templateId)
                                    .setElement(Element.newBuilder().setElementList(
                                            ElementList.getDefaultInstance()))
                                    .build();
        when(mFrameContext.getTemplate(templateId)).thenReturn(template);
        PietSharedState sharedState = PietSharedState.newBuilder().addTemplates(template).build();
        List<PietSharedState> sharedStates = Collections.singletonList(sharedState);
        when(mFrameContext.getPietSharedStates()).thenReturn(sharedStates);
        BindingContext bindingContext =
                BindingContext.newBuilder()
                        .addBindingValues(BindingValue.newBuilder().setBindingId("potato"))
                        .build();
        TemplateAdapterModel model = new TemplateAdapterModel(template, bindingContext);

        // Set frameContext to return a new frameContext when a template is bound
        FrameContext templateContext = mock(FrameContext.class);
        when(mFrameContext.createTemplateContext(template, bindingContext))
                .thenReturn(templateContext);
        doReturn(mElementListAdapter)
                .when(mAdapterFactory)
                .createAdapterForElement(DEFAULT_TEMPLATE_ELEMENT, templateContext);

        // Create adapter and ensure template key is set.
        ElementAdapter<? extends View, ?> adapter =
                mTemplateBinder.createTemplateAdapter(model, mFrameContext);
        ArgumentCaptor<RecyclerKey> keyArgumentCaptor = ArgumentCaptor.forClass(RecyclerKey.class);
        verify(mElementListAdapter).setKey(keyArgumentCaptor.capture());
        when(mElementListAdapter.getKey()).thenReturn(keyArgumentCaptor.getValue());

        mTemplateBinder.bindTemplateAdapter(adapter, model, mFrameContext);

        // Assert that adapter is bound with the template frameContext
        verify(mElementListAdapter).bindModel(model.getTemplate().getElement(), templateContext);
    }

    @Test
    public void testBindTemplateAdapter_nullKey() {
        TemplateAdapterModel templateAdapterModel =
                new TemplateAdapterModel(Template.getDefaultInstance(), null);
        when(mElementListAdapter.getKey()).thenReturn(null);

        assertThatRunnable(()
                                   -> mTemplateBinder.bindTemplateAdapter(mElementListAdapter,
                                           templateAdapterModel, mFrameContext))
                .throwsAnExceptionOfType(NullPointerException.class)
                .that()
                .hasMessageThat()
                .contains("Adapter key was null");
    }

    @Test
    public void testBindTemplateAdapter_notATemplateAdapter() {
        TemplateAdapterModel templateAdapterModel =
                new TemplateAdapterModel(Template.getDefaultInstance(), null);
        when(mElementListAdapter.getKey()).thenReturn(ElementListAdapter.KeySupplier.SINGLETON_KEY);

        assertThatRunnable(()
                                   -> mTemplateBinder.bindTemplateAdapter(mElementListAdapter,
                                           templateAdapterModel, mFrameContext))
                .throwsAnExceptionOfType(IllegalStateException.class)
                .that()
                .hasMessageThat()
                .contains("bindTemplateAdapter only applicable for template adapters");
    }

    @Test
    public void testBindTemplateAdapter_templateMismatch() {
        // Set up data for test: two templates, shared states, binding context, template adapter
        // model
        String templateId1 = "papa";
        Template template1 = Template.newBuilder()
                                     .setTemplateId(templateId1)
                                     .setElement(DEFAULT_TEMPLATE_ELEMENT)
                                     .build();
        String templateId2 = "mama";
        Template template2 =
                Template.newBuilder()
                        .setTemplateId(templateId2)
                        .setElement(Element.newBuilder().setGridRow(GridRow.getDefaultInstance()))
                        .build();
        when(mFrameContext.getTemplate(templateId1)).thenReturn(template1);
        when(mFrameContext.getTemplate(templateId2)).thenReturn(template2);
        PietSharedState sharedState = PietSharedState.newBuilder()
                                              .addTemplates(template1)
                                              .addTemplates(template2)
                                              .build();
        List<PietSharedState> sharedStates = Collections.singletonList(sharedState);
        when(mFrameContext.getPietSharedStates()).thenReturn(sharedStates);
        BindingContext bindingContext =
                BindingContext.newBuilder()
                        .addBindingValues(BindingValue.newBuilder().setBindingId("potato"))
                        .build();
        TemplateAdapterModel model1 = new TemplateAdapterModel(template1, bindingContext);
        TemplateAdapterModel model2 = new TemplateAdapterModel(template2, bindingContext);
        assertThat(new TemplateKey(template1, sharedStates, new ArrayList<>()))
                .isNotEqualTo(new TemplateKey(template2, sharedStates, new ArrayList<>()));

        // Set frameContext to return a new frameContext when a template is bound
        FrameContext templateContext = mock(FrameContext.class);
        when(mFrameContext.createTemplateContext(template1, bindingContext))
                .thenReturn(templateContext);
        when(mFrameContext.createTemplateContext(template2, bindingContext))
                .thenReturn(templateContext);
        doReturn(mElementListAdapter)
                .when(mAdapterFactory)
                .createAdapterForElement(DEFAULT_TEMPLATE_ELEMENT, templateContext);

        // Create adapter with first template and ensure template key is set.
        ElementAdapter<? extends View, ?> adapter =
                mTemplateBinder.createTemplateAdapter(model1, mFrameContext);
        ArgumentCaptor<RecyclerKey> keyArgumentCaptor = ArgumentCaptor.forClass(RecyclerKey.class);
        verify(mElementListAdapter).setKey(keyArgumentCaptor.capture());
        when(mElementListAdapter.getKey()).thenReturn(keyArgumentCaptor.getValue());

        // Try to bind with a different template and fail.
        assertThatRunnable(
                () -> mTemplateBinder.bindTemplateAdapter(adapter, model2, mFrameContext))
                .throwsAnExceptionOfType(IllegalStateException.class)
                .that()
                .hasMessageThat()
                .contains("Template keys did not match");
    }

    @Test
    public void testCreateAndBindTemplateAdapter_newAdapter() {
        // Set up data for test: template, shared states, binding context, template adapter model
        String templateId = "papa";
        Template template = Template.newBuilder()
                                    .setTemplateId(templateId)
                                    .setElement(DEFAULT_TEMPLATE_ELEMENT)
                                    .build();
        when(mFrameContext.getTemplate(templateId)).thenReturn(template);
        PietSharedState sharedState = PietSharedState.newBuilder().addTemplates(template).build();
        List<PietSharedState> sharedStates = Collections.singletonList(sharedState);
        when(mFrameContext.getPietSharedStates()).thenReturn(sharedStates);
        BindingContext bindingContext =
                BindingContext.newBuilder()
                        .addBindingValues(BindingValue.newBuilder().setBindingId("potato"))
                        .build();
        TemplateAdapterModel model = new TemplateAdapterModel(template, bindingContext);

        // Set frameContext to return a new frameContext when a template is bound
        FrameContext templateContext = mock(FrameContext.class);
        when(mFrameContext.createTemplateContext(template, bindingContext))
                .thenReturn(templateContext);
        doReturn(mElementListAdapter)
                .when(mAdapterFactory)
                .createAdapterForElement(DEFAULT_TEMPLATE_ELEMENT, templateContext);

        // Create adapter and ensure template key is set.
        ElementAdapter<? extends View, ?> adapter =
                mTemplateBinder.createAndBindTemplateAdapter(model, mFrameContext);

        assertThat(adapter).isSameInstanceAs(mElementListAdapter);
        verify(mElementListAdapter)
                .setKey(new TemplateKey(template, sharedStates, new ArrayList<>()));

        // Assert that adapter is bound with the template frameContext
        verify(mElementListAdapter).bindModel(model.getTemplate().getElement(), templateContext);
    }

    @Test
    public void testCreateAndBindTemplateAdapter_recycled() {
        // Set up data for test: template, shared states, binding context, template adapter model
        String templateId = "papa";
        Template template = Template.newBuilder()
                                    .setTemplateId(templateId)
                                    .setElement(DEFAULT_TEMPLATE_ELEMENT)
                                    .build();
        when(mFrameContext.getTemplate(templateId)).thenReturn(template);
        PietSharedState sharedState = PietSharedState.newBuilder().addTemplates(template).build();
        List<PietSharedState> sharedStates = Collections.singletonList(sharedState);
        when(mFrameContext.getPietSharedStates()).thenReturn(sharedStates);
        BindingContext bindingContext =
                BindingContext.newBuilder()
                        .addBindingValues(BindingValue.newBuilder().setBindingId("potato"))
                        .build();
        TemplateAdapterModel model = new TemplateAdapterModel(template, bindingContext);

        // Set frameContext to return a new frameContext when a template is bound
        FrameContext templateContext = mock(FrameContext.class);
        when(mFrameContext.createTemplateContext(template, bindingContext))
                .thenReturn(templateContext);
        doReturn(mElementListAdapter)
                .when(mAdapterFactory)
                .createAdapterForElement(DEFAULT_TEMPLATE_ELEMENT, templateContext);

        TemplateKey templateKey = new TemplateKey(template, sharedStates, new ArrayList<>());
        when(mElementListAdapter.getKey()).thenReturn(templateKey);
        mTemplateRecyclerPool.put(templateKey, mElementListAdapter);

        // Create adapter and ensure template key is set.
        ElementAdapter<? extends View, ?> adapter =
                mTemplateBinder.createAndBindTemplateAdapter(model, mFrameContext);
        assertThat(adapter).isSameInstanceAs(mElementListAdapter);

        // We don't get the adapter from the factory
        verifyZeroInteractions(mAdapterFactory);
        verify(mElementListAdapter, never()).setKey(any(TemplateKey.class));

        // Assert that adapter is bound with the template frameContext
        verify(mElementListAdapter).bindModel(model.getTemplate().getElement(), templateContext);
    }

    @Test
    public void testTemplateKey_equalWithSameObjects() {
        Template template = Template.newBuilder().setTemplateId("T").build();
        Stylesheet stylesheet = Stylesheet.newBuilder().setStylesheetId("S").build();
        List<PietSharedState> sharedStates =
                listOfSharedStates(PietSharedState.newBuilder().addTemplates(template).build());
        List<Stylesheet> stylesheets = Collections.singletonList(stylesheet);
        TemplateKey key1 = new TemplateKey(template, sharedStates, stylesheets);
        TemplateKey key2 = new TemplateKey(template, sharedStates, stylesheets);

        assertThat(key1.hashCode()).isEqualTo(key2.hashCode());
        assertThat(key1).isEqualTo(key2);
    }

    @Test
    public void testTemplateKey_equalWithDifferentTemplateObject() {
        Template template1 = Template.newBuilder().setTemplateId("T").build();
        Template template2 = Template.newBuilder().setTemplateId("T").build();
        PietSharedState sharedState = PietSharedState.newBuilder().addTemplates(template1).build();
        TemplateKey key1 =
                new TemplateKey(template1, listOfSharedStates(sharedState), new ArrayList<>());
        TemplateKey key2 =
                new TemplateKey(template2, listOfSharedStates(sharedState), new ArrayList<>());

        assertThat(key1.hashCode()).isEqualTo(key2.hashCode());
        assertThat(key1).isEqualTo(key2);
    }

    @Test
    public void testTemplateKey_equalWithDifferentSharedStateObject() {
        Template template = Template.newBuilder().setTemplateId("T").build();
        PietSharedState sharedState1 = PietSharedState.newBuilder().addTemplates(template).build();
        PietSharedState sharedState2 = PietSharedState.newBuilder().addTemplates(template).build();
        TemplateKey key1 =
                new TemplateKey(template, listOfSharedStates(sharedState1), new ArrayList<>());
        TemplateKey key2 =
                new TemplateKey(template, listOfSharedStates(sharedState2), new ArrayList<>());

        assertThat(key1.hashCode()).isEqualTo(key2.hashCode());
        assertThat(key1).isEqualTo(key2);
    }

    @Test
    public void testTemplateKey_equalWithDifferentStylesheetObject() {
        Template template = Template.newBuilder().setTemplateId("T").build();
        PietSharedState sharedState = PietSharedState.newBuilder().addTemplates(template).build();
        Stylesheet stylesheet1 = Stylesheet.newBuilder().setStylesheetId("1").build();
        Stylesheet stylesheet2 = Stylesheet.newBuilder().setStylesheetId("1").build();
        TemplateKey key1 = new TemplateKey(
                template, listOfSharedStates(sharedState), Collections.singletonList(stylesheet1));
        TemplateKey key2 = new TemplateKey(
                template, listOfSharedStates(sharedState), Collections.singletonList(stylesheet2));

        assertThat(key1.hashCode()).isEqualTo(key2.hashCode());
        assertThat(key1).isEqualTo(key2);
    }

    @Test
    public void testTemplateKey_differentWithDifferentLengthSharedStates() {
        Template template = Template.newBuilder().setTemplateId("T").build();
        PietSharedState sharedState1 = PietSharedState.newBuilder().addTemplates(template).build();
        TemplateKey key1 =
                new TemplateKey(template, listOfSharedStates(sharedState1), new ArrayList<>());
        TemplateKey key2 = new TemplateKey(
                template, listOfSharedStates(sharedState1, sharedState1), new ArrayList<>());

        assertThat(key1.hashCode()).isNotEqualTo(key2.hashCode());
        assertThat(key1).isNotEqualTo(key2);
    }

    @Test
    public void testTemplateKey_differentWithDifferentTemplate() {
        Template template1 = Template.newBuilder().setTemplateId("T1").build();
        Template template2 = Template.newBuilder().setTemplateId("T2").build();
        List<PietSharedState> sharedStates =
                listOfSharedStates(PietSharedState.newBuilder().addTemplates(template1).build());
        TemplateKey key1 = new TemplateKey(template1, sharedStates, new ArrayList<>());
        TemplateKey key2 = new TemplateKey(template2, sharedStates, new ArrayList<>());

        assertThat(key1.hashCode()).isNotEqualTo(key2.hashCode());
        assertThat(key1).isNotEqualTo(key2);
    }

    @Test
    public void testTemplateKey_differentWithDifferentSharedState() {
        Template template = Template.newBuilder().setTemplateId("T").build();
        PietSharedState sharedState1 = PietSharedState.newBuilder().addTemplates(template).build();
        PietSharedState sharedState2 = PietSharedState.getDefaultInstance();
        TemplateKey key1 =
                new TemplateKey(template, listOfSharedStates(sharedState1), new ArrayList<>());
        TemplateKey key2 =
                new TemplateKey(template, listOfSharedStates(sharedState2), new ArrayList<>());

        assertThat(key1.hashCode()).isNotEqualTo(key2.hashCode());
        assertThat(key1).isNotEqualTo(key2);
    }

    @Test
    public void testTemplateKey_differentWithDifferentStylesheet() {
        Template template = Template.newBuilder().setTemplateId("T").build();
        PietSharedState sharedState = PietSharedState.newBuilder().addTemplates(template).build();
        Stylesheet stylesheet1 = Stylesheet.newBuilder().setStylesheetId("1").build();
        Stylesheet stylesheet2 = Stylesheet.newBuilder().setStylesheetId("2").build();
        TemplateKey key1 = new TemplateKey(
                template, listOfSharedStates(sharedState), Collections.singletonList(stylesheet1));
        TemplateKey key2 = new TemplateKey(
                template, listOfSharedStates(sharedState), Collections.singletonList(stylesheet2));

        assertThat(key1.hashCode()).isNotEqualTo(key2.hashCode());
        assertThat(key1).isNotEqualTo(key2);
    }

    @Test
    public void testTemplateAdapterModel_getters() {
        TemplateAdapterModel model =
                new TemplateAdapterModel(MODEL_TEMPLATE, MODEL_BINDING_CONTEXT);
        assertThat(model.getTemplate()).isSameInstanceAs(MODEL_TEMPLATE);
        assertThat(model.getBindingContext()).isSameInstanceAs(MODEL_BINDING_CONTEXT);
    }

    @Test
    public void testTemplateAdapterModel_lookUpTemplate() {
        ActionHandler actionHandler = mock(ActionHandler.class);
        List<PietSharedState> pietSharedStates = Collections.emptyList();
        Context context = Robolectric.buildActivity(Activity.class).get();

        HostProviders mockHostProviders = mock(HostProviders.class);
        AssetProvider mockAssetProvider = mock(AssetProvider.class);
        when(mockHostProviders.getAssetProvider()).thenReturn(mockAssetProvider);

        MediaQueryHelper mediaQueryHelper =
                new MediaQueryHelper(FRAME_WIDTH_PX, mockAssetProvider, context);
        PietStylesHelper pietStylesHelper =
                new PietStylesHelperFactory().get(pietSharedStates, mediaQueryHelper);

        FrameContext frameContext = FrameContext.createFrameContext(
                DEFAULT_FRAME, // This defines MODEL_TEMPLATE
                pietSharedStates, pietStylesHelper, DebugBehavior.VERBOSE, new DebugLogger(),
                actionHandler, mockHostProviders, new FrameLayout(context));

        TemplateAdapterModel model = new TemplateAdapterModel(
                MODEL_TEMPLATE.getTemplateId(), frameContext, MODEL_BINDING_CONTEXT);
        assertThat(model.getTemplate()).isSameInstanceAs(MODEL_TEMPLATE);
        assertThat(model.getBindingContext()).isSameInstanceAs(MODEL_BINDING_CONTEXT);
    }

    @Test
    public void testTemplateAdapterModel_equalsSame() {
        TemplateAdapterModel model1 =
                new TemplateAdapterModel(MODEL_TEMPLATE, MODEL_BINDING_CONTEXT);
        TemplateAdapterModel model2 =
                new TemplateAdapterModel(MODEL_TEMPLATE, MODEL_BINDING_CONTEXT);
        assertThat(model1).isEqualTo(model2);
        assertThat(model1.hashCode()).isEqualTo(model2.hashCode());
    }

    @Test
    public void testTemplateAdapterModel_equalsOtherInstance() {
        TemplateAdapterModel model1 =
                new TemplateAdapterModel(MODEL_TEMPLATE, MODEL_BINDING_CONTEXT);
        TemplateAdapterModel model2 = new TemplateAdapterModel(
                MODEL_TEMPLATE.toBuilder().build(), MODEL_BINDING_CONTEXT.toBuilder().build());
        assertThat(model1.getTemplate()).isNotSameInstanceAs(model2.getTemplate());
        assertThat(model1.getBindingContext()).isNotSameInstanceAs(model2.getBindingContext());
        assertThat(model1).isEqualTo(model2);
        assertThat(model1.hashCode()).isEqualTo(model2.hashCode());
    }

    @Test
    public void testTemplateAdapterModel_notEquals() {
        TemplateAdapterModel model1 =
                new TemplateAdapterModel(MODEL_TEMPLATE, MODEL_BINDING_CONTEXT);
        TemplateAdapterModel model2 = new TemplateAdapterModel(
                MODEL_TEMPLATE.toBuilder().clearTemplateId().build(), MODEL_BINDING_CONTEXT);
        TemplateAdapterModel model3 = new TemplateAdapterModel(
                MODEL_TEMPLATE, MODEL_BINDING_CONTEXT.toBuilder().clearBindingValues().build());
        assertThat(model1).isNotEqualTo(model2);
        assertThat(model1.hashCode()).isNotEqualTo(model2.hashCode());
        assertThat(model1).isNotEqualTo(model3);
        assertThat(model1.hashCode()).isNotEqualTo(model3.hashCode());
    }

    private List<PietSharedState> listOfSharedStates(PietSharedState... pietSharedStates) {
        List<PietSharedState> sharedStates = new ArrayList<>();
        Collections.addAll(sharedStates, pietSharedStates);
        return sharedStates;
    }
}
