// Copyright 2016 the Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/computed_style_property_map.h"

#include "third_party/blink/renderer/core/css/computed_style_css_value_mapping.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/transforms/matrix_3d_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/matrix_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/perspective_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/skew_transform_operation.h"

namespace blink {

namespace {

// We collapse functions like translateX into translate, since we will reify
// them as a translate anyway.
const CSSValue* ComputedTransformComponent(const TransformOperation& operation,
                                           float zoom) {
  switch (operation.GetType()) {
    case TransformOperation::kScaleX:
    case TransformOperation::kScaleY:
    case TransformOperation::kScaleZ:
    case TransformOperation::kScale:
    case TransformOperation::kScale3D: {
      const auto& scale = To<ScaleTransformOperation>(operation);
      CSSFunctionValue* result = MakeGarbageCollected<CSSFunctionValue>(
          operation.Is3DOperation() ? CSSValueID::kScale3d
                                    : CSSValueID::kScale);
      result->Append(*CSSNumericLiteralValue::Create(
          scale.X(), CSSPrimitiveValue::UnitType::kNumber));
      result->Append(*CSSNumericLiteralValue::Create(
          scale.Y(), CSSPrimitiveValue::UnitType::kNumber));
      if (operation.Is3DOperation()) {
        result->Append(*CSSNumericLiteralValue::Create(
            scale.Z(), CSSPrimitiveValue::UnitType::kNumber));
      }
      return result;
    }
    case TransformOperation::kTranslateX:
    case TransformOperation::kTranslateY:
    case TransformOperation::kTranslateZ:
    case TransformOperation::kTranslate:
    case TransformOperation::kTranslate3D: {
      const auto& translate = To<TranslateTransformOperation>(operation);
      CSSFunctionValue* result = MakeGarbageCollected<CSSFunctionValue>(
          operation.Is3DOperation() ? CSSValueID::kTranslate3d
                                    : CSSValueID::kTranslate);
      result->Append(*CSSPrimitiveValue::CreateFromLength(translate.X(), zoom));
      result->Append(*CSSPrimitiveValue::CreateFromLength(translate.Y(), zoom));
      if (operation.Is3DOperation()) {
        result->Append(*CSSNumericLiteralValue::Create(
            translate.Z(), CSSPrimitiveValue::UnitType::kPixels));
      }
      return result;
    }
    case TransformOperation::kRotateX:
    case TransformOperation::kRotateY:
    case TransformOperation::kRotate3D: {
      const auto& rotate = To<RotateTransformOperation>(operation);
      CSSFunctionValue* result =
          MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kRotate3d);
      result->Append(*CSSNumericLiteralValue::Create(
          rotate.X(), CSSPrimitiveValue::UnitType::kNumber));
      result->Append(*CSSNumericLiteralValue::Create(
          rotate.Y(), CSSPrimitiveValue::UnitType::kNumber));
      result->Append(*CSSNumericLiteralValue::Create(
          rotate.Z(), CSSPrimitiveValue::UnitType::kNumber));
      result->Append(*CSSNumericLiteralValue::Create(
          rotate.Angle(), CSSPrimitiveValue::UnitType::kDegrees));
      return result;
    }
    case TransformOperation::kRotate: {
      const auto& rotate = To<RotateTransformOperation>(operation);
      auto* result =
          MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kRotate);
      result->Append(*CSSNumericLiteralValue::Create(
          rotate.Angle(), CSSPrimitiveValue::UnitType::kDegrees));
      return result;
    }
    case TransformOperation::kSkewX: {
      const auto& skew = To<SkewTransformOperation>(operation);
      auto* result = MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kSkewX);
      result->Append(*CSSNumericLiteralValue::Create(
          skew.AngleX(), CSSPrimitiveValue::UnitType::kDegrees));
      return result;
    }
    case TransformOperation::kSkewY: {
      const auto& skew = To<SkewTransformOperation>(operation);
      auto* result = MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kSkewY);
      result->Append(*CSSNumericLiteralValue::Create(
          skew.AngleY(), CSSPrimitiveValue::UnitType::kDegrees));
      return result;
    }
    case TransformOperation::kSkew: {
      const auto& skew = To<SkewTransformOperation>(operation);
      auto* result = MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kSkew);
      result->Append(*CSSNumericLiteralValue::Create(
          skew.AngleX(), CSSPrimitiveValue::UnitType::kDegrees));
      result->Append(*CSSNumericLiteralValue::Create(
          skew.AngleY(), CSSPrimitiveValue::UnitType::kDegrees));
      return result;
    }
    case TransformOperation::kPerspective: {
      const auto& perspective = To<PerspectiveTransformOperation>(operation);
      auto* result =
          MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kPerspective);
      result->Append(*CSSNumericLiteralValue::Create(
          perspective.Perspective() / zoom,
          CSSPrimitiveValue::UnitType::kPixels));
      return result;
    }
    case TransformOperation::kMatrix: {
      const auto& matrix = To<MatrixTransformOperation>(operation).Matrix();
      auto* result =
          MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kMatrix);
      double values[6] = {matrix.A(), matrix.B(),        matrix.C(),
                          matrix.D(), matrix.E() / zoom, matrix.F() / zoom};
      for (double value : values) {
        result->Append(*CSSNumericLiteralValue::Create(
            value, CSSPrimitiveValue::UnitType::kNumber));
      }
      return result;
    }
    case TransformOperation::kMatrix3D: {
      const auto& matrix = To<Matrix3DTransformOperation>(operation).Matrix();
      CSSFunctionValue* result =
          MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kMatrix3d);
      double values[16] = {
          matrix.M11(),        matrix.M12(),        matrix.M13(),
          matrix.M14(),        matrix.M21(),        matrix.M22(),
          matrix.M23(),        matrix.M24(),        matrix.M31(),
          matrix.M32(),        matrix.M33(),        matrix.M34(),
          matrix.M41() / zoom, matrix.M42() / zoom, matrix.M43() / zoom,
          matrix.M44()};
      for (double value : values) {
        result->Append(*CSSNumericLiteralValue::Create(
            value, CSSPrimitiveValue::UnitType::kNumber));
      }
      return result;
    }
    case TransformOperation::kInterpolated:
      // TODO(816803): The computed value in this case is not fully spec'd
      // See https://github.com/w3c/css-houdini-drafts/issues/425
      return CSSIdentifierValue::Create(CSSValueID::kNone);
    default:
      // The remaining operations are unsupported.
      NOTREACHED();
      return CSSIdentifierValue::Create(CSSValueID::kNone);
  }
}

