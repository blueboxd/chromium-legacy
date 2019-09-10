// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/mpris/mpris_service_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/singleton.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "components/dbus/properties/dbus_properties.h"
#include "components/dbus/properties/success_barrier_callback.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "ui/base/mpris/mpris_service_observer.h"

namespace mpris {

namespace {

constexpr int kNumMethodsToExport = 11;

}  // namespace

// static
MprisServiceImpl* MprisServiceImpl::GetInstance() {
  return base::Singleton<MprisServiceImpl>::get();
}

MprisServiceImpl::MprisServiceImpl()
    : service_name_(std::string(kMprisAPIServiceNamePrefix) +
                    base::NumberToString(base::Process::Current().Pid())) {}

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
  properties_->SetProperty(kMprisAPIPlayerInterfaceName, "CanGoNext",
                           DbusBoolean(value));
}

void MprisServiceImpl::SetCanGoPrevious(bool value) {
  properties_->SetProperty(kMprisAPIPlayerInterfaceName, "CanGoPrevious",
                           DbusBoolean(value));
}

void MprisServiceImpl::SetCanPlay(bool value) {
  properties_->SetProperty(kMprisAPIPlayerInterfaceName, "CanPlay",
                           DbusBoolean(value));
}

void MprisServiceImpl::SetCanPause(bool value) {
  properties_->SetProperty(kMprisAPIPlayerInterfaceName, "CanPause",
                           DbusBoolean(value));
}

void MprisServiceImpl::SetPlaybackStatus(PlaybackStatus value) {
  auto status = [&]() {
    switch (value) {
      case PlaybackStatus::kPlaying:
        return DbusString("Playing");
      case PlaybackStatus::kPaused:
        return DbusString("Paused");
      case PlaybackStatus::kStopped:
        return DbusString("Stopped");
    }
  };
  properties_->SetProperty(kMprisAPIPlayerInterfaceName, "PlaybackStatus",
                           status());
}

void MprisServiceImpl::SetTitle(const base::string16& value) {
  SetMetadataPropertyInternal(
      "xesam:title", MakeDbusVariant(DbusString(base::UTF16ToUTF8(value))));
}

void MprisServiceImpl::SetArtist(const base::string16& value) {
  SetMetadataPropertyInternal(
      "xesam:artist",
      MakeDbusVariant(MakeDbusArray(DbusString(base::UTF16ToUTF8(value)))));
}

void MprisServiceImpl::SetAlbum(const base::string16& value) {
  SetMetadataPropertyInternal(
      "xesam:album", MakeDbusVariant(DbusString(base::UTF16ToUTF8(value))));
}

std::string MprisServiceImpl::GetServiceName() const {
  return service_name_;
}

void MprisServiceImpl::InitializeProperties() {
  // org.mpris.MediaPlayer2 interface properties.
  auto set_property = [&](const std::string& property_name, auto&& value) {
    properties_->SetProperty(kMprisAPIInterfaceName, property_name,
                             std::move(value), false);
  };
  set_property("CanQuit", DbusBoolean(false));
  set_property("CanRaise", DbusBoolean(false));
  set_property("HasTrackList", DbusBoolean(false));

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  set_property("Identity", DbusString("Chrome"));
#else
  set_property("Identity", DbusString("Chromium"));
#endif
  set_property("SupportedUriSchemes", DbusArray<DbusString>());
  set_property("SupportedMimeTypes", DbusArray<DbusString>());

  // org.mpris.MediaPlayer2.Player interface properties.
  auto set_player_property = [&](const std::string& property_name,
                                 auto&& value) {
    properties_->SetProperty(kMprisAPIPlayerInterfaceName, property_name,
                             std::move(value), false);
  };
  set_player_property("PlaybackStatus", DbusString("Stopped"));
  set_player_property("Rate", DbusDouble(1.0));
  set_player_property("Metadata", DbusDictionary());
  set_player_property("Volume", DbusDouble(1.0));
  set_player_property("Position", DbusInt64(0));
  set_player_property("MinimumRate", DbusDouble(1.0));
  set_player_property("MaximumRate", DbusDouble(1.0));
  set_player_property("CanGoNext", DbusBoolean(false));
  set_player_property("CanGoPrevious", DbusBoolean(false));
  set_player_property("CanPlay", DbusBoolean(false));
  set_player_property("CanPause", DbusBoolean(false));
  set_player_property("CanSeek", DbusBoolean(false));
  set_player_property("CanControl", DbusBoolean(false));
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

  // kNumMethodsToExport calls for method export, 1 call for properties
  // initialization.
  barrier_ = SuccessBarrierCallback(
      kNumMethodsToExport + 1,
      base::BindOnce(&MprisServiceImpl::OnInitialized, base::Unretained(this)));

  properties_ = std::make_unique<DbusProperties>(exported_object_, barrier_);
  properties_->RegisterInterface(kMprisAPIInterfaceName);
  properties_->RegisterInterface(kMprisAPIPlayerInterfaceName);
  InitializeProperties();

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

  DCHECK_EQ(kNumMethodsToExport, num_methods_attempted_to_export);
}

void MprisServiceImpl::OnExported(const std::string& interface_name,
                                  const std::string& method_name,
                                  bool success) {
  barrier_.Run(success);
}

void MprisServiceImpl::OnInitialized(bool success) {
  if (success) {
    bus_->RequestOwnership(service_name_,
                           dbus::Bus::ServiceOwnershipOptions::REQUIRE_PRIMARY,
                           base::BindRepeating(&MprisServiceImpl::OnOwnership,
                                               base::Unretained(this)));
  }
}

void MprisServiceImpl::OnOwnership(const std::string& service_name,
                                   bool success) {
  if (!success)
    return;

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

void MprisServiceImpl::SetMetadataPropertyInternal(
    const std::string& property_name,
    DbusVariant&& new_value) {
  DbusVariant* dictionary_variant =
      properties_->GetProperty(kMprisAPIPlayerInterfaceName, "Metadata");
  DCHECK(dictionary_variant);
  DbusDictionary* dictionary = dictionary_variant->GetAs<DbusDictionary>();
  DCHECK(dictionary);
  if (dictionary->Put(property_name, std::move(new_value)))
    properties_->PropertyUpdated(kMprisAPIPlayerInterfaceName, "Metadata");
}

}  // namespace mpris
