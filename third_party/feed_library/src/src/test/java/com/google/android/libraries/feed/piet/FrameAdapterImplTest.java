// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.ColorDrawable;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;

import com.google.android.libraries.feed.api.host.config.DebugBehavior;
import com.google.android.libraries.feed.common.functional.Suppliers;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.piet.DebugLogger.MessageType;
import com.google.android.libraries.feed.piet.PietStylesHelper.PietStylesHelperFactory;
import com.google.android.libraries.feed.piet.TemplateBinder.TemplateAdapterModel;
import com.google.android.libraries.feed.piet.host.ActionHandler;
import com.google.android.libraries.feed.piet.host.ActionHandler.ActionType;
import com.google.android.libraries.feed.piet.host.AssetProvider;
import com.google.android.libraries.feed.piet.host.CustomElementProvider;
import com.google.android.libraries.feed.piet.host.EventLogger;
import com.google.android.libraries.feed.piet.host.HostBindingProvider;
import com.google.android.libraries.feed.piet.host.LogDataCallback;
import com.google.android.libraries.feed.piet.ui.RoundedCornerMaskCache;
import com.google.search.now.ui.piet.ActionsProto.Action;
import com.google.search.now.ui.piet.ActionsProto.Actions;
import com.google.search.now.ui.piet.ActionsProto.VisibilityAction;
import com.google.search.now.ui.piet.BindingRefsProto.ActionsBindingRef;
import com.google.search.now.ui.piet.ElementsProto.BindingContext;
import com.google.search.now.ui.piet.ElementsProto.BindingValue;
import com.google.search.now.ui.piet.ElementsProto.Content;
import com.google.search.now.ui.piet.ElementsProto.Element;
import com.google.search.now.ui.piet.ElementsProto.ElementList;
import com.google.search.now.ui.piet.ElementsProto.ElementStack;
import com.google.search.now.ui.piet.ElementsProto.GridRow;
import com.google.search.now.ui.piet.ElementsProto.ImageElement;
import com.google.search.now.ui.piet.ElementsProto.TemplateInvocation;
import com.google.search.now.ui.piet.ErrorsProto.ErrorCode;
import com.google.search.now.ui.piet.GradientsProto.Fill;
import com.google.search.now.ui.piet.ImagesProto.Image;
import com.google.search.now.ui.piet.LogDataProto.LogData;
import com.google.search.now.ui.piet.PietAndroidSupport.ShardingControl;
import com.google.search.now.ui.piet.PietProto.Frame;
import com.google.search.now.ui.piet.PietProto.PietSharedState;
import com.google.search.now.ui.piet.PietProto.Stylesheet;
import com.google.search.now.ui.piet.PietProto.Stylesheets;
import com.google.search.now.ui.piet.PietProto.Template;
import com.google.search.now.ui.piet.RoundedCornersProto.RoundedCorners;
import com.google.search.now.ui.piet.ShadowsProto.ElevationShadow;
import com.google.search.now.ui.piet.ShadowsProto.Shadow;
import com.google.search.now.ui.piet.StylesProto.EdgeWidths;
import com.google.search.now.ui.piet.StylesProto.GravityHorizontal;
import com.google.search.now.ui.piet.StylesProto.GravityVertical;
import com.google.search.now.ui.piet.StylesProto.Style;
import com.google.search.now.ui.piet.StylesProto.StyleIdsStack;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Tests of the {@link FrameAdapterImpl}. */
@RunWith(RobolectricTestRunner.class)
public class FrameAdapterImplTest {
    private static final Content DEFAULT_CONTENT =
            Content.newBuilder()
                    .setElement(Element.newBuilder().setElementList(
                            ElementList.newBuilder().addContents(
                                    Content.newBuilder().setElement(Element.getDefaultInstance()))))
                    .build();
    private static final int FRAME_WIDTH = 321;
    private final LogData logDataTest = LogData.getDefaultInstance();

    @Mock
    private AssetProvider assetProvider;
    @Mock
    private ElementAdapterFactory adapterFactory;
    @Mock
    private TemplateBinder templateBinder;
    @Mock
    private FrameContext frameContext;
    @Mock
    private DebugBehavior debugBehavior;
    @Mock
    private DebugLogger debugLogger;
    @Mock
    private StyleProvider styleProvider;
    @Mock
    private ElementAdapter<? extends View, ?> elementAdapter;
    @Mock
    private ElementAdapter<? extends View, ?> templateAdapter;
    @Mock
    private CustomElementProvider customElementProvider;
    @Mock
    private ActionHandler actionHandler;
    @Mock
    private HostProviders hostProviders;
    @Mock
    private EventLogger eventLogger;
    @Mock
    private PietStylesHelperFactory stylesHelpers;
    @Mock
    private RoundedCornerMaskCache maskCache;

