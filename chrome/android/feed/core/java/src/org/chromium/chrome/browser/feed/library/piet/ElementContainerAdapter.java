// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static org.chromium.chrome.browser.feed.library.common.Validators.checkState;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.browser.feed.library.piet.DebugLogger.MessageType;
import org.chromium.chrome.browser.feed.library.piet.TemplateBinder.TemplateAdapterModel;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingValue;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Content;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.TemplateInvocation;
import org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Template;

import java.util.ArrayList;
import java.util.List;

/**
 * Base class for adapters that act as containers for other adapters, such as ElementList and
 * GridRow. Ensures that lifecycle methods are called on child adapters when the parent class binds,
 * unbinds, or releases.
 */
abstract class ElementContainerAdapter<V extends ViewGroup, M> extends ElementAdapter<V, M> {
    final List<ElementAdapter<? extends View, ?>> mChildAdapters;
    int[] mAdaptersPerContent = new int[0];

    /** Cached reference to the factory for convenience. */
    private final ElementAdapterFactory mFactory;

    ElementContainerAdapter(
            Context context, AdapterParameters parameters, V view, RecyclerKey key) {
        super(context, parameters, view, key);
        mChildAdapters = new ArrayList<>();
        mFactory = parameters.mElementAdapterFactory;
    }

    ElementContainerAdapter(Context context, AdapterParameters parameters, V view) {
        super(context, parameters, view);
        mChildAdapters = new ArrayList<>();
        mFactory = parameters.mElementAdapterFactory;
    }

    @Override
    void onCreateAdapter(M model, Element baseElement, FrameContext frameContext) {
        createInlineChildAdapters(getContentsFromModel(model), frameContext);
    }

    abstract List<Content> getContentsFromModel(M model);

    @Override
    void onBindModel(M model, Element baseElement, FrameContext frameContext) {
        bindChildAdapters(getContentsFromModel(model), frameContext);
    }

    /** Unbind the model and release child adapters. Be sure to call this in any overrides. */
    @Override
    void onUnbindModel() {
        if (getRawModel() != null && !mChildAdapters.isEmpty()) {
            unbindChildAdapters(getContentsFromModel(getModel()));
        }

        super.onUnbindModel();
    }

    @Override
    void onReleaseAdapter() {
        V containerView = getBaseView();
        if (containerView != null) {
            containerView.removeAllViews();
        }
        for (ElementAdapter<?, ?> childAdapter : mChildAdapters) {
            mFactory.releaseAdapter(childAdapter);
        }
        mChildAdapters.clear();
    }

    /** Creates and adds adapters for all inline Content items. */
    private void createInlineChildAdapters(List<Content> contents, FrameContext frameContext) {
        mAdaptersPerContent = new int[contents.size()];

        checkState(mChildAdapters.isEmpty(),
                "Child adapters is not empty (has %s elements); release adapter before creating.",
                mChildAdapters.size());
        // Could also check that getBaseView has no children, but it may have children due to
        // alignment padding elements.

        for (int contentIndex = 0; contentIndex < contents.size(); contentIndex++) {
            Content content = contents.get(contentIndex);
            switch (content.getContentTypeCase()) {
                case ELEMENT:
                    mAdaptersPerContent[contentIndex] =
                            createAndAddElementAdapter(content.getElement(), frameContext);
                    break;
                case TEMPLATE_INVOCATION:
                    mAdaptersPerContent[contentIndex] = createAndAddTemplateAdapters(
                            content.getTemplateInvocation(), frameContext);
                    break;
                case BOUND_ELEMENT:
                case TEMPLATE_BINDING:
                    // Do nothing; create these adapters in the bindModel call.
                    mAdaptersPerContent[contentIndex] = 0;
                    continue;
                default:
                    throw new PietFatalException(ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                            frameContext.reportMessage(MessageType.ERROR,
                                    ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                                    String.format("Unhandled Content type: %s",
                                            content.getContentTypeCase())));
            }
        }
    }

    /**
     * Create an adapter for the element and add it to this container's layout.
     *
     * @return number of adapters created
     */
    private int createAndAddElementAdapter(Element element, FrameContext frameContext) {
        ElementAdapter<? extends View, ?> adapter =
                mFactory.createAdapterForElement(element, frameContext);
        addChildAdapter(adapter);
        getBaseView().addView(adapter.getView());
        return 1;
    }

    /**
     * Create adapters for the template invocation and add them to this container's layout.
     *
     * @return number of adapters created
     */
    private int createAndAddTemplateAdapters(
            TemplateInvocation templateInvocation, FrameContext frameContext) {
        Template template = frameContext.getTemplate(templateInvocation.getTemplateId());
        if (template == null) {
            return 0;
        }
        for (int templateIndex = 0; templateIndex < templateInvocation.getBindingContextsCount();
                templateIndex++) {
            TemplateAdapterModel templateModel = new TemplateAdapterModel(
                    template, templateInvocation.getBindingContexts(templateIndex));
            ElementAdapter<? extends View, ?> templateAdapter =
                    getParameters().mTemplateBinder.createTemplateAdapter(
                            templateModel, frameContext);
            addChildAdapter(templateAdapter);
            getBaseView().addView(templateAdapter.getView());
        }
        return templateInvocation.getBindingContextsCount();
    }

