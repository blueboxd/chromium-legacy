// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode.ERR_BINDING_VALUE_TYPE_MISMATCH;
import static org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode.ERR_DUPLICATE_BINDING_VALUE;
import static org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode.ERR_DUPLICATE_STYLE;
import static org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode.ERR_DUPLICATE_TEMPLATE;
import static org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode.ERR_MISSING_BINDING_VALUE;
import static org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode.ERR_MISSING_TEMPLATE;

import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feed.library.api.host.config.DebugBehavior;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.piet.DebugLogger.MessageType;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler;
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
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.GridCellWidth;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Visibility;
import org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.Image;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.ImageSource;
import org.chromium.components.feed.core.proto.ui.piet.LogDataProto.LogData;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Frame;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.PietSharedState;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Stylesheet;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Template;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.BoundStyle;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Style;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.StyleIdsStack;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * This is a {@link FrameContext} that is bound. Binding means we have a {@code Frame}, an optional
 * map of {@code BindingValues} and a {@code StyleProvider}.
 */
class FrameContext {
    private static final String TAG = "FrameContext";

    @VisibleForTesting
    final PietStylesHelper mStyleshelper;
    private final List<PietSharedState> mPietSharedStates;
    private final DebugBehavior mDebugBehavior;
    private final DebugLogger mDebugLogger;
    private final ActionHandler mActionHandler;
    private final HostProviders mHostProviders;
    private final HostBindingProvider mHostBindingProvider;
    private final Map<String, Template> mTemplates;

    // List of stylesheets for each Template that could be affected by media queries.
    private final Map<Template, List<Stylesheet>> mTemplateMediaQueryStylesheets;

    // This is the Frame which contains all the slices being processed.
    private final Frame mCurrentFrame;

    // The Current Stylesheet as a map from style_id to Style.
    private final Map<String, Style> mStylesheet;

    // The in-scope bindings as a map from state_id to binding value
    private final Map<String, BindingValue> mBindingValues;

    // The base / default style; set to default instance for Frame or childDefaultStyleId for
    // Template
    private final Style mBaseStyle;

    // The root view of this frame.
    private final View mFrameView;

    /** Initialize a FrameContext for the first time from a top-level Frame. */
    @VisibleForTesting
    FrameContext(Frame frame, Map<String, Style> stylesheet, List<PietSharedState> pietSharedStates,
            PietStylesHelper pietStylesHelper, DebugBehavior debugBehavior, DebugLogger debugLogger,
            ActionHandler actionHandler, HostProviders hostProviders, View frameView) {
        this(frame, stylesheet, new ThrowingEmptyMap(), pietSharedStates, pietStylesHelper,
                debugBehavior, debugLogger, actionHandler, hostProviders,
                new NoKeyOverwriteHashMap<>("Template", ERR_DUPLICATE_TEMPLATE), frameView);

        mStyleshelper.addSharedStateTemplatesToFrame(mTemplates);
        if (frame.getTemplatesCount() > 0) {
            for (Template template : frame.getTemplatesList()) {
                if (pietStylesHelper.areMediaQueriesMet(template.getConditionsList())) {
                    mTemplates.put(template.getTemplateId(), template);
                }
            }
        }
    }

    /** Set up a new FrameContext; typically called to copy an existing FrameContext. */
    FrameContext(Frame frame, Map<String, Style> stylesheet,
            Map<String, BindingValue> bindingValues, List<PietSharedState> pietSharedStates,
            PietStylesHelper pietStylesHelper, DebugBehavior debugBehavior, DebugLogger debugLogger,
            ActionHandler actionHandler, HostProviders hostProviders,
            Map<String, Template> templates, View frameView) {
        mCurrentFrame = frame;
        this.mStylesheet = stylesheet;
        this.mBindingValues = bindingValues;
        this.mBaseStyle = Style.getDefaultInstance();
        this.mStyleshelper = pietStylesHelper;
        this.mPietSharedStates = pietSharedStates;
        this.mDebugBehavior = debugBehavior;
        this.mDebugLogger = debugLogger;
        this.mActionHandler = actionHandler;
        this.mHostProviders = hostProviders;
        this.mHostBindingProvider = hostProviders.getHostBindingProvider();
        this.mTemplates = templates;
        this.mFrameView = frameView;
        this.mTemplateMediaQueryStylesheets = new HashMap<>();
    }