const CSSValue* ComputedTransform(const ComputedStyle& style) {
  if (style.Transform().Operations().size() == 0)
    return CSSIdentifierValue::Create(CSSValueID::kNone);

  CSSValueList* components = CSSValueList::CreateSpaceSeparated();
  for (const auto& operation : style.Transform().Operations()) {
    components->Append(
        *ComputedTransformComponent(*operation, style.EffectiveZoom()));
  }
  return components;
}

}  // namespace

unsigned int ComputedStylePropertyMap::size() const {
  const ComputedStyle* style = UpdateStyle();
  if (!style)
    return 0;

  DCHECK(StyledNode());
  const Document& document = StyledNode()->GetDocument();
  return CSSComputedStyleDeclaration::ComputableProperties(
             StyledNode()->GetExecutionContext())
             .size() +
         ComputedStyleCSSValueMapping::GetVariables(
             *style, document.GetPropertyRegistry())
             .size();
}

bool ComputedStylePropertyMap::ComparePropertyNames(
    const CSSPropertyName& name_a,
    const CSSPropertyName& name_b) {
  AtomicString a = name_a.ToAtomicString();
  AtomicString b = name_b.ToAtomicString();
  if (a.StartsWith("--"))
    return b.StartsWith("--") && WTF::CodeUnitCompareLessThan(a, b);
  if (a.StartsWith("-")) {
    return b.StartsWith("--") ||
           (b.StartsWith("-") && WTF::CodeUnitCompareLessThan(a, b));
  }
  return b.StartsWith("-") || WTF::CodeUnitCompareLessThan(a, b);
}

