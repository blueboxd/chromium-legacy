// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/dbus/system_proxy/system_proxy_client.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/system_proxy/fake_system_proxy_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/system_proxy/dbus-constants.h"

namespace chromeos {
namespace {

SystemProxyClient* g_instance = nullptr;

const char kDbusCallFailure[] = "Failed to call system_proxy.";
const char kProtoMessageParsingFailure[] =
    "Failed to parse response message from system_proxy.";

// Tries to parse a proto message from |response| into |proto| and returns null
// if successful. If |response| is nullptr or the message cannot be parsed it
// will return an appropriate error message.
const char* DeserializeProto(dbus::Response* response,
                             google::protobuf::MessageLite* proto) {
  if (!response)
    return kDbusCallFailure;

  dbus::MessageReader reader(response);
  if (!reader.PopArrayOfBytesAsProto(proto))
    return kProtoMessageParsingFailure;

  return nullptr;
}

// "Real" implementation of SystemProxyClient talking to the SystemProxy daemon
// on the Chrome OS side.
class SystemProxyClientImpl : public SystemProxyClient {
 public:
  SystemProxyClientImpl() = default;
  SystemProxyClientImpl(const SystemProxyClientImpl&) = delete;
  SystemProxyClientImpl& operator=(const SystemProxyClientImpl&) = delete;
  ~SystemProxyClientImpl() override = default;

  // SystemProxyClient overrides:
  void SetSystemTrafficCredentials(
      const system_proxy::SetSystemTrafficCredentialsRequest& request,
      SetSystemTrafficCredentialsCallback callback) override {
    CallProtoMethodWithRequest(system_proxy::kSetSystemTrafficCredentialsMethod,
                               request, std::move(callback));
  }

  void ShutDownDaemon(ShutDownDaemonCallback callback) override {
    CallProtoMethod(system_proxy::kShutDownMethod, std::move(callback));
  }

  void Init(dbus::Bus* bus) {
    proxy_ = bus->GetObjectProxy(
        system_proxy::kSystemProxyServiceName,
        dbus::ObjectPath(system_proxy::kSystemProxyServicePath));
  }

 private:
  TestInterface* GetTestInterface() override { return nullptr; }

  // Calls System-proxy's |method_name| method. Once the (asynchronous) call
  // finishes, |callback| is called with the response proto (on the same thread
  // as this call).
  template <class TResponse>
  void CallProtoMethod(const char* method_name,
                       base::OnceCallback<void(const TResponse&)> callback) {
    dbus::MethodCall method_call(system_proxy::kSystemProxyInterface,
                                 method_name);
    dbus::MessageWriter writer(&method_call);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SystemProxyClientImpl::HandleResponse<TResponse>,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  // Same as |CallProtoMethod| but passes in |request| as input.
  template <class TRequest, class TResponse>
  void CallProtoMethodWithRequest(
      const char* method_name,
      const TRequest& request,
      base::OnceCallback<void(const TResponse&)> callback) {
    dbus::MethodCall method_call(system_proxy::kSystemProxyInterface,
                                 method_name);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      TResponse response;
      response.set_error_message(
          base::StrCat({"Failure to call d-bus method: ", method_name}));
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), response));
      return;
    }

    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SystemProxyClientImpl::HandleResponse<TResponse>,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  // Parses the response proto message from |response| and calls |callback| with
  // the decoded message. Calls |callback| with an |ERROR_DBUS_FAILURE| message
  // on error.
  template <class TProto>
  void HandleResponse(base::OnceCallback<void(const TProto&)> callback,
                      dbus::Response* response) {
    TProto response_proto;
    const char* error_message = DeserializeProto(response, &response_proto);
    if (error_message) {
      response_proto.set_error_message(error_message);
    }
    std::move(callback).Run(response_proto);
  }

  // D-Bus proxy for the SystemProxy daemon, not owned.
  dbus::ObjectProxy* proxy_ = nullptr;

  base::WeakPtrFactory<SystemProxyClientImpl> weak_factory_{this};
};

}  // namespace

SystemProxyClient::SystemProxyClient() {
  CHECK(!g_instance);
  g_instance = this;
}

SystemProxyClient::~SystemProxyClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void SystemProxyClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new SystemProxyClientImpl())->Init(bus);
}

// static
void SystemProxyClient::InitializeFake() {
  new FakeSystemProxyClient();
}

// static
void SystemProxyClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

// static
SystemProxyClient* SystemProxyClient::Get() {
  return g_instance;
}

}  // namespace chromeos
