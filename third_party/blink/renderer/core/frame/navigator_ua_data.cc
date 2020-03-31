// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/navigator_ua_data.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_ua_data_values.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

NavigatorUAData::NavigatorUAData(ExecutionContext* context)
    : ExecutionContextClient(context) {
  NavigatorUABrandVersion* dict = NavigatorUABrandVersion::Create();
  dict->setBrand("");
  dict->setVersion("");
  empty_brand_set_.push_back(dict);
}

void NavigatorUAData::AddBrand(const String& brand, const String& version) {
  NavigatorUABrandVersion* dict = NavigatorUABrandVersion::Create();
  dict->setBrand(brand);
  dict->setVersion(version);
  brand_set_.push_back(dict);
}

void NavigatorUAData::SetMobile(bool mobile) {
  is_mobile_ = mobile;
}

void NavigatorUAData::SetPlatform(const String& brand, const String& version) {
  platform_ = brand;
  platform_version_ = version;
}

void NavigatorUAData::SetArchitecture(const String& architecture) {
  architecture_ = architecture;
}

void NavigatorUAData::SetModel(const String& model) {
  model_ = model;
}

void NavigatorUAData::SetUAFullVersion(const String& ua_full_version) {
  ua_full_version_ = ua_full_version;
}

bool NavigatorUAData::mobile() const {
  if (GetExecutionContext()) {
    return is_mobile_;
  }
  return false;
}

const HeapVector<Member<NavigatorUABrandVersion>>& NavigatorUAData::uaList()
    const {
  if (GetExecutionContext()) {
    return brand_set_;
  }
  return empty_brand_set_;
}

ScriptPromise NavigatorUAData::getHighEntropyValues(
    ScriptState* script_state,
    Vector<String>& hints) const {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  UADataValues* values = MakeGarbageCollected<UADataValues>();
  for (const String& hint : hints) {
    if (hint == "platform") {
      values->setPlatform(platform_);
    } else if (hint == "platformVersion") {
      values->setPlatformVersion(platform_version_);
    } else if (hint == "architecture") {
      values->setArchitecture(architecture_);
    } else if (hint == "model") {
      values->setModel(model_);
    } else if (hint == "uaFullVersion") {
      values->setUaFullVersion(ua_full_version_);
    }
  }
  resolver->Resolve(values);
  return promise;
}

void NavigatorUAData::Trace(Visitor* visitor) {
  visitor->Trace(brand_set_);
  visitor->Trace(empty_brand_set_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
