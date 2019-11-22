// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.android.libraries.feed.piet.StyleProvider.DIMENSION_NOT_SET;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.FrameLayout;

import com.google.android.libraries.feed.common.functional.Suppliers;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.piet.host.ActionHandler;
import com.google.android.libraries.feed.piet.host.ActionHandler.ActionType;
import com.google.android.libraries.feed.piet.host.LogDataCallback;
import com.google.android.libraries.feed.piet.ui.RoundedCornerMaskCache;
import com.google.android.libraries.feed.piet.ui.RoundedCornerWrapperView;
import com.google.android.libraries.feed.testing.shadows.ExtendedShadowView;
import com.google.search.now.ui.piet.AccessibilityProto.Accessibility;
import com.google.search.now.ui.piet.AccessibilityProto.AccessibilityRole;
import com.google.search.now.ui.piet.ActionsProto.Action;
import com.google.search.now.ui.piet.ActionsProto.Actions;
import com.google.search.now.ui.piet.ActionsProto.VisibilityAction;
import com.google.search.now.ui.piet.BindingRefsProto.ActionsBindingRef;
import com.google.search.now.ui.piet.BindingRefsProto.LogDataBindingRef;
import com.google.search.now.ui.piet.BindingRefsProto.ParameterizedTextBindingRef;
import com.google.search.now.ui.piet.BindingRefsProto.VisibilityBindingRef;
import com.google.search.now.ui.piet.ElementsProto.BindingValue;
import com.google.search.now.ui.piet.ElementsProto.Element;
import com.google.search.now.ui.piet.ElementsProto.Visibility;
import com.google.search.now.ui.piet.ElementsProto.VisibilityState;
import com.google.search.now.ui.piet.LogDataProto.LogData;
import com.google.search.now.ui.piet.PietProto.Frame;
import com.google.search.now.ui.piet.RoundedCornersProto.RoundedCorners;
import com.google.search.now.ui.piet.StylesProto.Borders;
import com.google.search.now.ui.piet.StylesProto.StyleIdsStack;
import com.google.search.now.ui.piet.TextProto.ParameterizedText;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;

/** Tests of the {@link ElementAdapter}. */
@RunWith(RobolectricTestRunner.class)
public class ElementAdapterTest {
    private static final int WIDTH = 123;
    private static final int HEIGHT = 456;
    private static final boolean LEGACY_CORNERS_FLAG;
    private static final boolean OUTLINE_CORNERS_FLAG;

    private final LogData logDataTest = LogData.newBuilder().build();

    @Mock
    private FrameContext frameContext;
    @Mock
    private HostProviders hostProviders;
    @Mock
    private ActionHandler actionHandler;
    @Mock
    private StyleProvider styleProvider;

    private Context context;
    private AdapterParameters parameters;
    private View view;
    private RoundedCornerMaskCache maskCache;

    private TestElementAdapter adapter;
    private boolean callbackBound;
    private boolean callbackUnbound;

    @Before
    public void setUp() {
        initMocks(this);
        context = Robolectric.buildActivity(Activity.class).get();
        LogDataCallback logDataCallback = new LogDataCallback() {
            @Override
            public void onBind(LogData logData, View view) {
                assertThat(logDataTest).isEqualTo(logData);
                callbackBound = true;
            }

            @Override
            public void onUnbind(LogData logData, View view) {
                assertThat(logDataTest).isEqualTo(logData);
                callbackUnbound = true;
            }
        };
        parameters = new AdapterParameters(context, Suppliers.of((ViewGroup) null), hostProviders,
                new FakeClock(), LEGACY_CORNERS_FLAG, OUTLINE_CORNERS_FLAG);
        maskCache = parameters.roundedCornerMaskCache;
        when(hostProviders.getLogDataCallback()).thenReturn(logDataCallback);
        when(frameContext.makeStyleFor(any(StyleIdsStack.class))).thenReturn(styleProvider);
        when(frameContext.getActionHandler()).thenReturn(actionHandler);
        when(styleProvider.hasRoundedCorners()).thenReturn(false);
        when(styleProvider.getRoundedCorners()).thenReturn(RoundedCorners.getDefaultInstance());
        when(styleProvider.createWrapperView(
                     context, maskCache, LEGACY_CORNERS_FLAG, OUTLINE_CORNERS_FLAG))
                .thenReturn(new FrameLayout(context));

        view = new View(context);
        adapter = new TestElementAdapter(context, parameters, view);
    }

    @Test
    public void testGetters() {
        // Pre-creation
        assertThat(adapter.getHorizontalGravity(Gravity.CLIP_HORIZONTAL))
                .isEqualTo(Gravity.CLIP_HORIZONTAL);
        assertThat(adapter.getVerticalGravity(Gravity.CLIP_VERTICAL))
                .isEqualTo(Gravity.CLIP_VERTICAL);
        assertThat(adapter.getGravity(Gravity.AXIS_Y_SHIFT)).isEqualTo(Gravity.AXIS_Y_SHIFT);

        Element defaultElement = Element.newBuilder().build();
        Frame frame = Frame.newBuilder().setTag("FRAME").build();
        when(frameContext.getFrame()).thenReturn(frame);
        when(styleProvider.getGravityHorizontal(Gravity.CLIP_HORIZONTAL))
                .thenReturn(Gravity.CENTER_HORIZONTAL);
        when(styleProvider.hasGravityHorizontal()).thenReturn(true);
        when(styleProvider.hasGravityVertical()).thenReturn(true);
        when(styleProvider.getGravityHorizontal(anyInt())).thenReturn(Gravity.CENTER_HORIZONTAL);
        when(styleProvider.getGravityVertical(anyInt())).thenReturn(Gravity.CENTER_VERTICAL);
        // This should typically be CENTER_HORIZONTAL | CENTER_VERTICAL but I want to make sure that
        // ElementAdapter is calling this method instead of OR'ing the H/V gravities itself.
        when(styleProvider.getGravity(anyInt())).thenReturn(Gravity.FILL);

        adapter.createAdapter(defaultElement, defaultElement, frameContext);

        assertThat(adapter.getContext()).isSameInstanceAs(context);
        assertThat(adapter.getModel()).isSameInstanceAs(defaultElement);
        assertThat(adapter.getRawModel()).isSameInstanceAs(defaultElement);
        assertThat(adapter.getHorizontalGravity(Gravity.CLIP_HORIZONTAL))
                .isEqualTo(Gravity.CENTER_HORIZONTAL);
        assertThat(adapter.getParameters()).isSameInstanceAs(parameters);
        assertThat(adapter.getTemplatedStringEvaluator())
                .isSameInstanceAs(parameters.templatedStringEvaluator);
        assertThat(adapter.getElementStyle()).isSameInstanceAs(styleProvider);
        assertThat(adapter.getElementStyleIdsStack()).isEqualTo(StyleIdsStack.getDefaultInstance());

        assertThat(adapter.getHorizontalGravity(Gravity.CLIP_VERTICAL))
                .isEqualTo(Gravity.CENTER_HORIZONTAL);
        assertThat(adapter.getVerticalGravity(Gravity.CLIP_HORIZONTAL))
                .isEqualTo(Gravity.CENTER_VERTICAL);
        assertThat(adapter.getGravity(Gravity.CLIP_VERTICAL)).isEqualTo(Gravity.FILL);
    }

