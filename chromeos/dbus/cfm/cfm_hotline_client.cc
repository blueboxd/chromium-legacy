// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cfm/cfm_hotline_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

#include "chromeos/dbus/cfm/fake_cfm_hotline_client.h"

namespace chromeos {

namespace {

CfmHotlineClient* g_instance = nullptr;

class CfmHotlineClientImpl : public CfmHotlineClient {
 public:
  CfmHotlineClientImpl() = default;
  CfmHotlineClientImpl(const CfmHotlineClientImpl&) = delete;
  CfmHotlineClientImpl& operator=(const CfmHotlineClientImpl&) = delete;
  ~CfmHotlineClientImpl() override = default;

  void Init(dbus::Bus* const bus) {
    dbus_proxy_ = bus->GetObjectProxy(
        cfm::broker::kServiceName, dbus::ObjectPath(cfm::broker::kServicePath));
  }

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback)
      override {
    dbus_proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void BootstrapMojoConnection(
      base::ScopedFD fd,
      BootstrapMojoConnectionCallback result_callback) override {
    dbus::MethodCall method_call(cfm::broker::kServiceInterfaceName,
                                 cfm::broker::kBootstrapMojoConnectionMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(/*is_outgoing_invitation=*/true);
    writer.AppendFileDescriptor(fd.get());
    dbus_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CfmHotlineClientImpl::OnBootstrapMojoConnectionResponse,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(result_callback)));
  }

 private:
  dbus::ObjectProxy* dbus_proxy_ = nullptr;

  // Passes the invitation token of |dbus_response| on to |result_callback|.
  void OnBootstrapMojoConnectionResponse(
      BootstrapMojoConnectionCallback result_callback,
      dbus::Response* const response) {
    std::move(result_callback).Run(response != nullptr);
  }

  // Must be last class member.
  base::WeakPtrFactory<CfmHotlineClientImpl> weak_ptr_factory_{this};
};

}  // namespace

CfmHotlineClient::CfmHotlineClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

CfmHotlineClient::~CfmHotlineClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void CfmHotlineClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  (new CfmHotlineClientImpl())->Init(bus);
}

// static
void CfmHotlineClient::InitializeFake() {
  new FakeCfmHotlineClient();
}

// static
void CfmHotlineClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
CfmHotlineClient* CfmHotlineClient::Get() {
  return g_instance;
}

}  // namespace chromeos
