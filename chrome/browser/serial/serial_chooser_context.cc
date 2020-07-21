// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/serial_chooser_context.h"

#include <utility>

#include "base/base64.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/serial/serial_chooser_histograms.h"
#include "content/public/browser/device_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace {

constexpr char kPortNameKey[] = "name";
constexpr char kPersistentIdKey[] = "persistent_id";
constexpr char kTokenKey[] = "token";

std::string EncodeToken(const base::UnguessableToken& token) {
  const uint64_t data[2] = {token.GetHighForSerialization(),
                            token.GetLowForSerialization()};
  std::string buffer;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(&data[0]), sizeof(data)),
      &buffer);
  return buffer;
}

base::UnguessableToken DecodeToken(base::StringPiece input) {
  std::string buffer;
  if (!base::Base64Decode(input, &buffer) ||
      buffer.length() != sizeof(uint64_t) * 2) {
    return base::UnguessableToken();
  }

  const uint64_t* data = reinterpret_cast<const uint64_t*>(buffer.data());
  return base::UnguessableToken::Deserialize(data[0], data[1]);
}

base::Value PortInfoToValue(const device::mojom::SerialPortInfo& port) {
  base::Value value(base::Value::Type::DICTIONARY);
  if (port.display_name && !port.display_name->empty())
    value.SetStringKey(kPortNameKey, *port.display_name);
  else
    value.SetStringKey(kPortNameKey, port.path.LossyDisplayName());
  if (SerialChooserContext::CanStorePersistentEntry(port))
    value.SetStringKey(kPersistentIdKey, port.persistent_id.value());
  else
    value.SetStringKey(kTokenKey, EncodeToken(port.token));
  return value;
}

void RecordPermissionRevocation(SerialPermissionRevoked type) {
  UMA_HISTOGRAM_ENUMERATION("Permissions.Serial.Revoked", type);
}

}  // namespace

SerialChooserContext::SerialChooserContext(Profile* profile)
    : ChooserContextBase(ContentSettingsType::SERIAL_GUARD,
                         ContentSettingsType::SERIAL_CHOOSER_DATA,
                         HostContentSettingsMapFactory::GetForProfile(profile)),
      is_incognito_(profile->IsOffTheRecord()) {}

SerialChooserContext::~SerialChooserContext() = default;

bool SerialChooserContext::IsValidObject(const base::Value& object) {
  const std::string* token = object.FindStringKey(kTokenKey);
  return object.is_dict() && object.DictSize() == 2 &&
         object.FindStringKey(kPortNameKey) &&
         ((token && DecodeToken(*token)) ||
          object.FindStringKey(kPersistentIdKey));
}

base::string16 SerialChooserContext::GetObjectDisplayName(
    const base::Value& object) {
  const std::string* name = object.FindStringKey(kPortNameKey);
  DCHECK(name);
  return base::UTF8ToUTF16(*name);
}

std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
SerialChooserContext::GetGrantedObjects(const url::Origin& requesting_origin,
                                        const url::Origin& embedding_origin) {
  std::vector<std::unique_ptr<Object>> objects =
      ChooserContextBase::GetGrantedObjects(requesting_origin,
                                            embedding_origin);

  if (CanRequestObjectPermission(requesting_origin, embedding_origin)) {
    auto it = ephemeral_ports_.find({requesting_origin, embedding_origin});
    if (it != ephemeral_ports_.end()) {
      const std::set<base::UnguessableToken>& ports = it->second;
      for (const auto& token : ports) {
        auto port_it = port_info_.find(token);
        if (port_it == port_info_.end())
          continue;

        const base::Value& port = port_it->second;
        objects.push_back(std::make_unique<Object>(
            requesting_origin, embedding_origin, port.Clone(),
            content_settings::SettingSource::SETTING_SOURCE_USER,
            is_incognito_));
      }
    }
  }

  return objects;
}

std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
SerialChooserContext::GetAllGrantedObjects() {
  std::vector<std::unique_ptr<Object>> objects =
      ChooserContextBase::GetAllGrantedObjects();
  for (const auto& map_entry : ephemeral_ports_) {
    const url::Origin& requesting_origin = map_entry.first.first;
    const url::Origin& embedding_origin = map_entry.first.second;

    if (!CanRequestObjectPermission(requesting_origin, embedding_origin))
      continue;

    for (const auto& token : map_entry.second) {
      auto it = port_info_.find(token);
      if (it == port_info_.end())
        continue;

      objects.push_back(std::make_unique<Object>(
          requesting_origin, embedding_origin, it->second.Clone(),
          content_settings::SettingSource::SETTING_SOURCE_USER, is_incognito_));
    }
  }

  return objects;
}

void SerialChooserContext::RevokeObjectPermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const base::Value& object) {
  const std::string* token = object.FindStringKey(kTokenKey);
  if (!token) {
    ChooserContextBase::RevokeObjectPermission(requesting_origin,
                                               embedding_origin, object);
    RecordPermissionRevocation(SerialPermissionRevoked::kPersistent);
    return;
  }

  auto it = ephemeral_ports_.find({requesting_origin, embedding_origin});
  if (it == ephemeral_ports_.end())
    return;
  std::set<base::UnguessableToken>& ports = it->second;

  DCHECK(IsValidObject(object));
  ports.erase(DecodeToken(*token));
  RecordPermissionRevocation(SerialPermissionRevoked::kEphemeralByUser);
  NotifyPermissionRevoked(requesting_origin, embedding_origin);
}

