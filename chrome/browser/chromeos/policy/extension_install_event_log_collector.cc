// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/extension_install_event_log_collector.h"

#include "base/command_line.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"

namespace em = enterprise_management;

namespace policy {

namespace {

std::unique_ptr<em::ExtensionInstallReportLogEvent> CreateSessionChangeEvent(
    em::ExtensionInstallReportLogEvent::SessionStateChangeType type) {
  auto event = std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->set_event_type(
      em::ExtensionInstallReportLogEvent::SESSION_STATE_CHANGE);
  event->set_session_state_change_type(type);
  return event;
}

bool GetOnlineState() {
  chromeos::NetworkStateHandler::NetworkStateList network_state_list;
  chromeos::NetworkHandler::Get()
      ->network_state_handler()
      ->GetNetworkListByType(
          chromeos::NetworkTypePattern::Default(), true /* configured_only */,
          false /* visible_only */, 0 /* limit */, &network_state_list);
  for (const chromeos::NetworkState* network_state : network_state_list) {
    if (network_state->connection_state() == shill::kStateOnline) {
      return true;
    }
  }
  return false;
}

}  // namespace

ExtensionInstallEventLogCollector::ExtensionInstallEventLogCollector(
    extensions::ExtensionRegistry* registry,
    Delegate* delegate,
    Profile* profile)
    : registry_(registry),
      delegate_(delegate),
      profile_(profile),
      online_(GetOnlineState()) {
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
  registry_observer_.Add(registry_);
  collector_observer_.Add(extensions::InstallStageTracker::Get(profile_));
}

ExtensionInstallEventLogCollector::~ExtensionInstallEventLogCollector() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
}

void ExtensionInstallEventLogCollector::AddLoginEvent() {
  // Don't log in case session is restarted or recovered from crash.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kLoginUser) ||
      profile_->GetLastSessionExitType() == Profile::EXIT_CRASHED) {
    return;
  }
  online_ = GetOnlineState();
  std::unique_ptr<em::ExtensionInstallReportLogEvent> event =
      CreateSessionChangeEvent(em::ExtensionInstallReportLogEvent::LOGIN);
  event->set_online(online_);
  delegate_->AddForAllExtensions(std::move(event));
}

void ExtensionInstallEventLogCollector::AddLogoutEvent() {
  // Don't log in case session is restared.
  if (g_browser_process->local_state()->GetBoolean(prefs::kWasRestarted))
    return;

  delegate_->AddForAllExtensions(
      CreateSessionChangeEvent(em::ExtensionInstallReportLogEvent::LOGOUT));
}

void ExtensionInstallEventLogCollector::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  delegate_->AddForAllExtensions(
      CreateSessionChangeEvent(em::ExtensionInstallReportLogEvent::SUSPEND));
}

void ExtensionInstallEventLogCollector::SuspendDone(
    const base::TimeDelta& sleep_duration) {
  delegate_->AddForAllExtensions(
      CreateSessionChangeEvent(em::ExtensionInstallReportLogEvent::RESUME));
}

void ExtensionInstallEventLogCollector::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  const bool currently_online = GetOnlineState();
  if (currently_online == online_)
    return;
  online_ = currently_online;

  std::unique_ptr<em::ExtensionInstallReportLogEvent> event =
      std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->set_event_type(
      em::ExtensionInstallReportLogEvent::CONNECTIVITY_CHANGE);
  event->set_online(online_);
  delegate_->AddForAllExtensions(std::move(event));
}

void ExtensionInstallEventLogCollector::OnExtensionInstallationFailed(
    const extensions::ExtensionId& extension_id,
    extensions::InstallStageTracker::FailureReason reason) {
  if (!delegate_->IsExtensionPending(extension_id))
    return;
  auto event = std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->set_event_type(
      em::ExtensionInstallReportLogEvent::INSTALLATION_FAILED);
  delegate_->Add(extension_id, true /* gather_disk_space_info */,
                 std::move(event));
  delegate_->OnExtensionInstallationFinished(extension_id);
}

void ExtensionInstallEventLogCollector::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (!delegate_->IsExtensionPending(extension->id()))
    return;
  AddSuccessEvent(extension->id());
}

void ExtensionInstallEventLogCollector::OnExtensionsRequested(
    const extensions::ExtensionIdSet& extension_ids) {
  for (const auto& extension_id : extension_ids) {
    if (registry_->enabled_extensions().Contains(extension_id))
      AddSuccessEvent(extension_id);
  }
}

void ExtensionInstallEventLogCollector::AddSuccessEvent(
    const extensions::ExtensionId& id) {
  auto event = std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  delegate_->Add(id, true /* gather_disk_space_info */, std::move(event));
  delegate_->OnExtensionInstallationFinished(id);
}

}  // namespace policy
