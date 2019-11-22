// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.android.libraries.feed.common.Validators.checkNotNull;

import android.support.annotation.VisibleForTesting;
import android.util.LruCache;

import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.piet.DebugLogger.MessageType;
import com.google.search.now.ui.piet.ErrorsProto.ErrorCode;
import com.google.search.now.ui.piet.MediaQueriesProto.MediaQueryCondition;
import com.google.search.now.ui.piet.PietProto.PietSharedState;
import com.google.search.now.ui.piet.PietProto.Stylesheet;
import com.google.search.now.ui.piet.PietProto.Stylesheets;
import com.google.search.now.ui.piet.PietProto.Template;
import com.google.search.now.ui.piet.StylesProto.BoundStyle;
import com.google.search.now.ui.piet.StylesProto.Style;
import com.google.search.now.ui.piet.StylesProto.StyleIdsStack;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/** This class provides support helpers methods for managing styles in Piet. */
public class PietStylesHelper {
    private static final String TAG = "PietStylesHelper";

    private final Map<Stylesheets, NoKeyOverwriteHashMap<String, Style>> multiStylesheetScopes =
            new NoKeyOverwriteHashMap<>("Style", ErrorCode.ERR_DUPLICATE_STYLESHEET);
    private final Map<String, Stylesheet> stylesheets =
            new NoKeyOverwriteHashMap<>("Stylesheet", ErrorCode.ERR_DUPLICATE_STYLESHEET);
    private final Map<String, Template> templates =
            new NoKeyOverwriteHashMap<>("Template", ErrorCode.ERR_DUPLICATE_TEMPLATE);

    private final MediaQueryHelper mediaQueryHelper;

    private PietStylesHelper(
            List<PietSharedState> pietSharedStates, MediaQueryHelper mediaQueryHelper) {
        this.mediaQueryHelper = mediaQueryHelper;
        for (PietSharedState sharedState : pietSharedStates) {
            if (sharedState.getStylesheetsCount() > 0) {
                for (Stylesheet stylesheet : sharedState.getStylesheetsList()) {
                    if (mediaQueryHelper.areMediaQueriesMet(stylesheet.getConditionsList())) {
                        stylesheets.put(stylesheet.getStylesheetId(), stylesheet);
                    }
                }
            }

            for (Template template : sharedState.getTemplatesList()) {
                if (mediaQueryHelper.areMediaQueriesMet(template.getConditionsList())) {
                    templates.put(template.getTemplateId(), template);
                }
            }
        }
    }

    boolean areMediaQueriesMet(List<MediaQueryCondition> conditions) {
        return mediaQueryHelper.areMediaQueriesMet(conditions);
    }

    /*@Nullable*/
    Stylesheet getStylesheet(String stylesheetId) {
        return stylesheets.get(stylesheetId);
    }

    /** Returns a Map of style_id to Style. This represents the Stylesheet. */
    NoKeyOverwriteHashMap<String, Style> getStylesheetMap(
            Stylesheets stylesheetRefs, DebugLogger debugLogger) {
        if (multiStylesheetScopes.containsKey(stylesheetRefs)) {
            return multiStylesheetScopes.get(stylesheetRefs);
        }

        NoKeyOverwriteHashMap<String, Style> styleMap =
                new NoKeyOverwriteHashMap<>("Style", ErrorCode.ERR_DUPLICATE_STYLE);

        // Add inline stylesheets.
        for (Stylesheet stylesheet : stylesheetRefs.getStylesheetsList()) {
            addStylesToMapIfMediaQueryConditionsMet(stylesheet, styleMap);
        }

        // Add stylesheets referenced from PietSharedState.
        for (String stylesheetId : stylesheetRefs.getStylesheetIdsList()) {
            Stylesheet stylesheet = stylesheets.get(stylesheetId);
            if (stylesheet == null) {
                String message = String.format(
                        "Stylesheet [%s] was not found in the PietSharedState", stylesheetId);
                debugLogger.recordMessage(
                        MessageType.WARNING, ErrorCode.ERR_MISSING_STYLESHEET, message);
                Logger.w(TAG, message);
                continue;
            }
            addStylesToMapIfMediaQueryConditionsMet(stylesheet, styleMap);
        }

        multiStylesheetScopes.put(stylesheetRefs, styleMap);

        return styleMap;
    }

    /** Adds all styles from the Stylesheet to the Map, subject to MediaQueryCondition filtering. */
    private void addStylesToMapIfMediaQueryConditionsMet(
            Stylesheet stylesheet, NoKeyOverwriteHashMap<String, Style> styleMap) {
        if (!areMediaQueriesMet(stylesheet.getConditionsList())) {
            return;
        }
        for (Style style : stylesheet.getStylesList()) {
            if (!areMediaQueriesMet(style.getConditionsList())) {
                continue;
            }
            styleMap.put(style.getStyleId(), style);
        }
    }

    /** Returns a {@link Template} for the template */
    /*@Nullable*/
    public Template getTemplate(String templateId) {
        return templates.get(templateId);
    }

    void addSharedStateTemplatesToFrame(Map<String, Template> frameTemplates) {
        frameTemplates.putAll(templates);
    }