    @Test
    public void setLayoutParams() {
        adapter.setLayoutParams(new LayoutParams(WIDTH, HEIGHT));

        assertThat(view.getLayoutParams().width).isEqualTo(WIDTH);
        assertThat(view.getLayoutParams().height).isEqualTo(HEIGHT);
    }

    @Test
    public void testGetComputedDimensions_defaults() {
        assertThat(adapter.getComputedWidthPx()).isEqualTo(DIMENSION_NOT_SET);
        assertThat(adapter.getComputedHeightPx()).isEqualTo(DIMENSION_NOT_SET);
    }

    @Test
    public void getComputedDimensions_explicit() {
        adapter.setDims(WIDTH, HEIGHT);

        assertThat(adapter.getComputedWidthPx()).isEqualTo(WIDTH);
        assertThat(adapter.getComputedHeightPx()).isEqualTo(HEIGHT);
    }

    @Test
    public void createAdapter_callsOnCreateAdapter() {
        Element defaultElement = Element.getDefaultInstance();

        adapter.createAdapter(defaultElement, defaultElement, frameContext);

        assertThat(adapter.testAdapterCreated).isTrue();
    }

    @Test
    public void createAdapter_extractsModelFromElement() {
        Element element = Element.getDefaultInstance();

        adapter.createAdapter(element, frameContext);

        assertThat(adapter.getModel()).isEqualTo(TestElementAdapter.DEFAULT_MODEL);
    }

    @Test
    public void testCreateAdapter_addsBorders() {
        when(styleProvider.hasBorders()).thenReturn(true);

        adapter.createAdapter(
                Element.getDefaultInstance(), Element.getDefaultInstance(), frameContext);

        verify(styleProvider)
                .addBordersWithoutRoundedCorners((FrameLayout) adapter.getView(), context);
    }

    @Test
    public void createAdapter_appliesVisibility() {
        Element defaultElement =
                Element.newBuilder()
                        .setVisibilityState(VisibilityState.newBuilder().setDefaultVisibility(
                                Visibility.INVISIBLE))
                        .build();

        adapter.createAdapter(defaultElement, frameContext);

        assertThat(adapter.getView().getVisibility()).isEqualTo(View.INVISIBLE);
    }

    @Test
    public void createAdapter_doesNothingWithVisibilityGone() {
        Element defaultElement =
                Element.newBuilder()
                        .setVisibilityState(
                                VisibilityState.newBuilder().setDefaultVisibility(Visibility.GONE))
                        .build();

        adapter.createAdapter(defaultElement, frameContext);

        assertThat(adapter.testAdapterCreated).isFalse();
        assertThat(adapter.getView().getVisibility()).isEqualTo(View.GONE);
    }

    @Test
    public void createAdapter_ignoresBoundVisibility() {
        String visibilityBindingId = "invisible";
        VisibilityBindingRef visibilityBinding =
                VisibilityBindingRef.newBuilder().setBindingId(visibilityBindingId).build();
        Element defaultElement =
                Element.newBuilder()
                        .setVisibilityState(
                                VisibilityState.newBuilder()
                                        .setDefaultVisibility(Visibility.VISIBLE)
                                        .setOverridingBoundVisibility(visibilityBinding))
                        .build();
        when(frameContext.getVisibilityFromBinding(visibilityBinding)).thenReturn(Visibility.GONE);

        adapter.createAdapter(defaultElement, frameContext);

        assertThat(adapter.testAdapterCreated).isTrue();
        assertThat(adapter.getView().getVisibility()).isEqualTo(View.VISIBLE);
    }

    @Test
    public void createAdapter_setsDimensions() {
        int width = 10;
        int height = 20;

        when(styleProvider.hasWidth()).thenReturn(true);
        when(styleProvider.hasHeight()).thenReturn(true);
        when(styleProvider.getWidthSpecPx(context)).thenReturn(width);
        when(styleProvider.getHeightSpecPx(context)).thenReturn(height);

        adapter.createAdapter(Element.getDefaultInstance(), frameContext);

        assertThat(adapter.getComputedWidthPx()).isEqualTo(width);
        assertThat(adapter.getComputedHeightPx()).isEqualTo(height);
    }

