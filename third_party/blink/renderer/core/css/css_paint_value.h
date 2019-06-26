// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PAINT_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PAINT_VALUE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_image_generator_value.h"
#include "third_party/blink/renderer/core/css/css_paint_image_generator.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_style_value.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CORE_EXPORT CSSPaintValue : public CSSImageGeneratorValue {
 public:
  explicit CSSPaintValue(CSSCustomIdentValue* name);
  CSSPaintValue(CSSCustomIdentValue* name,
                Vector<scoped_refptr<CSSVariableData>>&);
  ~CSSPaintValue();

  String CustomCSSText() const;

  String GetName() const;

  // The |target_size| is container size with subpixel snapping when used
  // in the context of paint images.
  scoped_refptr<Image> GetImage(const ImageResourceObserver&,
                                const Document&,
                                const ComputedStyle&,
                                const FloatSize& target_size);
  bool IsFixedSize() const { return false; }
  FloatSize FixedSize(const Document&) { return FloatSize(); }

  bool IsPending() const { return true; }
  bool KnownToBeOpaque(const Document&, const ComputedStyle&) const;

  void LoadSubimages(const Document&) {}

  bool Equals(const CSSPaintValue&) const;

  const Vector<CSSPropertyID>* NativeInvalidationProperties() const {
    return generator_ ? &generator_->NativeInvalidationProperties() : nullptr;
  }
  const Vector<AtomicString>* CustomInvalidationProperties() const {
    return generator_ ? &generator_->CustomInvalidationProperties() : nullptr;
  }

  const CSSStyleValueVector* GetParsedInputArgumentsForTesting() {
    return parsed_input_arguments_;
  }
  void BuildInputArgumentValuesForTesting(
      Vector<std::unique_ptr<CrossThreadStyleValue>>& style_value) {
    BuildInputArgumentValues(style_value);
  }

  void TraceAfterDispatch(blink::Visitor*);

 private:
  class Observer final : public CSSPaintImageGenerator::Observer {
   public:
    explicit Observer(CSSPaintValue* owner_value) : owner_value_(owner_value) {}

    ~Observer() override = default;
    void Trace(blink::Visitor* visitor) override {
      visitor->Trace(owner_value_);
      CSSPaintImageGenerator::Observer::Trace(visitor);
    }

    void PaintImageGeneratorReady() final;

   private:
    Member<CSSPaintValue> owner_value_;
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  void PaintImageGeneratorReady();

  bool ParseInputArguments(const Document&);

  void BuildInputArgumentValues(
      Vector<std::unique_ptr<CrossThreadStyleValue>>&);

  bool input_arguments_invalid_ = false;

  Member<CSSCustomIdentValue> name_;
  Member<CSSPaintImageGenerator> generator_;
  Member<Observer> paint_image_generator_observer_;
  Member<CSSStyleValueVector> parsed_input_arguments_;
  Vector<scoped_refptr<CSSVariableData>> argument_variable_data_;
  bool paint_off_thread_;
};

template <>
struct DowncastTraits<CSSPaintValue> {
  static bool AllowFrom(const CSSValue& value) { return value.IsPaintValue(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PAINT_VALUE_H_