    /**
     * Creates a {@link FrameContext} and constructs the stylesheets from the frame and the list of
     * {@link PietSharedState}. Any errors found with the styling will be reported.
     */
    public static FrameContext createFrameContext(Frame frame,
            List<PietSharedState> pietSharedStates, PietStylesHelper pietStylesHelper,
            DebugBehavior debugBehavior, DebugLogger debugLogger, ActionHandler actionHandler,
            HostProviders hostProviders, View frameView) {
        NoKeyOverwriteHashMap<String, Style> styleMap = null;
        if (frame.hasStylesheets()) {
            styleMap = pietStylesHelper.getStylesheetMap(frame.getStylesheets(), debugLogger);
        }

        if (styleMap == null) {
            styleMap = new NoKeyOverwriteHashMap<>("Style", ERR_DUPLICATE_STYLE);
        }
        return new FrameContext(frame, styleMap, pietSharedStates, pietStylesHelper, debugBehavior,
                debugLogger, actionHandler, hostProviders, frameView);
    }

    /** Return any of the template's stylesheets that could be affected by a MediaQuery. */
    List<Stylesheet> getMediaQueryStylesheets(Template template) {
        if (mTemplateMediaQueryStylesheets.containsKey(template)) {
            return mTemplateMediaQueryStylesheets.get(template);
        }

        ArrayList<Stylesheet> mediaQueryStylesheets = new ArrayList<>();

        for (Stylesheet stylesheet : template.getStylesheets().getStylesheetsList()) {
            if (stylesheet.getConditionsCount() > 0) {
                mediaQueryStylesheets.add(stylesheet);
            }
        }
        for (String stylesheetId : template.getStylesheets().getStylesheetIdsList()) {
            Stylesheet stylesheet = mStyleshelper.getStylesheet(stylesheetId);
            if (stylesheet != null && stylesheet.getConditionsCount() > 0) {
                mediaQueryStylesheets.add(stylesheet);
            }
        }

        mediaQueryStylesheets.trimToSize();
        List<Stylesheet> mediaQueryStylesheetsImmutable =
                Collections.unmodifiableList(mediaQueryStylesheets);

        mTemplateMediaQueryStylesheets.put(template, mediaQueryStylesheetsImmutable);

        return mediaQueryStylesheetsImmutable;
    }

    /**
     * Creates a new FrameContext which is scoped properly for the Template. The Frame's stylesheet
     * is discarded and replaced by the Template's stylesheet. In addition, a Template can define a
     * root level style which applies to all child elements.
     */
    FrameContext createTemplateContext(Template template, BindingContext bindingContext) {
        Map<String, BindingValue> bindingValues = createBindingValueMap(bindingContext);
        Map<String, Style> localStylesheet =
                mStyleshelper.getStylesheetMap(template.getStylesheets(), getDebugLogger());

        return new FrameContext(mCurrentFrame, localStylesheet, bindingValues, mPietSharedStates,
                mStyleshelper, mDebugBehavior, mDebugLogger, mActionHandler, mHostProviders,
                mTemplates, mFrameView);
    }

    public DebugBehavior getDebugBehavior() {
        return mDebugBehavior;
    }

    public DebugLogger getDebugLogger() {
        return mDebugLogger;
    }

    public Frame getFrame() {
        return mCurrentFrame;
    }

    public ActionHandler getActionHandler() {
        return mActionHandler;
    }

    public Template getTemplate(String templateId) {
        Template template = mTemplates.get(templateId);
        if (template == null) {
            throw new PietFatalException(ERR_MISSING_TEMPLATE,
                    reportMessage(MessageType.ERROR, ERR_MISSING_TEMPLATE,
                            String.format("Template '%s' not found", templateId)));
        }
        return template;
    }

    public List<PietSharedState> getPietSharedStates() {
        return mPietSharedStates;
    }

