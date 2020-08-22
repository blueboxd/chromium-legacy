// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_OPERATOR_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_OPERATOR_ELEMENT_H_

#include "third_party/blink/renderer/core/mathml/mathml_element.h"

namespace blink {

class ComputedStyle;
class CSSToLengthConversionData;
class Document;

class CORE_EXPORT MathMLOperatorElement final : public MathMLElement {
 public:
  explicit MathMLOperatorElement(Document&);

  void AddMathLSpaceIfNeeded(ComputedStyle&, const CSSToLengthConversionData&);
  void AddMathRSpaceIfNeeded(ComputedStyle&, const CSSToLengthConversionData&);
  void AddMathMinSizeIfNeeded(ComputedStyle&, const CSSToLengthConversionData&);
  void AddMathMaxSizeIfNeeded(ComputedStyle&, const CSSToLengthConversionData&);

 private:
  // FIXME: add operator dictionary.
  // FIXME: add OperatorContent struct.
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_OPERATOR_ELEMENT_H_
