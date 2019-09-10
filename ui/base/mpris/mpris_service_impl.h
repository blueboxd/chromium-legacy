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
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "components/dbus/properties/types.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "ui/base/mpris/mpris_service.h"

class DbusProperties;

namespace dbus {
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
  void OnInitialized(bool success);
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

  // Sets a value on the Metadata property map and sends a PropertiesChanged
  // signal if necessary.
  void SetMetadataPropertyInternal(const std::string& property_name,
                                   DbusVariant&& new_value);

  std::unique_ptr<DbusProperties> properties_;

  scoped_refptr<dbus::Bus> bus_;
  dbus::ExportedObject* exported_object_;

  // The generated service name given to |bus_| when requesting ownership.
  const std::string service_name_;

  base::RepeatingCallback<void(bool)> barrier_;

  // True if we have finished creating the DBus service and received ownership.
  bool service_ready_ = false;

  base::ObserverList<MprisServiceObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(MprisServiceImpl);
};

}  // namespace mpris

#endif  // UI_BASE_MPRIS_MPRIS_SERVICE_IMPL_H_
