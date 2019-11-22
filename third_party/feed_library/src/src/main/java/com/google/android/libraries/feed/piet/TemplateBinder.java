// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.android.libraries.feed.common.Validators.checkNotNull;
import static com.google.android.libraries.feed.common.Validators.checkState;

import android.view.View;

import com.google.search.now.ui.piet.ElementsProto.BindingContext;
import com.google.search.now.ui.piet.PietProto.PietSharedState;
import com.google.search.now.ui.piet.PietProto.Stylesheet;
import com.google.search.now.ui.piet.PietProto.Template;

import java.util.List;

/**
 * Methods to enable creation and binding of Templates.
 *
 * <p>This class creates and binds adapters for templates allowing for reuse of full template
 * layouts as a unit. Release and recycling is handled by the ElementAdapterFactory.
 */
class TemplateBinder {
    private final KeyedRecyclerPool<ElementAdapter<? extends View, ?>> templateRecyclerPool;
    private final ElementAdapterFactory adapterFactory;

    TemplateBinder(KeyedRecyclerPool<ElementAdapter<? extends View, ?>> templateRecyclerPool,
            ElementAdapterFactory adapterFactory) {
        this.templateRecyclerPool = templateRecyclerPool;
        this.adapterFactory = adapterFactory;
    }

    /** Higher-performance method that creates and binds an adapter for a template all at once. */
    ElementAdapter<? extends View, ?> createAndBindTemplateAdapter(
            TemplateAdapterModel model, FrameContext frameContext) {
        // Create the template context for use in both create and bind
        // This is a modestly expensive operation, and calling createTemplateAdapter followed by
        // bindTemplateAdapter would do it twice.
        FrameContext templateContext =
                frameContext.createTemplateContext(model.getTemplate(), model.getBindingContext());

        TemplateKey templateKey = makeTemplateKey(model, frameContext);
        ElementAdapter<? extends View, ?> adapter = templateRecyclerPool.get(templateKey);

        if (adapter == null) {
            // Create new adapter
            adapter = adapterFactory.createAdapterForElement(
                    model.getTemplate().getElement(), templateContext);

            adapter.setKey(templateKey);
        }

        adapter.bindModel(model.getTemplate().getElement(), templateContext);

        return adapter;
    }

    ElementAdapter<? extends View, ?> createTemplateAdapter(
            TemplateAdapterModel model, FrameContext frameContext) {
        TemplateKey templateKey = makeTemplateKey(model, frameContext);
        ElementAdapter<? extends View, ?> adapter = templateRecyclerPool.get(templateKey);

        if (adapter == null) {
            // Make new FrameContext here
            FrameContext templateContext = frameContext.createTemplateContext(
                    model.getTemplate(), model.getBindingContext());

            // Create new adapter
            adapter = adapterFactory.createAdapterForElement(
                    model.getTemplate().getElement(), templateContext);

            adapter.setKey(templateKey);
        }

        return adapter;
    }

    void bindTemplateAdapter(ElementAdapter<? extends View, ?> adapter, TemplateAdapterModel model,
            FrameContext frameContext) {
        TemplateKey templateKey = makeTemplateKey(model, frameContext);

        checkNotNull(adapter.getKey(), "Adapter key was null; not initialized correctly?");
        checkState(adapter.getKey() instanceof TemplateKey,
                "bindTemplateAdapter only applicable for template adapters");
        checkState(templateKey.equals(adapter.getKey()), "Template keys did not match");

        FrameContext templateContext =
                frameContext.createTemplateContext(model.getTemplate(), model.getBindingContext());

        adapter.bindModel(model.getTemplate().getElement(), templateContext);
    }

    private static TemplateKey makeTemplateKey(
            TemplateAdapterModel model, FrameContext frameContext) {
        return new TemplateKey(model.getTemplate(), frameContext.getPietSharedStates(),
                frameContext.getMediaQueryStylesheets(model.getTemplate()));
    }

