// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/origin_trial_features_for_core.cc.tmpl
// by the script generate_origin_trial_features.py.
// DO NOT MODIFY!

// clang-format off

#include "third_party/blink/renderer/bindings/core/v8/origin_trial_features_for_core.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_object.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"
#include "third_party/blink/renderer/core/context_features/context_feature_settings.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/platform/bindings/origin_trial_features.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {
InstallOriginTrialFeaturesFunction g_old_install_origin_trial_features_function =
    nullptr;
InstallPendingOriginTrialFeatureFunction
    g_old_install_pending_origin_trial_feature_function = nullptr;

void InstallOriginTrialFeaturesForCore(
    const WrapperTypeInfo* wrapper_type_info,
    const ScriptState* script_state,
    v8::Local<v8::Object> prototype_object,
    v8::Local<v8::Function> interface_object) {
  (*g_old_install_origin_trial_features_function)(
      wrapper_type_info, script_state, prototype_object, interface_object);

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (!execution_context)
    return;
  v8::Isolate* isolate = script_state->GetIsolate();
  const DOMWrapperWorld& world = script_state->World();
  // TODO(iclelland): Unify ContextFeatureSettings with the rest of the
  // conditional features.
  if (wrapper_type_info == V8Window::GetWrapperTypeInfo()) {
    auto* settings = ContextFeatureSettings::From(
        execution_context,
        ContextFeatureSettings::CreationMode::kDontCreateIfNotExists);
    if (settings && settings->isMojoJSEnabled()) {
      v8::Local<v8::Object> instance_object =
          script_state->GetContext()->Global();
      V8Window::InstallMojoJS(isolate, world, instance_object, prototype_object,
                              interface_object);
    }
  }
  // TODO(iclelland): Extract this common code out of OriginTrialFeaturesForCore
  // and OriginTrialFeaturesForModules into a block.
  if (wrapper_type_info == V8TestInterface::GetWrapperTypeInfo()) {
    if (RuntimeEnabledFeatures::OriginTrialFeatureEnabled(execution_context)) {
      V8TestInterface::InstallOriginTrialFeature(
          isolate, world, v8::Local<v8::Object>(), prototype_object, interface_object);
    }
  }
  if (wrapper_type_info == V8TestObject::GetWrapperTypeInfo()) {
    if (RuntimeEnabledFeatures::OriginTrialFeatureEnabled(execution_context)) {
      V8TestObject::InstallOriginTrialFeature(
          isolate, world, v8::Local<v8::Object>(), prototype_object, interface_object);
    }
  }
}

void InstallPendingOriginTrialFeatureForCore(OriginTrialFeature feature,
                                             const ScriptState* script_state) {
  (*g_old_install_pending_origin_trial_feature_function)(feature, script_state);

  // TODO(iclelland): Extract this common code out of OriginTrialFeaturesForCore
  // and OriginTrialFeaturesForModules into a block.
  v8::Local<v8::Object> prototype_object;
  v8::Local<v8::Function> interface_object;
  v8::Isolate* isolate = script_state->GetIsolate();
  const DOMWrapperWorld& world = script_state->World();
  V8PerContextData* context_data = script_state->PerContextData();
  v8::Local<v8::Context> current_context = script_state->GetContext();
  v8::Local<v8::Object> global_object = current_context->Global();
  ALLOW_UNUSED_LOCAL(global_object);
  ExecutionContext* execution_context = ToExecutionContext(current_context);
  ALLOW_UNUSED_LOCAL(execution_context);
  switch (feature) {
    case OriginTrialFeature::kOriginTrialFeature: {
      if (context_data->GetExistingConstructorAndPrototypeForType(
              V8TestInterface::GetWrapperTypeInfo(), &prototype_object, &interface_object)) {
        V8TestInterface::InstallOriginTrialFeature(
            isolate, world, v8::Local<v8::Object>(), prototype_object, interface_object);
      }
      if (context_data->GetExistingConstructorAndPrototypeForType(
              V8TestObject::GetWrapperTypeInfo(), &prototype_object, &interface_object)) {
        V8TestObject::InstallOriginTrialFeature(
            isolate, world, v8::Local<v8::Object>(), prototype_object, interface_object);
      }
      break;
    }
    default:
      break;
  }
}

}  // namespace

void RegisterInstallOriginTrialFeaturesForCore() {
  g_old_install_origin_trial_features_function =
      SetInstallOriginTrialFeaturesFunction(&InstallOriginTrialFeaturesForCore);
  g_old_install_pending_origin_trial_feature_function =
      SetInstallPendingOriginTrialFeatureFunction(
          &InstallPendingOriginTrialFeatureForCore);
}

}  // namespace blink