    /** Return a {@link StyleProvider} for the style. */
    public StyleProvider makeStyleFor(StyleIdsStack styles) {
        return new StyleProvider(
                PietStylesHelper.mergeStyleIdsStack(mBaseStyle, styles, mStylesheet, this),
                mHostProviders.getAssetProvider());
    }

    /**
     * Return a {@link GridCellWidth} for the binding if there is one defined; otherwise returns
     * {@code null}.
     */
    @Nullable
    GridCellWidth getGridCellWidthFromBinding(GridCellWidthBindingRef binding) {
        BindingValue bindingValue = mBindingValues.get(binding.getBindingId());
        // Purposefully check for host binding and overwrite here as we want to perform the
        // hasCellWidth checks on host binding.   This allows the host to act more like the server.
        if (bindingValue != null && bindingValue.hasHostBindingData()) {
            bindingValue = mHostBindingProvider.getGridCellWidthBindingForValue(bindingValue);
        }
        return bindingValue == null || !bindingValue.hasCellWidth() ? null
                                                                    : bindingValue.getCellWidth();
    }

    /**
     * Return an {@link Actions} for the binding if there is one defined; otherwise returns the
     * default instance.
     */
    Actions getActionsFromBinding(ActionsBindingRef binding) {
        BindingValue bindingValue = mBindingValues.get(binding.getBindingId());
        if (bindingValue != null && bindingValue.hasHostBindingData()) {
            bindingValue = mHostBindingProvider.getActionsBindingForValue(bindingValue);
        }
        if (bindingValue == null) {
            return Actions.getDefaultInstance();
        }
        if (!bindingValue.hasActions()) {
            Logger.w(TAG,
                    reportMessage(MessageType.WARNING, ERR_BINDING_VALUE_TYPE_MISMATCH,
                            String.format(
                                    "No actions found for binding %s", binding.getBindingId())));
        }
        return bindingValue.getActions();
    }

    /**
     * Return a {@link BoundStyle} for the binding if there is one defined; otherwise returns the
     * default instance.
     */
    BoundStyle getStyleFromBinding(StyleBindingRef binding) {
        return getStyleFromBinding(binding, mBindingValues);
    }

    /**
     * Return a {@link BoundStyle} for the binding if there is one defined; otherwise returns the
     * default instance.
     */
    BoundStyle getStyleFromBinding(
            StyleBindingRef binding, Map<String, BindingValue> bindingValues) {
        BindingValue bindingValue = bindingValues.get(binding.getBindingId());
        if (bindingValue != null && bindingValue.hasHostBindingData()) {
            bindingValue = mHostBindingProvider.getStyleBindingForValue(bindingValue);
        }
        if (bindingValue == null) {
            return BoundStyle.getDefaultInstance();
        }
        if (!bindingValue.hasBoundStyle()) {
            Logger.w(TAG,
                    reportMessage(MessageType.WARNING, ERR_BINDING_VALUE_TYPE_MISMATCH,
                            String.format(
                                    "No style found for binding %s", binding.getBindingId())));
        }
        return bindingValue.getBoundStyle();
    }

    /** Returns the {@link BindingValue} for the BindingRef; otherwise returns null. */
    @Nullable
    Visibility getVisibilityFromBinding(VisibilityBindingRef binding) {
        BindingValue bindingValue = mBindingValues.get(binding.getBindingId());
        if (bindingValue != null && bindingValue.hasHostBindingData()) {
            bindingValue = mHostBindingProvider.getVisibilityBindingForValue(bindingValue);
        }
        if (bindingValue == null) {
            return null;
        } else if (!bindingValue.hasVisibility()) {
            Logger.w(TAG,
                    reportMessage(MessageType.WARNING, ERR_BINDING_VALUE_TYPE_MISMATCH,
                            String.format(
                                    "No visibility found for binding %s", binding.getBindingId())));
            return null;
        } else {
            return bindingValue.getVisibility();
        }
    }

    Image filterImageSourcesByMediaQueryCondition(Image image) {
        Image.Builder imageBuilder = image.toBuilder().clearSources();
        for (ImageSource source : image.getSourcesList()) {
            if (mStyleshelper.areMediaQueriesMet(source.getConditionsList())) {
                imageBuilder.addSources(source);
            }
        }
        return imageBuilder.build();
    }