void SerialChooserContext::GrantPortPermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const device::mojom::SerialPortInfo& port) {
  base::Value value = PortInfoToValue(port);
  port_info_.insert({port.token, value.Clone()});

  if (CanStorePersistentEntry(port)) {
    GrantObjectPermission(requesting_origin, embedding_origin,
                          std::move(value));
    return;
  }

  ephemeral_ports_[{requesting_origin, embedding_origin}].insert(port.token);
  NotifyPermissionChanged();
}

bool SerialChooserContext::HasPortPermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const device::mojom::SerialPortInfo& port) {
  if (!CanRequestObjectPermission(requesting_origin, embedding_origin)) {
    return false;
  }

  auto it = ephemeral_ports_.find({requesting_origin, embedding_origin});
  if (it != ephemeral_ports_.end()) {
    const std::set<base::UnguessableToken> ports = it->second;
    if (base::Contains(ports, port.token))
      return true;
  }

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      object_list = GetGrantedObjects(requesting_origin, embedding_origin);
  for (const auto& object : object_list) {
    const base::Value& device = object->value;
    DCHECK(IsValidObject(device));

    const std::string* persistent_id = device.FindStringKey(kPersistentIdKey);
    if (port.persistent_id == *persistent_id)
      return true;
  }
  return false;
}

// static
bool SerialChooserContext::CanStorePersistentEntry(
    const device::mojom::SerialPortInfo& port) {
  // If there is no display name then the path name will be used instead. The
  // path name is not guaranteed to be stable. For example, on Linux the name
  // "ttyUSB0" is reused for any USB serial device. A name like that would be
  // confusing to show in settings when the device is disconnected.
  if (!port.display_name || port.display_name->empty())
    return false;

  return port.persistent_id && !port.persistent_id->empty();
}

device::mojom::SerialPortManager* SerialChooserContext::GetPortManager() {
  EnsurePortManagerConnection();
  return port_manager_.get();
}

void SerialChooserContext::AddPortObserver(PortObserver* observer) {
  port_observer_list_.AddObserver(observer);
}

void SerialChooserContext::RemovePortObserver(PortObserver* observer) {
  port_observer_list_.RemoveObserver(observer);
}

void SerialChooserContext::SetPortManagerForTesting(
    mojo::PendingRemote<device::mojom::SerialPortManager> manager) {
  SetUpPortManagerConnection(std::move(manager));
}

void SerialChooserContext::FlushPortManagerConnectionForTesting() {
  port_manager_.FlushForTesting();
}

base::WeakPtr<SerialChooserContext> SerialChooserContext::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void SerialChooserContext::OnPortAdded(device::mojom::SerialPortInfoPtr port) {
  for (auto& observer : port_observer_list_)
    observer.OnPortAdded(*port);
}

void SerialChooserContext::OnPortRemoved(
    device::mojom::SerialPortInfoPtr port) {
  for (auto& observer : port_observer_list_)
    observer.OnPortRemoved(*port);

  std::vector<std::pair<url::Origin, url::Origin>> revoked_url_pairs;
  for (auto& map_entry : ephemeral_ports_) {
    std::set<base::UnguessableToken>& ports = map_entry.second;
    if (ports.erase(port->token) > 0) {
      RecordPermissionRevocation(
          SerialPermissionRevoked::kEphemeralByDisconnect);
      revoked_url_pairs.push_back(map_entry.first);
    }
  }

  port_info_.erase(port->token);

  for (auto& observer : permission_observer_list_) {
    if (!revoked_url_pairs.empty()) {
      observer.OnChooserObjectPermissionChanged(guard_content_settings_type_,
                                                data_content_settings_type_);
    }
    for (const auto& url_pair : revoked_url_pairs)
      observer.OnPermissionRevoked(url_pair.first, url_pair.second);
  }
}

void SerialChooserContext::EnsurePortManagerConnection() {
  if (port_manager_)
    return;

  mojo::PendingRemote<device::mojom::SerialPortManager> manager;
  content::GetDeviceService().BindSerialPortManager(
      manager.InitWithNewPipeAndPassReceiver());
  SetUpPortManagerConnection(std::move(manager));
}

void SerialChooserContext::SetUpPortManagerConnection(
    mojo::PendingRemote<device::mojom::SerialPortManager> manager) {
  port_manager_.Bind(std::move(manager));
  port_manager_.set_disconnect_handler(
      base::BindOnce(&SerialChooserContext::OnPortManagerConnectionError,
                     base::Unretained(this)));

  port_manager_->SetClient(client_receiver_.BindNewPipeAndPassRemote());
}

void SerialChooserContext::OnPortManagerConnectionError() {
  port_manager_.reset();
  client_receiver_.reset();

  port_info_.clear();

  std::vector<std::pair<url::Origin, url::Origin>> revoked_origins;
  revoked_origins.reserve(ephemeral_ports_.size());
  for (const auto& map_entry : ephemeral_ports_)
    revoked_origins.push_back(map_entry.first);
  ephemeral_ports_.clear();

  // Notify permission observers that all ephemeral permissions have been
  // revoked.
  for (auto& observer : permission_observer_list_) {
    observer.OnChooserObjectPermissionChanged(guard_content_settings_type_,
                                              data_content_settings_type_);
    for (const auto& origin : revoked_origins)
      observer.OnPermissionRevoked(origin.first, origin.second);
  }
}