    @Test
    public void createAdapter_resetsUnsetDimensions() {
        // Set some dimensions first
        int width = 10;
        int height = 20;

        when(styleProvider.hasWidth()).thenReturn(true);
        when(styleProvider.hasHeight()).thenReturn(true);
        when(styleProvider.getWidthSpecPx(context)).thenReturn(width);
        when(styleProvider.getHeightSpecPx(context)).thenReturn(height);

        adapter.createAdapter(Element.getDefaultInstance(), frameContext);
        adapter.releaseAdapter();

        // Recreate with new style that unsets the dimensions
        when(styleProvider.hasWidth()).thenReturn(false);
        when(styleProvider.hasHeight()).thenReturn(false);
        when(styleProvider.getWidthSpecPx(context)).thenReturn(DIMENSION_NOT_SET);
        when(styleProvider.getHeightSpecPx(context)).thenReturn(DIMENSION_NOT_SET);

        adapter.createAdapter(Element.getDefaultInstance(), frameContext);

        assertThat(adapter.getComputedWidthPx()).isEqualTo(DIMENSION_NOT_SET);
        assertThat(adapter.getComputedHeightPx()).isEqualTo(DIMENSION_NOT_SET);
    }

    @Test
    public void createAdapter_dimensionsDontOverrideSubclass() {
        int width = 10;
        int height = 20;

        final int subclassWidth = 30;
        final int subclassHeight = 40;

        adapter = new TestElementAdapter(context, parameters, view) {
            @Override
            protected void onCreateAdapter(
                    Object model, Element baseElement, FrameContext frameContext) {
                widthPx = subclassWidth;
                heightPx = subclassHeight;
            }
        };

        when(styleProvider.hasWidth()).thenReturn(true);
        when(styleProvider.hasHeight()).thenReturn(true);
        when(styleProvider.getWidthSpecPx(context)).thenReturn(width);
        when(styleProvider.getHeightSpecPx(context)).thenReturn(height);

        adapter.createAdapter(Element.getDefaultInstance(), frameContext);

        assertThat(adapter.getComputedWidthPx()).isEqualTo(subclassWidth);
        assertThat(adapter.getComputedHeightPx()).isEqualTo(subclassHeight);
    }

    @Test
    public void bindModel_callsOnBindModel() {
        Element defaultElement = Element.getDefaultInstance();

        adapter.bindModel(defaultElement, defaultElement, frameContext);

        assertThat(adapter.testAdapterBound).isTrue();
    }

    @Test
    public void testBindModel_callsOnBindLogDataCallback() {
        Element defaultElement = Element.newBuilder().setLogData(logDataTest).build();

        adapter.bindModel(defaultElement, defaultElement, frameContext);

        assertThat(callbackBound).isTrue();
    }

    @Test
    public void testBindModel_callsOnBindLogDataCallback_bindingRef() {
        LogDataBindingRef logDataBinding =
                LogDataBindingRef.newBuilder().setBindingId("LogData!").build();
        Element defaultElement = Element.newBuilder().setLogDataRef(logDataBinding).build();
        when(frameContext.getLogDataFromBinding(logDataBinding)).thenReturn(logDataTest);
        adapter.bindModel(defaultElement, defaultElement, frameContext);

        assertThat(callbackBound).isTrue();
    }

    @Test
    public void bindModel_extractsModelFromElement() {
        Element element = Element.getDefaultInstance();

        adapter.bindModel(element, frameContext);

        assertThat(adapter.getModel()).isEqualTo(TestElementAdapter.DEFAULT_MODEL);
    }

    @Test
    public void testBindModel_setsActions() {
        Actions actions =
                Actions.newBuilder().setOnClickAction(Action.getDefaultInstance()).build();
        Element elementWithActions = Element.newBuilder().setActions(actions).build();

        adapter.bindModel(elementWithActions, elementWithActions, frameContext);
        assertThat(adapter.getView().hasOnClickListeners()).isTrue();
        assertThat(adapter.actions).isSameInstanceAs(actions);
    }

    @Test
    public void bindModel_setsActionsOnBaseView() {
        Actions actions =
                Actions.newBuilder().setOnClickAction(Action.getDefaultInstance()).build();
        Element elementWithActions = Element.newBuilder().setActions(actions).build();
        when(styleProvider.hasBorders()).thenReturn(true);

        adapter.createAdapter(elementWithActions, elementWithActions, frameContext);
        adapter.bindModel(elementWithActions, elementWithActions, frameContext);
        assertThat(adapter.getView().hasOnClickListeners()).isFalse();
        assertThat(adapter.getBaseView().hasOnClickListeners()).isTrue();
        assertThat(adapter.actions).isSameInstanceAs(actions);
    }

    @Test
    public void bindModel_setsActionsWithBinding() {
        Actions actions =
                Actions.newBuilder().setOnClickAction(Action.getDefaultInstance()).build();
        ActionsBindingRef actionsBinding =
                ActionsBindingRef.newBuilder().setBindingId("ACTION!").build();
        Element elementWithActions = Element.newBuilder().setActionsBinding(actionsBinding).build();
        when(frameContext.getActionsFromBinding(actionsBinding)).thenReturn(actions);

        adapter.bindModel(elementWithActions, elementWithActions, frameContext);
        assertThat(adapter.getView().hasOnClickListeners()).isTrue();
        assertThat(adapter.actions).isSameInstanceAs(actions);
    }

    @Test
    public void bindModel_unsetsActions() {
        Actions actions =
                Actions.newBuilder().setOnClickAction(Action.getDefaultInstance()).build();
        Element elementWithActions = Element.newBuilder().setActions(actions).build();
        Element elementWithoutActions = Element.getDefaultInstance();

        adapter.bindModel(elementWithActions, elementWithActions, frameContext);
        assertThat(adapter.getView().hasOnClickListeners()).isTrue();
        assertThat(adapter.actions).isSameInstanceAs(actions);

        adapter.bindModel(elementWithoutActions, elementWithoutActions, frameContext);
        assertThat(adapter.getView().hasOnClickListeners()).isFalse();
        assertThat(adapter.actions).isSameInstanceAs(Actions.getDefaultInstance());
    }

