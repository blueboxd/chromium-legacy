// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_paint_value.h"

#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_paint_image_generator.h"
#include "third_party/blink/renderer/core/css/css_syntax_descriptor.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_deferred_image.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_input.h"
#include "third_party/blink/renderer/core/css/cssom/style_value_factory.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSPaintValue::CSSPaintValue(CSSCustomIdentValue* name)
    : CSSImageGeneratorValue(kPaintClass),
      name_(name),
      paint_image_generator_observer_(MakeGarbageCollected<Observer>(this)),
      paint_off_thread_(true) {}

CSSPaintValue::CSSPaintValue(
    CSSCustomIdentValue* name,
    Vector<scoped_refptr<CSSVariableData>>& variable_data)
    : CSSPaintValue(name) {
  argument_variable_data_.swap(variable_data);
}

CSSPaintValue::~CSSPaintValue() = default;

String CSSPaintValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("paint(");
  result.Append(name_->CustomCSSText());
  for (const auto& variable_data : argument_variable_data_) {
    result.Append(", ");
    result.Append(variable_data.get()->TokenRange().Serialize());
  }
  result.Append(')');
  return result.ToString();
}

String CSSPaintValue::GetName() const {
  return name_->Value();
}

scoped_refptr<Image> CSSPaintValue::GetImage(
    const ImageResourceObserver& client,
    const Document& document,
    const ComputedStyle& style,
    const FloatSize& target_size) {
  // https://crbug.com/835589: early exit when paint target is associated with
  // a link.
  if (style.InsideLink() != EInsideLink::kNotInsideLink)
    return nullptr;

  if (!generator_) {
    generator_ = CSSPaintImageGenerator::Create(
        GetName(), document, paint_image_generator_observer_);
  }

  // If the generator isn't ready yet, we have nothing to paint. Our
  // |paint_image_generator_observer_| will cause us to be called again once the
  // generator is ready.
  if (!generator_->IsImageGeneratorReady())
    return nullptr;

  if (!ParseInputArguments(document))
    return nullptr;

  // TODO(crbug.com/946515): Break dependency on LayoutObject.
  const LayoutObject& layout_object = static_cast<const LayoutObject&>(client);

  // TODO(crbug.com/716231): Remove this hack once zoom_for_dsf is enabled on
  // all platforms (currently not enabled on Mac).
  float device_scale_factor = 1;
  if (layout_object.GetFrame() && layout_object.GetFrame()->GetPage()) {
    // The value of DeviceScaleFactorDeprecated would be 1 on a platform where
    // zoom_for_dsf is enabled, even if we run chrome with
    // --force-device-scale-factor with a value that is not 1.
    device_scale_factor =
        layout_object.GetFrame()->GetPage()->DeviceScaleFactorDeprecated();
  }

  // For Off-Thread PaintWorklet, we just collect the necessary inputs together
  // and defer the actual JavaScript call until much later (during cc Raster).
  if (RuntimeEnabledFeatures::OffMainThreadCSSPaintEnabled()) {
    if (paint_off_thread_) {
      Vector<CSSPropertyID> native_properties =
          generator_->NativeInvalidationProperties();
      Vector<AtomicString> custom_properties =
          generator_->CustomInvalidationProperties();
      float zoom = layout_object.StyleRef().EffectiveZoom();
      auto style_data = PaintWorkletStylePropertyMap::BuildCrossThreadData(
          document, style, layout_object.GetNode(), native_properties,
          custom_properties);
      paint_off_thread_ = style_data.has_value();
      if (paint_off_thread_) {
        Vector<std::unique_ptr<CrossThreadStyleValue>>
            cross_thread_input_arguments;
        BuildInputArgumentValues(cross_thread_input_arguments);
        scoped_refptr<PaintWorkletInput> input =
            base::MakeRefCounted<PaintWorkletInput>(
                GetName(), target_size, zoom, generator_->WorkletId(),
                std::move(style_data.value()),
                std::move(cross_thread_input_arguments));
        return PaintWorkletDeferredImage::Create(std::move(input), target_size);
      }
    }
  }

  return generator_->Paint(client, target_size, parsed_input_arguments_,
                           device_scale_factor);
}

void CSSPaintValue::BuildInputArgumentValues(
    Vector<std::unique_ptr<CrossThreadStyleValue>>&
        cross_thread_input_arguments) {
  if (!parsed_input_arguments_)
    return;
  for (const auto& style_value : *parsed_input_arguments_) {
    std::unique_ptr<CrossThreadStyleValue> cross_thread_style =
        ComputedStyleUtils::CrossThreadStyleValueFromCSSStyleValue(style_value);
    cross_thread_input_arguments.push_back(std::move(cross_thread_style));
  }
}

bool CSSPaintValue::ParseInputArguments(const Document& document) {
  if (input_arguments_invalid_)
    return false;

  if (parsed_input_arguments_ ||
      !RuntimeEnabledFeatures::CSSPaintAPIArgumentsEnabled())
    return true;

  DCHECK(generator_->IsImageGeneratorReady());
  const Vector<CSSSyntaxDescriptor>& input_argument_types =
      generator_->InputArgumentTypes();
  if (argument_variable_data_.size() != input_argument_types.size()) {
    input_arguments_invalid_ = true;
    return false;
  }

  parsed_input_arguments_ = MakeGarbageCollected<CSSStyleValueVector>();

  for (wtf_size_t i = 0; i < argument_variable_data_.size(); ++i) {
    // If we are parsing a paint() function, we must be a secure context.
    DCHECK_EQ(SecureContextMode::kSecureContext,
              document.GetSecureContextMode());
    const CSSValue* parsed_value = argument_variable_data_[i]->ParseForSyntax(
        input_argument_types[i], SecureContextMode::kSecureContext);
    if (!parsed_value) {
      input_arguments_invalid_ = true;
      parsed_input_arguments_ = nullptr;
      return false;
    }
    parsed_input_arguments_->AppendVector(
        StyleValueFactory::CssValueToStyleValueVector(*parsed_value));
  }
  return true;
}

void CSSPaintValue::Observer::PaintImageGeneratorReady() {
  owner_value_->PaintImageGeneratorReady();
}

void CSSPaintValue::PaintImageGeneratorReady() {
  for (const ImageResourceObserver* client : Clients().Keys()) {
    // TODO(ikilpatrick): We shouldn't be casting like this or mutate the layout
    // tree from a const pointer.
    const_cast<ImageResourceObserver*>(client)->ImageChanged(
        static_cast<WrappedImagePtr>(this),
        ImageResourceObserver::CanDeferInvalidation::kNo);
  }
}

bool CSSPaintValue::KnownToBeOpaque(const Document&,
                                    const ComputedStyle&) const {
  return generator_ && !generator_->HasAlpha();
}

bool CSSPaintValue::Equals(const CSSPaintValue& other) const {
  return GetName() == other.GetName() &&
         CustomCSSText() == other.CustomCSSText();
}

void CSSPaintValue::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(name_);
  visitor->Trace(generator_);
  visitor->Trace(paint_image_generator_observer_);
  visitor->Trace(parsed_input_arguments_);
  CSSImageGeneratorValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
