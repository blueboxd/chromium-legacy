// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/web_test/web_test_render_thread_observer.h"

#include "content/public/common/content_client.h"
#include "content/public/renderer/render_thread.h"
#include "content/shell/common/web_test/web_test_switches.h"
#include "content/shell/renderer/web_test/test_interfaces.h"
#include "content/shell/renderer/web_test/test_runner.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/blink.h"

namespace content {

namespace {
WebTestRenderThreadObserver* g_instance = nullptr;
}

// static
WebTestRenderThreadObserver* WebTestRenderThreadObserver::GetInstance() {
  return g_instance;
}

WebTestRenderThreadObserver::WebTestRenderThreadObserver() {
  CHECK(!g_instance);
  g_instance = this;
  RenderThread::Get()->AddObserver(this);

  blink::SetWebTestMode(true);

  test_interfaces_ = std::make_unique<TestInterfaces>();
  test_interfaces_->ResetAll();
}

WebTestRenderThreadObserver::~WebTestRenderThreadObserver() {
  CHECK(g_instance == this);
  g_instance = nullptr;
}

void WebTestRenderThreadObserver::RegisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->AddInterface(base::BindRepeating(
      &WebTestRenderThreadObserver::OnWebTestRenderThreadAssociatedRequest,
      base::Unretained(this)));
}

void WebTestRenderThreadObserver::UnregisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->RemoveInterface(mojom::WebTestRenderThread::Name_);
}

void WebTestRenderThreadObserver::OnWebTestRenderThreadAssociatedRequest(
    mojo::PendingAssociatedReceiver<mojom::WebTestRenderThread> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void WebTestRenderThreadObserver::ReplicateWebTestRuntimeFlagsChanges(
    base::Value changed_layout_test_runtime_flags) {
  base::DictionaryValue* changed_web_test_runtime_flags_dictionary = nullptr;
  bool ok = changed_layout_test_runtime_flags.GetAsDictionary(
      &changed_web_test_runtime_flags_dictionary);
  DCHECK(ok);
  test_interfaces()->GetTestRunner()->ReplicateWebTestRuntimeFlagsChanges(
      *changed_web_test_runtime_flags_dictionary);
}

}  // namespace content
