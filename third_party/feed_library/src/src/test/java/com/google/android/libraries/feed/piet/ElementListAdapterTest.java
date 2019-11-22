// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.android.libraries.feed.common.testing.RunnableSubject.assertThatRunnable;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

import com.google.android.libraries.feed.common.functional.Suppliers;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.piet.ElementListAdapter.KeySupplier;
import com.google.android.libraries.feed.piet.host.ActionHandler;
import com.google.android.libraries.feed.piet.host.AssetProvider;
import com.google.android.libraries.feed.testing.shadows.ExtendedShadowLinearLayout;
import com.google.search.now.ui.piet.BindingRefsProto.ElementBindingRef;
import com.google.search.now.ui.piet.BindingRefsProto.StyleBindingRef;
import com.google.search.now.ui.piet.ElementsProto.BindingValue;
import com.google.search.now.ui.piet.ElementsProto.Content;
import com.google.search.now.ui.piet.ElementsProto.CustomElement;
import com.google.search.now.ui.piet.ElementsProto.Element;
import com.google.search.now.ui.piet.ElementsProto.ElementList;
import com.google.search.now.ui.piet.ElementsProto.ElementStack;
import com.google.search.now.ui.piet.ElementsProto.ImageElement;
import com.google.search.now.ui.piet.ElementsProto.Visibility;
import com.google.search.now.ui.piet.PietProto.Frame;
import com.google.search.now.ui.piet.RoundedCornersProto.RoundedCorners;
import com.google.search.now.ui.piet.StylesProto.EdgeWidths;
import com.google.search.now.ui.piet.StylesProto.StyleIdsStack;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;

/** Tests of the {@link ElementListAdapter}. */
@RunWith(RobolectricTestRunner.class)
public class ElementListAdapterTest {
    private static final String LIST_STYLE_ID = "manycats";
    private static final StyleIdsStack LIST_STYLES =
            StyleIdsStack.newBuilder().addStyleIds(LIST_STYLE_ID).build();

    private static final Element DEFAULT_ELEMENT =
            Element.newBuilder().setElementStack(ElementStack.getDefaultInstance()).build();
    private static final Content DEFAULT_CONTENT =
            Content.newBuilder().setElement(DEFAULT_ELEMENT).build();
    private static final Element IMAGE_ELEMENT =
            Element.newBuilder().setImageElement(ImageElement.getDefaultInstance()).build();
    private static final ElementList DEFAULT_LIST =
            ElementList.newBuilder().addContents(DEFAULT_CONTENT).build();

    private static final ElementBindingRef ELEMENT_BINDING_REF =
            ElementBindingRef.newBuilder().setBindingId("shopping").build();
    private static final Element LIST_WITH_BOUND_LIST = asElement(
            ElementList.newBuilder()
                    .addContents(Content.newBuilder().setBoundElement(ELEMENT_BINDING_REF))
                    .build());

    @Mock
    private ActionHandler actionHandler;
    @Mock
    private FrameContext frameContext;
    @Mock
    private StyleProvider styleProvider;
    @Mock
    private HostProviders hostProviders;
    @Mock
    private AssetProvider assetProvider;

    private Context context;
    private AdapterParameters adapterParameters;

    private ElementListAdapter adapter;

    @Before
    public void setUp() {
        initMocks(this);
        context = Robolectric.buildActivity(Activity.class).get();

        when(frameContext.makeStyleFor(LIST_STYLES)).thenReturn(styleProvider);
        when(styleProvider.getPadding()).thenReturn(EdgeWidths.getDefaultInstance());
        when(frameContext.getActionHandler()).thenReturn(actionHandler);
        when(hostProviders.getAssetProvider()).thenReturn(assetProvider);
        when(assetProvider.isRtL()).thenReturn(false);
        when(styleProvider.getRoundedCorners()).thenReturn(RoundedCorners.getDefaultInstance());

        adapterParameters = new AdapterParameters(
                context, Suppliers.of(null), hostProviders, new FakeClock(), false, false);

        when(frameContext.makeStyleFor(StyleIdsStack.getDefaultInstance()))
                .thenReturn(adapterParameters.defaultStyleProvider);

        adapter = new KeySupplier().getAdapter(context, adapterParameters);
    }