    @Captor
    private ArgumentCaptor<LayoutParams> layoutParamsCaptor;

    private Context context;
    private List<PietSharedState> pietSharedStates;
    private AdapterParameters adapterParameters;
    private boolean callbackBound;
    private boolean callbackUnbound;
    private FrameAdapterImpl frameAdapter;

    @Before
    public void setUp() throws Exception {
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
        adapterParameters = new AdapterParameters(null, Suppliers.of(new FrameLayout(context)),
                hostProviders, new ParameterizedTextEvaluator(new FakeClock()), adapterFactory,
                templateBinder, new FakeClock(), stylesHelpers, maskCache, false, false);
        when(elementAdapter.getView()).thenReturn(new LinearLayout(context));
        when(templateAdapter.getView()).thenReturn(new LinearLayout(context));
        doReturn(elementAdapter)
                .when(adapterFactory)
                .createAdapterForElement(any(Element.class), eq(frameContext));
        doReturn(templateAdapter)
                .when(templateBinder)
                .createAndBindTemplateAdapter(any(TemplateAdapterModel.class), eq(frameContext));
        when(elementAdapter.getHorizontalGravity(anyInt())).thenReturn(Gravity.CENTER_HORIZONTAL);
        when(elementAdapter.getVerticalGravity(anyInt())).thenReturn(Gravity.BOTTOM);
        when(templateAdapter.getHorizontalGravity(anyInt())).thenReturn(Gravity.CENTER_HORIZONTAL);
        when(templateAdapter.getVerticalGravity(anyInt())).thenReturn(Gravity.BOTTOM);
        when(hostProviders.getAssetProvider()).thenReturn(assetProvider);
        when(hostProviders.getLogDataCallback()).thenReturn(logDataCallback);
        when(assetProvider.isRtLSupplier()).thenReturn(Suppliers.of(false));
        when(frameContext.reportMessage(eq(MessageType.ERROR), anyString()))
                .thenAnswer(invocationOnMock -> {
                    throw new RuntimeException((String) invocationOnMock.getArguments()[1]);
                });
        when(frameContext.getDebugLogger()).thenReturn(debugLogger);
        when(frameContext.getDebugBehavior()).thenReturn(DebugBehavior.VERBOSE);
        when(frameContext.makeStyleFor(any(StyleIdsStack.class))).thenReturn(styleProvider);
        when(frameContext.getFrame()).thenReturn(Frame.getDefaultInstance());
        when(elementAdapter.getElementStyle()).thenReturn(styleProvider);
        when(templateAdapter.getElementStyle()).thenReturn(styleProvider);

        pietSharedStates = new ArrayList<>();
        frameAdapter = new FrameAdapterImpl(
                context, adapterParameters, actionHandler, eventLogger, debugBehavior) {
            @Override
            FrameContext createFrameContext(Frame frame, int frameWidthPx,
                    List<PietSharedState> pietSharedStates, View view) {
                return frameContext;
            }
        };
    }

    @Test
    public void testCreate() {
        LinearLayout linearLayout = frameAdapter.getFrameContainer();
        assertThat(linearLayout).isNotNull();
        LayoutParams layoutParams = linearLayout.getLayoutParams();
        assertThat(layoutParams).isNotNull();
        assertThat(layoutParams.width).isEqualTo(LayoutParams.MATCH_PARENT);
        assertThat(layoutParams.height).isEqualTo(LayoutParams.WRAP_CONTENT);
    }

