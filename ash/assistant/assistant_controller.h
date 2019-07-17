// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/assistant/assistant_alarm_timer_controller.h"
#include "ash/assistant/assistant_cache_controller.h"
#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/assistant_notification_controller.h"
#include "ash/assistant/assistant_prefs_controller.h"
#include "ash/assistant/assistant_screen_context_controller.h"
#include "ash/assistant/assistant_setup_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/assistant_view_delegate_impl.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/public/cpp/assistant/assistant_image_downloader.h"
#include "ash/public/cpp/assistant/default_voice_interaction_observer.h"
#include "ash/public/interfaces/assistant_controller.mojom.h"
#include "ash/public/interfaces/assistant_volume_control.mojom.h"
#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/content/public/mojom/navigable_contents_factory.mojom.h"

class PrefRegistrySimple;

namespace ash {

class AssistantAlarmTimerController;
class AssistantCacheController;
class AssistantInteractionController;
class AssistantNotificationController;
class AssistantPrefsController;
class AssistantScreenContextController;
class AssistantSetupController;
class AssistantUiController;

class ASH_EXPORT AssistantController
    : public mojom::AssistantController,
      public AssistantControllerObserver,
      public DefaultVoiceInteractionObserver,
      public mojom::AssistantVolumeControl,
      public chromeos::CrasAudioHandler::AudioObserver,
      public AccessibilityObserver {
 public:
  AssistantController();
  ~AssistantController() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  void BindRequest(mojom::AssistantControllerRequest request);
  void BindRequest(mojom::AssistantVolumeControlRequest request);

  // Adds/removes the specified |observer|.
  void AddObserver(AssistantControllerObserver* observer);
  void RemoveObserver(AssistantControllerObserver* observer);

  // Downloads the image found at the specified |url|. On completion, the
  // supplied |callback| will be run with the downloaded image. If the download
  // attempt is unsuccessful, a NULL image is returned.
  void DownloadImage(const GURL& url,
                     AssistantImageDownloader::DownloadCallback callback);

  // mojom::AssistantController:
  // TODO(updowndota): Refactor Set() calls to use a factory pattern.
  void SetAssistant(
      chromeos::assistant::mojom::AssistantPtr assistant) override;
  void StartSpeakerIdEnrollmentFlow() override;
  void SendAssistantFeedback(bool assistant_debug_info_allowed,
                             const std::string& feedback_description,
                             const std::string& screenshot_png) override;

  // AssistantControllerObserver:
  void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) override;

  // mojom::VolumeControl:
  void SetVolume(int volume, bool user_initiated) override;
  void SetMuted(bool muted) override;
  void AddVolumeObserver(mojom::VolumeObserverPtr observer) override;

  // chromeos::CrasAudioHandler::AudioObserver:
  void OnOutputMuteChanged(bool mute_on) override;
  void OnOutputNodeVolumeChanged(uint64_t node, int volume) override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // Opens the specified |url| in a new browser tab. Special handling is applied
  // to deep links which may cause deviation from this behavior.
  void OpenUrl(const GURL& url,
               bool in_background = false,
               bool from_server = false);

  // Acquires a NavigableContentsFactory from the Content Service to allow
  // Assistant to display embedded web contents.
  void GetNavigableContentsFactory(
      mojo::PendingReceiver<content::mojom::NavigableContentsFactory> receiver);

  AssistantAlarmTimerController* alarm_timer_controller() {
    return &assistant_alarm_timer_controller_;
  }

  AssistantCacheController* cache_controller() {
    return &assistant_cache_controller_;
  }

  AssistantInteractionController* interaction_controller() {
    return &assistant_interaction_controller_;
  }

  AssistantNotificationController* notification_controller() {
    return &assistant_notification_controller_;
  }

  AssistantPrefsController* prefs_controller() {
    return &assistant_prefs_controller_;
  }

  AssistantScreenContextController* screen_context_controller() {
    return &assistant_screen_context_controller_;
  }

  AssistantSetupController* setup_controller() {
    return &assistant_setup_controller_;
  }

  AssistantUiController* ui_controller() { return &assistant_ui_controller_; }

  AssistantViewDelegate* view_delegate() { return &view_delegate_; }

  bool IsAssistantReady() const;

  base::WeakPtr<AssistantController> GetWeakPtr();

 private:
  void NotifyConstructed();
  void NotifyDestroying();
  void NotifyDeepLinkReceived(const GURL& deep_link);
  void NotifyOpeningUrl(const GURL& url, bool in_background, bool from_server);
  void NotifyUrlOpened(const GURL& url, bool from_server);

  // mojom::VoiceInteractionObserver:
  void OnVoiceInteractionStatusChanged(
      mojom::VoiceInteractionState state) override;
  void OnLockedFullScreenStateChanged(bool enabled) override;

  // The observer list should be initialized early so that sub-controllers may
  // register as observers during their construction.
  base::ObserverList<AssistantControllerObserver> observers_;

  mojo::BindingSet<mojom::AssistantController> assistant_controller_bindings_;

  mojo::Binding<mojom::AssistantVolumeControl>
      assistant_volume_control_binding_;
  mojo::InterfacePtrSet<mojom::VolumeObserver> volume_observer_;

  chromeos::assistant::mojom::AssistantPtr assistant_;

  // Assistant sub-controllers.
  AssistantAlarmTimerController assistant_alarm_timer_controller_;
  AssistantCacheController assistant_cache_controller_;
  AssistantInteractionController assistant_interaction_controller_;
  AssistantNotificationController assistant_notification_controller_;
  AssistantPrefsController assistant_prefs_controller_;
  AssistantScreenContextController assistant_screen_context_controller_;
  AssistantSetupController assistant_setup_controller_;
  AssistantUiController assistant_ui_controller_;

  AssistantViewDelegateImpl view_delegate_;

  base::WeakPtrFactory<AssistantController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssistantController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_CONTROLLER_H_
