// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_scripts_element.h"

#include "third_party/blink/renderer/core/layout/ng/mathml/layout_ng_mathml_block.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

static MathScriptType ScriptTypeOf(const QualifiedName& tagName) {
  if (tagName == mathml_names::kMsubTag)
    return MathScriptType::kSub;
  if (tagName == mathml_names::kMsupTag)
    return MathScriptType::kSuper;
  if (tagName == mathml_names::kMsubsupTag)
    return MathScriptType::kSubSup;
  if (tagName == mathml_names::kMunderTag)
    return MathScriptType::kUnder;
  if (tagName == mathml_names::kMoverTag)
    return MathScriptType::kOver;
  DCHECK(tagName == mathml_names::kMunderoverTag);
  return MathScriptType::kUnderOver;
}

MathMLScriptsElement::MathMLScriptsElement(const QualifiedName& tagName,
                                           Document& document)
    : MathMLElement(tagName, document), script_type_(ScriptTypeOf(tagName)) {}

LayoutObject* MathMLScriptsElement::CreateLayoutObject(
    const ComputedStyle& style,
    LegacyLayout legacy) {
  // TODO(crbug.com/1070600): Use LayoutObjectFactory for MathML layout object
  // creation.
  if (!RuntimeEnabledFeatures::MathMLCoreEnabled() ||
      legacy == LegacyLayout::kForce || !style.IsDisplayMathType())
    return MathMLElement::CreateLayoutObject(style, legacy);
  return new LayoutNGMathMLBlock(this);
}

}  // namespace blink
