// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_SCOPED_SERVICE_BINDING_H_
#define BASE_FUCHSIA_SCOPED_SERVICE_BINDING_H_

#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/fidl/cpp/interface_request.h>
#include <lib/zx/channel.h>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/fuchsia/scoped_service_publisher.h"

namespace sys {
class OutgoingDirectory;
}  // namespace sys

namespace vfs {
class PseudoDir;
}  // namespace vfs

namespace base {
namespace fuchsia {

template <typename Interface>
class BASE_EXPORT ScopedServiceBinding {
 public:
  // Published a public service in the specified |outgoing_directory|.
  // |outgoing_directory| and |impl| must outlive the binding.
  ScopedServiceBinding(sys::OutgoingDirectory* outgoing_directory,
                       Interface* impl)
      : publisher_(outgoing_directory, bindings_.GetHandler(impl)) {}

  // Publishes a service in the specified |pseudo_dir|. |pseudo_dir| and |impl|
  // must outlive the binding.
  ScopedServiceBinding(vfs::PseudoDir* pseudo_dir, Interface* impl)
      : publisher_(pseudo_dir, bindings_.GetHandler(impl)) {}

  ~ScopedServiceBinding() = default;

  void SetOnLastClientCallback(base::OnceClosure on_last_client_callback) {
    on_last_client_callback_ = std::move(on_last_client_callback);
    bindings_.set_empty_set_handler(
        fit::bind_member(this, &ScopedServiceBinding::OnBindingSetEmpty));
  }

  bool has_clients() const { return bindings_.size() != 0; }

 private:
  void OnBindingSetEmpty() {
    bindings_.set_empty_set_handler(nullptr);
    std::move(on_last_client_callback_).Run();
  }

  fidl::BindingSet<Interface> bindings_;
  ScopedServicePublisher<Interface> publisher_;
  base::OnceClosure on_last_client_callback_;

  DISALLOW_COPY_AND_ASSIGN(ScopedServiceBinding);
};

// Scoped service binding which allows only a single client to be connected
// at any time. By default a new connection will disconnect an existing client.
enum class ScopedServiceBindingPolicy { kPreferNew, kPreferExisting };

template <typename Interface,
          ScopedServiceBindingPolicy Policy =
              ScopedServiceBindingPolicy::kPreferNew>
class BASE_EXPORT ScopedSingleClientServiceBinding {
 public:
  // |outgoing_directory| and |impl| must outlive the binding.
  ScopedSingleClientServiceBinding(sys::OutgoingDirectory* outgoing_directory,
                                   Interface* impl)
      : binding_(impl),
        publisher_(
            outgoing_directory,
            fit::bind_member(this,
                             &ScopedSingleClientServiceBinding::BindClient)) {}

  ~ScopedSingleClientServiceBinding() = default;

  typename Interface::EventSender_& events() { return binding_.events(); }

  void SetOnLastClientCallback(base::OnceClosure on_last_client_callback) {
    on_last_client_callback_ = std::move(on_last_client_callback);
    binding_.set_error_handler(fit::bind_member(
        this, &ScopedSingleClientServiceBinding::OnBindingEmpty));
  }

  bool has_clients() const { return binding_.is_bound(); }

 private:
  void BindClient(fidl::InterfaceRequest<Interface> request) {
    if (Policy == ScopedServiceBindingPolicy::kPreferExisting &&
        binding_.is_bound()) {
      return;
    }
    binding_.Bind(std::move(request));
  }

  void OnBindingEmpty(zx_status_t status) {
    binding_.set_error_handler(nullptr);
    std::move(on_last_client_callback_).Run();
  }

  fidl::Binding<Interface> binding_;
  ScopedServicePublisher<Interface> publisher_;
  base::OnceClosure on_last_client_callback_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSingleClientServiceBinding);
};

}  // namespace fuchsia
}  // namespace base

#endif  // BASE_FUCHSIA_SCOPED_SERVICE_BINDING_H_
