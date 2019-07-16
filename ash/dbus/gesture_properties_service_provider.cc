// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/dbus/gesture_properties_service_provider.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ash {

GesturePropertiesServiceProvider::GesturePropertiesServiceProvider()
    : weak_ptr_factory_(this) {}

GesturePropertiesServiceProvider::~GesturePropertiesServiceProvider() = default;

void GesturePropertiesServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  dbus::ExportedObject::OnExportedCallback on_exported =
      base::BindRepeating(&GesturePropertiesServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr());
  exported_object->ExportMethod(
      chromeos::kGesturePropertiesServiceInterface,
      chromeos::kGesturePropertiesServiceListDevicesMethod,
      base::BindRepeating(&GesturePropertiesServiceProvider::ListDevices,
                          weak_ptr_factory_.GetWeakPtr()),
      on_exported);
}

void GesturePropertiesServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  if (!success)
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
}

namespace {

void ListDevicesCallback(
    std::unique_ptr<dbus::Response> response,
    const dbus::ExportedObject::ResponseSender& response_sender,
    const base::flat_map<int, std::string>& result) {
  dbus::MessageWriter writer(response.get());
  writer.AppendUint32(result.size());
  dbus::MessageWriter dict_writer(nullptr);
  writer.OpenArray("{is}", &dict_writer);
  for (const auto& pair : result) {
    dbus::MessageWriter dict_entry_writer(nullptr);
    dict_writer.OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendInt32(pair.first);
    dict_entry_writer.AppendString(pair.second);
    dict_writer.CloseContainer(&dict_entry_writer);
  }
  writer.CloseContainer(&dict_writer);
  response_sender.Run(std::move(response));
}

}  // namespace

void GesturePropertiesServiceProvider::ListDevices(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  GetService()->ListDevices(base::BindOnce(
      &ListDevicesCallback, std::move(response), response_sender));
}

ui::ozone::mojom::GesturePropertiesService*
GesturePropertiesServiceProvider::GetService() {
  if (service_for_test_ != nullptr)
    return service_for_test_;

  if (!service_.is_bound()) {
    ui::ozone::mojom::GesturePropertiesServiceRequest request =
        mojo::MakeRequest(&service_);
    ui::OzonePlatform::GetInstance()
        ->GetInputController()
        ->GetGesturePropertiesService(std::move(request));
  }
  return service_.get();
}

}  // namespace ash
