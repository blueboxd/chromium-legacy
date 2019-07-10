// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/mpris/mpris_service_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/process/process.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "dbus/values_util.h"
#include "ui/base/mpris/mpris_service_observer.h"

namespace mpris {

namespace {

constexpr int kDebounceTimeoutMilliseconds = 50;
constexpr int kNumMethodsToExport = 14;

}  // namespace

// static
MprisServiceImpl* MprisServiceImpl::GetInstance() {
  return base::Singleton<MprisServiceImpl>::get();
}

MprisServiceImpl::MprisServiceImpl()
    : service_name_(std::string(kMprisAPIServiceNamePrefix) +
                    std::to_string(base::Process::Current().Pid())) {
  InitializeProperties();
}

MprisServiceImpl::~MprisServiceImpl() {
  if (bus_) {
    dbus_thread_linux::GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&dbus::Bus::ShutdownAndBlock, bus_));
  }
}

void MprisServiceImpl::StartService() {
  InitializeDbusInterface();
}

void MprisServiceImpl::AddObserver(MprisServiceObserver* observer) {
  observers_.AddObserver(observer);

  // If the service is already ready, inform the observer.
  if (service_ready_)
    observer->OnServiceReady();
}

void MprisServiceImpl::RemoveObserver(MprisServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MprisServiceImpl::SetCanGoNext(bool value) {
  SetPropertyInternal(media_player2_player_properties_, "CanGoNext",
                      base::Value(value));
}

void MprisServiceImpl::SetCanGoPrevious(bool value) {
  SetPropertyInternal(media_player2_player_properties_, "CanGoPrevious",
                      base::Value(value));
}

void MprisServiceImpl::SetCanPlay(bool value) {
  SetPropertyInternal(media_player2_player_properties_, "CanPlay",
                      base::Value(value));
}

void MprisServiceImpl::SetCanPause(bool value) {
  SetPropertyInternal(media_player2_player_properties_, "CanPause",
                      base::Value(value));
}

void MprisServiceImpl::SetPlaybackStatus(PlaybackStatus value) {
  base::Value status;
  switch (value) {
    case PlaybackStatus::kPlaying:
      status = base::Value("Playing");
      break;
    case PlaybackStatus::kPaused:
      status = base::Value("Paused");
      break;
    case PlaybackStatus::kStopped:
      status = base::Value("Stopped");
      break;
  }
  SetPropertyInternal(media_player2_player_properties_, "PlaybackStatus",
                      status);
}

void MprisServiceImpl::SetTitle(const base::string16& value) {
  SetMetadataPropertyInternal("xesam:title", base::Value(value));
}

void MprisServiceImpl::SetArtist(const base::string16& value) {
  SetMetadataPropertyInternal("xesam:artist", base::Value(value));
}

void MprisServiceImpl::SetAlbum(const base::string16& value) {
  SetMetadataPropertyInternal("xesam:album", base::Value(value));
}

std::string MprisServiceImpl::GetServiceName() const {
  return service_name_;
}

void MprisServiceImpl::InitializeProperties() {
  // org.mpris.MediaPlayer2 interface properties.
  media_player2_properties_["CanQuit"] = base::Value(false);
  media_player2_properties_["CanRaise"] = base::Value(false);
  media_player2_properties_["HasTrackList"] = base::Value(false);

#if defined(GOOGLE_CHROME_BUILD)
  media_player2_properties_["Identity"] = base::Value("Chrome");
#else
  media_player2_properties_["Identity"] = base::Value("Chromium");
#endif
  media_player2_properties_["SupportedUriSchemes"] =
      base::Value(base::Value::Type::LIST);
  media_player2_properties_["SupportedMimeTypes"] =
      base::Value(base::Value::Type::LIST);

  // org.mpris.MediaPlayer2.Player interface properties.
  media_player2_player_properties_["PlaybackStatus"] = base::Value("Stopped");
  media_player2_player_properties_["Rate"] = base::Value(1.0);
  media_player2_player_properties_["Metadata"] =
      base::Value(base::Value::DictStorage());
  media_player2_player_properties_["Volume"] = base::Value(1.0);
  media_player2_player_properties_["Position"] = base::Value(0);
  media_player2_player_properties_["MinimumRate"] = base::Value(1.0);
  media_player2_player_properties_["MaximumRate"] = base::Value(1.0);
  media_player2_player_properties_["CanGoNext"] = base::Value(false);
  media_player2_player_properties_["CanGoPrevious"] = base::Value(false);
  media_player2_player_properties_["CanPlay"] = base::Value(false);
  media_player2_player_properties_["CanPause"] = base::Value(false);
  media_player2_player_properties_["CanSeek"] = base::Value(false);
  media_player2_player_properties_["CanControl"] = base::Value(true);
}

void MprisServiceImpl::InitializeDbusInterface() {
  // Bus may be set for testing.
  if (!bus_) {
    dbus::Bus::Options bus_options;
    bus_options.bus_type = dbus::Bus::SESSION;
    bus_options.connection_type = dbus::Bus::PRIVATE;
    bus_options.dbus_task_runner = dbus_thread_linux::GetTaskRunner();
    bus_ = base::MakeRefCounted<dbus::Bus>(bus_options);
  }

  exported_object_ =
      bus_->GetExportedObject(dbus::ObjectPath(kMprisAPIObjectPath));
  int num_methods_attempted_to_export = 0;

  // Helper lambdas for exporting methods while keeping track of the number of
  // exported methods.
  auto export_method =
      [&](const std::string& interface_name, const std::string& method_name,
          dbus::ExportedObject::MethodCallCallback method_call_callback) {
        exported_object_->ExportMethod(
            interface_name, method_name, method_call_callback,
            base::BindRepeating(&MprisServiceImpl::OnExported,
                                base::Unretained(this)));
        num_methods_attempted_to_export++;
      };
  auto export_unhandled_method = [&](const std::string& interface_name,
                                     const std::string& method_name) {
    export_method(interface_name, method_name,
                  base::BindRepeating(&MprisServiceImpl::DoNothing,
                                      base::Unretained(this)));
  };

  // Set up org.mpris.MediaPlayer2 interface.
  // https://specifications.freedesktop.org/mpris-spec/2.2/Media_Player.html
  export_unhandled_method(kMprisAPIInterfaceName, "Raise");
  export_unhandled_method(kMprisAPIInterfaceName, "Quit");

  // Set up org.mpris.MediaPlayer2.Player interface.
  // https://specifications.freedesktop.org/mpris-spec/2.2/Player_Interface.html
  export_method(
      kMprisAPIPlayerInterfaceName, "Next",
      base::BindRepeating(&MprisServiceImpl::Next, base::Unretained(this)));
  export_method(
      kMprisAPIPlayerInterfaceName, "Previous",
      base::BindRepeating(&MprisServiceImpl::Previous, base::Unretained(this)));
  export_method(
      kMprisAPIPlayerInterfaceName, "Pause",
      base::BindRepeating(&MprisServiceImpl::Pause, base::Unretained(this)));
  export_method(kMprisAPIPlayerInterfaceName, "PlayPause",
                base::BindRepeating(&MprisServiceImpl::PlayPause,
                                    base::Unretained(this)));
  export_method(
      kMprisAPIPlayerInterfaceName, "Stop",
      base::BindRepeating(&MprisServiceImpl::Stop, base::Unretained(this)));
  export_method(
      kMprisAPIPlayerInterfaceName, "Play",
      base::BindRepeating(&MprisServiceImpl::Play, base::Unretained(this)));
  export_unhandled_method(kMprisAPIPlayerInterfaceName, "Seek");
  export_unhandled_method(kMprisAPIPlayerInterfaceName, "SetPosition");
  export_unhandled_method(kMprisAPIPlayerInterfaceName, "OpenUri");

  // Set up org.freedesktop.DBus.Properties interface.
  // https://dbus.freedesktop.org/doc/dbus-specification.html#standard-interfaces-properties
  export_method(dbus::kPropertiesInterface, dbus::kPropertiesGetAll,
                base::BindRepeating(&MprisServiceImpl::GetAllProperties,
                                    base::Unretained(this)));
  export_method(dbus::kPropertiesInterface, dbus::kPropertiesGet,
                base::BindRepeating(&MprisServiceImpl::GetProperty,
                                    base::Unretained(this)));
  export_unhandled_method(dbus::kPropertiesInterface, dbus::kPropertiesSet);

  DCHECK_EQ(kNumMethodsToExport, num_methods_attempted_to_export);
}

void MprisServiceImpl::OnExported(const std::string& interface_name,
                                  const std::string& method_name,
                                  bool success) {
  if (!success) {
    service_failed_to_start_ = true;
    return;
  }

  num_methods_exported_++;

  // Still waiting for more methods to finish exporting.
  if (num_methods_exported_ < kNumMethodsToExport)
    return;

  bus_->RequestOwnership(service_name_,
                         dbus::Bus::ServiceOwnershipOptions::REQUIRE_PRIMARY,
                         base::BindRepeating(&MprisServiceImpl::OnOwnership,
                                             base::Unretained(this)));
}

void MprisServiceImpl::OnOwnership(const std::string& service_name,
                                   bool success) {
  if (!success) {
    service_failed_to_start_ = true;
    return;
  }

  service_ready_ = true;

  for (MprisServiceObserver& obs : observers_)
    obs.OnServiceReady();
}

void MprisServiceImpl::Next(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  for (MprisServiceObserver& obs : observers_)
    obs.OnNext();
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

void MprisServiceImpl::Previous(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  for (MprisServiceObserver& obs : observers_)
    obs.OnPrevious();
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

void MprisServiceImpl::Pause(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  for (MprisServiceObserver& obs : observers_)
    obs.OnPause();
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

void MprisServiceImpl::PlayPause(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  for (MprisServiceObserver& obs : observers_)
    obs.OnPlayPause();
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

void MprisServiceImpl::Stop(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  for (MprisServiceObserver& obs : observers_)
    obs.OnStop();
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

void MprisServiceImpl::Play(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  for (MprisServiceObserver& obs : observers_)
    obs.OnPlay();
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

void MprisServiceImpl::DoNothing(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

// org.freedesktop.DBus.Properties.GetAll(in STRING interface_name,
//                                        out DICT<STRING,VARIANT> props);
void MprisServiceImpl::GetAllProperties(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  std::string interface;
  if (!reader.PopString(&interface)) {
    response_sender.Run(nullptr);
    return;
  }

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());

  if (interface == kMprisAPIInterfaceName) {
    AddPropertiesToWriter(&writer, media_player2_properties_);
  } else if (interface == kMprisAPIPlayerInterfaceName) {
    AddPropertiesToWriter(&writer, media_player2_player_properties_);
  } else if (interface == dbus::kPropertiesInterface) {
    // There are no properties to give for this interface.
    PropertyMap empty_properties_map;
    AddPropertiesToWriter(&writer, empty_properties_map);
  } else {
    // The given interface is not supported, so return a null response.
    response_sender.Run(nullptr);
    return;
  }

  response_sender.Run(std::move(response));
}

// org.freedesktop.DBus.Properties.Get(in STRING interface_name,
//                                     in STRING property_name,
//                                     out VARIANT value);
void MprisServiceImpl::GetProperty(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  std::string interface;
  if (!reader.PopString(&interface)) {
    response_sender.Run(nullptr);
    return;
  }

  std::string property_name;
  if (!reader.PopString(&property_name)) {
    response_sender.Run(nullptr);
    return;
  }

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());

  bool success = false;
  if (interface == kMprisAPIInterfaceName) {
    auto property_iter = media_player2_properties_.find(property_name);
    if (property_iter != media_player2_properties_.end()) {
      dbus::AppendValueDataAsVariant(&writer, property_iter->second);
      success = true;
    }
  } else if (interface == kMprisAPIPlayerInterfaceName) {
    auto property_iter = media_player2_player_properties_.find(property_name);
    if (property_iter != media_player2_player_properties_.end()) {
      dbus::AppendValueDataAsVariant(&writer, property_iter->second);
      success = true;
    }
  }

  // If we don't support the given property, return a null response.
  if (!success) {
    response_sender.Run(nullptr);
    return;
  }

  response_sender.Run(std::move(response));
}

void MprisServiceImpl::AddPropertiesToWriter(dbus::MessageWriter* writer,
                                             const PropertyMap& properties) {
  DCHECK(writer);

  dbus::MessageWriter array_writer(nullptr);
  dbus::MessageWriter dict_entry_writer(nullptr);

  writer->OpenArray("{sv}", &array_writer);

  for (auto& property : properties) {
    array_writer.OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(property.first);
    dbus::AppendValueDataAsVariant(&dict_entry_writer, property.second);
    array_writer.CloseContainer(&dict_entry_writer);
  }

  writer->CloseContainer(&array_writer);
}

void MprisServiceImpl::SetPropertyInternal(PropertyMap& property_map,
                                           const std::string& property_name,
                                           const base::Value& new_value) {
  if (property_map[property_name] == new_value)
    return;

  property_map[property_name] = new_value.Clone();

  changed_properties_.insert(property_name);

  EmitPropertiesChangedSignalDebounced();
}

void MprisServiceImpl::SetMetadataPropertyInternal(
    const std::string& property_name,
    const base::Value& new_value) {
  base::Value& metadata = media_player2_player_properties_["Metadata"];
  base::Value* current_value = metadata.FindKey(property_name);

  if (current_value && *current_value == new_value)
    return;
  metadata.SetKey(property_name, new_value.Clone());

  changed_properties_.insert("Metadata");

  EmitPropertiesChangedSignalDebounced();
}

void MprisServiceImpl::EmitPropertiesChangedSignalDebounced() {
  properties_changed_debounce_timer_.Stop();
  properties_changed_debounce_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kDebounceTimeoutMilliseconds), this,
      &MprisServiceImpl::EmitPropertiesChangedSignal);
}

void MprisServiceImpl::EmitPropertiesChangedSignal() {
  // If we're still trying to start the service, delay emitting.
  if (!service_ready_ && !service_failed_to_start_) {
    EmitPropertiesChangedSignalDebounced();
    return;
  }

  if (!bus_ || !exported_object_ || !service_ready_)
    return;

  PropertyMap changed_property_map;
  for (std::string property_name : changed_properties_) {
    changed_property_map[property_name] =
        media_player2_player_properties_[property_name].Clone();
  }
  changed_properties_.clear();

  // |signal| follows the PropertiesChanged API:
  // org.freedesktop.DBus.Properties.PropertiesChanged(
  //     STRING interface_name,
  //     DICT<STRING,VARIANT> changed_properties,
  //     ARRAY<STRING> invalidated_properties);
  dbus::Signal signal(dbus::kPropertiesInterface, dbus::kPropertiesChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendString(kMprisAPIPlayerInterfaceName);

  AddPropertiesToWriter(&writer, changed_property_map);

  std::vector<std::string> empty_invalidated_properties;
  writer.AppendArrayOfStrings(empty_invalidated_properties);

  exported_object_->SendSignal(&signal);
}

}  // namespace mpris