Node* ComputedStylePropertyMap::StyledNode() const {
  DCHECK(node_);
  if (!pseudo_id_)
    return node_;
  if (auto* element_node = DynamicTo<Element>(node_.Get())) {
    if (PseudoElement* element = element_node->GetPseudoElement(pseudo_id_)) {
      return element;
    }
  }
  return nullptr;
}

const ComputedStyle* ComputedStylePropertyMap::UpdateStyle() const {
  Node* node = StyledNode();
  if (!node || !node->InActiveDocument())
    return nullptr;

  // Update style before getting the value for the property
  // This could cause the node to be blown away. This code is copied from
  // CSSComputedStyleDeclaration::GetPropertyCSSValue.
  node->GetDocument().UpdateStyleAndLayoutTreeForNode(node);
  node = StyledNode();
  if (!node)
    return nullptr;
  // This is copied from CSSComputedStyleDeclaration::computeComputedStyle().
  // PseudoIdNone must be used if node() is a PseudoElement.
  const ComputedStyle* style = node->EnsureComputedStyle(
      node->IsPseudoElement() ? kPseudoIdNone : pseudo_id_);
  node = StyledNode();
  if (!node || !node->InActiveDocument() || !style)
    return nullptr;
  return style;
}

const CSSValue* ComputedStylePropertyMap::GetProperty(
    CSSPropertyID property_id) const {
  const ComputedStyle* style = UpdateStyle();
  if (!style)
    return nullptr;

  // Special cases for properties where CSSProperty::CSSValueFromComputedStyle
  // doesn't return the correct computed value
  switch (property_id) {
    case CSSPropertyID::kTransform:
      return ComputedTransform(*style);
    case CSSPropertyID::kLineHeight:
      return ComputedStyleUtils::ComputedValueForLineHeight(*style);
    default:
      return CSSProperty::Get(property_id)
          .CSSValueFromComputedStyle(*style, nullptr /* layout_object */,
                                     false /* allow_visited_style */);
  }
}

const CSSValue* ComputedStylePropertyMap::GetCustomProperty(
    AtomicString property_name) const {
  const ComputedStyle* style = UpdateStyle();
  if (!style)
    return nullptr;
  CSSPropertyRef ref(property_name, node_->GetDocument());
  return ref.GetProperty().CSSValueFromComputedStyle(
      *style, nullptr /* layout_object */, false /* allow_visited_style */);
}

void ComputedStylePropertyMap::ForEachProperty(
    const IterationCallback& callback) {
  const ComputedStyle* style = UpdateStyle();
  if (!style)
    return;

  DCHECK(StyledNode());
  const Document& document = StyledNode()->GetDocument();
  // Have to sort by all properties by code point, so we have to store
  // them in a buffer first.
  HeapVector<std::pair<CSSPropertyName, Member<const CSSValue>>> values;
  for (const CSSProperty* property :
       CSSComputedStyleDeclaration::ComputableProperties(
           StyledNode()->GetExecutionContext())) {
    DCHECK(property);
    DCHECK(!property->IDEquals(CSSPropertyID::kVariable));
    const CSSValue* value = property->CSSValueFromComputedStyle(
        *style, nullptr /* layout_object */, false);
    if (value)
      values.emplace_back(CSSPropertyName(property->PropertyID()), value);
  }

  const PropertyRegistry* registry = document.GetPropertyRegistry();

  for (const auto& name_value :
       ComputedStyleCSSValueMapping::GetVariables(*style, registry)) {
    values.emplace_back(CSSPropertyName(name_value.key), name_value.value);
  }

  std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) {
    return ComparePropertyNames(a.first, b.first);
  });

  for (const auto& value : values)
    callback(value.first, *value.second);
}

String ComputedStylePropertyMap::SerializationForShorthand(
    const CSSProperty& property) const {
  DCHECK(property.IsShorthand());
  const ComputedStyle* style = UpdateStyle();
  if (!style) {
    NOTREACHED();
    return "";
  }

  if (const CSSValue* value = property.CSSValueFromComputedStyle(
          *style, nullptr /* layout_object */, false)) {
    return value->CssText();
  }

  NOTREACHED();
  return "";
}

}  // namespace blink