    @Test
    public void bindModel_unsetsActionsOnBaseView() {
        Actions actions =
                Actions.newBuilder().setOnClickAction(Action.getDefaultInstance()).build();
        Element elementWithActions = Element.newBuilder().setActions(actions).build();
        Element elementWithoutActions = elementWithActions.toBuilder().clearActions().build();

        when(styleProvider.hasBorders()).thenReturn(true);
        adapter.createAdapter(elementWithActions, elementWithActions, frameContext);
        adapter.bindModel(elementWithActions, elementWithActions, frameContext);
        assertThat(adapter.getView().hasOnClickListeners()).isFalse();
        assertThat(adapter.getBaseView().hasOnClickListeners()).isTrue();
        assertThat(adapter.actions).isSameInstanceAs(actions);

        adapter.unbindModel();

        when(styleProvider.hasBorders()).thenReturn(false);
        adapter.bindModel(elementWithoutActions, elementWithoutActions, frameContext);
        assertThat(adapter.getView().hasOnClickListeners()).isFalse();
        assertThat(adapter.getBaseView().hasOnClickListeners()).isFalse();
        assertThat(adapter.actions).isSameInstanceAs(Actions.getDefaultInstance());
    }

    @Test
    public void testBindModel_setsVisibility() {
        Element visibleElement =
                Element.newBuilder()
                        .setVisibilityState(VisibilityState.newBuilder().setDefaultVisibility(
                                Visibility.VISIBLE))
                        .build();
        adapter.createAdapter(visibleElement, frameContext);
        Element invisibleElement =
                Element.newBuilder()
                        .setVisibilityState(VisibilityState.newBuilder().setDefaultVisibility(
                                Visibility.INVISIBLE))
                        .build();

        adapter.bindModel(invisibleElement, frameContext);

        assertThat(adapter.testAdapterBound).isTrue();
        assertThat(adapter.getView().getVisibility()).isEqualTo(View.INVISIBLE);
    }

    @Test
    public void bindModel_doesNothingWhenVisibilityIsGone() {
        Element visibleElement =
                Element.newBuilder()
                        .setVisibilityState(VisibilityState.newBuilder().setDefaultVisibility(
                                Visibility.VISIBLE))
                        .build();
        adapter.createAdapter(visibleElement, frameContext);
        Element goneElement =
                Element.newBuilder()
                        .setVisibilityState(
                                VisibilityState.newBuilder().setDefaultVisibility(Visibility.GONE))
                        .build();

        adapter.bindModel(goneElement, frameContext);

        assertThat(adapter.testAdapterBound).isFalse();
        assertThat(adapter.getView().getVisibility()).isEqualTo(View.GONE);
    }

    @Test
    public void bindModel_recreatesWhenVisibilityWasGone() {
        Element goneElement =
                Element.newBuilder()
                        .setVisibilityState(
                                VisibilityState.newBuilder().setDefaultVisibility(Visibility.GONE))
                        .build();
        adapter.createAdapter(goneElement, frameContext);
        Element invisibleElement =
                Element.newBuilder()
                        .setVisibilityState(VisibilityState.newBuilder().setDefaultVisibility(
                                Visibility.INVISIBLE))
                        .build();

        adapter.bindModel(invisibleElement, frameContext);

        assertThat(adapter.testAdapterCreated).isTrue();
        assertThat(adapter.testAdapterBound).isTrue();
        assertThat(adapter.getView().getVisibility()).isEqualTo(View.INVISIBLE);
    }

    @Test
    public void bindModel_recreatesWhenVisibilityWasGone_bound() {
        VisibilityBindingRef visibilityBinding =
                VisibilityBindingRef.newBuilder().setBindingId("camo").build();
        Element elementWithVisibilityBinding =
                Element.newBuilder()
                        .setVisibilityState(
                                VisibilityState.newBuilder()
                                        .setDefaultVisibility(Visibility.GONE)
                                        .setOverridingBoundVisibility(visibilityBinding))
                        .build();

        // When adapter is created, the Element is GONE
        when(frameContext.getVisibilityFromBinding(visibilityBinding)).thenReturn(Visibility.GONE);

        adapter.createAdapter(elementWithVisibilityBinding, frameContext);

        assertThat(adapter.testAdapterCreated).isFalse();
        assertThat(adapter.getView().getVisibility()).isEqualTo(View.GONE);

        // Now on binding, the Element is VISIBLE
        when(frameContext.getVisibilityFromBinding(visibilityBinding))
                .thenReturn(Visibility.VISIBLE);

        adapter.bindModel(elementWithVisibilityBinding, frameContext);

        assertThat(adapter.testAdapterCreated).isTrue();
        assertThat(adapter.testAdapterBound).isTrue();
        assertThat(adapter.getView().getVisibility()).isEqualTo(View.VISIBLE);
    }

    @Test
    public void bindModel_doesntRecreateWhenWasCreatedButVisibilityGone() {
        // Create a real adapter for a visible element.
        Element visibileElement =
                Element.newBuilder()
                        .setVisibilityState(VisibilityState.newBuilder().setDefaultVisibility(
                                Visibility.VISIBLE))
                        .build();
        adapter.createAdapter(visibileElement, frameContext);

        // Bind a gone element, and unbind it so the visibility is GONE.
        Element goneElement =
                Element.newBuilder()
                        .setVisibilityState(
                                VisibilityState.newBuilder().setDefaultVisibility(Visibility.GONE))
                        .build();
        adapter.bindModel(goneElement, frameContext);
        adapter.unbindModel();

        assertThat(adapter.testAdapterCreated).isTrue();
        adapter.testAdapterCreated = false; // set this so we can test that it doesn't get recreated

        // Bind a real element, and ensure that the adapter is not recreated.
        Element invisibleElement =
                Element.newBuilder()
                        .setVisibilityState(VisibilityState.newBuilder().setDefaultVisibility(
                                Visibility.INVISIBLE))
                        .build();

        adapter.bindModel(invisibleElement, frameContext);

        assertThat(adapter.testAdapterCreated).isFalse();
        assertThat(adapter.testAdapterBound).isTrue();
        assertThat(adapter.getView().getVisibility()).isEqualTo(View.INVISIBLE);
    }

