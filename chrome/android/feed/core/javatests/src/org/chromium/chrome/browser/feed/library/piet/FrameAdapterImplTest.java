// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

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

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.config.DebugBehavior;
import org.chromium.chrome.browser.feed.library.common.functional.Suppliers;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.piet.DebugLogger.MessageType;
import org.chromium.chrome.browser.feed.library.piet.PietStylesHelper.PietStylesHelperFactory;
import org.chromium.chrome.browser.feed.library.piet.TemplateBinder.TemplateAdapterModel;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler.ActionType;
import org.chromium.chrome.browser.feed.library.piet.host.AssetProvider;
import org.chromium.chrome.browser.feed.library.piet.host.CustomElementProvider;
import org.chromium.chrome.browser.feed.library.piet.host.EventLogger;
import org.chromium.chrome.browser.feed.library.piet.host.HostBindingProvider;
import org.chromium.chrome.browser.feed.library.piet.host.LogDataCallback;
import org.chromium.chrome.browser.feed.library.piet.ui.RoundedCornerMaskCache;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.Action;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.Actions;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.VisibilityAction;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ActionsBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingContext;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingValue;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Content;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ElementList;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ElementStack;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.GridRow;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ImageElement;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.TemplateInvocation;
import org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode;
import org.chromium.components.feed.core.proto.ui.piet.GradientsProto.Fill;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.Image;
import org.chromium.components.feed.core.proto.ui.piet.LogDataProto.LogData;
import org.chromium.components.feed.core.proto.ui.piet.PietAndroidSupport.ShardingControl;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Frame;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.PietSharedState;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Stylesheet;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Stylesheets;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Template;
import org.chromium.components.feed.core.proto.ui.piet.RoundedCornersProto.RoundedCorners;
import org.chromium.components.feed.core.proto.ui.piet.ShadowsProto.ElevationShadow;
import org.chromium.components.feed.core.proto.ui.piet.ShadowsProto.Shadow;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.EdgeWidths;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.GravityHorizontal;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.GravityVertical;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Style;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.StyleIdsStack;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Tests of the {@link FrameAdapterImpl}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FrameAdapterImplTest {
    private static final Content DEFAULT_CONTENT =
            Content.newBuilder()
                    .setElement(Element.newBuilder().setElementList(
                            ElementList.newBuilder().addContents(
                                    Content.newBuilder().setElement(Element.getDefaultInstance()))))
                    .build();
    private static final int FRAME_WIDTH = 321;
    private final LogData mLogDataTest = LogData.getDefaultInstance();

    @Mock
    private AssetProvider mAssetProvider;
    @Mock
    private ElementAdapterFactory mAdapterFactory;
    @Mock
    private TemplateBinder mTemplateBinder;
    @Mock
    private FrameContext mFrameContext;
    @Mock
    private DebugBehavior mDebugBehavior;
    @Mock
    private DebugLogger mDebugLogger;
    @Mock
    private StyleProvider mStyleProvider;
    @Mock
    private ElementAdapter<? extends View, ?> mElementAdapter;
    @Mock
    private ElementAdapter<? extends View, ?> mTemplateAdapter;
    @Mock
    private CustomElementProvider mCustomElementProvider;
    @Mock
    private ActionHandler mActionHandler;
    @Mock
    private HostProviders mHostProviders;
    @Mock
    private EventLogger mEventLogger;
    @Mock
    private PietStylesHelperFactory mStylesHelpers;
    @Mock
    private RoundedCornerMaskCache mMaskCache;

    @Captor
    private ArgumentCaptor<LayoutParams> mLayoutParamsCaptor;

    private Context mContext;
    private List<PietSharedState> mPietSharedStates;
    private AdapterParameters mAdapterParameters;
    private boolean mCallbackBound;
    private boolean mCallbackUnbound;
    private FrameAdapterImpl mFrameAdapter;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        LogDataCallback logDataCallback = new LogDataCallback() {
            @Override
            public void onBind(LogData logData, View view) {
                assertThat(mLogDataTest).isEqualTo(logData);
                mCallbackBound = true;
            }

            @Override
            public void onUnbind(LogData logData, View view) {
                assertThat(mLogDataTest).isEqualTo(logData);
                mCallbackUnbound = true;
            }
        };
        mAdapterParameters = new AdapterParameters(null, Suppliers.of(new FrameLayout(mContext)),
                mHostProviders, new ParameterizedTextEvaluator(new FakeClock()), mAdapterFactory,
                mTemplateBinder, new FakeClock(), mStylesHelpers, mMaskCache, false, false);
        when(mElementAdapter.getView()).thenReturn(new LinearLayout(mContext));
        when(mTemplateAdapter.getView()).thenReturn(new LinearLayout(mContext));
        doReturn(mElementAdapter)
                .when(mAdapterFactory)
                .createAdapterForElement(any(Element.class), eq(mFrameContext));
        doReturn(mTemplateAdapter)
                .when(mTemplateBinder)
                .createAndBindTemplateAdapter(any(TemplateAdapterModel.class), eq(mFrameContext));
        when(mElementAdapter.getHorizontalGravity(anyInt())).thenReturn(Gravity.CENTER_HORIZONTAL);
        when(mElementAdapter.getVerticalGravity(anyInt())).thenReturn(Gravity.BOTTOM);
        when(mTemplateAdapter.getHorizontalGravity(anyInt())).thenReturn(Gravity.CENTER_HORIZONTAL);
        when(mTemplateAdapter.getVerticalGravity(anyInt())).thenReturn(Gravity.BOTTOM);
        when(mHostProviders.getAssetProvider()).thenReturn(mAssetProvider);
        when(mHostProviders.getLogDataCallback()).thenReturn(logDataCallback);
        when(mAssetProvider.isRtLSupplier()).thenReturn(Suppliers.of(false));
        when(mFrameContext.reportMessage(eq(MessageType.ERROR), anyString()))
                .thenAnswer(invocationOnMock -> {
                    throw new RuntimeException((String) invocationOnMock.getArguments()[1]);
                });
        when(mFrameContext.getDebugLogger()).thenReturn(mDebugLogger);
        when(mFrameContext.getDebugBehavior()).thenReturn(DebugBehavior.VERBOSE);
        when(mFrameContext.makeStyleFor(any(StyleIdsStack.class))).thenReturn(mStyleProvider);
        when(mFrameContext.getFrame()).thenReturn(Frame.getDefaultInstance());
        when(mElementAdapter.getElementStyle()).thenReturn(mStyleProvider);
        when(mTemplateAdapter.getElementStyle()).thenReturn(mStyleProvider);

        mPietSharedStates = new ArrayList<>();
        mFrameAdapter = new FrameAdapterImpl(
                mContext, mAdapterParameters, mActionHandler, mEventLogger, mDebugBehavior) {
            @Override
            FrameContext createFrameContext(Frame frame, int frameWidthPx,
                    List<PietSharedState> pietSharedStates, View view) {
                return mFrameContext;
            }
        };
    }

    @Test
    public void testCreate() {
        LinearLayout linearLayout = mFrameAdapter.getFrameContainer();
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
        when(mFrameContext.getTemplate(templateId)).thenReturn(Template.getDefaultInstance());
        List<ElementAdapter<?, ?>> viewAdapters =
                mFrameAdapter.getBoundAdaptersForContent(content, mFrameContext);
        assertThat(viewAdapters).containsExactly(mElementAdapter);

        content = Content.newBuilder()
                          .setTemplateInvocation(
                                  TemplateInvocation.newBuilder().setTemplateId(templateId))
                          .build();
        viewAdapters = mFrameAdapter.getBoundAdaptersForContent(content, mFrameContext);
        assertThat(viewAdapters).isEmpty();

        content = Content.newBuilder()
                          .setTemplateInvocation(
                                  TemplateInvocation.newBuilder()
                                          .setTemplateId(templateId)
                                          .addBindingContexts(BindingContext.getDefaultInstance()))
                          .build();
        viewAdapters = mFrameAdapter.getBoundAdaptersForContent(content, mFrameContext);
        assertThat(viewAdapters).containsExactly(mTemplateAdapter);

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
        viewAdapters = mFrameAdapter.getBoundAdaptersForContent(content, mFrameContext);
        assertThat(viewAdapters)
                .containsExactly(mTemplateAdapter, mTemplateAdapter, mTemplateAdapter);
        verify(mTemplateBinder)
                .createAndBindTemplateAdapter(
                        new TemplateAdapterModel(Template.getDefaultInstance(),
                                content.getTemplateInvocation().getBindingContexts(0)),
                        mFrameContext);
        verify(mTemplateBinder)
                .createAndBindTemplateAdapter(
                        new TemplateAdapterModel(Template.getDefaultInstance(),
                                content.getTemplateInvocation().getBindingContexts(1)),
                        mFrameContext);
        verify(mTemplateBinder)
                .createAndBindTemplateAdapter(
                        new TemplateAdapterModel(Template.getDefaultInstance(),
                                content.getTemplateInvocation().getBindingContexts(2)),
                        mFrameContext);

        verify(mFrameContext, never()).reportMessage(anyInt(), anyString());
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
                new AdapterParameters(mContext, Suppliers.of(new LinearLayout(mContext)),
                        mHostProviders, new FakeClock(), false, false);
        PietSharedState pietSharedState =
                PietSharedState.newBuilder()
                        .addTemplates(Template.newBuilder()
                                              .setTemplateId(templateId)
                                              .setElement(Element.newBuilder().setElementList(
                                                      defaultList)))
                        .build();
        mPietSharedStates.add(pietSharedState);
        MediaQueryHelper mediaQueryHelper = new MediaQueryHelper(
                FRAME_WIDTH, adapterParameters.mHostProviders.getAssetProvider(), mContext);
        PietStylesHelper stylesHelper =
                adapterParameters.mPietStylesHelperFactory.get(mPietSharedStates, mediaQueryHelper);
        FrameContext frameContext = FrameContext.createFrameContext(
                Frame.newBuilder().setStylesheets(Stylesheets.getDefaultInstance()).build(),
                mPietSharedStates, stylesHelper, mDebugBehavior, new DebugLogger(), mActionHandler,
                new HostProviders(
                        mAssetProvider, mCustomElementProvider, new HostBindingProvider(), null),
                new FrameLayout(mContext));

        mFrameAdapter = new FrameAdapterImpl(
                mContext, adapterParameters, mActionHandler, mEventLogger, mDebugBehavior);

        Content content = Content.newBuilder()
                                  .setElement(Element.newBuilder().setElementList(defaultList))
                                  .build();

        List<ElementAdapter<?, ?>> viewAdapters =
                mFrameAdapter.getBoundAdaptersForContent(content, frameContext);
        assertThat(viewAdapters).hasSize(1);
        ElementAdapter<?, ?> viewAdapter = viewAdapters.get(0);
        assertThat(viewAdapter.getModel()).isEqualTo(content.getElement().getElementList());
        mFrameAdapter.bindModel(Frame.newBuilder().addContents(content).build(), FRAME_WIDTH, null,
                mPietSharedStates);
        assertThat(frameContext.getDebugLogger().getMessages(MessageType.ERROR)).isEmpty();
        mFrameAdapter.unbindModel();

        content = Content.newBuilder()
                          .setTemplateInvocation(
                                  TemplateInvocation.newBuilder()
                                          .setTemplateId(templateId)
                                          .addBindingContexts(BindingContext.getDefaultInstance()))
                          .build();
        viewAdapters = mFrameAdapter.getBoundAdaptersForContent(content, frameContext);
        assertThat(viewAdapters).hasSize(1);
        viewAdapter = viewAdapters.get(0);
        assertThat(viewAdapter.getModel()).isEqualTo(defaultList);
        mFrameAdapter.bindModel(Frame.newBuilder().addContents(content).build(), FRAME_WIDTH, null,
                mPietSharedStates);
        assertThat(frameContext.getDebugLogger().getMessages(MessageType.ERROR)).isEmpty();
        mFrameAdapter.unbindModel();
    }

    /**
     * This test sets up all real objects to ensure that real adapters are bound and unbound, etc.
     */
    @Test
    public void testBindAndUnbind_resetsStylesWhenWrapperViewIsAdded() {
        AdapterParameters adapterParameters =
                new AdapterParameters(mContext, Suppliers.of(new LinearLayout(mContext)),
                        mHostProviders, new FakeClock(), false, false);
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
        mPietSharedStates.add(pietSharedState);

        mFrameAdapter = new FrameAdapterImpl(
                mContext, adapterParameters, mActionHandler, mEventLogger, mDebugBehavior);

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
        mFrameAdapter.bindModel(
                Frame.newBuilder()
                        .setStylesheets(Stylesheets.newBuilder().addStylesheetIds("stylesheet"))
                        .addContents(content1)
                        .build(),
                0, null, mPietSharedStates);

        ImageView imageView =
                (ImageView) ((LinearLayout) mFrameAdapter.getView().getChildAt(0)).getChildAt(0);
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

        mFrameAdapter.unbindModel();

        // Re-bind to a Frame with a wrapper view
        // This will recycle the ImageView, but it will be within a wrapper view.
        // Ensure that the properties on the ImageView get unset correctly.
        mFrameAdapter.bindModel(
                Frame.newBuilder()
                        .setStylesheets(Stylesheets.newBuilder().addStylesheetIds("stylesheet"))
                        .addContents(content2)
                        .build(),
                0, null, Collections.singletonList(pietSharedState));

        FrameLayout wrapperView =
                (FrameLayout) ((LinearLayout) mFrameAdapter.getView().getChildAt(0)).getChildAt(0);
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
        when(mElementAdapter.getComputedWidthPx()).thenReturn(StyleProvider.DIMENSION_NOT_SET);
        when(mElementAdapter.getComputedHeightPx()).thenReturn(StyleProvider.DIMENSION_NOT_SET);

        mFrameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, mPietSharedStates);

        assertThat(mFrameAdapter.getView().getChildCount()).isEqualTo(1);

        verify(mElementAdapter).setLayoutParams(mLayoutParamsCaptor.capture());
        assertThat(mLayoutParamsCaptor.getValue().width).isEqualTo(LayoutParams.MATCH_PARENT);
        assertThat(mLayoutParamsCaptor.getValue().height).isEqualTo(LayoutParams.WRAP_CONTENT);
    }

    @Test
    public void testBindModel_explicitDimensions() {
        Frame frame = Frame.newBuilder().addContents(DEFAULT_CONTENT).build();
        int width = 123;
        int height = 456;
        when(mElementAdapter.getComputedWidthPx()).thenReturn(width);
        when(mElementAdapter.getComputedHeightPx()).thenReturn(height);

        mFrameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, mPietSharedStates);

        assertThat(mFrameAdapter.getView().getChildCount()).isEqualTo(1);

        verify(mElementAdapter).setLayoutParams(mLayoutParamsCaptor.capture());
        assertThat(mLayoutParamsCaptor.getValue().width).isEqualTo(width);
        assertThat(mLayoutParamsCaptor.getValue().height).isEqualTo(height);
    }

    @Test
    public void testBindModel_gravity() {
        Frame frame = Frame.newBuilder().addContents(DEFAULT_CONTENT).build();
        when(mElementAdapter.getHorizontalGravity(anyInt())).thenReturn(Gravity.CENTER_HORIZONTAL);
        when(mElementAdapter.getVerticalGravity(anyInt())).thenReturn(Gravity.BOTTOM);
        when(mElementAdapter.getGravity(anyInt()))
                .thenReturn(Gravity.CENTER_HORIZONTAL | Gravity.BOTTOM);

        mFrameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, mPietSharedStates);

        verify(mElementAdapter).setLayoutParams(mLayoutParamsCaptor.capture());
        assertThat(((LinearLayout.LayoutParams) mLayoutParamsCaptor.getValue()).gravity)
                .isEqualTo(Gravity.CENTER_HORIZONTAL | Gravity.BOTTOM);
    }

    @Test
    public void testCreateFrameContext_recyclesPietStylesHelper() {
        AdapterParameters adapterParameters =
                new AdapterParameters(mContext, Suppliers.of(new LinearLayout(mContext)),
                        mHostProviders, new FakeClock(), false, false);
        PietSharedState pietSharedState = PietSharedState.newBuilder()
                                                  .addStylesheets(Stylesheet.newBuilder().addStyles(
                                                          Style.newBuilder().setStyleId("style")))
                                                  .build();
        mPietSharedStates.add(pietSharedState);
        FrameAdapterImpl frameAdapter = new FrameAdapterImpl(
                mContext, adapterParameters, mActionHandler, mEventLogger, mDebugBehavior);

        FrameContext frameContext1 =
                frameAdapter.createFrameContext(Frame.newBuilder().setTag("frame1").build(),
                        FRAME_WIDTH, mPietSharedStates, frameAdapter.getView());
        FrameContext frameContext2 =
                frameAdapter.createFrameContext(Frame.newBuilder().setTag("frame2").build(),
                        FRAME_WIDTH, mPietSharedStates, frameAdapter.getView());
        FrameContext frameContext3 =
                frameAdapter.createFrameContext(Frame.newBuilder().setTag("frame3").build(),
                        FRAME_WIDTH + 1, mPietSharedStates, frameAdapter.getView());

        // These should both pull the same object from the cache.
        assertThat(frameContext1.mStyleshelper).isSameInstanceAs(frameContext2.mStyleshelper);

        // This one is different because of the different frame width.
        assertThat(frameContext1.mStyleshelper).isNotEqualTo(frameContext3.mStyleshelper);
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

        mPietSharedStates.add(pietSharedState);

        AdapterParameters parameters = new AdapterParameters(
                mContext, Suppliers.of(null), mHostProviders, new FakeClock(), false, false);
        FrameAdapterImpl frameAdapter = new FrameAdapterImpl(
                mContext, parameters, mActionHandler, mEventLogger, mDebugBehavior);

        frameAdapter.bindModel(frame, FRAME_WIDTH, null, mPietSharedStates);

        assertThat(frameAdapter.getFrameContainer().getChildCount()).isEqualTo(1);
        assertThat(frameAdapter.getFrameContainer().getChildAt(0).hasOnClickListeners()).isTrue();
    }

    @Test
    public void testBackgroundColor() {
        Frame frame = Frame.newBuilder().addContents(DEFAULT_CONTENT).build();
        mFrameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, mPietSharedStates);
        mStyleProvider.createBackground();
    }

    @Test
    public void testUnsetBackgroundIfNotDefined() {
        Frame frame = Frame.newBuilder().addContents(DEFAULT_CONTENT).build();
        when(mFrameContext.getFrame()).thenReturn(frame);

        // Set background
        when(mStyleProvider.createBackground()).thenReturn(new ColorDrawable(12345));
        mFrameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, mPietSharedStates);
        assertThat(mFrameAdapter.getView().getBackground()).isNotNull();
        mFrameAdapter.unbindModel();

        // Re-bind and check that background is unset
        when(mStyleProvider.createBackground()).thenReturn(null);
        mFrameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, mPietSharedStates);
        assertThat(mFrameAdapter.getView().getBackground()).isNull();
    }

    @Test
    public void testUnsetBackgroundIfCreateBackgroundFails() {
        Frame frame = Frame.newBuilder().addContents(DEFAULT_CONTENT).build();
        when(mFrameContext.getFrame()).thenReturn(frame);

        // Set background
        when(mStyleProvider.createBackground()).thenReturn(new ColorDrawable(12345));
        mFrameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, mPietSharedStates);
        assertThat(mFrameAdapter.getView().getBackground()).isNotNull();
        mFrameAdapter.unbindModel();

        // Re-bind and check that background is unset
        when(mStyleProvider.createBackground()).thenReturn(null);
        mFrameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, mPietSharedStates);
        assertThat(mFrameAdapter.getView().getBackground()).isNull();
    }

    @Test
    public void testRecycling_inlineSlice() {
        Element element =
                Element.newBuilder().setElementList(ElementList.getDefaultInstance()).build();
        Frame inlineSliceFrame =
                Frame.newBuilder().addContents(Content.newBuilder().setElement(element)).build();
        when(mFrameContext.getFrame()).thenReturn(inlineSliceFrame);

        mFrameAdapter.bindModel(
                inlineSliceFrame, FRAME_WIDTH, (ShardingControl) null, mPietSharedStates);
        verify(mAdapterFactory).createAdapterForElement(element, mFrameContext);
        verify(mElementAdapter).bindModel(element, mFrameContext);

        mFrameAdapter.unbindModel();
        verify(mAdapterFactory).releaseAdapter(mElementAdapter);
    }

    @Test
    public void testRecycling_templateSlice() {
        String templateId = "bread";
        Template template = Template.newBuilder().setTemplateId(templateId).build();
        when(mFrameContext.getTemplate(templateId)).thenReturn(template);
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
        when(mFrameContext.getFrame()).thenReturn(templateSliceFrame);
        mFrameAdapter.bindModel(
                templateSliceFrame, FRAME_WIDTH, (ShardingControl) null, mPietSharedStates);
        TemplateAdapterModel model = new TemplateAdapterModel(template, bindingContext);
        verify(mTemplateBinder).createAndBindTemplateAdapter(model, mFrameContext);

        mFrameAdapter.unbindModel();
        verify(mAdapterFactory).releaseAdapter(mTemplateAdapter);
    }

    @Test
    public void testErrorViewReporting() {
        View warningView = new View(mContext);
        View errorView = new View(mContext);
        when(mDebugLogger.getReportView(MessageType.WARNING, mContext)).thenReturn(warningView);
        when(mDebugLogger.getReportView(MessageType.ERROR, mContext)).thenReturn(errorView);

        Frame frame = Frame.newBuilder().addContents(DEFAULT_CONTENT).build();
        when(mFrameContext.getFrame()).thenReturn(frame);

        // Errors silenced by debug behavior
        when(mFrameContext.getDebugBehavior()).thenReturn(DebugBehavior.SILENT);
        mFrameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, mPietSharedStates);

        assertThat(mFrameAdapter.getView().getChildCount()).isEqualTo(1);
        verify(mDebugLogger, never()).getReportView(anyInt(), any());

        mFrameAdapter.unbindModel();

        // Errors displayed in extra views
        when(mFrameContext.getDebugBehavior()).thenReturn(DebugBehavior.VERBOSE);
        mFrameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, mPietSharedStates);

        assertThat(mFrameAdapter.getView().getChildCount()).isEqualTo(3);
        assertThat(mFrameAdapter.getView().getChildAt(1)).isSameInstanceAs(errorView);
        assertThat(mFrameAdapter.getView().getChildAt(2)).isSameInstanceAs(warningView);

        mFrameAdapter.unbindModel();

        // No errors
        when(mDebugLogger.getReportView(MessageType.WARNING, mContext)).thenReturn(null);
        when(mDebugLogger.getReportView(MessageType.ERROR, mContext)).thenReturn(null);

        mFrameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, mPietSharedStates);

        assertThat(mFrameAdapter.getView().getChildCount()).isEqualTo(1);
    }

    @Test
    public void testEventLoggerReporting() {
        List<ErrorCode> errorCodes = new ArrayList<>();
        errorCodes.add(ErrorCode.ERR_DUPLICATE_STYLE);
        errorCodes.add(ErrorCode.ERR_POOR_FRAME_RATE);
        when(mDebugLogger.getErrorCodes()).thenReturn(errorCodes);

        Frame frame = Frame.newBuilder().addContents(DEFAULT_CONTENT).build();

        mFrameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, mPietSharedStates);

        verify(mEventLogger).logEvents(errorCodes);
    }

    @Test
    public void testEventLoggerReporting_withFatalError() {
        List<ErrorCode> errorCodes = new ArrayList<>();
        errorCodes.add(ErrorCode.ERR_DUPLICATE_STYLE);
        errorCodes.add(ErrorCode.ERR_POOR_FRAME_RATE);
        when(mDebugLogger.getErrorCodes()).thenReturn(errorCodes);

        when(mFrameContext.makeStyleFor(any()))
                .thenThrow(new PietFatalException(ErrorCode.ERR_UNSPECIFIED, "test exception"));

        Frame frame = Frame.newBuilder().addContents(DEFAULT_CONTENT).build();

        mFrameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, mPietSharedStates);

        verify(mFrameContext).makeStyleFor(any()); // Ensure an exception was thrown
        verify(mEventLogger).logEvents(errorCodes);
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
        when(mFrameContext.getFrame()).thenReturn(frameWithNoActions);
        mFrameAdapter.bindModel(
                frameWithNoActions, FRAME_WIDTH, (ShardingControl) null, mPietSharedStates);

        when(mFrameContext.getFrame()).thenReturn(frameWithActions);
        when(mFrameContext.getActionHandler()).thenReturn(mActionHandler);
        mFrameAdapter.unbindModel();

        verify(mActionHandler)
                .handleAction(same(frameHideAction), eq(ActionType.VIEW), eq(frameWithActions),
                        any(View.class), eq(LogData.getDefaultInstance()));
        verify(mElementAdapter).triggerHideActions(mFrameContext);
    }

    @Test
    public void testBindModel_callsOnBindLogDataCallback() {
        Frame frame =
                Frame.newBuilder().addContents(DEFAULT_CONTENT).setLogData(mLogDataTest).build();
        when(mFrameContext.getFrame()).thenReturn(frame);
        mFrameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, mPietSharedStates);

        assertThat(mCallbackBound).isTrue();
    }

    @Test
    public void testUnbindModel_callsOnBindLogDataCallback() {
        Frame frame =
                Frame.newBuilder().addContents(DEFAULT_CONTENT).setLogData(mLogDataTest).build();
        when(mFrameContext.getFrame()).thenReturn(frame);
        mFrameAdapter.bindModel(frame, FRAME_WIDTH, (ShardingControl) null, mPietSharedStates);

        when(mFrameContext.getActionHandler()).thenReturn(mActionHandler);
        mFrameAdapter.unbindModel();
        assertThat(mCallbackUnbound).isTrue();
    }
}
