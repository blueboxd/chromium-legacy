// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_keyframe_effect_model.h"

#include "third_party/blink/renderer/core/animation/animation_input_helpers.h"
#include "third_party/blink/renderer/core/animation/animation_utils.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"

namespace blink {

namespace {

using MissingPropertyValueMap = HashMap<String, String>;

void ResolveUnderlyingPropertyValues(Element& element,
                                     const PropertyHandleSet& properties,
                                     MissingPropertyValueMap& map) {
  // TODO(crbug.com/1069235): Should sample the underlying animation.
  ActiveInterpolationsMap empty_interpolations_map;
  AnimationUtils::ForEachInterpolatedPropertyValue(
      &element, properties, empty_interpolations_map,
      WTF::BindRepeating(
          [](MissingPropertyValueMap* map, PropertyHandle property,
             const CSSValue* value) {
            if (property.IsCSSProperty()) {
              String property_name =
                  AnimationInputHelpers::PropertyHandleToKeyframeAttribute(
                      property);
              map->Set(property_name, value->CssText());
            }
          },
          WTF::Unretained(&map)));
}

void AddMissingProperties(const MissingPropertyValueMap& property_map,
                          const PropertyHandleSet& all_properties,
                          const PropertyHandleSet& keyframe_properties,
                          StringKeyframe* keyframe) {
  for (const auto& property : all_properties) {
    if (keyframe_properties.Contains(property))
      continue;

    String property_name =
        AnimationInputHelpers::PropertyHandleToKeyframeAttribute(property);
    if (property_map.Contains(property_name)) {
      const String& value = property_map.at(property_name);
      if (property.IsCSSCustomProperty()) {
        keyframe->SetCSSPropertyValue(property.CustomPropertyName(), value,
                                      SecureContextMode::kInsecureContext,
                                      nullptr);
      } else {
        keyframe->SetCSSPropertyValue(
            property.GetCSSProperty().PropertyID(), value,
            SecureContextMode::kInsecureContext, nullptr);
      }
    }
  }
}

}  // namespace

KeyframeEffectModelBase::KeyframeVector
CssKeyframeEffectModel::GetComputedKeyframes(Element* element) {
  const KeyframeEffectModelBase::KeyframeVector& keyframes = GetFrames();
  if (!element)
    return keyframes;

  KeyframeEffectModelBase::KeyframeVector computed_keyframes;

  // Lazy resolution of values for missing properties.
  PropertyHandleSet all_properties = Properties();
  PropertyHandleSet from_properties;
  PropertyHandleSet to_properties;

  Vector<double> computed_offsets =
      KeyframeEffectModelBase::GetComputedOffsets(keyframes);
  computed_keyframes.ReserveInitialCapacity(keyframes.size());
  for (wtf_size_t i = 0; i < keyframes.size(); i++) {
    Keyframe* keyframe = keyframes[i];
    // TODO(crbug.com/1070627): Use computed values, prune variable references,
    // and convert logical properties to physical properties.
    computed_keyframes.push_back(keyframe->Clone());
    double offset = computed_offsets[i];
    if (offset == 0) {
      for (const auto& property : keyframe->Properties()) {
        from_properties.insert(property);
      }
    } else if (offset == 1) {
      for (const auto& property : keyframe->Properties()) {
        to_properties.insert(property);
      }
    }
  }

  // Add missing properties from the bounding keyframes.
  MissingPropertyValueMap missing_property_value_map;
  if (from_properties.size() < all_properties.size() ||
      to_properties.size() < all_properties.size()) {
    ResolveUnderlyingPropertyValues(*element, all_properties,
                                    missing_property_value_map);
  }
  if (from_properties.size() < all_properties.size() &&
      !computed_keyframes.IsEmpty()) {
    AddMissingProperties(
        missing_property_value_map, all_properties, from_properties,
        DynamicTo<StringKeyframe>(computed_keyframes[0].Get()));
  }
  if (to_properties.size() < all_properties.size() &&
      !computed_keyframes.IsEmpty()) {
    wtf_size_t index = keyframes.size() - 1;
    AddMissingProperties(
        missing_property_value_map, all_properties, to_properties,
        DynamicTo<StringKeyframe>(computed_keyframes[index].Get()));
  }
  return computed_keyframes;
}

}  // namespace blink
