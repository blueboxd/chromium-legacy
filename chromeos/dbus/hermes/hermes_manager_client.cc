// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/hermes/hermes_manager_client.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "chromeos/dbus/hermes/fake_hermes_manager_client.h"
#include "chromeos/dbus/hermes/hermes_response_status.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace hermes {
namespace manager {
// TODO(crbug.com/1093185): Remove when hermes/dbus-constants.h is updated.
const char kPendingProfilesProperty[] = "PendingProfiles";
}  // namespace manager
}  // namespace hermes

namespace chromeos {

namespace {

HermesManagerClient* g_instance = nullptr;

}  // namespace

// The HermesManagerClient implementation.
class HermesManagerClientImpl : public HermesManagerClient {
 public:
  explicit HermesManagerClientImpl(dbus::Bus* bus) {
    dbus::ObjectPath hermes_manager_path(hermes::kHermesManagerPath);
    object_proxy_ =
        bus->GetObjectProxy(hermes::kHermesServiceName, hermes_manager_path);
    properties_ = new Properties(
        object_proxy_,
        base::BindRepeating(&HermesManagerClientImpl::OnPropertyChanged,
                            weak_ptr_factory_.GetWeakPtr(),
                            hermes_manager_path));
    properties_->ConnectSignals();
    properties_->GetAll();
  }

  explicit HermesManagerClientImpl(const HermesManagerClient&) = delete;

  ~HermesManagerClientImpl() override = default;

  // HermesManagerClient:
  void InstallProfileFromActivationCode(
      const std::string& activation_code,
      const std::string& confirmation_code,
      InstallCarrierProfileCallback callback) override {
    dbus::MethodCall method_call(
        hermes::kHermesManagerInterface,
        hermes::manager::kInstallProfileFromActivationCode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(activation_code);
    writer.AppendString(confirmation_code);
    object_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&HermesManagerClientImpl::OnProfileInstallResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void InstallPendingProfile(const dbus::ObjectPath& carrier_profile_path,
                             const std::string& confirmation_code,
                             InstallCarrierProfileCallback callback) override {
    dbus::MethodCall method_call(hermes::kHermesManagerInterface,
                                 hermes::manager::kInstallPendingProfile);
    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(carrier_profile_path);
    writer.AppendString(confirmation_code);
    object_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&HermesManagerClientImpl::OnProfileInstallResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void RequestPendingEvents(HermesResponseCallback callback) override {
    dbus::MethodCall method_call(hermes::kHermesManagerInterface,
                                 hermes::manager::kRequestPendingEvents);
    object_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&HermesManagerClientImpl::OnHermesStatusResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void UninstallProfile(const dbus::ObjectPath& carrier_profile_path,
                        HermesResponseCallback callback) override {
    dbus::MethodCall method_call(hermes::kHermesManagerInterface,
                                 hermes::manager::kUninstallProfile);
    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(carrier_profile_path);
    object_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&HermesManagerClientImpl::OnHermesStatusResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  const std::vector<dbus::ObjectPath>& GetInstalledCarrierProfiles() override {
    return properties_->installed_carrier_profiles().value();
  }

  const std::vector<dbus::ObjectPath>& GetPendingCarrierProfiles() override {
    return properties_->pending_carrier_profiles().value();
  }

  TestInterface* GetTestInterface() override { return nullptr; }

  HermesManagerClientImpl& operator=(const HermesManagerClient&) = delete;

 private:
  // Hermes Manager properties.
  class Properties : public dbus::PropertySet {
   public:
    Properties(dbus::ObjectProxy* object_proxy,
               const PropertyChangedCallback& callback)
        : dbus::PropertySet(object_proxy,
                            hermes::kHermesManagerInterface,
                            callback) {
      RegisterProperty(hermes::manager::kProfilesProperty,
                       &installed_carrier_profiles_);
      RegisterProperty(hermes::manager::kPendingProfilesProperty,
                       &pending_carrier_profiles_);
    }
    ~Properties() override = default;

    dbus::Property<std::vector<dbus::ObjectPath>>&
    installed_carrier_profiles() {
      return installed_carrier_profiles_;
    }
    dbus::Property<std::vector<dbus::ObjectPath>>& pending_carrier_profiles() {
      return pending_carrier_profiles_;
    }

   private:
    // List of paths to carrier profiles currently installed on the device.
    dbus::Property<std::vector<dbus::ObjectPath>> installed_carrier_profiles_;

    // List of pending carrier profiles from SMDS available for
    // installation on this device.
    dbus::Property<std::vector<dbus::ObjectPath>> pending_carrier_profiles_;
  };

  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name) {
    if (property_name == hermes::manager::kProfilesProperty) {
      for (auto& observer : observers()) {
        observer.OnInstalledCarrierProfileListChanged();
      }
    } else {
      for (auto& observer : observers()) {
        observer.OnPendingCarrierProfileListChanged();
      }
    }
  }

  void OnProfileInstallResponse(InstallCarrierProfileCallback callback,
                                dbus::Response* response,
                                dbus::ErrorResponse* error_response) {
    if (error_response) {
      std::move(callback).Run(
          HermesResponseStatusFromErrorName(error_response->GetErrorName()),
          nullptr);
      return;
    }

    if (!response) {
      // No Error or Response received.
      NET_LOG(ERROR) << "Carrier profile installation Error: No error or "
                        "response received.";
      std::move(callback).Run(HermesResponseStatus::kErrorNoResponse, nullptr);
      return;
    }

    dbus::MessageReader reader(response);
    dbus::ObjectPath profile_path;
    reader.PopObjectPath(&profile_path);
    std::move(callback).Run(HermesResponseStatus::kSuccess, &profile_path);
  }

  void OnHermesStatusResponse(HermesResponseCallback callback,
                              dbus::Response* response,
                              dbus::ErrorResponse* error_response) {
    if (error_response) {
      std::move(callback).Run(
          HermesResponseStatusFromErrorName(error_response->GetErrorName()));
      return;
    }
    std::move(callback).Run(HermesResponseStatus::kSuccess);
  }

  dbus::ObjectProxy* object_proxy_;
  Properties* properties_;
  base::WeakPtrFactory<HermesManagerClientImpl> weak_ptr_factory_{this};
};

HermesManagerClient::HermesManagerClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

HermesManagerClient::~HermesManagerClient() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void HermesManagerClient::AddObserver(HermesManagerClient::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void HermesManagerClient::RemoveObserver(
    HermesManagerClient::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

// static
void HermesManagerClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  DCHECK(!g_instance);
  new HermesManagerClientImpl(bus);
}

// static
void HermesManagerClient::InitializeFake() {
  new FakeHermesManagerClient();
}

// static
void HermesManagerClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
HermesManagerClient* HermesManagerClient::Get() {
  return g_instance;
}

}  // namespace chromeos
