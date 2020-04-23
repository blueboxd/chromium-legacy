// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_SETUP_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_SETUP_H_

#include <memory>
#include <string>

#include "ash/public/cpp/assistant/assistant_setup.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "chromeos/services/assistant/public/mojom/settings.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace network {

class SharedURLLoaderFactory;
class SimpleURLLoader;

}  // namespace network

// AssistantSetup is the class responsible for start Assistant OptIn flow.
class AssistantSetup : public ash::AssistantSetup,
                       public ash::AssistantStateObserver {
 public:
  AssistantSetup();
  ~AssistantSetup() override;

  // ash::AssistantSetup:
  void StartAssistantOptInFlow(
      ash::FlowType type,
      StartAssistantOptInFlowCallback callback) override;
  bool BounceOptInWindowIfActive() override;

  // If prefs::kVoiceInteractionConsentStatus is nullptr, means the
  // pref is not set by user. Therefore we need to start OOBE.
  void MaybeStartAssistantOptInFlow();

 private:
  // ash::AssistantStateObserver:
  void OnAssistantStatusChanged(ash::mojom::AssistantState state) override;

  void SyncSettingsState();
  void OnGetSettingsResponse(const std::string& settings);

  void SyncSearchAndAssistantState();
  void OnSimpleURLLoaderComplete(std::unique_ptr<std::string> response_body);
  void OnJsonParsed(data_decoder::DataDecoder::ValueOrError response);

  mojo::Remote<chromeos::assistant::mojom::AssistantSettingsManager>
      settings_manager_;

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::WeakPtrFactory<AssistantSetup> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AssistantSetup);
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_SETUP_H_