    @Test
    public void bindModel_accessibility() {
        Element accessibilityElement =
                Element.newBuilder()
                        .setAccessibility(
                                Accessibility.newBuilder()
                                        .addRoles(AccessibilityRole.HEADER)
                                        .setDescription(ParameterizedText.newBuilder().setText(
                                                "Accessible!")))
                        .build();

        adapter.bindModel(accessibilityElement, frameContext);
        assertThat(adapter.getView().getContentDescription().toString()).isEqualTo("Accessible!");
        final AccessibilityNodeInfo nodeInfo = AccessibilityNodeInfo.obtain();
        adapter.getView().onInitializeAccessibilityNodeInfo(nodeInfo);
        assertThat(nodeInfo.getCollectionItemInfo().isHeading()).isTrue();
    }

    @Test
    public void bindModel_accessibilityBinding() {
        ParameterizedTextBindingRef textBinding =
                ParameterizedTextBindingRef.newBuilder().setBindingId("binding").build();
        Element accessibilityElement =
                Element.newBuilder()
                        .setAccessibility(
                                Accessibility.newBuilder().setDescriptionBinding(textBinding))
                        .build();
        when(frameContext.getParameterizedTextBindingValue(textBinding))
                .thenReturn(BindingValue.newBuilder()
                                    .setParameterizedText(
                                            ParameterizedText.newBuilder().setText("Accessible!"))
                                    .build());

        adapter.bindModel(accessibilityElement, frameContext);

        assertThat(adapter.getView().getContentDescription().toString()).isEqualTo("Accessible!");
    }

    @Test
    public void bindModel_accessibilityReset() {
        adapter.getView().setContentDescription("OLD CONTENT DESCRIPTION");

        adapter.bindModel(Element.getDefaultInstance(), frameContext);

        assertThat(adapter.getView().getContentDescription()).isNull();
    }

    @Test
    public void unbindModel_callsOnUnbindModel() {
        Element defaultElement = Element.getDefaultInstance();

        adapter.createAdapter(defaultElement, defaultElement, frameContext);
        adapter.bindModel(defaultElement, defaultElement, frameContext);
        assertThat(adapter.testAdapterBound).isTrue();

        adapter.unbindModel();
        assertThat(adapter.testAdapterBound).isFalse();
    }

    @Test
    public void testUnbindModel_callsOnBindLogDataCallback() {
        Element defaultElement = Element.newBuilder().setLogData(logDataTest).build();

        adapter.bindModel(defaultElement, defaultElement, frameContext);
        adapter.unbindModel();
        assertThat(callbackUnbound).isTrue();
    }

    @Test
    public void testUnbindModel_callsOnBindLogDataCallback_bindingRef() {
        LogDataBindingRef logDataBinding =
                LogDataBindingRef.newBuilder().setBindingId("LogData!").build();
        Element defaultElement = Element.newBuilder().setLogDataRef(logDataBinding).build();
        when(frameContext.getLogDataFromBinding(logDataBinding)).thenReturn(logDataTest);

        adapter.bindModel(defaultElement, defaultElement, frameContext);
        adapter.unbindModel();
        assertThat(callbackUnbound).isTrue();
    }

    @Test
    public void unbindModel_unsetsModel() {
        Element element = Element.getDefaultInstance();

        adapter.createAdapter(element, element, frameContext);
        adapter.bindModel(element, element, frameContext);
        assertThat(adapter.getModel()).isEqualTo(element);

        adapter.unbindModel();
        assertThat(adapter.getRawModel()).isNull();
    }

    // If onBindModel was never called, onUnbindModel should not be called.
    @Test
    public void unbindModel_visibilityWasGone() {
        Element goneElement =
                Element.newBuilder()
                        .setVisibilityState(
                                VisibilityState.newBuilder().setDefaultVisibility(Visibility.GONE))
                        .build();
        adapter.createAdapter(goneElement, frameContext);
        adapter.bindModel(goneElement, frameContext);

        assertThat(adapter.testAdapterCreated).isFalse();
        assertThat(adapter.testAdapterBound).isFalse();

        // Set this to something else so we can check if onUnbindModel gets called - it should not.
        adapter.testAdapterBound = true;

        adapter.unbindModel();

        // onUnbindModel was not called so this is still true.
        assertThat(adapter.testAdapterBound).isTrue();
        assertThat(adapter.testAdapterCreated).isFalse();
    }

    @Test
    public void unbindModel_unsetsActions() {
        Element elementWithWrapperAndActions =
                Element.newBuilder()
                        .setActions(
                                Actions.newBuilder().setOnClickAction(Action.getDefaultInstance()))
                        .build();

        when(styleProvider.hasBorders()).thenReturn(true);

        adapter.createAdapter(
                elementWithWrapperAndActions, elementWithWrapperAndActions, frameContext);
        adapter.bindModel(elementWithWrapperAndActions, elementWithWrapperAndActions, frameContext);
        View adapterView = adapter.getBaseView();
        View wrapperView = adapter.getView();

        adapter.unbindModel();

        assertThat(adapterView.hasOnClickListeners()).isFalse();
        assertThat(wrapperView.hasOnClickListeners()).isFalse();
    }

    @Test
    public void testUnbindModel_affectsVisibilityCalculations() {
        VisibilityBindingRef visibilityBinding =
                VisibilityBindingRef.newBuilder().setBindingId("vis").build();
        Element element = Element.newBuilder()
                                  .setVisibilityState(
                                          VisibilityState.newBuilder()
                                                  .setDefaultVisibility(Visibility.INVISIBLE)
                                                  .setOverridingBoundVisibility(visibilityBinding))
                                  .build();
        when(frameContext.getVisibilityFromBinding(visibilityBinding))
                .thenReturn(Visibility.VISIBLE);

        adapter.createAdapter(element, element, frameContext);
        assertThat(adapter.getVisibilityForElement(element, frameContext))
                .isEqualTo(Visibility.INVISIBLE);

        adapter.bindModel(element, element, frameContext);
        assertThat(adapter.getVisibilityForElement(element, frameContext))
                .isEqualTo(Visibility.VISIBLE);

        adapter.unbindModel();
        assertThat(adapter.getVisibilityForElement(element, frameContext))
                .isEqualTo(Visibility.INVISIBLE);
    }