    /**
     * Binds all static adapters, and creates+binds+adds adapters for Content bindings.
     *
     * <p>There are a few inefficient O(N^2) mid-list inserts in this method, but since we're
     * dealing with views here, N should always be small (probably less than ~10).
     */
    private void bindChildAdapters(List<Content> contents, FrameContext frameContext) {
        checkState(mAdaptersPerContent.length == contents.size(),
                "Internal error in adapters per content (%s != %s). Adapter has not been created?",
                mAdaptersPerContent.length, contents.size());
        int adapterIndex = 0;
        int viewIndex = 0;
        for (int contentIndex = 0; contentIndex < contents.size(); contentIndex++) {
            Content content = contents.get(contentIndex);
            switch (content.getContentTypeCase()) {
                case ELEMENT:
                    // An Element generates exactly one adapter+view; bind it here.
                    mChildAdapters.get(adapterIndex).bindModel(content.getElement(), frameContext);
                    adapterIndex++;
                    viewIndex++;
                    break;
                case TEMPLATE_INVOCATION:
                    // Bind one TemplateInstanceAdapter for each BindingContext.
                    TemplateInvocation templateInvocation = content.getTemplateInvocation();
                    Template template =
                            frameContext.getTemplate(templateInvocation.getTemplateId());
                    if (template == null) {
                        continue;
                    }
                    for (int templateIndex = 0; templateIndex < mAdaptersPerContent[contentIndex];
                            templateIndex++) {
                        ElementAdapter<? extends View, ?> templateAdapter =
                                mChildAdapters.get(adapterIndex);
                        getParameters().mTemplateBinder.bindTemplateAdapter(templateAdapter,
                                new TemplateAdapterModel(template,
                                        templateInvocation.getBindingContexts(templateIndex)),
                                frameContext);
                        adapterIndex++;
                        viewIndex++;
                    }
                    break;
                case BOUND_ELEMENT:
                    // Look up the binding, then create, bind, and add a single adapter.
                    BindingValue elementBinding =
                            frameContext.getElementBindingValue(content.getBoundElement());
                    if (!elementBinding.hasElement()) {
                        continue;
                    }
                    Element element = elementBinding.getElement();
                    ElementAdapter<? extends View, ?> adapter =
                            mFactory.createAdapterForElement(element, frameContext);
                    adapter.bindModel(element, frameContext);
                    mChildAdapters.add(adapterIndex++, adapter);
                    getBaseView().addView(adapter.getView(), viewIndex++);
                    mAdaptersPerContent[contentIndex] = 1;
                    break;
                case TEMPLATE_BINDING:
                    // Look up the binding, then create, bind, and add template adapters.
                    BindingValue templateBindingValue =
                            frameContext.getTemplateInvocationBindingValue(
                                    content.getTemplateBinding());
                    if (!templateBindingValue.hasTemplateInvocation()) {
                        continue;
                    }
                    TemplateInvocation boundTemplateInvocation =
                            templateBindingValue.getTemplateInvocation();
                    Template boundTemplate =
                            frameContext.getTemplate(boundTemplateInvocation.getTemplateId());
                    if (boundTemplate == null) {
                        continue;
                    }
                    mAdaptersPerContent[contentIndex] =
                            boundTemplateInvocation.getBindingContextsCount();
                    for (int templateIndex = 0;
                            templateIndex < boundTemplateInvocation.getBindingContextsCount();
                            templateIndex++) {
                        TemplateAdapterModel templateModel = new TemplateAdapterModel(boundTemplate,
                                boundTemplateInvocation.getBindingContexts(templateIndex));
                        ElementAdapter<? extends View, ?> boundTemplateAdapter =
                                getParameters().mTemplateBinder.createAndBindTemplateAdapter(
                                        templateModel, frameContext);
                        mChildAdapters.add(adapterIndex++, boundTemplateAdapter);
                        getBaseView().addView(boundTemplateAdapter.getView(), viewIndex++);
                    }
                    break;
                default:
                    throw new PietFatalException(ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                            frameContext.reportMessage(MessageType.ERROR,
                                    ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                                    String.format("Unhandled Content type: %s",
                                            content.getContentTypeCase())));
            }
        }
    }

    /** Unbind all inline adapters, and destroy all bound adapters. */
    private void unbindChildAdapters(List<Content> contents) {
        int adapterIndex = 0;
        int viewIndex = 0;
        for (int contentIndex = 0; contentIndex < contents.size(); contentIndex++) {
            Content content = contents.get(contentIndex);
            switch (content.getContentTypeCase()) {
                case ELEMENT:
                case TEMPLATE_INVOCATION:
                    // For inline content, just unbind to allow re-binding in the future.
                    for (int i = 0; i < mAdaptersPerContent[contentIndex]; i++) {
                        mChildAdapters.get(adapterIndex).unbindModel();
                        adapterIndex++;
                        viewIndex++;
                    }
                    break;
                case BOUND_ELEMENT:
                case TEMPLATE_BINDING:
                    // For bound content, release, recycle, and remove adapters.
                    for (int i = 0; i < mAdaptersPerContent[contentIndex]; i++) {
                        mFactory.releaseAdapter(mChildAdapters.get(adapterIndex));
                        mChildAdapters.remove(adapterIndex);
                        getBaseView().removeViewAt(viewIndex);
                        // Don't increment adapterIndex or viewIndex because we removed this
                        // adapter/view.
                    }
                    mAdaptersPerContent[contentIndex] = 0;
                    break;
                default:
                    throw new PietFatalException(ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                            String.format(
                                    "Unhandled Content type: %s", content.getContentTypeCase()));
            }
        }
    }

    void addChildAdapter(ElementAdapter<? extends View, ?> adapter) {
        mChildAdapters.add(adapter);
    }

    @Override
    public void triggerViewActions(View viewport, FrameContext frameContext) {
        super.triggerViewActions(viewport, frameContext);
        for (ElementAdapter<?, ?> childAdapter : mChildAdapters) {
            childAdapter.triggerViewActions(viewport, frameContext);
        }
    }
}
