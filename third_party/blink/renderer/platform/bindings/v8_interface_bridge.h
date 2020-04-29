// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_INTERFACE_BRIDGE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_INTERFACE_BRIDGE_H_

#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/v8.h"

namespace blink {

namespace bindings {

// The common base class of code-generated V8-Blink bridge class of IDL
// interfaces and namespaces.
class PLATFORM_EXPORT V8InterfaceBridgeBase {
  STATIC_ONLY(V8InterfaceBridgeBase);

 public:
  // Selects properties to be installed according to origin trial features.
  //
  // There are two usages.
  // 1) At the first call of V8T::InstallContextDependentProperties, install
  //   (a) properties that are not associated to origin trial features (e.g.
  //   secure contexts) plus (b) properties that are associated to origin trial
  //   features and are already enabled by the moment of the call.
  // 2) On 2nd+ call of V8T::InstallContextDependentProperties (when an origin
  //   trial feature gets enabled later on), install only (c) properties that
  //   are associated to the origin trial feature that has got enabled.
  //
  // FeatureSelector(world) is used for usage 1) and
  // FeatureSelector(world, feature) is used for usage 2).
  class FeatureSelector final {
   public:
    // Selects all properties not associated to any origin trial feature and
    // properties associated with the origin trial features that are already
    // enabled.
    FeatureSelector(const DOMWrapperWorld& world)
        : does_support_origin_trials_(DoesSupportOriginTrials(world)),
          does_select_all_(true) {}
    // Selects only the properties that are associated to the given origin
    // trial feature.
    FeatureSelector(const DOMWrapperWorld& world, OriginTrialFeature feature)
        : does_support_origin_trials_(DoesSupportOriginTrials(world)),
          selector_(feature) {}
    FeatureSelector(const FeatureSelector&) = default;
    FeatureSelector(FeatureSelector&&) = default;
    ~FeatureSelector() = default;

    FeatureSelector& operator=(const FeatureSelector&) = default;
    FeatureSelector& operator=(FeatureSelector&&) = default;

    // Returns true if properties should be installed.  Arguments |featureN|
    // represent the origin trial features to which the properties are
    // associated.  No argument means that the properties are not associated
    // with any origin trial feature.
    bool AnyOf() const { return does_select_all_; }
    bool AnyOf(OriginTrialFeature feature1) const {
      if (!does_support_origin_trials_)
        return false;
      return does_select_all_ || selector_ == feature1;
    }
    bool AnyOf(OriginTrialFeature feature1, OriginTrialFeature feature2) const {
      if (!does_support_origin_trials_)
        return false;
      return does_select_all_ || selector_ == feature1 || selector_ == feature2;
    }

   private:
    static bool DoesSupportOriginTrials(const DOMWrapperWorld& world) {
      return world.IsMainWorld() || world.IsWorkerWorld();
    }

    bool does_support_origin_trials_;
    bool does_select_all_ = false;
    OriginTrialFeature selector_ = OriginTrialFeature::kNonExisting;
  };

  using InstallInterfaceTemplateFuncType =
      void (*)(v8::Isolate* isolate,
               const DOMWrapperWorld& world,
               v8::Local<v8::FunctionTemplate> interface_template);
  using InstallUnconditionalPropertiesFuncType =
      void (*)(v8::Isolate* isolate,
               const DOMWrapperWorld& world,
               v8::Local<v8::ObjectTemplate> instance_template,
               v8::Local<v8::ObjectTemplate> prototype_template,
               v8::Local<v8::FunctionTemplate> interface_template);
  using InstallContextIndependentPropertiesFuncType =
      void (*)(v8::Isolate* isolate,
               const DOMWrapperWorld& world,
               v8::Local<v8::ObjectTemplate> instance_template,
               v8::Local<v8::ObjectTemplate> prototype_template,
               v8::Local<v8::FunctionTemplate> interface_template);
  using InstallContextDependentPropertiesFuncType =
      void (*)(v8::Local<v8::Context> context,
               const DOMWrapperWorld& world,
               v8::Local<v8::Object> instance_object,
               v8::Local<v8::Object> prototype_object,
               v8::Local<v8::Function> interface_object,
               v8::Local<v8::FunctionTemplate> interface_template,
               FeatureSelector feature_selector);
};

template <class V8T, class T>
class V8InterfaceBridge : public V8InterfaceBridgeBase {
 public:
  static T* ToWrappable(v8::Isolate* isolate, v8::Local<v8::Value> value) {
    return HasInstance(isolate, value)
               ? ToWrappableUnsafe(value.As<v8::Object>())
               : nullptr;
  }

  static T* ToWrappableUnsafe(v8::Local<v8::Object> value) {
    return ToScriptWrappable(value)->ToImpl<T>();
  }

  static bool HasInstance(v8::Isolate* isolate, v8::Local<v8::Value> value) {
    return V8PerIsolateData::From(isolate)->HasInstance(
        V8T::GetWrapperTypeInfo(), value);
  }

  // Migration adapter
  static bool HasInstance(v8::Local<v8::Value> value, v8::Isolate* isolate) {
    return HasInstance(isolate, value);
  }

  static T* ToImpl(v8::Local<v8::Object> value) {
    return ToWrappableUnsafe(value);
  }

  static T* ToImplWithTypeCheck(v8::Isolate* isolate,
                                v8::Local<v8::Value> value) {
    return ToWrappable(isolate, value);
  }
};

}  // namespace bindings

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_INTERFACE_BRIDGE_H_