    @Test
    public void releaseAdapter_callsOnReleaseAdapter() {
        Element defaultElement = Element.getDefaultInstance();

        adapter.createAdapter(defaultElement, defaultElement, frameContext);
        assertThat(adapter.testAdapterCreated).isTrue();

        adapter.releaseAdapter();
        assertThat(adapter.testAdapterCreated).isFalse();
    }

    @Test
    public void testReleaseAdapter_resetsVisibility() {
        Element defaultElement = Element.getDefaultInstance();

        adapter.createAdapter(defaultElement, defaultElement, frameContext);
        adapter.setVisibilityOnView(Visibility.INVISIBLE);
        assertThat(adapter.getBaseView().getVisibility()).isEqualTo(View.INVISIBLE);

        adapter.releaseAdapter();
        assertThat(adapter.getBaseView().getVisibility()).isEqualTo(View.VISIBLE);
    }

    @Test
    public void releaseAdapter_resetsDimensions() {
        Element defaultElement = Element.getDefaultInstance();

        adapter.createAdapter(defaultElement, defaultElement, frameContext);
        adapter.setDims(123, 456);

        adapter.releaseAdapter();

        assertThat(adapter.getComputedWidthPx()).isEqualTo(DIMENSION_NOT_SET);
        assertThat(adapter.getComputedHeightPx()).isEqualTo(DIMENSION_NOT_SET);
    }

    @Test
    public void setVisibility() {
        adapter.createAdapter(
                Element.getDefaultInstance(), Element.getDefaultInstance(), frameContext);
        adapter.getBaseView().setVisibility(View.GONE);
        adapter.setVisibilityOnView(Visibility.VISIBLE);
        assertThat(adapter.getBaseView().getVisibility()).isEqualTo(View.VISIBLE);
        adapter.setVisibilityOnView(Visibility.INVISIBLE);
        assertThat(adapter.getBaseView().getVisibility()).isEqualTo(View.INVISIBLE);
        adapter.setVisibilityOnView(Visibility.GONE);
        assertThat(adapter.getBaseView().getVisibility()).isEqualTo(View.GONE);
        adapter.setVisibilityOnView(Visibility.VISIBILITY_UNSPECIFIED);
        assertThat(adapter.getBaseView().getVisibility()).isEqualTo(View.VISIBLE);
    }

    @Test
    public void getVisibilityForElement() {
        VisibilityBindingRef bindingRef =
                VisibilityBindingRef.newBuilder().setBindingId("binding").build();
        Element elementWithDefaultVisibility =
                Element.newBuilder()
                        .setVisibilityState(VisibilityState.newBuilder().setDefaultVisibility(
                                Visibility.INVISIBLE))
                        .build();
        Element elementWithVisibilityOverride =
                Element.newBuilder()
                        .setVisibilityState(VisibilityState.newBuilder()
                                                    .setDefaultVisibility(Visibility.INVISIBLE)
                                                    .setOverridingBoundVisibility(bindingRef))
                        .build();

        // Default visibility; no override
        assertThat(adapter.getVisibilityForElement(elementWithDefaultVisibility, frameContext))
                .isEqualTo(Visibility.INVISIBLE);

        // Don't look up override when not binding
        when(frameContext.getVisibilityFromBinding(bindingRef)).thenReturn(Visibility.GONE);
        assertThat(adapter.getVisibilityForElement(elementWithVisibilityOverride, frameContext))
                .isEqualTo(Visibility.INVISIBLE);

        adapter.bindModel(Element.getDefaultInstance(), frameContext);

        // Look up override successfully
        when(frameContext.getVisibilityFromBinding(bindingRef)).thenReturn(Visibility.GONE);
        assertThat(adapter.getVisibilityForElement(elementWithVisibilityOverride, frameContext))
                .isEqualTo(Visibility.GONE);

        // Override not found; use default
        when(frameContext.getVisibilityFromBinding(bindingRef)).thenReturn(null);
        assertThat(adapter.getVisibilityForElement(elementWithVisibilityOverride, frameContext))
                .isEqualTo(Visibility.INVISIBLE);
    }

    @Config(shadows = {ExtendedShadowView.class})
    @Test
    public void getOnFullViewActions() {
        Frame frame = Frame.newBuilder().setTag("FRAME").build();
        when(frameContext.getFrame()).thenReturn(frame);
        View viewport = new View(context);

        // No actions defined
        adapter.createAdapter(Element.getDefaultInstance(), frameContext);
        adapter.bindModel(Element.getDefaultInstance(), frameContext);
        adapter.triggerViewActions(viewport, frameContext);
        verifyZeroInteractions(actionHandler);
        adapter.releaseAdapter();

        // Actions defined, but not fullview actions
        Element elementWithNoViewActions =
                Element.newBuilder()
                        .setActions(
                                Actions.newBuilder().setOnClickAction(Action.newBuilder().build()))
                        .build();
        adapter.createAdapter(elementWithNoViewActions, frameContext);
        adapter.bindModel(elementWithNoViewActions, frameContext);
        adapter.triggerViewActions(viewport, frameContext);
        verifyZeroInteractions(actionHandler);
        adapter.unbindModel();
        adapter.releaseAdapter();

        // Actions defined, but not fully visible
        ExtendedShadowView viewShadow = Shadow.extract(adapter.getView());
        viewShadow.setAttachedToWindow(false);
        Actions fullViewActions = Actions.newBuilder()
                                          .addOnViewActions(VisibilityAction.newBuilder().setAction(
                                                  Action.newBuilder().build()))
                                          .build();
        Element elementWithActions = Element.newBuilder().setActions(fullViewActions).build();
        adapter.createAdapter(elementWithActions, frameContext);
        adapter.bindModel(elementWithActions, frameContext);
        adapter.triggerViewActions(viewport, frameContext);
        verifyZeroInteractions(actionHandler);
        adapter.unbindModel();
        adapter.releaseAdapter();

        // Actions defined, and fully visible
        ExtendedShadowView viewportShadow = Shadow.extract(viewport);
        viewportShadow.setLocationInWindow(0, 0);
        viewportShadow.setHeight(100);
        viewportShadow.setWidth(100);
        viewShadow.setLocationInWindow(10, 10);
        viewShadow.setHeight(50);
        viewShadow.setWidth(50);
        viewShadow.setAttachedToWindow(true);

        adapter.createAdapter(elementWithActions, frameContext);
        adapter.bindModel(elementWithActions, frameContext);
        adapter.triggerViewActions(viewport, frameContext);
        verify(actionHandler)
                .handleAction(same(fullViewActions.getOnViewActions(0).getAction()),
                        eq(ActionType.VIEW), eq(frame), eq(view), eq(LogData.getDefaultInstance()));
    }