    /**
     * Determines whether two templates are compatible for recycling. We're going to call the hash
     * code good enough for performance reasons (.equals() is expensive), and hope we don't get a
     * lot of collisions.
     */
    @SuppressWarnings({"ReferenceEquality", "EqualsUsingHashCode"})
    static boolean templateEquals(
            /*@Nullable*/ Template template1, /*@Nullable*/ Template template2) {
        if (template1 == template2) {
            return true;
        } else if (template1 == null || template2 == null) {
            return false;
        }
        return template1.hashCode() == template2.hashCode();
    }

    /** Wrap the Template proto object as the recycler key. */
    static class TemplateKey extends RecyclerKey {
        private final Template template;
        /*@Nullable*/ private final List<PietSharedState> pietSharedStates;

        // If the Template references Stylesheets that have MediaQueryConditions on them, they need
        // to be part of the key.
        private final List<Stylesheet> mediaQueryBasedStylesheets;

        TemplateKey(Template template,
                /*@Nullable*/ List<PietSharedState> pietSharedStates,
                List<Stylesheet> mediaQueryBasedStylesheets) {
            this.template = template;
            this.pietSharedStates = pietSharedStates;
            this.mediaQueryBasedStylesheets = mediaQueryBasedStylesheets;
        }

        /** Equals checks the hashCode of the component fields to avoid expensive proto equals. */
        @SuppressWarnings({"ReferenceEquality", "EqualsUsingHashCode"})
        @Override
        public boolean equals(/*@Nullable*/ Object o) {
            if (this == o) {
                return true;
            }
            if (!(o instanceof TemplateKey)) {
                return false;
            }

            TemplateKey that = (TemplateKey) o;

            // Check that Templates are equal
            if (!templateEquals(template, that.template)) {
                return false;
            }
            // Check that PietSharedStates are equal or both null
            if (that.pietSharedStates == null ^ this.pietSharedStates == null) {
                return false;
            }
            if (this.pietSharedStates != null && that.pietSharedStates != null
                    && (this.pietSharedStates.size() != that.pietSharedStates.size()
                            || this.pietSharedStates.hashCode()
                                    != that.pietSharedStates.hashCode())) {
                return false;
            }
            // Check that stylesheets are equal or both empty
            if (that.mediaQueryBasedStylesheets.isEmpty()
                    ^ this.mediaQueryBasedStylesheets.isEmpty()) {
                return false;
            }
            return this.mediaQueryBasedStylesheets == that.mediaQueryBasedStylesheets
                    || this.mediaQueryBasedStylesheets.hashCode()
                    == that.mediaQueryBasedStylesheets.hashCode();
        }

        @Override
        public int hashCode() {
            int result = template.hashCode();
            result = 31 * result + (pietSharedStates != null ? pietSharedStates.hashCode() : 0);
            result = 31 * result
                    + (mediaQueryBasedStylesheets != null ? mediaQueryBasedStylesheets.hashCode()
                                                          : 0);
            return result;
        }
    }

    static class TemplateAdapterModel {
        private final Template template;
        private final BindingContext bindingContext;

        TemplateAdapterModel(Template template, BindingContext bindingContext) {
            this.template = template;
            this.bindingContext = bindingContext;
        }

        TemplateAdapterModel(
                String templateId, FrameContext frameContext, BindingContext bindingContext) {
            this.template = checkNotNull(
                    frameContext.getTemplate(templateId), "Template was not found: %s", templateId);
            this.bindingContext = bindingContext;
        }

        public Template getTemplate() {
            return template;
        }

        BindingContext getBindingContext() {
            return bindingContext;
        }

        @SuppressWarnings({"ReferenceEquality", "EqualsUsingHashCode"})
        @Override
        public boolean equals(/*@Nullable*/ Object o) {
            if (this == o) {
                return true;
            }
            if (!(o instanceof TemplateAdapterModel)) {
                return false;
            }

            TemplateAdapterModel that = (TemplateAdapterModel) o;

            return templateEquals(template, that.template)
                    && (bindingContext == that.bindingContext
                            || (bindingContext.getBindingValuesCount()
                                            == that.bindingContext.getBindingValuesCount()
                                    && bindingContext.hashCode()
                                            == that.bindingContext.hashCode()));
        }

        @Override
        public int hashCode() {
            int result = template.hashCode();
            result = 31 * result + bindingContext.hashCode();
            return result;
        }
    }
}