    static Style mergeStyleIdsStack(StyleIdsStack stack, Map<String, Style> styleMap,
            /*@Nullable*/ FrameContext frameContext) {
        return mergeStyleIdsStack(Style.getDefaultInstance(), stack, styleMap, frameContext);
    }

    /**
     * Given a StyleIdsStack, a base style, and a styleMap that contains the styles definition,
     * returns a Style that is the proto-merge of all the styles in the stack, starting with the
     * base.
     */
    static Style mergeStyleIdsStack(Style baseStyle, StyleIdsStack stack,
            Map<String, Style> styleMap,
            /*@Nullable*/ FrameContext frameContext) {
        Style.Builder mergedStyle = baseStyle.toBuilder();
        for (String style : stack.getStyleIdsList()) {
            if (styleMap.containsKey(style)) {
                mergedStyle.mergeFrom(styleMap.get(style)).build();
            } else {
                String error = String.format(
                        "Unable to bind style [%s], style not found in Stylesheet", style);
                if (frameContext != null) {
                    frameContext.reportMessage(
                            MessageType.ERROR, ErrorCode.ERR_MISSING_STYLE, error);
                }
                Logger.w(TAG, error);
            }
        }
        if (stack.hasStyleBinding()) {
            // LINT.IfChange
            FrameContext localFrameContext = checkNotNull(
                    frameContext, "Binding styles not supported when frameContext is null");
            BoundStyle boundStyle = localFrameContext.getStyleFromBinding(stack.getStyleBinding());
            if (boundStyle.hasBackground()) {
                mergedStyle.setBackground(mergedStyle.getBackground().toBuilder().mergeFrom(
                        boundStyle.getBackground()));
            }
            if (boundStyle.hasColor()) {
                mergedStyle.setColor(boundStyle.getColor());
            }
            if (boundStyle.hasImageLoadingSettings()) {
                mergedStyle.setImageLoadingSettings(
                        mergedStyle.getImageLoadingSettings().toBuilder().mergeFrom(
                                boundStyle.getImageLoadingSettings()));
            }
            if (boundStyle.hasScaleType()) {
                mergedStyle.setScaleType(boundStyle.getScaleType());
            }
            // LINT.ThenChange
        }
        return mergedStyle.build();
    }

    @VisibleForTesting
    static class PietStylesHelperKey {
        private final List<PietSharedState> pietSharedStates;
        private final MediaQueryHelper mediaQueryHelper;

        PietStylesHelperKey(
                List<PietSharedState> pietSharedStates, MediaQueryHelper mediaQueryHelper) {
            this.pietSharedStates = pietSharedStates;
            this.mediaQueryHelper = mediaQueryHelper;
        }

        PietStylesHelper newPietStylesHelper() {
            return new PietStylesHelper(pietSharedStates, mediaQueryHelper);
        }

        @SuppressWarnings({"ReferenceEquality", "EqualsUsingHashCode"})
        @Override
        public boolean equals(/*@Nullable*/ Object o) {
            if (this == o) {
                return true;
            }
            if (!(o instanceof PietStylesHelperKey)) {
                return false;
            }
            PietStylesHelperKey that = (PietStylesHelperKey) o;

            if (!mediaQueryHelper.equals(that.mediaQueryHelper)
                    || pietSharedStates.size() != that.pietSharedStates.size()) {
                return false;
            }
            // Compare the two lists of PietSharedState in an container-independent way.
            // Add everything from "this" list to a list, then remove everything from "that" list,
            // and ensure the list is empty. O(N^2) will become a problem with large numbers of
            // shared states, but we expect this to usually be less than ~10. Comparing .equals() on
            // PietSharedState protos is very expensive - shortcut with hashcode.
            ArrayList<Integer> sharedStateHashCodes = new ArrayList<>();
            for (PietSharedState sharedState : pietSharedStates) {
                sharedStateHashCodes.add(sharedState.hashCode());
            }
            for (PietSharedState sharedState : that.pietSharedStates) {
                if (!sharedStateHashCodes.remove((Integer) sharedState.hashCode())) {
                    return false;
                }
            }
            return sharedStateHashCodes.isEmpty();
        }

        @Override
        public int hashCode() {
            int result = pietSharedStates.hashCode();
            result = 31 * result + mediaQueryHelper.hashCode();
            return result;
        }
    }

    /** Class that creates and caches PietStylesHelpers. */
    static class PietStylesHelperFactory {
        // TODO: Make this size configurable.
        private static final int DEFAULT_STYLES_HELPER_POOL_SIZE = 8;

        final LruCache<PietStylesHelperKey, PietStylesHelper> stylesHelpers;

        PietStylesHelperFactory() {
            this.stylesHelpers = new LruCache<PietStylesHelperKey, PietStylesHelper>(
                    DEFAULT_STYLES_HELPER_POOL_SIZE) {
                @Override
                protected PietStylesHelper create(PietStylesHelperKey key) {
                    return key.newPietStylesHelper();
                }
            };
        }

        PietStylesHelper get(
                List<PietSharedState> pietSharedStates, MediaQueryHelper mediaQueryHelper) {
            return checkNotNull(
                    stylesHelpers.get(new PietStylesHelperKey(pietSharedStates, mediaQueryHelper)));
        }

        void purge() {
            stylesHelpers.evictAll();
        }
    }
}
