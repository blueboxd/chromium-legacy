// Copyright 2018 the Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/css/cssom/paint_worklet_style_property_map.h"

#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/cssom/computed_style_property_map.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_keyword_value.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_unit_value.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_unsupported_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unparsed_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unsupported_style_value.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

class PaintWorkletStylePropertyMapIterationSource final
    : public PairIterable<String, CSSStyleValueVector>::IterationSource {
 public:
  explicit PaintWorkletStylePropertyMapIterationSource(
      HeapVector<PaintWorkletStylePropertyMap::StylePropertyMapEntry> values)
      : index_(0), values_(values) {}

  bool Next(ScriptState*,
            String& key,
            CSSStyleValueVector& value,
            ExceptionState&) override {
    if (index_ >= values_.size())
      return false;

    const PaintWorkletStylePropertyMap::StylePropertyMapEntry& pair =
        values_.at(index_++);
    key = pair.first;
    value = pair.second;
    return true;
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(values_);
    PairIterable<String, CSSStyleValueVector>::IterationSource::Trace(visitor);
  }

 private:
  wtf_size_t index_;
  const HeapVector<PaintWorkletStylePropertyMap::StylePropertyMapEntry> values_;
};

bool BuildNativeValues(const ComputedStyle& style,
                       Node* styled_node,
                       const Vector<CSSPropertyID>& native_properties,
                       PaintWorkletStylePropertyMap::CrossThreadData& data) {
  DCHECK(IsMainThread());
  for (const auto& property_id : native_properties) {
    // Silently drop shorthand properties.
    DCHECK_NE(property_id, CSSPropertyID::kInvalid);
    DCHECK_NE(property_id, CSSPropertyID::kVariable);
    if (CSSProperty::Get(property_id).IsShorthand())
      continue;
    std::unique_ptr<CrossThreadStyleValue> value =
        CSSProperty::Get(property_id)
            .CrossThreadStyleValueFromComputedStyle(
                style, /* layout_object */ nullptr, styled_node,
                /* allow_visited_style */ false);
    if (value->GetType() == CrossThreadStyleValue::StyleValueType::kUnknownType)
      return false;
    String key = CSSProperty::Get(property_id).GetPropertyNameString();
    if (!key.IsSafeToSendToAnotherThread())
      key = key.IsolatedCopy();
    data.Set(key, std::move(value));
  }
  return true;
}

bool BuildCustomValues(const Document& document,
                       const ComputedStyle& style,
                       Node* styled_node,
                       const Vector<AtomicString>& custom_properties,
                       PaintWorkletStylePropertyMap::CrossThreadData& data) {
  DCHECK(IsMainThread());
  for (const auto& property_name : custom_properties) {
    CSSPropertyRef ref(property_name, document);
    std::unique_ptr<CrossThreadStyleValue> value =
        ref.GetProperty().CrossThreadStyleValueFromComputedStyle(
            style, /* layout_object */ nullptr, styled_node,
            /* allow_visited_style */ false);
    if (value->GetType() == CrossThreadStyleValue::StyleValueType::kUnknownType)
      return false;
    // Ensure that the String can be safely passed cross threads.
    String key = property_name.GetString();
    if (!key.IsSafeToSendToAnotherThread())
      key = key.IsolatedCopy();
    data.Set(key, std::move(value));
  }
  return true;
}

}  // namespace

// static
base::Optional<PaintWorkletStylePropertyMap::CrossThreadData>
PaintWorkletStylePropertyMap::BuildCrossThreadData(
    const Document& document,
    const ComputedStyle& style,
    Node* styled_node,
    const Vector<CSSPropertyID>& native_properties,
    const Vector<AtomicString>& custom_properties) {
  DCHECK(IsMainThread());
  PaintWorkletStylePropertyMap::CrossThreadData data;
  data.ReserveCapacityForSize(native_properties.size() +
                              custom_properties.size());
  if (!BuildNativeValues(style, styled_node, native_properties, data))
    return base::nullopt;
  if (!BuildCustomValues(document, style, styled_node, custom_properties, data))
    return base::nullopt;
  return data;
}

// static
PaintWorkletStylePropertyMap::CrossThreadData
PaintWorkletStylePropertyMap::CopyCrossThreadData(const CrossThreadData& data) {
  PaintWorkletStylePropertyMap::CrossThreadData copied_data;
  copied_data.ReserveCapacityForSize(data.size());
  for (auto& pair : data)
    copied_data.Set(pair.key.IsolatedCopy(), pair.value->IsolatedCopy());
  return copied_data;
}

// The |data| comes from PaintWorkletInput, where its string is already an
// isolated copy from the main thread string, so we don't need to make another
// isolated copy here.
PaintWorkletStylePropertyMap::PaintWorkletStylePropertyMap(CrossThreadData data)
    : data_(std::move(data)) {
  DCHECK(!IsMainThread());
}

CSSStyleValue* PaintWorkletStylePropertyMap::get(
    const ExecutionContext* execution_context,
    const String& property_name,
    ExceptionState& exception_state) const {
  CSSStyleValueVector all_values =
      getAll(execution_context, property_name, exception_state);
  return all_values.IsEmpty() ? nullptr : all_values[0];
}

CSSStyleValueVector PaintWorkletStylePropertyMap::getAll(
    const ExecutionContext* execution_context,
    const String& property_name,
    ExceptionState& exception_state) const {
  CSSPropertyID property_id = cssPropertyID(property_name);
  if (property_id == CSSPropertyID::kInvalid) {
    exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
    return CSSStyleValueVector();
  }

  DCHECK(isValidCSSPropertyID(property_id));

  CSSStyleValueVector values;
  auto value = data_.find(property_name);
  if (value == data_.end())
    return CSSStyleValueVector();
  values.push_back(value->value->ToCSSStyleValue());
  return values;
}

bool PaintWorkletStylePropertyMap::has(
    const ExecutionContext* execution_context,
    const String& property_name,
    ExceptionState& exception_state) const {
  return !getAll(execution_context, property_name, exception_state).IsEmpty();
}

unsigned PaintWorkletStylePropertyMap::size() const {
  return data_.size();
}

PaintWorkletStylePropertyMap::IterationSource*
PaintWorkletStylePropertyMap::StartIteration(ScriptState* script_state,
                                             ExceptionState& exception_state) {
  // TODO(xidachen): implement this function. Note that the output should be
  // sorted.
  HeapVector<PaintWorkletStylePropertyMap::StylePropertyMapEntry> result;
  return MakeGarbageCollected<PaintWorkletStylePropertyMapIterationSource>(
      result);
}

void PaintWorkletStylePropertyMap::Trace(blink::Visitor* visitor) {
  StylePropertyMapReadOnly::Trace(visitor);
}

}  // namespace blink
