// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/custom/css_layout_definition.h"

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_iterator.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_fragment_result_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_layout_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_layout_fragment_request.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_no_argument_constructor.h"
#include "third_party/blink/renderer/core/css/cssom/prepopulated_computed_style_property_map.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/custom/custom_layout_constraints.h"
#include "third_party/blink/renderer/core/layout/custom/custom_layout_fragment.h"
#include "third_party/blink/renderer/core/layout/custom/fragment_result_options.h"
#include "third_party/blink/renderer/core/layout/custom/layout_custom.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"

namespace blink {

namespace {

bool IsLogicalHeightDefinite(const LayoutCustom& layout_custom) {
  if (layout_custom.HasOverrideLogicalHeight())
    return true;

  // In quirks mode the document and body element stretch to the viewport.
  if (layout_custom.StretchesToViewport())
    return true;

  if (layout_custom.HasDefiniteLogicalHeight())
    return true;

  return false;
}

}  // namespace

CSSLayoutDefinition::CSSLayoutDefinition(
    ScriptState* script_state,
    V8NoArgumentConstructor* constructor,
    V8Function* intrinsic_sizes,
    V8LayoutCallback* layout,
    const Vector<CSSPropertyID>& native_invalidation_properties,
    const Vector<AtomicString>& custom_invalidation_properties,
    const Vector<CSSPropertyID>& child_native_invalidation_properties,
    const Vector<AtomicString>& child_custom_invalidation_properties)
    : script_state_(script_state),
      constructor_(constructor),
      unused_intrinsic_sizes_(intrinsic_sizes),
      layout_(layout),
      native_invalidation_properties_(native_invalidation_properties),
      custom_invalidation_properties_(custom_invalidation_properties),
      child_native_invalidation_properties_(
          child_native_invalidation_properties),
      child_custom_invalidation_properties_(
          child_custom_invalidation_properties) {}

CSSLayoutDefinition::~CSSLayoutDefinition() = default;

CSSLayoutDefinition::Instance::Instance(CSSLayoutDefinition* definition,
                                        v8::Local<v8::Value> instance)
    : definition_(definition),
      instance_(definition->GetScriptState()->GetIsolate(), instance) {}

bool CSSLayoutDefinition::Instance::Layout(
    const LayoutCustom& layout_custom,
    FragmentResultOptions* fragment_result_options,
    scoped_refptr<SerializedScriptValue>* fragment_result_data) {
  ScriptState* script_state = definition_->GetScriptState();
  v8::Isolate* isolate = script_state->GetIsolate();

  if (!script_state->ContextIsValid())
    return false;

  ScriptState::Scope scope(script_state);

  // TODO(ikilpatrick): Determine if knowing the size of the array ahead of
  // time improves performance in any noticable way.
  HeapVector<Member<CustomLayoutChild>> children;
  for (LayoutBox* child = layout_custom.FirstChildBox(); child;
       child = child->NextSiblingBox()) {
    if (child->IsOutOfFlowPositioned())
      continue;

    CustomLayoutChild* layout_child = child->GetCustomLayoutChild();
    DCHECK(layout_child);
    children.push_back(layout_child);
  }

  // TODO(ikilpatrick): Fill in layout edges.
  ScriptValue edges(script_state, v8::Undefined(isolate));

  LayoutUnit fixed_block_size(-1);
  if (IsLogicalHeightDefinite(layout_custom)) {
    LayoutBox::LogicalExtentComputedValues computed_values;
    layout_custom.ComputeLogicalHeight(LayoutUnit(-1), LayoutUnit(),
                                       computed_values);
    fixed_block_size = computed_values.extent_;
  }

  // TODO(ikilpatrick): Fill in layout constraints.
  CustomLayoutConstraints* constraints =
      MakeGarbageCollected<CustomLayoutConstraints>(
          layout_custom.LogicalWidth(), fixed_block_size,
          layout_custom.GetConstraintData(), isolate);

  // TODO(ikilpatrick): Instead of creating a new style_map each time here,
  // store on LayoutCustom, and update when the style changes.
  StylePropertyMapReadOnly* style_map =
      MakeGarbageCollected<PrepopulatedComputedStylePropertyMap>(
          layout_custom.GetDocument(), layout_custom.StyleRef(),
          definition_->native_invalidation_properties_,
          definition_->custom_invalidation_properties_);

  ScriptValue return_value;
  if (!definition_->layout_
           ->Invoke(instance_.NewLocal(isolate), children, edges, constraints,
                    style_map)
           .To(&return_value)) {
    return false;
  }

  DCHECK(return_value.V8Value()->IsGeneratorObject());
  ScriptIterator iterator(return_value.V8Value().As<v8::Object>(), isolate);
  v8::Local<v8::Value> next_value;

  // We run the generator until it's exhausted.
  v8::Local<v8::Context> context = script_state->GetContext();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  ExceptionState exception_state(isolate, ExceptionState::kExecutionContext,
                                 "CSSLayoutAPI", "Layout");
  while (iterator.Next(execution_context, exception_state, next_value)) {
    if (exception_state.HadException()) {
      ReportException(&exception_state);
      return false;
    }

    // The value should already be non-empty, if not it should have be caught
    // by the exception_state.HadException() above.
    v8::Local<v8::Value> value = iterator.GetValue().ToLocalChecked();

    // Process a single fragment request.
    if (V8LayoutFragmentRequest::HasInstance(value, isolate)) {
      CustomLayoutFragmentRequest* fragment_request =
          V8LayoutFragmentRequest::ToImpl(v8::Local<v8::Object>::Cast(value));

      CustomLayoutFragment* fragment = fragment_request->PerformLayout(isolate);
      if (!fragment) {
        execution_context->AddConsoleMessage(ConsoleMessage::Create(
            mojom::ConsoleMessageSource::kJavaScript,
            mojom::ConsoleMessageLevel::kInfo,
            "Unable to perform layout request due to an invalid child, "
            "falling back to block layout."));
        return false;
      }

      next_value = ToV8(fragment, context->Global(), isolate);
      continue;
    }

    // Process multiple fragment requests.
    if (HasCallableIteratorSymbol(isolate, value, exception_state)) {
      HeapVector<Member<CustomLayoutFragmentRequest>> requests =
          NativeValueTraits<IDLSequence<CustomLayoutFragmentRequest>>::
              NativeValue(isolate, value, exception_state);
      if (exception_state.HadException()) {
        ReportException(&exception_state);
        return false;
      }

      v8::Local<v8::Array> results = v8::Array::New(isolate, requests.size());
      uint32_t index = 0;
      for (const auto& request : requests) {
        CustomLayoutFragment* fragment = request->PerformLayout(isolate);

        if (!fragment) {
          execution_context->AddConsoleMessage(ConsoleMessage::Create(
              mojom::ConsoleMessageSource::kJavaScript,
              mojom::ConsoleMessageLevel::kInfo,
              "Unable to perform layout request due to an invalid child, "
              "falling back to block layout."));
          return false;
        }

        bool success;
        if (!results
                 ->CreateDataProperty(
                     context, index++,
                     ToV8(fragment, context->Global(), isolate))
                 .To(&success) &&
            success)
          return false;
      }

      next_value = results;
      continue;
    }

    // We recieved something that wasn't either a CustomLayoutFragmentRequest,
    // or a sequence of CustomLayoutFragmentRequests. Fallback to block layout.
    execution_context->AddConsoleMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                               mojom::ConsoleMessageLevel::kInfo,
                               "Unable to parse the layout request, "
                               "falling back to block layout."));
    return false;
  }

  if (exception_state.HadException()) {
    ReportException(&exception_state);
    return false;
  }

  // The value should already be non-empty, if not it should have be caught by
  // the ReportException() above.
  v8::Local<v8::Value> result_value = iterator.GetValue().ToLocalChecked();

  // Attempt to convert the result.
  V8FragmentResultOptions::ToImpl(isolate, result_value,
                                  fragment_result_options, exception_state);

  if (exception_state.HadException()) {
    V8ScriptRunner::ReportException(isolate, exception_state.GetException());
    exception_state.ClearException();
    execution_context->AddConsoleMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                               mojom::ConsoleMessageLevel::kInfo,
                               "Unable to parse the layout function "
                               "result, falling back to block layout."));
    return false;
  }

  // Serialize any extra data provided by the web-developer to potentially pass
  // up to the parent custom layout.
  if (fragment_result_options->hasData()) {
    // We serialize "kForStorage" so that SharedArrayBuffers can't be shared
    // between LayoutWorkletGlobalScopes.
    *fragment_result_data = SerializedScriptValue::Serialize(
        isolate, fragment_result_options->data().V8Value(),
        SerializedScriptValue::SerializeOptions(
            SerializedScriptValue::kForStorage),
        exception_state);
  }

  if (exception_state.HadException()) {
    V8ScriptRunner::ReportException(isolate, exception_state.GetException());
    exception_state.ClearException();
    execution_context->AddConsoleMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                               mojom::ConsoleMessageLevel::kInfo,
                               "Unable to serialize the data provided in the "
                               "result, falling back to block layout."));
    return false;
  }

  return true;
}

