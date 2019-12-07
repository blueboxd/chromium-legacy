// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static org.chromium.chrome.browser.feed.library.common.Validators.checkNotNull;
import static org.chromium.chrome.browser.feed.library.common.Validators.checkState;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingContext;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.PietSharedState;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Stylesheet;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Template;

import java.util.List;

/**
 * Methods to enable creation and binding of Templates.
 *
 * <p>This class creates and binds adapters for templates allowing for reuse of full template
 * layouts as a unit. Release and recycling is handled by the ElementAdapterFactory.
 */
class TemplateBinder {
    private final KeyedRecyclerPool<ElementAdapter<? extends View, ?>> mTemplateRecyclerPool;
    private final ElementAdapterFactory mAdapterFactory;

    TemplateBinder(KeyedRecyclerPool<ElementAdapter<? extends View, ?>> mTemplateRecyclerPool,
            ElementAdapterFactory adapterFactory) {
        this.mTemplateRecyclerPool = mTemplateRecyclerPool;
        this.mAdapterFactory = adapterFactory;
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
        ElementAdapter<? extends View, ?> adapter = mTemplateRecyclerPool.get(templateKey);

        if (adapter == null) {
            // Create new adapter
            adapter = mAdapterFactory.createAdapterForElement(
                    model.getTemplate().getElement(), templateContext);

            adapter.setKey(templateKey);
        }

        adapter.bindModel(model.getTemplate().getElement(), templateContext);

        return adapter;
    }

    ElementAdapter<? extends View, ?> createTemplateAdapter(
            TemplateAdapterModel model, FrameContext frameContext) {
        TemplateKey templateKey = makeTemplateKey(model, frameContext);
        ElementAdapter<? extends View, ?> adapter = mTemplateRecyclerPool.get(templateKey);

        if (adapter == null) {
            // Make new FrameContext here
            FrameContext templateContext = frameContext.createTemplateContext(
                    model.getTemplate(), model.getBindingContext());

            // Create new adapter
            adapter = mAdapterFactory.createAdapterForElement(
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
    static boolean templateEquals(@Nullable Template template1, @Nullable Template template2) {
        if (template1 == template2) {
            return true;
        } else if (template1 == null || template2 == null) {
            return false;
        }
        return template1.hashCode() == template2.hashCode();
    }

    /** Wrap the Template proto object as the recycler key. */
    static class TemplateKey extends RecyclerKey {
        private final Template mTemplate;
        @Nullable
        private final List<PietSharedState> mPietSharedStates;

        // If the Template references Stylesheets that have MediaQueryConditions on them, they need
        // to be part of the key.
        private final List<Stylesheet> mMediaQueryBasedStylesheets;

        TemplateKey(Template template, @Nullable List<PietSharedState> pietSharedStates,
                List<Stylesheet> mediaQueryBasedStylesheets) {
            this.mTemplate = template;
            this.mPietSharedStates = pietSharedStates;
            this.mMediaQueryBasedStylesheets = mediaQueryBasedStylesheets;
        }

        /** Equals checks the hashCode of the component fields to avoid expensive proto equals. */
        @SuppressWarnings({"ReferenceEquality", "EqualsUsingHashCode"})
        @Override
        public boolean equals(@Nullable Object o) {
            if (this == o) {
                return true;
            }
            if (!(o instanceof TemplateKey)) {
                return false;
            }

            TemplateKey that = (TemplateKey) o;

            // Check that Templates are equal
            if (!templateEquals(mTemplate, that.mTemplate)) {
                return false;
            }
            // Check that PietSharedStates are equal or both null
            if (that.mPietSharedStates == null ^ this.mPietSharedStates == null) {
                return false;
            }
            if (this.mPietSharedStates != null && that.mPietSharedStates != null
                    && (this.mPietSharedStates.size() != that.mPietSharedStates.size()
                            || this.mPietSharedStates.hashCode()
                                    != that.mPietSharedStates.hashCode())) {
                return false;
            }
            // Check that stylesheets are equal or both empty
            if (that.mMediaQueryBasedStylesheets.isEmpty()
                    ^ this.mMediaQueryBasedStylesheets.isEmpty()) {
                return false;
            }
            return this.mMediaQueryBasedStylesheets == that.mMediaQueryBasedStylesheets
                    || this.mMediaQueryBasedStylesheets.hashCode()
                    == that.mMediaQueryBasedStylesheets.hashCode();
        }

        @Override
        public int hashCode() {
            int result = mTemplate.hashCode();
            result = 31 * result + (mPietSharedStates != null ? mPietSharedStates.hashCode() : 0);
            result = 31 * result
                    + (mMediaQueryBasedStylesheets != null ? mMediaQueryBasedStylesheets.hashCode()
                                                           : 0);
            return result;
        }
    }

    static class TemplateAdapterModel {
        private final Template mTemplate;
        private final BindingContext mBindingContext;

        TemplateAdapterModel(Template template, BindingContext bindingContext) {
            this.mTemplate = template;
            this.mBindingContext = bindingContext;
        }

        TemplateAdapterModel(
                String templateId, FrameContext frameContext, BindingContext bindingContext) {
            this.mTemplate = checkNotNull(
                    frameContext.getTemplate(templateId), "Template was not found: %s", templateId);
            this.mBindingContext = bindingContext;
        }

        public Template getTemplate() {
            return mTemplate;
        }

        BindingContext getBindingContext() {
            return mBindingContext;
        }

        @SuppressWarnings({"ReferenceEquality", "EqualsUsingHashCode"})
        @Override
        public boolean equals(@Nullable Object o) {
            if (this == o) {
                return true;
            }
            if (!(o instanceof TemplateAdapterModel)) {
                return false;
            }

            TemplateAdapterModel that = (TemplateAdapterModel) o;

            return templateEquals(mTemplate, that.mTemplate)
                    && (mBindingContext == that.mBindingContext
                            || (mBindingContext.getBindingValuesCount()
                                            == that.mBindingContext.getBindingValuesCount()
                                    && mBindingContext.hashCode()
                                            == that.mBindingContext.hashCode()));
        }

        @Override
        public int hashCode() {
            int result = mTemplate.hashCode();
            result = 31 * result + mBindingContext.hashCode();
            return result;
        }
    }
}