    @Test
    public void testGetBoundAdaptersForContent() {
        Content content = DEFAULT_CONTENT;

        String templateId = "loaf";
        when(frameContext.getTemplate(templateId)).thenReturn(Template.getDefaultInstance());
        List<ElementAdapter<?, ?>> viewAdapters =
                frameAdapter.getBoundAdaptersForContent(content, frameContext);
        assertThat(viewAdapters).containsExactly(elementAdapter);

        content = Content.newBuilder()
                          .setTemplateInvocation(
                                  TemplateInvocation.newBuilder().setTemplateId(templateId))
                          .build();
        viewAdapters = frameAdapter.getBoundAdaptersForContent(content, frameContext);
        assertThat(viewAdapters).isEmpty();

        content = Content.newBuilder()
                          .setTemplateInvocation(
                                  TemplateInvocation.newBuilder()
                                          .setTemplateId(templateId)
                                          .addBindingContexts(BindingContext.getDefaultInstance()))
                          .build();
        viewAdapters = frameAdapter.getBoundAdaptersForContent(content, frameContext);
        assertThat(viewAdapters).containsExactly(templateAdapter);

        content = Content.newBuilder()
                          .setTemplateInvocation(
                                  TemplateInvocation.newBuilder()
                                          .setTemplateId(templateId)
                                          .addBindingContexts(
                                                  BindingContext.newBuilder().addBindingValues(
                                                          BindingValue.newBuilder().setBindingId(
                                                                  "1")))
                                          .addBindingContexts(
                                                  BindingContext.newBuilder().addBindingValues(
                                                          BindingValue.newBuilder().setBindingId(
                                                                  "2")))
                                          .addBindingContexts(
                                                  BindingContext.newBuilder().addBindingValues(
                                                          BindingValue.newBuilder().setBindingId(
                                                                  "3"))))
                          .build();
        viewAdapters = frameAdapter.getBoundAdaptersForContent(content, frameContext);
        assertThat(viewAdapters).containsExactly(templateAdapter, templateAdapter, templateAdapter);
        verify(templateBinder)
                .createAndBindTemplateAdapter(
                        new TemplateAdapterModel(Template.getDefaultInstance(),
                                content.getTemplateInvocation().getBindingContexts(0)),
                        frameContext);
        verify(templateBinder)
                .createAndBindTemplateAdapter(
                        new TemplateAdapterModel(Template.getDefaultInstance(),
                                content.getTemplateInvocation().getBindingContexts(1)),
                        frameContext);
        verify(templateBinder)
                .createAndBindTemplateAdapter(
                        new TemplateAdapterModel(Template.getDefaultInstance(),
                                content.getTemplateInvocation().getBindingContexts(2)),
                        frameContext);

        verify(frameContext, never()).reportMessage(anyInt(), anyString());
    }

    /**
     * This test sets up all real objects to ensure that real adapters are bound and unbound, etc.
     */
    @Test
    public void testBindAndUnbind_respectsAdapterLifecycle() {
        String templateId = "template";
        ElementList defaultList =
                ElementList.newBuilder()
                        .addContents(
                                Content.newBuilder().setElement(Element.newBuilder().setElementList(
                                        ElementList.getDefaultInstance())))
                        .addContents(Content.newBuilder().setElement(
                                Element.newBuilder().setGridRow(GridRow.getDefaultInstance())))
                        .build();
        AdapterParameters adapterParameters =
                new AdapterParameters(context, Suppliers.of(new LinearLayout(context)),
                        hostProviders, new FakeClock(), false, false);
        PietSharedState pietSharedState =
                PietSharedState.newBuilder()
                        .addTemplates(Template.newBuilder()
                                              .setTemplateId(templateId)
                                              .setElement(Element.newBuilder().setElementList(
                                                      defaultList)))
                        .build();
        pietSharedStates.add(pietSharedState);
        MediaQueryHelper mediaQueryHelper = new MediaQueryHelper(
                FRAME_WIDTH, adapterParameters.hostProviders.getAssetProvider(), context);
        PietStylesHelper stylesHelper =
                adapterParameters.pietStylesHelperFactory.get(pietSharedStates, mediaQueryHelper);
        FrameContext frameContext = FrameContext.createFrameContext(
                Frame.newBuilder().setStylesheets(Stylesheets.getDefaultInstance()).build(),
                pietSharedStates, stylesHelper, debugBehavior, new DebugLogger(), actionHandler,
                new HostProviders(
                        assetProvider, customElementProvider, new HostBindingProvider(), null),
                new FrameLayout(context));

        frameAdapter = new FrameAdapterImpl(
                context, adapterParameters, actionHandler, eventLogger, debugBehavior);

        Content content = Content.newBuilder()
                                  .setElement(Element.newBuilder().setElementList(defaultList))
                                  .build();

        List<ElementAdapter<?, ?>> viewAdapters =
                frameAdapter.getBoundAdaptersForContent(content, frameContext);
        assertThat(viewAdapters).hasSize(1);
        ElementAdapter<?, ?> viewAdapter = viewAdapters.get(0);
        assertThat(viewAdapter.getModel()).isEqualTo(content.getElement().getElementList());
        frameAdapter.bindModel(Frame.newBuilder().addContents(content).build(), FRAME_WIDTH, null,
                pietSharedStates);
        assertThat(frameContext.getDebugLogger().getMessages(MessageType.ERROR)).isEmpty();
        frameAdapter.unbindModel();

        content = Content.newBuilder()
                          .setTemplateInvocation(
                                  TemplateInvocation.newBuilder()
                                          .setTemplateId(templateId)
                                          .addBindingContexts(BindingContext.getDefaultInstance()))
                          .build();
        viewAdapters = frameAdapter.getBoundAdaptersForContent(content, frameContext);
        assertThat(viewAdapters).hasSize(1);
        viewAdapter = viewAdapters.get(0);
        assertThat(viewAdapter.getModel()).isEqualTo(defaultList);
        frameAdapter.bindModel(Frame.newBuilder().addContents(content).build(), FRAME_WIDTH, null,
                pietSharedStates);
        assertThat(frameContext.getDebugLogger().getMessages(MessageType.ERROR)).isEmpty();
        frameAdapter.unbindModel();
    }

