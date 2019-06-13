// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/browser/application_host.h"

#import <Cocoa/Cocoa.h>

#include "components/remote_cocoa/browser/window.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace remote_cocoa {

ApplicationHost::ApplicationHost(mojom::ApplicationAssociatedRequest* request) {
  *request = mojo::MakeRequest(&application_ptr_);
}

ApplicationHost::~ApplicationHost() {
  for (Observer& obs : observers_)
    obs.OnApplicationHostDestroying(this);
}

mojom::Application* ApplicationHost::GetApplication() {
  return application_ptr_.get();
}

void ApplicationHost::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ApplicationHost::RemoveObserver(const Observer* observer) {
  observers_.RemoveObserver(observer);
}

// static
ApplicationHost* ApplicationHost::GetForNativeView(gfx::NativeView view) {
  gfx::NativeWindow window([view.GetNativeNSView() window]);
  return GetWindowApplicationHost(window);
}

}  // namespace remote_cocoa
