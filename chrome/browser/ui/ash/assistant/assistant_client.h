// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/ash/assistant/device_actions.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "mojo/public/cpp/bindings/binding.h"

class Profile;
class AssistantImageDownloader;
class AssistantSetup;

// Class to handle all assistant in-browser-process functionalities.
class AssistantClient : chromeos::assistant::mojom::Client,
                        public signin::IdentityManager::Observer,
                        public session_manager::SessionManagerObserver {
 public:
  static AssistantClient* Get();

  AssistantClient();
  ~AssistantClient() override;

  void MaybeInit(Profile* profile);
  void MaybeStartAssistantOptInFlow();

  // assistant::mojom::Client overrides:
  void OnAssistantStatusChanged(bool running) override;
  void RequestAssistantStructure(
      RequestAssistantStructureCallback callback) override;

 private:
  // signin::IdentityManager::Observer:
  // Retry to initiate Assistant service when account info has been updated.
  // This is necessary if previous calls of MaybeInit() failed due to Assistant
  // disallowed by account type. This can happen when the chromeos sign-in
  // finished before account info fetching is finished (|hosted_domain| field
  // will be empty under this case).
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

  // session_manager::SessionManagerObserver:
  void OnUserProfileLoaded(const AccountId& account_id) override;
  void OnPrimaryUserSessionStarted() override;

  mojo::Binding<chromeos::assistant::mojom::Client> client_binding_;
  chromeos::assistant::mojom::AssistantPlatformPtr assistant_connection_;

  DeviceActions device_actions_;

  std::unique_ptr<AssistantImageDownloader> assistant_image_downloader_;
  std::unique_ptr<AssistantSetup> assistant_setup_;

  bool initialized_ = false;

  // Non-owning pointers.
  Profile* profile_ = nullptr;
  signin::IdentityManager* identity_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AssistantClient);
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_CLIENT_H_
