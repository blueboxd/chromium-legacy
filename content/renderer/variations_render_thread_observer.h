// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_VARIATIONS_RENDER_THREAD_OBSERVER_H_
#define CONTENT_RENDERER_VARIATIONS_RENDER_THREAD_OBSERVER_H_

#include "base/macros.h"
#include "content/common/renderer_variations_configuration.mojom.h"
#include "content/public/renderer/render_thread_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace content {

// This is the renderer side of the FieldTrialSynchronizer which applies
// field trial group settings.
class VariationsRenderThreadObserver
    : public content::RenderThreadObserver,
      public mojom::RendererVariationsConfiguration {
 public:
  VariationsRenderThreadObserver();
  ~VariationsRenderThreadObserver() override;

  // Appends a throttle if the browser has sent us a variations header.
  static void AppendThrottleIfNeeded(
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>>* throttles);

  // content::RenderThreadObserver:
  void RegisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;
  void UnregisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;

  // content::mojom::RendererConfiguration:
  void SetVariationsHeader(const std::string& variation_ids_header) override;
  void SetFieldTrialGroup(const std::string& trial_name,
                          const std::string& group_name) override;

 private:
  mojo::AssociatedReceiver<mojom::RendererVariationsConfiguration>
      renderer_configuration_receiver_{this};

  void OnRendererConfigurationAssociatedRequest(
      mojo::PendingAssociatedReceiver<mojom::RendererVariationsConfiguration>
          receiver);

  DISALLOW_COPY_AND_ASSIGN(VariationsRenderThreadObserver);
};

}  // namespace content

#endif  // CONTENT_RENDERER_VARIATIONS_RENDER_THREAD_OBSERVER_H_