void CSSLayoutDefinition::Instance::ReportException(
    ExceptionState* exception_state) {
  ScriptState* script_state = definition_->GetScriptState();
  v8::Isolate* isolate = script_state->GetIsolate();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  // We synchronously report and clear the exception as we may never enter V8
  // again (as the callbacks are invoked directly by the UA).
  V8ScriptRunner::ReportException(isolate, exception_state->GetException());
  exception_state->ClearException();
  execution_context->AddConsoleMessage(ConsoleMessage::Create(
      mojom::ConsoleMessageSource::kJavaScript,
      mojom::ConsoleMessageLevel::kInfo,
      "The layout function failed, falling back to block layout."));
}

CSSLayoutDefinition::Instance* CSSLayoutDefinition::CreateInstance() {
  if (constructor_has_failed_)
    return nullptr;

  // Ensure that we don't create an instance on a detached context.
  if (!GetScriptState()->ContextIsValid())
    return nullptr;

  ScriptState::Scope scope(GetScriptState());

  ScriptValue instance;
  if (!constructor_->Construct().To(&instance)) {
    constructor_has_failed_ = true;
    return nullptr;
  }

  return MakeGarbageCollected<Instance>(this, instance.V8Value());
}

void CSSLayoutDefinition::Instance::Trace(blink::Visitor* visitor) {
  visitor->Trace(definition_);
}

void CSSLayoutDefinition::Trace(Visitor* visitor) {
  visitor->Trace(constructor_);
  visitor->Trace(unused_intrinsic_sizes_);
  visitor->Trace(layout_);
  visitor->Trace(script_state_);
}

}  // namespace blink