    @Test
    public void testViewDoesNotClip() {
        assertThat(adapter.getBaseView().getClipToPadding()).isFalse();
    }

    @Test
    public void testOnCreateAdapter_makesList() {
        ElementList listWithStyles = ElementList.newBuilder()
                                             .addContents(DEFAULT_CONTENT)
                                             .addContents(DEFAULT_CONTENT)
                                             .addContents(DEFAULT_CONTENT)
                                             .build();

        adapter.createAdapter(asElementWithDefaultStyle(listWithStyles), frameContext);

        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(3);
        assertThat(adapter.getBaseView().getOrientation()).isEqualTo(LinearLayout.VERTICAL);
    }

    @Test
    public void testOnCreateAdapter_setsStyles() {
        ElementList listWithStyles = ElementList.newBuilder().addContents(DEFAULT_CONTENT).build();

        adapter.createAdapter(asElementWithDefaultStyle(listWithStyles), frameContext);

        verify(styleProvider).applyElementStyles(adapter);
    }

    @Config(shadows = {ExtendedShadowLinearLayout.class})
    @Test
    public void testTriggerActions_triggersChildren() {
        Frame frame = Frame.newBuilder().setTag("Frame").build();
        when(frameContext.getFrame()).thenReturn(frame);
        Element baseElement =
                Element.newBuilder()
                        .setElementList(ElementList.newBuilder().addContents(
                                Content.newBuilder().setElement(Element.newBuilder().setElementList(
                                        ElementList.getDefaultInstance()))))
                        .build();
        adapter.createAdapter(baseElement, frameContext);
        adapter.bindModel(baseElement, frameContext);

        // Replace the child adapter so we can verify on it
        ElementAdapter<?, ?> mockChildAdapter = mock(ElementAdapter.class);
        adapter.childAdapters.set(0, mockChildAdapter);

        ExtendedShadowLinearLayout shadowView = Shadow.extract(adapter.getView());
        adapter.getView().setVisibility(View.VISIBLE);
        shadowView.setAttachedToWindow(true);

        View viewport = new View(context);

        adapter.triggerViewActions(viewport, frameContext);

        verify(mockChildAdapter).triggerViewActions(viewport, frameContext);
    }

    @Test
    public void testOnBindModel_setsStylesOnlyIfBindingIsDefined() {
        ElementList list = ElementList.newBuilder().addContents(DEFAULT_CONTENT).build();

        adapter.createAdapter(asElement(list, LIST_STYLES), frameContext);
        verify(frameContext).makeStyleFor(LIST_STYLES);

        // Binding an element with a different style will not apply the new style
        StyleIdsStack otherStyles = StyleIdsStack.newBuilder().addStyleIds("bobcat").build();

        adapter.bindModel(asElement(list, otherStyles), frameContext);
        verify(frameContext, never()).makeStyleFor(otherStyles);

        // But binding an element with a style binding will re-apply the style
        StyleIdsStack otherStylesWithBinding =
                StyleIdsStack.newBuilder()
                        .addStyleIds("bobcat")
                        .setStyleBinding(StyleBindingRef.newBuilder().setBindingId("lynx"))
                        .build();
        when(frameContext.makeStyleFor(otherStylesWithBinding)).thenReturn(styleProvider);

        adapter.bindModel(asElement(list, otherStylesWithBinding), frameContext);
        verify(frameContext).makeStyleFor(otherStylesWithBinding);
    }

    @Test
    public void testOnBindModel_failsWithIncompatibleModel() {
        ElementList listWithThreeElements = ElementList.newBuilder()
                                                    .addContents(DEFAULT_CONTENT)
                                                    .addContents(DEFAULT_CONTENT)
                                                    .addContents(DEFAULT_CONTENT)
                                                    .build();

        adapter.createAdapter(asElementWithDefaultStyle(listWithThreeElements), frameContext);
        adapter.bindModel(asElementWithDefaultStyle(listWithThreeElements), frameContext);
        adapter.unbindModel();

        ElementList listWithTwoElements = ElementList.newBuilder()
                                                  .addContents(DEFAULT_CONTENT)
                                                  .addContents(DEFAULT_CONTENT)
                                                  .build();

        assertThatRunnable(
                ()
                        -> adapter.bindModel(
                                asElementWithDefaultStyle(listWithTwoElements), frameContext))
                .throwsAnExceptionOfType(IllegalStateException.class)
                .that()
                .hasMessageThat()
                .contains("Internal error in adapters per content");
    }