    @Config(shadows = {ExtendedShadowView.class})
    @Test
    public void getOnPartialViewActions() {
        Frame frame = Frame.newBuilder().setTag("FRAME").build();
        when(frameContext.getFrame()).thenReturn(frame);
        View viewport = new View(context);

        // No actions defined
        adapter.createAdapter(Element.getDefaultInstance(), frameContext);
        adapter.bindModel(Element.getDefaultInstance(), frameContext);
        adapter.triggerViewActions(viewport, frameContext);
        verifyZeroInteractions(actionHandler);
        adapter.releaseAdapter();

        // Actions defined, but not partial view actions
        Element elementWithNoViewActions =
                Element.newBuilder()
                        .setActions(
                                Actions.newBuilder().setOnClickAction(Action.newBuilder().build()))
                        .build();
        adapter.createAdapter(elementWithNoViewActions, frameContext);
        adapter.bindModel(elementWithNoViewActions, frameContext);
        adapter.triggerViewActions(viewport, frameContext);
        verifyZeroInteractions(actionHandler);
        adapter.unbindModel();
        adapter.releaseAdapter();

        // Actions defined, but not attached to window
        ExtendedShadowView viewShadow = Shadow.extract(adapter.getView());
        viewShadow.setAttachedToWindow(false);
        Actions partialViewActions =
                Actions.newBuilder()
                        .addOnViewActions(VisibilityAction.newBuilder()
                                                  .setAction(Action.newBuilder().build())
                                                  .setProportionVisible(0.01f))
                        .build();
        Element elementWithActions = Element.newBuilder().setActions(partialViewActions).build();
        adapter.createAdapter(elementWithActions, frameContext);
        adapter.bindModel(elementWithActions, frameContext);
        adapter.triggerViewActions(viewport, frameContext);
        verifyZeroInteractions(actionHandler);
        adapter.unbindModel();
        adapter.releaseAdapter();

        // Actions defined, and partially visible
        ExtendedShadowView viewportShadow = Shadow.extract(viewport);
        viewportShadow.setLocationInWindow(0, 0);
        viewportShadow.setHeight(100);
        viewportShadow.setWidth(100);
        viewShadow.setLocationInWindow(10, 10);
        viewShadow.setHeight(200);
        viewShadow.setWidth(200);
        viewShadow.setAttachedToWindow(true);

        adapter.createAdapter(elementWithActions, frameContext);
        adapter.bindModel(elementWithActions, frameContext);
        adapter.triggerViewActions(viewport, frameContext);
        verify(actionHandler)
                .handleAction(same(partialViewActions.getOnViewActions(0).getAction()),
                        eq(ActionType.VIEW), eq(frame), eq(view), eq(LogData.getDefaultInstance()));

        // Actions defined, and fully visible
        viewShadow.setHeight(50);
        viewShadow.setWidth(50);

        adapter.createAdapter(elementWithActions, frameContext);
        adapter.bindModel(elementWithActions, frameContext);
        adapter.triggerViewActions(viewport, frameContext);
        verify(actionHandler, times(2))
                .handleAction(same(partialViewActions.getOnViewActions(0).getAction()),
                        eq(ActionType.VIEW), eq(frame), eq(view), eq(LogData.getDefaultInstance()));
    }

    @Config(shadows = {ExtendedShadowView.class})
    @Test
    public void triggerHideActions() {
        Frame frame = Frame.newBuilder().setTag("FRAME").build();
        when(frameContext.getFrame()).thenReturn(frame);
        View viewport = new View(context);

        // No actions defined
        adapter.createAdapter(Element.getDefaultInstance(), frameContext);
        adapter.bindModel(Element.getDefaultInstance(), frameContext);
        adapter.triggerHideActions(frameContext);
        verifyZeroInteractions(actionHandler);
        adapter.releaseAdapter();

        // Actions defined, but not hide actions
        Element elementWithNoViewActions =
                Element.newBuilder()
                        .setActions(
                                Actions.newBuilder().setOnClickAction(Action.getDefaultInstance()))
                        .build();
        adapter.createAdapter(elementWithNoViewActions, frameContext);
        adapter.bindModel(elementWithNoViewActions, frameContext);
        adapter.triggerHideActions(frameContext);
        verifyZeroInteractions(actionHandler);
        adapter.unbindModel();
        adapter.releaseAdapter();

        // Trigger hide actions on the fully-visible view to make hide actions not active.
        Action hideAction = Action.newBuilder().build();
        Actions hideActions =
                Actions.newBuilder()
                        .addOnHideActions(VisibilityAction.newBuilder().setAction(hideAction))
                        .build();
        Element elementWithActions = Element.newBuilder().setActions(hideActions).build();
        ExtendedShadowView viewShadow = Shadow.extract(adapter.getView());
        ExtendedShadowView viewportShadow = Shadow.extract(viewport);
        viewportShadow.setLocationInWindow(0, 0);
        viewportShadow.setHeight(100);
        viewportShadow.setWidth(100);
        viewShadow.setLocationInWindow(10, 10);
        viewShadow.setHeight(50);
        viewShadow.setWidth(50);
        viewShadow.setAttachedToWindow(true);

        adapter.createAdapter(elementWithActions, frameContext);
        adapter.bindModel(elementWithActions, frameContext);
        adapter.triggerViewActions(viewport, frameContext);
        verifyZeroInteractions(actionHandler);

        // Trigger hide actions
        adapter.triggerHideActions(frameContext);

        verify(actionHandler)
                .handleAction(same(hideAction), eq(ActionType.VIEW), eq(frame), eq(view),
                        eq(LogData.getDefaultInstance()));

        // Ensure actions do not trigger again
        adapter.triggerHideActions(frameContext);

        verify(actionHandler) // Only once, not twice
                .handleAction(same(hideAction), eq(ActionType.VIEW), eq(frame), eq(view),
                        eq(LogData.getDefaultInstance()));
    }

