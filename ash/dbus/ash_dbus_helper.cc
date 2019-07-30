// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/dbus/ash_dbus_helper.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/system/sys_info.h"
#include "base/threading/thread.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "dbus/bus.h"
#include "dbus/dbus_statistics.h"

namespace ash {

// static
std::unique_ptr<AshDBusHelper> AshDBusHelper::CreateWithExistingBus(
    scoped_refptr<dbus::Bus> bus) {
  bool use_real_clients = bus != nullptr;
  // Use WrapUnique so that the constructor can be made private.
  std::unique_ptr<AshDBusHelper> helper =
      base::WrapUnique(new AshDBusHelper(use_real_clients));
  helper->bus_ = bus;
  return helper;
}

// static
std::unique_ptr<AshDBusHelper> AshDBusHelper::Create() {
  bool use_real_clients = base::SysInfo::IsRunningOnChromeOS() &&
                          !base::CommandLine::ForCurrentProcess()->HasSwitch(
                              chromeos::switches::kDbusStub);
  // Use WrapUnique so that the constructor can be made private.
  std::unique_ptr<AshDBusHelper> helper =
      base::WrapUnique(new AshDBusHelper(use_real_clients));
  helper->InitializeDBus();
  return helper;
}

AshDBusHelper::AshDBusHelper(bool use_real_clients)
    : use_real_clients_(use_real_clients) {}

void AshDBusHelper::InitializeDBus() {
  dbus::statistics::Initialize();
  if (!use_real_clients_)
    return;

  // Create the D-Bus thread.
  base::Thread::Options thread_options;
  thread_options.message_pump_type = base::MessagePumpType::IO;
  dbus_thread_ = std::make_unique<base::Thread>("D-Bus thread");
  dbus_thread_->StartWithOptions(thread_options);

  // Create the connection to the system bus.
  dbus::Bus::Options bus_options;
  bus_options.bus_type = dbus::Bus::SYSTEM;
  bus_options.connection_type = dbus::Bus::PRIVATE;
  bus_options.dbus_task_runner = dbus_thread_->task_runner();
  bus_ = base::MakeRefCounted<dbus::Bus>(bus_options);
}

AshDBusHelper::~AshDBusHelper() {
  dbus::statistics::Shutdown();
}

}  // namespace ash