    @Test
    public void testOnBindModel_elementListBindingRecreatesAdapter() {
        Element listWithOneItem =
                Element.newBuilder()
                        .setElementList(ElementList.newBuilder().addContents(DEFAULT_CONTENT))
                        .build();
        Element listWithTwoItems = Element.newBuilder()
                                           .setElementList(ElementList.newBuilder()
                                                                   .addContents(DEFAULT_CONTENT)
                                                                   .addContents(DEFAULT_CONTENT))
                                           .build();

        when(frameContext.getElementBindingValue(ELEMENT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder().setElement(listWithOneItem).build());
        adapter.createAdapter(LIST_WITH_BOUND_LIST, frameContext);
        // No child adapters have been created yet.
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(adapter.childAdapters).isEmpty();

        when(frameContext.getElementBindingValue(ELEMENT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder().setElement(listWithTwoItems).build());
        adapter.bindModel(LIST_WITH_BOUND_LIST, frameContext);
        // The list adapter creates its one view on bind.
        assertThat(((LinearLayout) adapter.getBaseView().getChildAt(0)).getChildCount())
                .isEqualTo(2);

        adapter.unbindModel();
        // The list adapter has been released.
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(adapter.childAdapters).isEmpty();

        when(frameContext.getElementBindingValue(ELEMENT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder().setElement(listWithOneItem).build());
        adapter.bindModel(LIST_WITH_BOUND_LIST, frameContext);
        // The list adapter can bind to a different model.
        assertThat(((LinearLayout) adapter.getBaseView().getChildAt(0)).getChildCount())
                .isEqualTo(1);
    }

    @Test
    public void testOnBindModel_bindingWithVisibilityGone() {
        when(frameContext.getElementBindingValue(ELEMENT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(ELEMENT_BINDING_REF.getBindingId())
                                    .setElement(Element.newBuilder().setElementList(DEFAULT_LIST))
                                    .build());
        adapter.createAdapter(LIST_WITH_BOUND_LIST, frameContext);
        when(frameContext.getElementBindingValue(ELEMENT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(ELEMENT_BINDING_REF.getBindingId())
                                    .setVisibility(Visibility.GONE)
                                    .build());

        adapter.bindModel(LIST_WITH_BOUND_LIST, frameContext);

        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(adapter.childAdapters).isEmpty();
    }

    @Test
    public void testOnBindModel_bindingWithNoContent() {
        when(frameContext.getElementBindingValue(ELEMENT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(ELEMENT_BINDING_REF.getBindingId())
                                    .setElement(DEFAULT_ELEMENT)
                                    .build());
        adapter.createAdapter(LIST_WITH_BOUND_LIST, frameContext);
        when(frameContext.getElementBindingValue(ELEMENT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(ELEMENT_BINDING_REF.getBindingId())
                                    .build());

        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(adapter.childAdapters).isEmpty();
    }

    @Test
    public void testOnBindModel_bindingWithOptionalAbsent() {
        when(frameContext.getElementBindingValue(ELEMENT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(ELEMENT_BINDING_REF.getBindingId())
                                    .setElement(DEFAULT_ELEMENT)
                                    .build());
        adapter.createAdapter(LIST_WITH_BOUND_LIST, frameContext);

        ElementBindingRef optionalBinding =
                ELEMENT_BINDING_REF.toBuilder().setIsOptional(true).build();
        ElementList optionalBindingList =
                ElementList.newBuilder()
                        .addContents(Content.newBuilder().setBoundElement(optionalBinding))
                        .build();
        when(frameContext.getElementBindingValue(optionalBinding))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(optionalBinding.getBindingId())
                                    .build());

        adapter.bindModel(asElement(optionalBindingList), frameContext);

        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(adapter.childAdapters).isEmpty();
    }

    @Test
    public void testOnBindModel_setsLayoutParamsOnChild() {
        ElementList list =
                ElementList.newBuilder()
                        .addContents(Content.newBuilder().setElement(
                                DEFAULT_ELEMENT.toBuilder().setStyleReferences(LIST_STYLES)))
                        .build();
        when(styleProvider.getGravityHorizontal(anyInt())).thenReturn(Gravity.CENTER_HORIZONTAL);

        adapter.createAdapter(asElement(list), frameContext);

        adapter.childAdapters.get(0).widthPx = 123;
        adapter.childAdapters.get(0).heightPx = 456;

        adapter.bindModel(asElement(list), frameContext);

        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(1);
        assertThat(adapter.getBaseView().getChildAt(0).getLayoutParams())
                .isInstanceOf(LinearLayout.LayoutParams.class);
        assertThat(((LayoutParams) adapter.getBaseView().getChildAt(0).getLayoutParams()).gravity)
                .isEqualTo(Gravity.CENTER_HORIZONTAL);
        assertThat(((LayoutParams) adapter.getBaseView().getChildAt(0).getLayoutParams()).width)
                .isEqualTo(123);
        assertThat(((LayoutParams) adapter.getBaseView().getChildAt(0).getLayoutParams()).height)
                .isEqualTo(456);
    }

    @Test
    public void testOnBindModel_setsMargins() {
        String marginsStyleId = "spacecat";
        StyleIdsStack marginsStyles =
                StyleIdsStack.newBuilder().addStyleIds(marginsStyleId).build();
        Element elementWithMargins = Element.newBuilder()
                                             .setStyleReferences(marginsStyles)
                                             .setElementStack(ElementStack.getDefaultInstance())
                                             .build();
        ElementList list = ElementList.newBuilder()
                                   .addContents(Content.newBuilder().setElement(elementWithMargins))
                                   .build();
        when(frameContext.makeStyleFor(marginsStyles)).thenReturn(styleProvider);

        adapter.createAdapter(asElement(list), frameContext);
        adapter.bindModel(asElement(list), frameContext);

        // Assert that applyMargins is called on the child's layout params
        ArgumentCaptor<MarginLayoutParams> capturedLayoutParams =
                ArgumentCaptor.forClass(MarginLayoutParams.class);
        verify(styleProvider).applyMargins(eq(context), capturedLayoutParams.capture());

        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(1);
        assertThat(adapter.getBaseView().getChildAt(0).getLayoutParams())
                .isInstanceOf(MarginLayoutParams.class);
        assertThat(adapter.getBaseView().getChildAt(0).getLayoutParams())
                .isSameInstanceAs(capturedLayoutParams.getValue());
    }

    @Test
    public void testReleaseAdapter() {
        ElementList listWithStyles = ElementList.newBuilder()
                                             .addContents(DEFAULT_CONTENT)
                                             .addContents(DEFAULT_CONTENT)
                                             .addContents(DEFAULT_CONTENT)
                                             .build();

        adapter.createAdapter(asElementWithDefaultStyle(listWithStyles), frameContext);
        adapter.bindModel(asElementWithDefaultStyle(listWithStyles), frameContext);

        adapter.releaseAdapter();
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(adapter.childAdapters).isEmpty();
    }

    @Test
    public void testGetVerticalGravity_noModel() {
        assertThat(adapter.getVerticalGravity(Gravity.CLIP_VERTICAL))
                .isEqualTo(Gravity.CLIP_VERTICAL);
    }

    @Test
    public void testGetStyleIdsStack() {
        ElementList listWithStyles = ElementList.newBuilder().addContents(DEFAULT_CONTENT).build();

        adapter.createAdapter(asElement(listWithStyles, LIST_STYLES), frameContext);

        assertThat(adapter.getElementStyleIdsStack()).isEqualTo(LIST_STYLES);
    }

    @Test
    public void testGetModelFromElement() {
        ElementList model = ElementList.newBuilder().addContents(DEFAULT_CONTENT).build();

        Element elementWithModel = Element.newBuilder().setElementList(model).build();
        assertThat(adapter.getModelFromElement(elementWithModel)).isSameInstanceAs(model);

        Element elementWithWrongModel =
                Element.newBuilder().setCustomElement(CustomElement.getDefaultInstance()).build();
        assertThatRunnable(() -> adapter.getModelFromElement(elementWithWrongModel))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Missing ElementList");

        Element emptyElement = Element.getDefaultInstance();
        assertThatRunnable(() -> adapter.getModelFromElement(emptyElement))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Missing ElementList");
    }

    @Test
    public void testSetLayoutParamsOnChild() {
        ElementList list =
                ElementList.newBuilder()
                        .addContents(Content.newBuilder().setElement(
                                DEFAULT_ELEMENT.toBuilder().setStyleReferences(LIST_STYLES)))
                        .build();
        when(styleProvider.getWidthSpecPx(context)).thenReturn(123);
        when(styleProvider.getHeightSpecPx(context)).thenReturn(456);
        when(styleProvider.getGravityHorizontal(anyInt())).thenReturn(Gravity.CENTER_HORIZONTAL);

        adapter.createAdapter(asElement(list), frameContext);

        LayoutParams layoutParams = new LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        adapter.setLayoutParams(layoutParams);

        assertThat(((LayoutParams) adapter.getBaseView().getChildAt(0).getLayoutParams()).gravity)
                .isEqualTo(Gravity.CENTER_HORIZONTAL);
        assertThat(((LayoutParams) adapter.getBaseView().getChildAt(0).getLayoutParams()).width)
                .isEqualTo(123);
        assertThat(((LayoutParams) adapter.getBaseView().getChildAt(0).getLayoutParams()).height)
                .isEqualTo(456);
    }

    @Test
    public void testSetLayoutParams_childWidthSet() {
        int childWidth = 5;

        ElementList list = ElementList.newBuilder()
                                   .addContents(Content.newBuilder().setElement(IMAGE_ELEMENT))
                                   .build();

        adapter.createAdapter(asElement(list), frameContext);
        StyleProvider childStyleProvider = mock(StyleProvider.class);
        ImageElementAdapter mockChildAdapter = mock(ImageElementAdapter.class);
        when(mockChildAdapter.getComputedWidthPx()).thenReturn(childWidth);
        when(mockChildAdapter.getElementStyle()).thenReturn(childStyleProvider);
        when(mockChildAdapter.getHorizontalGravity(anyInt())).thenReturn(Gravity.CENTER_HORIZONTAL);
        when(mockChildAdapter.getElementStyle().hasGravityHorizontal()).thenReturn(true);
        adapter.childAdapters.set(0, mockChildAdapter);

        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        adapter.setLayoutParams(params);

        ArgumentCaptor<LinearLayout.LayoutParams> childLayoutParamsCaptor =
                ArgumentCaptor.forClass(LinearLayout.LayoutParams.class);
        verify(mockChildAdapter).setLayoutParams(childLayoutParamsCaptor.capture());

        LinearLayout.LayoutParams childLayoutParams = childLayoutParamsCaptor.getValue();
        assertThat(childLayoutParams.width).isEqualTo(childWidth);
        assertThat(childLayoutParams.gravity).isEqualTo(Gravity.CENTER_HORIZONTAL);

        verify(childStyleProvider).applyMargins(context, childLayoutParams);
    }

    @Test
    public void testSetLayoutParams_widthSetOnList() {
        ElementList list = ElementList.newBuilder()
                                   .addContents(Content.newBuilder().setElement(IMAGE_ELEMENT))
                                   .build();

        adapter.createAdapter(asElement(list), frameContext);

        LinearLayout.LayoutParams params =
                new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.weight = 1;
        adapter.setLayoutParams(params);

        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(1);
        assertThat(adapter.getBaseView().getChildAt(0).getLayoutParams())
                .isInstanceOf(LinearLayout.LayoutParams.class);
        assertThat(((LayoutParams) adapter.getBaseView().getChildAt(0).getLayoutParams()).width)
                .isEqualTo(ViewGroup.LayoutParams.MATCH_PARENT);
        assertThat(((LayoutParams) adapter.getBaseView().getChildAt(0).getLayoutParams()).height)
                .isEqualTo(ViewGroup.LayoutParams.WRAP_CONTENT);
    }

    private static Element asElement(ElementList elementList) {
        return Element.newBuilder().setElementList(elementList).build();
    }

    private static Element asElement(ElementList elementList, StyleIdsStack styles) {
        return Element.newBuilder().setStyleReferences(styles).setElementList(elementList).build();
    }

    private static Element asElementWithDefaultStyle(ElementList elementList) {
        return Element.newBuilder()
                .setStyleReferences(LIST_STYLES)
                .setElementList(elementList)
                .build();
    }
}
