// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_BROWSER_INTERFACE_BROKER_PROXY_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_BROWSER_INTERFACE_BROKER_PROXY_H_

#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom.h"

namespace blink {

// BrowserInterfaceBrokerProxy provides access to interfaces exposed by the
// browser to the renderer. It is intended to replace document- and
// worker-scoped InterfaceProvider (see crbug.com/718652).
class BLINK_COMMON_EXPORT BrowserInterfaceBrokerProxy {
 public:
  BrowserInterfaceBrokerProxy() = default;
  void Bind(mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>);
  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> Reset();
  void GetInterface(mojo::GenericPendingReceiver) const;

  // TODO(crbug.com/718652): Add a presubmit check for C++ call sites
  void GetInterface(const std::string& name,
                    mojo::ScopedMessagePipeHandle pipe) const;

  void SetBinderForTesting(
      const std::string& name,
      base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)> binder);

 private:
  mojo::Remote<blink::mojom::BrowserInterfaceBroker> broker_;

  using BinderMap =
      std::map<std::string,
               base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)>>;
  BinderMap binder_map_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(BrowserInterfaceBrokerProxy);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_BROWSER_INTERFACE_BROKER_PROXY_H_