    @Test
    public void createRoundedCorners() {
        RoundedCorners roundedCorners = RoundedCorners.newBuilder().setRadiusDp(10).build();
        RoundedCornerWrapperView roundedCornerWrapperView = new RoundedCornerWrapperView(context,
                roundedCorners, maskCache, Suppliers.of(false),
                /*radiusOverride= */ 0, Borders.getDefaultInstance(), LEGACY_CORNERS_FLAG,
                OUTLINE_CORNERS_FLAG);
        when(styleProvider.hasRoundedCorners()).thenReturn(true);
        when(styleProvider.createWrapperView(
                     context, maskCache, LEGACY_CORNERS_FLAG, OUTLINE_CORNERS_FLAG))
                .thenReturn(roundedCornerWrapperView);

        adapter.createAdapter(Element.getDefaultInstance(), frameContext);

        assertThat(adapter.getView()).isSameInstanceAs(roundedCornerWrapperView);
    }

    @Test
    public void setVisibilityWithRoundedCorners() {
        RoundedCorners roundedCorners = RoundedCorners.newBuilder().setRadiusDp(10).build();
        RoundedCornerWrapperView roundedCornerWrapperView = new RoundedCornerWrapperView(context,
                roundedCorners, maskCache, Suppliers.of(false),
                /*radiusOverride= */ 0, Borders.getDefaultInstance(), LEGACY_CORNERS_FLAG,
                OUTLINE_CORNERS_FLAG);
        when(styleProvider.hasRoundedCorners()).thenReturn(true);
        when(styleProvider.createWrapperView(
                     context, maskCache, LEGACY_CORNERS_FLAG, OUTLINE_CORNERS_FLAG))
                .thenReturn(roundedCornerWrapperView);

        VisibilityBindingRef visibilityBinding =
                VisibilityBindingRef.newBuilder().setBindingId("visibility").build();
        when(frameContext.getVisibilityFromBinding(visibilityBinding))
                .thenReturn(Visibility.VISIBLE);

        Element element = Element.newBuilder()
                                  .setVisibilityState(
                                          VisibilityState.newBuilder()
                                                  .setDefaultVisibility(Visibility.GONE)
                                                  .setOverridingBoundVisibility(visibilityBinding))
                                  .build();

        adapter.createAdapter(element, frameContext);

        FrameLayout parentView = new FrameLayout(context);
        parentView.addView(adapter.getView());

        adapter.bindModel(element, frameContext);

        assertThat(adapter.getView()).isSameInstanceAs(roundedCornerWrapperView);
        assertThat(adapter.getView().getVisibility()).isEqualTo(View.VISIBLE);

        // Try setting visibility back to GONE and ensure that RCWV is GONE
        when(frameContext.getVisibilityFromBinding(visibilityBinding)).thenReturn(Visibility.GONE);
        adapter.unbindModel();
        adapter.bindModel(element, frameContext);
        assertThat(adapter.getView()).isSameInstanceAs(roundedCornerWrapperView);
        assertThat(adapter.getView().getVisibility()).isEqualTo(View.GONE);
    }

    @Test
    public void getStyleIdsStack() {
        StyleIdsStack style = StyleIdsStack.newBuilder().addStyleIds("style").build();
        Element elementWithStyle = Element.newBuilder().setStyleReferences(style).build();

        adapter.createAdapter(elementWithStyle, frameContext);

        assertThat(adapter.getElementStyleIdsStack()).isSameInstanceAs(style);

        Element elementWithNoStyle = Element.getDefaultInstance();

        adapter.createAdapter(elementWithNoStyle, frameContext);

        assertThat(adapter.getElementStyleIdsStack()).isEqualTo(StyleIdsStack.getDefaultInstance());
    }

    // Dummy implementation
    static class TestElementAdapter extends ElementAdapter<View, Object> {
        static final String DEFAULT_MODEL = "MODEL";

        boolean testAdapterCreated = false;
        boolean testAdapterBound = false;

        View adapterView;

        TestElementAdapter(Context context, AdapterParameters parameters, View adapterView) {
            super(context, parameters, adapterView);
            this.adapterView = adapterView;
        }

        @Override
        protected Object getModelFromElement(Element baseElement) {
            return DEFAULT_MODEL;
        }

        @Override
        protected void onCreateAdapter(
                Object model, Element baseElement, FrameContext frameContext) {
            testAdapterCreated = true;
        }

        @Override
        protected void onBindModel(Object model, Element baseElement, FrameContext frameContext) {
            testAdapterBound = true;
        }

        @Override
        protected void onUnbindModel() {
            testAdapterBound = false;
        }

        @Override
        protected void onReleaseAdapter() {
            testAdapterCreated = false;
        }

        public void setDims(int width, int height) {
            widthPx = width;
            heightPx = height;
        }
    }
}
