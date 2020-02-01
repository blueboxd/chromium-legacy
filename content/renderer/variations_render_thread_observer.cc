// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/variations_render_thread_observer.h"

#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace content {

VariationsRenderThreadObserver::VariationsRenderThreadObserver() = default;

VariationsRenderThreadObserver::~VariationsRenderThreadObserver() = default;

void VariationsRenderThreadObserver::RegisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->AddInterface(base::BindRepeating(
      &VariationsRenderThreadObserver::OnRendererConfigurationAssociatedRequest,
      base::Unretained(this)));
}

void VariationsRenderThreadObserver::UnregisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->RemoveInterface(
      mojom::RendererVariationsConfiguration::Name_);
}

void VariationsRenderThreadObserver::SetFieldTrialGroup(
    const std::string& trial_name,
    const std::string& group_name) {
  content::RenderThread::Get()->SetFieldTrialGroup(trial_name, group_name);
}

void VariationsRenderThreadObserver::OnRendererConfigurationAssociatedRequest(
    mojo::PendingAssociatedReceiver<mojom::RendererVariationsConfiguration>
        receiver) {
  renderer_configuration_receiver_.reset();
  renderer_configuration_receiver_.Bind(std::move(receiver));
}

}  // namespace content