    /**
     * This test sets up all real objects to ensure that real adapters are bound and unbound, etc.
     */
    @Test
    public void testBindAndUnbind_resetsStylesWhenWrapperViewIsAdded() {
        AdapterParameters adapterParameters =
                new AdapterParameters(context, Suppliers.of(new LinearLayout(context)),
                        hostProviders, new FakeClock(), false, false);
        PietSharedState pietSharedState =
                PietSharedState.newBuilder()
                        .addStylesheets(
                                Stylesheet.newBuilder()
                                        .setStylesheetId("stylesheet")
                                        // clang-format off
                            .addStyles(
                                    Style.newBuilder()
                                            .setStyleId("style1")
                                            .setBackground(Fill.newBuilder().setColor(
                                                    0x11111111))
                                            .setPadding(EdgeWidths.newBuilder()
                                                                .setStart(1)
                                                                .setTop(1))
                                            .setMargins(EdgeWidths.newBuilder()
                                                                .setStart(1)
                                                                .setTop(1))
                                            .setColor(0x11111111)
                                            .setHeight(1)
                                            .setWidth(1)
                                            .setShadow(
                                                    Shadow.newBuilder()
                                                            .setElevationShadow(
                                                                    ElevationShadow
                                                                            .newBuilder()
                                                                            .setElevation(
                                                                                    1)))
                                            .setOpacity(0.5f)
                                            .setGravityHorizontal(
                                                    GravityHorizontal.GRAVITY_START)
                                            .setGravityVertical(
                                                    GravityVertical.GRAVITY_BOTTOM))
                            .addStyles(
                                    Style.newBuilder()
                                            .setStyleId("style2")
                                            .setBackground(Fill.newBuilder().setColor(
                                                    0x22222222))
                                            .setPadding(EdgeWidths.newBuilder()
                                                                .setEnd(2)
                                                                .setTop(2))
                                            .setMargins(EdgeWidths.newBuilder()
                                                                .setEnd(2)
                                                                .setTop(2))
                                            .setColor(0x222222)
                                            .setHeight(2)
                                            .setWidth(2)
                                            .setShadow(
                                                    Shadow.newBuilder()
                                                            .setElevationShadow(
                                                                    ElevationShadow
                                                                            .newBuilder()
                                                                            .setElevation(
                                                                                    2)))
                                            .setOpacity(0.25f)
                                            // Rounded corners will introduce an
                                            // overlay.
                                            .setRoundedCorners(
                                                    RoundedCorners.newBuilder()
                                                            .setRadiusDp(2)
                                                            .setBitmask(2))
                                            .setGravityHorizontal(
                                                    GravityHorizontal.GRAVITY_END)))
                        // clang-format on
                        .build();
        pietSharedStates.add(pietSharedState);

        frameAdapter = new FrameAdapterImpl(
                context, adapterParameters, actionHandler, eventLogger, debugBehavior);

        ImageElement defaultImage =
                ImageElement.newBuilder().setImage(Image.getDefaultInstance()).build();
        Content content1 =
                Content.newBuilder()
                        .setElement(Element.newBuilder().setElementList(
                                ElementList.newBuilder().addContents(
                                        Content.newBuilder().setElement(
                                                Element.newBuilder()
                                                        .setStyleReferences(
                                                                StyleIdsStack.newBuilder()
                                                                        .addStyleIds("style1"))
                                                        .setImageElement(defaultImage)))))
                        .build();
        Content content2 =
                Content.newBuilder()
                        .setElement(Element.newBuilder().setElementList(
                                ElementList.newBuilder().addContents(
                                        Content.newBuilder().setElement(
                                                Element.newBuilder()
                                                        .setStyleReferences(
                                                                StyleIdsStack.newBuilder()
                                                                        .addStyleIds("style2"))
                                                        .setImageElement(defaultImage)))))
                        .build();

        // Bind to a Frame with no wrapper view
        frameAdapter.bindModel(
                Frame.newBuilder()
                        .setStylesheets(Stylesheets.newBuilder().addStylesheetIds("stylesheet"))
                        .addContents(content1)
                        .build(),
                0, null, pietSharedStates);

        ImageView imageView =
                (ImageView) ((LinearLayout) frameAdapter.getView().getChildAt(0)).getChildAt(0);
        LinearLayout.LayoutParams imageViewParams =
                (LinearLayout.LayoutParams) imageView.getLayoutParams();
        assertThat(imageViewParams.leftMargin).isEqualTo(1);
        assertThat(imageViewParams.topMargin).isEqualTo(1);
        assertThat(imageViewParams.rightMargin).isEqualTo(0);
        assertThat(imageViewParams.bottomMargin).isEqualTo(0);
        assertThat(imageView.getPaddingStart()).isEqualTo(1);
        assertThat(imageView.getPaddingTop()).isEqualTo(1);
        assertThat(imageView.getPaddingEnd()).isEqualTo(0);
        assertThat(imageView.getPaddingBottom()).isEqualTo(0);
        assertThat(imageViewParams.height).isEqualTo(1);
        assertThat(imageViewParams.width).isEqualTo(1);
        assertThat(imageView.getElevation()).isWithin(0.1f).of(1);
        assertThat(imageView.getAlpha()).isWithin(0.1f).of(0.5f);

        frameAdapter.unbindModel();

        // Re-bind to a Frame with a wrapper view
        // This will recycle the ImageView, but it will be within a wrapper view.
        // Ensure that the properties on the ImageView get unset correctly.
        frameAdapter.bindModel(
                Frame.newBuilder()
                        .setStylesheets(Stylesheets.newBuilder().addStylesheetIds("stylesheet"))
                        .addContents(content2)
                        .build(),
                0, null, Collections.singletonList(pietSharedState));

        FrameLayout wrapperView =
                (FrameLayout) ((LinearLayout) frameAdapter.getView().getChildAt(0)).getChildAt(0);
        LinearLayout.LayoutParams wrapperViewParams =
                (LinearLayout.LayoutParams) wrapperView.getLayoutParams();

        // Exactly one of the wrapper view and the image view have the specified params set.
        assertThat(wrapperViewParams.leftMargin).isEqualTo(0);
        assertThat(wrapperViewParams.topMargin).isEqualTo(2);
        assertThat(wrapperViewParams.rightMargin).isEqualTo(2);
        assertThat(wrapperViewParams.bottomMargin).isEqualTo(0);
        assertThat(wrapperView.getPaddingStart()).isEqualTo(0);
        assertThat(wrapperView.getPaddingTop()).isEqualTo(0);
        assertThat(wrapperView.getPaddingEnd()).isEqualTo(0);
        assertThat(wrapperView.getPaddingBottom()).isEqualTo(0);
        assertThat(wrapperViewParams.height).isEqualTo(2);
        assertThat(wrapperViewParams.width).isEqualTo(2);
        assertThat(wrapperView.getElevation()).isWithin(0.1f).of(2);
        assertThat(wrapperView.getAlpha()).isWithin(0.1f).of(1.0f);

        assertThat(wrapperView.getChildAt(0)).isSameInstanceAs(imageView);
        FrameLayout.LayoutParams newImageViewParams =
                (FrameLayout.LayoutParams) imageView.getLayoutParams();
        assertThat(newImageViewParams.leftMargin).isEqualTo(0);
        assertThat(newImageViewParams.topMargin).isEqualTo(0);
        assertThat(newImageViewParams.rightMargin).isEqualTo(0);
        assertThat(newImageViewParams.bottomMargin).isEqualTo(0);
        assertThat(imageView.getPaddingStart()).isEqualTo(0);
        assertThat(imageView.getPaddingTop()).isEqualTo(2);
        assertThat(imageView.getPaddingEnd()).isEqualTo(2);
        assertThat(imageView.getPaddingBottom()).isEqualTo(0);
        assertThat(newImageViewParams.height).isEqualTo(LayoutParams.MATCH_PARENT);
        assertThat(newImageViewParams.width).isEqualTo(LayoutParams.MATCH_PARENT);
        assertThat(imageView.getElevation()).isWithin(0.1f).of(0);
        assertThat(imageView.getAlpha()).isWithin(0.1f).of(0.25f);
    }