    /**
     * Returns the {@link BindingValue} for the BindingRef; throws IllegalStateException if binding
     * id does not point to a template resource.
     */
    BindingValue getTemplateInvocationBindingValue(TemplateBindingRef binding) {
        BindingValue bindingValue = getBindingValue(binding.getBindingId());
        if (bindingValue.hasHostBindingData()) {
            bindingValue = mHostBindingProvider.getTemplateBindingForValue(bindingValue);
        }
        if (!bindingValue.hasTemplateInvocation() && !binding.getIsOptional()) {
            throw new PietFatalException(ERR_MISSING_BINDING_VALUE,
                    reportMessage(MessageType.ERROR, ERR_MISSING_BINDING_VALUE,
                            String.format(
                                    "Template binding not found for %s", binding.getBindingId())));
        } else {
            return bindingValue;
        }
    }

    /**
     * Returns the {@link BindingValue} for the BindingRef; throws IllegalStateException if binding
     * id does not point to a custom element resource.
     */
    BindingValue getCustomElementBindingValue(CustomBindingRef binding) {
        BindingValue bindingValue = getBindingValue(binding.getBindingId());
        if (bindingValue.hasHostBindingData()) {
            bindingValue = mHostBindingProvider.getCustomElementDataBindingForValue(bindingValue);
        }
        if (!bindingValue.hasCustomElementData() && !binding.getIsOptional()) {
            throw new PietFatalException(ERR_MISSING_BINDING_VALUE,
                    reportMessage(MessageType.ERROR, ERR_MISSING_BINDING_VALUE,
                            String.format("Custom element binding not found for %s",
                                    binding.getBindingId())));
        } else {
            return bindingValue;
        }
    }

    /**
     * Returns the {@link BindingValue} for the BindingRef; throws IllegalStateException if binding
     * id does not point to a chunked text resource.
     */
    BindingValue getChunkedTextBindingValue(ChunkedTextBindingRef binding) {
        BindingValue bindingValue = getBindingValue(binding.getBindingId());
        if (bindingValue.hasHostBindingData()) {
            bindingValue = mHostBindingProvider.getChunkedTextBindingForValue(bindingValue);
        }
        if (!bindingValue.hasChunkedText() && !binding.getIsOptional()) {
            throw new PietFatalException(ERR_MISSING_BINDING_VALUE,
                    reportMessage(MessageType.ERROR, ERR_MISSING_BINDING_VALUE,
                            String.format("Chunked text binding not found for %s",
                                    binding.getBindingId())));
        } else {
            return bindingValue;
        }
    }

    /**
     * Returns the {@link BindingValue} for the BindingRef; throws IllegalStateException if binding
     * id does not point to a parameterized text resource.
     */
    BindingValue getParameterizedTextBindingValue(ParameterizedTextBindingRef binding) {
        BindingValue bindingValue = getBindingValue(binding.getBindingId());
        if (bindingValue.hasHostBindingData()) {
            bindingValue = mHostBindingProvider.getParameterizedTextBindingForValue(bindingValue);
        }
        if (!bindingValue.hasParameterizedText() && !binding.getIsOptional()) {
            throw new PietFatalException(ERR_MISSING_BINDING_VALUE,
                    reportMessage(MessageType.ERROR, ERR_MISSING_BINDING_VALUE,
                            String.format("Parameterized text binding not found for %s",
                                    binding.getBindingId())));
        } else {
            return bindingValue;
        }
    }

    /**
     * Returns the {@link BindingValue} for the BindingRef; throws IllegalStateException if binding
     * id does not point to a image resource.
     */
    BindingValue getImageBindingValue(ImageBindingRef binding) {
        BindingValue bindingValue = getBindingValue(binding.getBindingId());
        if (bindingValue.hasHostBindingData()) {
            bindingValue = mHostBindingProvider.getImageBindingForValue(bindingValue);
        }
        if (!bindingValue.hasImage() && !binding.getIsOptional()) {
            throw new PietFatalException(ERR_MISSING_BINDING_VALUE,
                    reportMessage(MessageType.ERROR, ERR_MISSING_BINDING_VALUE,
                            String.format(
                                    "Image binding not found for %s", binding.getBindingId())));
        } else {
            return bindingValue;
        }
    }

