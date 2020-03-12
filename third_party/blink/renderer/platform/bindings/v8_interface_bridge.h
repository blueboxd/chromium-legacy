// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_INTERFACE_BRIDGE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_INTERFACE_BRIDGE_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/v8.h"

namespace blink {

class DOMWrapperWorld;

namespace bindings {

// The common base class of code-generated V8-Blink bridge class of IDL
// interfaces and namespaces.
class PLATFORM_EXPORT V8InterfaceBridgeBase {
  STATIC_ONLY(V8InterfaceBridgeBase);

 protected:
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
               v8::Local<v8::Function> interface_object);
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

  static void InstallContextDependentAdapter(
      v8::Local<v8::Context> context,
      const DOMWrapperWorld& world,
      v8::Local<v8::Object> instance_object,
      v8::Local<v8::Object> prototype_object,
      v8::Local<v8::Function> interface_object,
      v8::Local<v8::FunctionTemplate> interface_template) {
    V8T::InstallContextDependentProperties(context, world, instance_object,
                                           prototype_object, interface_object);
  }
};

}  // namespace bindings

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_INTERFACE_BRIDGE_H_