    @Test
    public void testBindModel_defaultDimensions() {
        Frame frame = Frame.newBuilder().addContents(DEFAULT_CONTENT).build();
        when(elementAdapter.getComputedWidthPx()).thenReturn(StyleProvider.DIMENSION_NOT_SET);
        when(elementAdapter.getComputedHeightPx()).thenReturn(StyleProvider.DIMENSION_NOT_SET);

        frameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, pietSharedStates);

        assertThat(frameAdapter.getView().getChildCount()).isEqualTo(1);

        verify(elementAdapter).setLayoutParams(layoutParamsCaptor.capture());
        assertThat(layoutParamsCaptor.getValue().width).isEqualTo(LayoutParams.MATCH_PARENT);
        assertThat(layoutParamsCaptor.getValue().height).isEqualTo(LayoutParams.WRAP_CONTENT);
    }

    @Test
    public void testBindModel_explicitDimensions() {
        Frame frame = Frame.newBuilder().addContents(DEFAULT_CONTENT).build();
        int width = 123;
        int height = 456;
        when(elementAdapter.getComputedWidthPx()).thenReturn(width);
        when(elementAdapter.getComputedHeightPx()).thenReturn(height);

        frameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, pietSharedStates);

        assertThat(frameAdapter.getView().getChildCount()).isEqualTo(1);

        verify(elementAdapter).setLayoutParams(layoutParamsCaptor.capture());
        assertThat(layoutParamsCaptor.getValue().width).isEqualTo(width);
        assertThat(layoutParamsCaptor.getValue().height).isEqualTo(height);
    }

    @Test
    public void testBindModel_gravity() {
        Frame frame = Frame.newBuilder().addContents(DEFAULT_CONTENT).build();
        when(elementAdapter.getHorizontalGravity(anyInt())).thenReturn(Gravity.CENTER_HORIZONTAL);
        when(elementAdapter.getVerticalGravity(anyInt())).thenReturn(Gravity.BOTTOM);
        when(elementAdapter.getGravity(anyInt()))
                .thenReturn(Gravity.CENTER_HORIZONTAL | Gravity.BOTTOM);

        frameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, pietSharedStates);

        verify(elementAdapter).setLayoutParams(layoutParamsCaptor.capture());
        assertThat(((LinearLayout.LayoutParams) layoutParamsCaptor.getValue()).gravity)
                .isEqualTo(Gravity.CENTER_HORIZONTAL | Gravity.BOTTOM);
    }

    @Test
    public void testCreateFrameContext_recyclesPietStylesHelper() {
        AdapterParameters adapterParameters =
                new AdapterParameters(context, Suppliers.of(new LinearLayout(context)),
                        hostProviders, new FakeClock(), false, false);
        PietSharedState pietSharedState = PietSharedState.newBuilder()
                                                  .addStylesheets(Stylesheet.newBuilder().addStyles(
                                                          Style.newBuilder().setStyleId("style")))
                                                  .build();
        pietSharedStates.add(pietSharedState);
        FrameAdapterImpl frameAdapter = new FrameAdapterImpl(
                context, adapterParameters, actionHandler, eventLogger, debugBehavior);

        FrameContext frameContext1 =
                frameAdapter.createFrameContext(Frame.newBuilder().setTag("frame1").build(),
                        FRAME_WIDTH, pietSharedStates, frameAdapter.getView());
        FrameContext frameContext2 =
                frameAdapter.createFrameContext(Frame.newBuilder().setTag("frame2").build(),
                        FRAME_WIDTH, pietSharedStates, frameAdapter.getView());
        FrameContext frameContext3 =
                frameAdapter.createFrameContext(Frame.newBuilder().setTag("frame3").build(),
                        FRAME_WIDTH + 1, pietSharedStates, frameAdapter.getView());

        // These should both pull the same object from the cache.
        assertThat(frameContext1.stylesHelper).isSameInstanceAs(frameContext2.stylesHelper);

        // This one is different because of the different frame width.
        assertThat(frameContext1.stylesHelper).isNotEqualTo(frameContext3.stylesHelper);
    }

    @Test
    public void testAdapterWithActions() {
        String templateId = "lights-camera";
        String actionBindingId = "action";
        ActionsBindingRef actionBinding =
                ActionsBindingRef.newBuilder().setBindingId(actionBindingId).build();
        Template templateWithActions =
                Template.newBuilder()
                        .setTemplateId(templateId)
                        .setElement(Element.newBuilder()
                                            .setActionsBinding(actionBinding)
                                            .setElementList(ElementList.getDefaultInstance()))
                        .build();
        BindingValue actionBindingValue = BindingValue.newBuilder()
                                                  .setBindingId(actionBindingId)
                                                  .setActions(Actions.newBuilder().setOnClickAction(
                                                          Action.getDefaultInstance()))
                                                  .build();
        Content content =
                Content.newBuilder()
                        .setTemplateInvocation(
                                TemplateInvocation.newBuilder()
                                        .setTemplateId(templateId)
                                        .addBindingContexts(
                                                BindingContext.newBuilder().addBindingValues(
                                                        actionBindingValue)))
                        .build();
        Frame frame = Frame.newBuilder().addContents(content).build();
        PietSharedState pietSharedState =
                PietSharedState.newBuilder().addTemplates(templateWithActions).build();

        pietSharedStates.add(pietSharedState);

        AdapterParameters parameters = new AdapterParameters(
                context, Suppliers.of(null), hostProviders, new FakeClock(), false, false);
        FrameAdapterImpl frameAdapter = new FrameAdapterImpl(
                context, parameters, actionHandler, eventLogger, debugBehavior);

        frameAdapter.bindModel(frame, FRAME_WIDTH, null, pietSharedStates);

        assertThat(frameAdapter.getFrameContainer().getChildCount()).isEqualTo(1);
        assertThat(frameAdapter.getFrameContainer().getChildAt(0).hasOnClickListeners()).isTrue();
    }

    @Test
    public void testBackgroundColor() {
        Frame frame = Frame.newBuilder().addContents(DEFAULT_CONTENT).build();
        frameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, pietSharedStates);
        styleProvider.createBackground();
    }

    @Test
    public void testUnsetBackgroundIfNotDefined() {
        Frame frame = Frame.newBuilder().addContents(DEFAULT_CONTENT).build();
        when(frameContext.getFrame()).thenReturn(frame);

        // Set background
        when(styleProvider.createBackground()).thenReturn(new ColorDrawable(12345));
        frameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, pietSharedStates);
        assertThat(frameAdapter.getView().getBackground()).isNotNull();
        frameAdapter.unbindModel();

        // Re-bind and check that background is unset
        when(styleProvider.createBackground()).thenReturn(null);
        frameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, pietSharedStates);
        assertThat(frameAdapter.getView().getBackground()).isNull();
    }

    @Test
    public void testUnsetBackgroundIfCreateBackgroundFails() {
        Frame frame = Frame.newBuilder().addContents(DEFAULT_CONTENT).build();
        when(frameContext.getFrame()).thenReturn(frame);

        // Set background
        when(styleProvider.createBackground()).thenReturn(new ColorDrawable(12345));
        frameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, pietSharedStates);
        assertThat(frameAdapter.getView().getBackground()).isNotNull();
        frameAdapter.unbindModel();

        // Re-bind and check that background is unset
        when(styleProvider.createBackground()).thenReturn(null);
        frameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, pietSharedStates);
        assertThat(frameAdapter.getView().getBackground()).isNull();
    }

    @Test
    public void testRecycling_inlineSlice() {
        Element element =
                Element.newBuilder().setElementList(ElementList.getDefaultInstance()).build();
        Frame inlineSliceFrame =
                Frame.newBuilder().addContents(Content.newBuilder().setElement(element)).build();
        when(frameContext.getFrame()).thenReturn(inlineSliceFrame);

        frameAdapter.bindModel(
                inlineSliceFrame, FRAME_WIDTH, (ShardingControl) null, pietSharedStates);
        verify(adapterFactory).createAdapterForElement(element, frameContext);
        verify(elementAdapter).bindModel(element, frameContext);

        frameAdapter.unbindModel();
        verify(adapterFactory).releaseAdapter(elementAdapter);
    }

    @Test
    public void testRecycling_templateSlice() {
        String templateId = "bread";
        Template template = Template.newBuilder().setTemplateId(templateId).build();
        when(frameContext.getTemplate(templateId)).thenReturn(template);
        BindingContext bindingContext =
                BindingContext.newBuilder()
                        .addBindingValues(BindingValue.newBuilder().setBindingId("grain"))
                        .build();
        TemplateInvocation templateSlice = TemplateInvocation.newBuilder()
                                                   .setTemplateId(templateId)
                                                   .addBindingContexts(bindingContext)
                                                   .build();
        Frame templateSliceFrame =
                Frame.newBuilder()
                        .addContents(Content.newBuilder().setTemplateInvocation(templateSlice))
                        .build();
        when(frameContext.getFrame()).thenReturn(templateSliceFrame);
        frameAdapter.bindModel(
                templateSliceFrame, FRAME_WIDTH, (ShardingControl) null, pietSharedStates);
        TemplateAdapterModel model = new TemplateAdapterModel(template, bindingContext);
        verify(templateBinder).createAndBindTemplateAdapter(model, frameContext);

        frameAdapter.unbindModel();
        verify(adapterFactory).releaseAdapter(templateAdapter);
    }

    @Test
    public void testErrorViewReporting() {
        View warningView = new View(context);
        View errorView = new View(context);
        when(debugLogger.getReportView(MessageType.WARNING, context)).thenReturn(warningView);
        when(debugLogger.getReportView(MessageType.ERROR, context)).thenReturn(errorView);

        Frame frame = Frame.newBuilder().addContents(DEFAULT_CONTENT).build();
        when(frameContext.getFrame()).thenReturn(frame);

        // Errors silenced by debug behavior
        when(frameContext.getDebugBehavior()).thenReturn(DebugBehavior.SILENT);
        frameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, pietSharedStates);

        assertThat(frameAdapter.getView().getChildCount()).isEqualTo(1);
        verify(debugLogger, never()).getReportView(anyInt(), any());

        frameAdapter.unbindModel();

        // Errors displayed in extra views
        when(frameContext.getDebugBehavior()).thenReturn(DebugBehavior.VERBOSE);
        frameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, pietSharedStates);

        assertThat(frameAdapter.getView().getChildCount()).isEqualTo(3);
        assertThat(frameAdapter.getView().getChildAt(1)).isSameInstanceAs(errorView);
        assertThat(frameAdapter.getView().getChildAt(2)).isSameInstanceAs(warningView);

        frameAdapter.unbindModel();

        // No errors
        when(debugLogger.getReportView(MessageType.WARNING, context)).thenReturn(null);
        when(debugLogger.getReportView(MessageType.ERROR, context)).thenReturn(null);

        frameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, pietSharedStates);

        assertThat(frameAdapter.getView().getChildCount()).isEqualTo(1);
    }

    @Test
    public void testEventLoggerReporting() {
        List<ErrorCode> errorCodes = new ArrayList<>();
        errorCodes.add(ErrorCode.ERR_DUPLICATE_STYLE);
        errorCodes.add(ErrorCode.ERR_POOR_FRAME_RATE);
        when(debugLogger.getErrorCodes()).thenReturn(errorCodes);

        Frame frame = Frame.newBuilder().addContents(DEFAULT_CONTENT).build();

        frameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, pietSharedStates);

        verify(eventLogger).logEvents(errorCodes);
    }

    @Test
    public void testEventLoggerReporting_withFatalError() {
        List<ErrorCode> errorCodes = new ArrayList<>();
        errorCodes.add(ErrorCode.ERR_DUPLICATE_STYLE);
        errorCodes.add(ErrorCode.ERR_POOR_FRAME_RATE);
        when(debugLogger.getErrorCodes()).thenReturn(errorCodes);

        when(frameContext.makeStyleFor(any()))
                .thenThrow(new PietFatalException(ErrorCode.ERR_UNSPECIFIED, "test exception"));

        Frame frame = Frame.newBuilder().addContents(DEFAULT_CONTENT).build();

        frameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, pietSharedStates);

        verify(frameContext).makeStyleFor(any()); // Ensure an exception was thrown
        verify(eventLogger).logEvents(errorCodes);
    }

    @Test
    public void testUnbindTriggersHideActions() {
        Action frameHideAction = Action.newBuilder().build();
        Action elementHideAction = Action.newBuilder().build();
        Frame frameWithNoActions = Frame.newBuilder()
                                           .addContents(Content.newBuilder().setElement(
                                                   Element.newBuilder().setElementStack(
                                                           ElementStack.getDefaultInstance())))
                                           .build();
        Frame frameWithActions =
                Frame.newBuilder()
                        .setActions(Actions.newBuilder().addOnHideActions(
                                VisibilityAction.newBuilder().setAction(frameHideAction)))
                        .addContents(Content.newBuilder().setElement(
                                Element.newBuilder()
                                        .setActions(Actions.newBuilder().addOnHideActions(
                                                VisibilityAction.newBuilder().setAction(
                                                        elementHideAction)))
                                        .setElementStack(ElementStack.getDefaultInstance())))
                        .build();

        // Bind to a frame with no actions so the onHide actions don't get added to activeActions
        when(frameContext.getFrame()).thenReturn(frameWithNoActions);
        frameAdapter.bindModel(
                frameWithNoActions, FRAME_WIDTH, (ShardingControl) null, pietSharedStates);

        when(frameContext.getFrame()).thenReturn(frameWithActions);
        when(frameContext.getActionHandler()).thenReturn(actionHandler);
        frameAdapter.unbindModel();

        verify(actionHandler)
                .handleAction(same(frameHideAction), eq(ActionType.VIEW), eq(frameWithActions),
                        any(View.class), eq(LogData.getDefaultInstance()));
        verify(elementAdapter).triggerHideActions(frameContext);
    }

    @Test
    public void testBindModel_callsOnBindLogDataCallback() {
        Frame frame =
                Frame.newBuilder().addContents(DEFAULT_CONTENT).setLogData(logDataTest).build();
        when(frameContext.getFrame()).thenReturn(frame);
        frameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, pietSharedStates);

        assertThat(callbackBound).isTrue();
    }

    @Test
    public void testUnbindModel_callsOnBindLogDataCallback() {
        Frame frame =
                Frame.newBuilder().addContents(DEFAULT_CONTENT).setLogData(logDataTest).build();
        when(frameContext.getFrame()).thenReturn(frame);
        frameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, pietSharedStates);

        when(frameContext.getActionHandler()).thenReturn(actionHandler);
        frameAdapter.unbindModel();
        assertThat(callbackUnbound).isTrue();
    }
}