    /**
     * Returns the {@link BindingValue} for the BindingRef; throws IllegalStateException if binding
     * id does not point to an Element.
     */
    BindingValue getElementBindingValue(ElementBindingRef binding) {
        BindingValue bindingValue = getBindingValue(binding.getBindingId());
        if (bindingValue.hasHostBindingData()) {
            bindingValue = mHostBindingProvider.getElementBindingForValue(bindingValue);
        }
        if (!bindingValue.hasElement() && !binding.getIsOptional()) {
            throw new PietFatalException(ERR_MISSING_BINDING_VALUE,
                    reportMessage(MessageType.ERROR, ERR_MISSING_BINDING_VALUE,
                            String.format(
                                    "Element binding not found for %s", binding.getBindingId())));
        } else {
            return bindingValue;
        }
    }

    /**
     * Return an {@link LogData} for the binding if there is one defined; otherwise returns the
     * default instance.
     */
    @Nullable
    LogData getLogDataFromBinding(LogDataBindingRef binding) {
        BindingValue bindingValue = mBindingValues.get(binding.getBindingId());
        if (bindingValue != null && bindingValue.hasHostBindingData()) {
            bindingValue = mHostBindingProvider.getLogDataBindingForValue(bindingValue);
        }
        if (bindingValue == null) {
            return null;
        }
        if (!bindingValue.hasLogData()) {
            Logger.w(TAG,
                    reportMessage(MessageType.WARNING, ERR_BINDING_VALUE_TYPE_MISMATCH,
                            String.format(
                                    "No logData found for binding %s", binding.getBindingId())));
            return null;
        }
        return bindingValue.getLogData();
    }

    /** Returns the root view of this Frame. */
    View getFrameView() {
        return mFrameView;
    }

    private BindingValue getBindingValue(String bindingId) {
        BindingValue returnValue = mBindingValues.get(bindingId);
        if (returnValue == null) {
            return BindingValue.getDefaultInstance();
        }
        return returnValue;
    }

    /**
     * Report user errors in frames. This will return the fully formed error so it can be logged at
     * the site of the error.
     */
    public String reportMessage(@MessageType int messageType, String message) {
        String e = String.format("[%s] %s", mCurrentFrame.getTag(), message);
        mDebugLogger.recordMessage(messageType, e);
        return e;
    }

    /**
     * Report user errors in frames. This will return the fully formed error so it can be logged at
     * the site of the error.
     */
    public String reportMessage(@MessageType int messageType, ErrorCode errorCode, String message) {
        String e = String.format("[%s] %s", mCurrentFrame.getTag(), message);
        mDebugLogger.recordMessage(messageType, errorCode, e);
        return e;
    }

    private Map<String, BindingValue> createBindingValueMap(BindingContext bindingContext) {
        Map<String, BindingValue> bindingValueMap =
                new NoKeyOverwriteHashMap<>("BindingValue", ERR_DUPLICATE_BINDING_VALUE);
        for (BindingValue bindingValue : bindingContext.getBindingValuesList()) {
            if (bindingValue.hasBindingIdFromTranscludingTemplate()) {
                BindingValue parentBindingValue =
                        mBindingValues.get(bindingValue.getBindingIdFromTranscludingTemplate());
                if (parentBindingValue != null) {
                    BindingValue bindingValueForChild =
                            parentBindingValue.toBuilder()
                                    .setBindingId(bindingValue.getBindingId())
                                    .build();
                    bindingValueMap.put(bindingValue.getBindingId(), bindingValueForChild);
                }
            } else {
                bindingValueMap.put(bindingValue.getBindingId(), bindingValue);
            }
        }
        return bindingValueMap;
    }

    /** Map that throws whenever you try to look anything up in it. */
    private static class ThrowingEmptyMap extends HashMap<String, BindingValue> {
        @Override
        public BindingValue get(@Nullable Object key) {
            throw new PietFatalException(ErrorCode.ERR_MISSING_BINDING_VALUE,
                    "Looking up bindings not supported in this context; no BindingValues defined.");
        }
    }
}
