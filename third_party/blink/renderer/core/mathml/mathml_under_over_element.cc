// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_under_over_element.h"

namespace blink {

MathMLUnderOverElement::MathMLUnderOverElement(const QualifiedName& tagName,
                                               Document& document)
    : MathMLScriptsElement(tagName, document) {}

}  // namespace blink
