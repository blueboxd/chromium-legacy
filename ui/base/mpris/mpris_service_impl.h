// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MPRIS_MPRIS_SERVICE_IMPL_H_
#define UI_BASE_MPRIS_MPRIS_SERVICE_IMPL_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "ui/base/mpris/mpris_service.h"

namespace base {
class DictionaryValue;
class Value;
}  // namespace base

namespace dbus {
class MessageWriter;
class MethodCall;
}  // namespace dbus

namespace mpris {

class MprisServiceObserver;

// A D-Bus service conforming to the MPRIS spec:
// https://specifications.freedesktop.org/mpris-spec/latest/
class COMPONENT_EXPORT(MPRIS) MprisServiceImpl : public MprisService {
 public:
  MprisServiceImpl();
  ~MprisServiceImpl() override;

  static MprisServiceImpl* GetInstance();

  // MprisService implementation.
  void StartService() override;
  void AddObserver(MprisServiceObserver* observer) override;
  void RemoveObserver(MprisServiceObserver* observer) override;
  void SetCanGoNext(bool value) override;
  void SetCanGoPrevious(bool value) override;
  void SetCanPlay(bool value) override;
  void SetCanPause(bool value) override;
  void SetPlaybackStatus(PlaybackStatus value) override;
  void SetTitle(const base::string16& value) override;
  void SetArtist(const base::string16& value) override;
  void SetAlbum(const base::string16& value) override;
  std::string GetServiceName() const override;

  // Used for testing with a mock DBus Bus.
  void SetBusForTesting(scoped_refptr<dbus::Bus> bus) { bus_ = bus; }

 private:
  void InitializeProperties();
  void InitializeDbusInterface();
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);
  void OnOwnership(const std::string& service_name, bool success);

  // org.mpris.MediaPlayer2.Player interface.
  void Next(dbus::MethodCall* method_call,
            dbus::ExportedObject::ResponseSender response_sender);
  void Previous(dbus::MethodCall* method_call,
                dbus::ExportedObject::ResponseSender response_sender);
  void Pause(dbus::MethodCall* method_call,
             dbus::ExportedObject::ResponseSender response_sender);
  void PlayPause(dbus::MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender response_sender);
  void Stop(dbus::MethodCall* method_call,
            dbus::ExportedObject::ResponseSender response_sender);
  void Play(dbus::MethodCall* method_call,
            dbus::ExportedObject::ResponseSender response_sender);

  // Used for API methods we don't support.
  void DoNothing(dbus::MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender response_sender);

  // org.freedesktop.DBus.Properties interface.
  void GetAllProperties(dbus::MethodCall* method_call,
                        dbus::ExportedObject::ResponseSender response_sender);
  void GetProperty(dbus::MethodCall* method_call,
                   dbus::ExportedObject::ResponseSender response_sender);

  using PropertyMap = base::flat_map<std::string, base::Value>;

  // Sets a value on the given PropertyMap and sends a PropertiesChanged signal
  // if necessary.
  void SetPropertyInternal(PropertyMap& property_map,
                           const std::string& property_name,
                           const base::Value& new_value);

  // Sets a value on the Metadata property map and sends a PropertiesChanged
  // signal if necessary.
  void SetMetadataPropertyInternal(const std::string& property_name,
                                   const base::Value& new_value);

  // Updates a timer to debounce calls to |EmitPropertiesChangedSignal|.
  void EmitPropertiesChangedSignalDebounced();

  // Emits a org.freedesktop.DBus.Properties.PropertiesChanged signal for the
  // given map of changed properties.
  void EmitPropertiesChangedSignal();

  // Writes all properties onto writer.
  void AddPropertiesToWriter(dbus::MessageWriter* writer,
                             const PropertyMap& properties);

  // Writes the metadata property onto writer. Metadata is handled differently
  // than other properties since it has sub-properties that need to be handled
  // as non-variants.
  void AddMetadataToWriter(dbus::MessageWriter* writer,
                           const base::DictionaryValue* metadata);

  // Map of org.mpris.MediaPlayer2 interface properties.
  PropertyMap media_player2_properties_;

  // Map of org.mpris.MediaPlayer2.Player interface properties.
  PropertyMap media_player2_player_properties_;

  scoped_refptr<dbus::Bus> bus_;
  dbus::ExportedObject* exported_object_;

  // The generated service name given to |bus_| when requesting ownership.
  const std::string service_name_;

  // The number of methods that have been successfully exported through
  // |exported_object_|.
  int num_methods_exported_ = 0;

  // True if we have finished creating the DBus service and received ownership.
  bool service_ready_ = false;

  // True if we failed to start the MPRIS DBus service.
  bool service_failed_to_start_ = false;

  // Used to only send 1 PropertiesChanged signal when many properties are
  // changed at once.
  base::OneShotTimer properties_changed_debounce_timer_;

  // Holds a list of properties that have changed since the last time we emitted
  // a PropertiesChanged signal.
  base::flat_set<std::string> changed_properties_;

  base::ObserverList<MprisServiceObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(MprisServiceImpl);
};

}  // namespace mpris

#endif  // UI_BASE_MPRIS_MPRIS_SERVICE_IMPL_H_
