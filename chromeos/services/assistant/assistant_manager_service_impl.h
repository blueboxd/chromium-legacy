// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/interfaces/assistant_controller.mojom.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "chromeos/assistant/internal/action/cros_action_module.h"
#include "chromeos/assistant/internal/cros_display_connection.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/services/assistant/assistant_manager_service.h"
#include "chromeos/services/assistant/assistant_settings_manager_impl.h"
#include "chromeos/services/assistant/chromium_api_delegate.h"
#include "chromeos/services/assistant/platform_api_impl.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "libassistant/shared/internal_api/assistant_manager_delegate.h"
#include "libassistant/shared/public/conversation_state_listener.h"
#include "libassistant/shared/public/device_state_listener.h"
#include "libassistant/shared/public/media_manager.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/accessibility/ax_assistant_structure.h"
#include "ui/accessibility/mojom/ax_assistant_structure.mojom.h"

namespace assistant_client {
class AssistantManager;
class AssistantManagerInternal;
}  // namespace assistant_client

namespace network {
class SharedURLLoaderFactoryInfo;
}  // namespace network

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace chromeos {
namespace assistant {

class Service;
class AssistantMediaSession;

// Enumeration of Assistant query response type, also recorded in histograms.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Only append to this enum is allowed
// if the possible type grows.
enum class AssistantQueryResponseType {
  // Query without response.
  kUnspecified = 0,
  // Query results in device actions (e.g. turn on bluetooth/WiFi).
  kDeviceAction = 1,
  // Query results in answer cards with contents rendered inside the
  // Assistant UI.
  kInlineElement = 2,
  // Query results in searching on Google, indicating that Assistant
  // doesn't know what to do.
  kSearchFallback = 3,
  // Query results in specific actions (e.g. opening a web app such as YouTube
  // or Facebook, some deeplink actions such as taking a screenshot or opening
  // chrome settings page), indicating that Assistant knows what to do.
  kTargetedAction = 4,
  // Special enumerator value used by histogram macros.
  kMaxValue = kTargetedAction
};

// Implementation of AssistantManagerService based on LibAssistant.
// This is the main class that interacts with LibAssistant.
// Since LibAssistant is a standalone library, all callbacks come from it
// running on threads not owned by Chrome. Thus we need to post the callbacks
// onto the main thread.
class AssistantManagerServiceImpl
    : public AssistantManagerService,
      public ::chromeos::assistant::action::AssistantActionObserver,
      public AssistantEventObserver,
      public assistant_client::ConversationStateListener,
      public assistant_client::AssistantManagerDelegate,
      public assistant_client::DeviceStateListener,
      public assistant_client::MediaManager::Listener,
      public media_session::mojom::MediaControllerObserver,
      public mojom::AppListEventSubscriber {
 public:
  // |service| owns this class and must outlive this class.
  AssistantManagerServiceImpl(
      service_manager::Connector* connector,
      device::mojom::BatteryMonitorPtr battery_monitor,
      Service* service,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>
          url_loader_factory_info);

  ~AssistantManagerServiceImpl() override;

  // assistant::AssistantManagerService overrides
  void Start(const base::Optional<std::string>& access_token,
             bool enable_hotword,
             base::OnceClosure callback) override;
  void Stop() override;
  State GetState() const override;
  void SetAccessToken(const std::string& access_token) override;
  void EnableListening(bool enable) override;
  void EnableHotword(bool enable) override;
  void SetArcPlayStoreEnabled(bool enable) override;
  AssistantSettingsManager* GetAssistantSettingsManager() override;

  // mojom::Assistant overrides:
  void StartCachedScreenContextInteraction() override;
  void StartEditReminderInteraction(const std::string& client_id) override;
  void StartMetalayerInteraction(const gfx::Rect& region) override;
  void StartTextInteraction(const std::string& query, bool allow_tts) override;
  void StartVoiceInteraction() override;
  void StartWarmerWelcomeInteraction(int num_warmer_welcome_triggered,
                                     bool allow_tts) override;
  void StopActiveInteraction(bool cancel_conversation) override;
  void AddAssistantInteractionSubscriber(
      mojom::AssistantInteractionSubscriberPtr subscriber) override;
  void RetrieveNotification(mojom::AssistantNotificationPtr notification,
                            int action_index) override;
  void DismissNotification(
      mojom::AssistantNotificationPtr notification) override;
  void CacheScreenContext(CacheScreenContextCallback callback) override;
  void ClearScreenContextCache() override;
  void OnAccessibilityStatusChanged(bool spoken_feedback_enabled) override;
  void SendAssistantFeedback(
      mojom::AssistantFeedbackPtr assistant_feedback) override;
  void StopAlarmTimerRinging() override;
  void CreateTimer(base::TimeDelta duration) override;

  // AssistantActionObserver overrides:
  void OnScheduleWait(int id, int time_ms) override;
  void OnShowContextualQueryFallback() override;
  void OnShowHtml(const std::string& html,
                  const std::string& fallback) override;
  void OnShowSuggestions(
      const std::vector<action::Suggestion>& suggestions) override;
  void OnShowText(const std::string& text) override;
  void OnOpenUrl(const std::string& url, bool in_background) override;
  void OnPlaybackStateChange(
      const assistant_client::MediaStatus& status) override;
  void OnShowNotification(const action::Notification& notification) override;
  void OnOpenAndroidApp(const action::AndroidAppInfo& app_info,
                        const action::InteractionInfo& interaction) override;
  void OnVerifyAndroidApp(const std::vector<action::AndroidAppInfo>& apps_info,
                          const action::InteractionInfo& interaction) override;

  // AssistantEventObserver overrides:
  void OnSpeechLevelUpdated(float speech_level) override;

  // assistant_client::ConversationStateListener overrides:
  void OnConversationTurnStarted(bool is_mic_open) override;
  void OnConversationTurnFinished(
      assistant_client::ConversationStateListener::Resolution resolution)
      override;
  void OnRecognitionStateChanged(
      assistant_client::ConversationStateListener::RecognitionState state,
      const assistant_client::ConversationStateListener::RecognitionResult&
          recognition_result) override;
  void OnRespondingStarted(bool is_error_response) override;

  // AssistantManagerDelegate overrides
  assistant_client::ActionModule::Result HandleModifySettingClientOp(
      const std::string& modify_setting_args_proto) override;
  bool IsSettingSupported(const std::string& setting_id) override;
  bool SupportsModifySettings() override;
  void OnNotificationRemoved(const std::string& grouping_key) override;
  void OnCommunicationError(int error_code) override;
  // Last search source will be cleared after it is retrieved.
  std::string GetLastSearchSource() override;

  // assistant_client::DeviceStateListener overrides:
  void OnStartFinished() override;
  void OnTimerSoundingStarted() override;
  void OnTimerSoundingFinished() override;

  // mojom::AppListEventSubscriber overrides:
  void OnAndroidAppListRefreshed(
      std::vector<mojom::AndroidAppInfoPtr> apps_info) override;

  void UpdateInternalOptions(
      assistant_client::AssistantManagerInternal* assistant_manager_internal);

  assistant_client::AssistantManager* assistant_manager() {
    return assistant_manager_.get();
  }
  assistant_client::AssistantManagerInternal* assistant_manager_internal() {
    return assistant_manager_internal_;
  }

  // media_session::mojom::MediaControllerObserver overrides:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr info) override;
  void MediaSessionMetadataChanged(
      const base::Optional<media_session::MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& action)
      override {}
  void MediaSessionChanged(
      const base::Optional<base::UnguessableToken>& request_id) override {}
  void MediaSessionPositionChanged(
      const base::Optional<media_session::MediaPosition>& position) override {}

  void UpdateInternalMediaPlayerStatus(
      media_session::mojom::MediaSessionAction action);

 private:
  void StartAssistantInternal(const base::Optional<std::string>& access_token);
  void PostInitAssistant(base::OnceClosure post_init_callback);

  // Update device id, type and locale
  void UpdateDeviceSettings();

  // Sync speaker id enrollment status.
  void SyncSpeakerIdEnrollmentStatus();

  void HandleOpenAndroidAppResponse(const action::InteractionInfo& interaction,
                                    bool app_opened);
  void HandleVerifyAndroidAppResponse(
      const action::InteractionInfo& interaction,
      std::vector<mojom::AndroidAppInfoPtr> apps_info);

  void HandleLaunchMediaIntentResponse(bool app_opened);

  void OnConversationTurnStartedOnMainThread(bool is_mic_open);
  void OnConversationTurnFinishedOnMainThread(
      assistant_client::ConversationStateListener::Resolution resolution);
  void OnShowHtmlOnMainThread(const std::string& html,
                              const std::string& fallback);
  void OnShowSuggestionsOnMainThread(
      const std::vector<mojom::AssistantSuggestionPtr>& suggestions);
  void OnShowTextOnMainThread(const std::string& text);
  void OnOpenUrlOnMainThread(const std::string& url, bool in_background);
  void OnShowNotificationOnMainThread(
      const mojom::AssistantNotificationPtr& notification);
  void OnNotificationRemovedOnMainThread(const std::string& grouping_id);
  void OnCommunicationErrorOnMainThread(int error_code);
  void OnRecognitionStateChangedOnMainThread(
      assistant_client::ConversationStateListener::RecognitionState state,
      const assistant_client::ConversationStateListener::RecognitionResult&
          recognition_result);
  void OnRespondingStartedOnMainThread(bool is_error_response);
  void OnSpeechLevelUpdatedOnMainThread(const float speech_level);
  void OnAlarmTimerStateChangedOnMainThread();
  void OnModifySettingsAction(const std::string& modify_setting_args_proto);
  void OnOpenMediaAndroidIntentOnMainThread(
      const std::string play_media_args_proto,
      action::AndroidAppInfo* android_app_info);
  void OnPlayMedia(const std::string play_media_args_proto);
  void OnMediaControlAction(const std::string& action_name,
                            const std::string& media_action_args_proto);

  void RegisterFallbackMediaHandler();
  void AddMediaControllerObserver();
  void RegisterAlarmsTimersListener();

  void CacheAssistantStructure(
      base::OnceClosure on_done,
      ax::mojom::AssistantExtraPtr assistant_extra,
      std::unique_ptr<ui::AssistantTree> assistant_tree);

  void CacheAssistantScreenshot(
      base::OnceClosure on_done,
      const std::vector<uint8_t>& assistant_screenshot);

  void SendScreenContextRequest(
      ax::mojom::AssistantExtra* assistant_extra,
      ui::AssistantTree* assistant_tree,
      const std::vector<uint8_t>& assistant_screenshot);

  void FillServerExperimentIds(std::vector<std::string>* server_experiment_ids);

  // Record the response type for each query. Note that query on device
  // actions (e.g. turn on Bluetooth, turn on WiFi) will cause duplicate
  // record because it interacts with server twice on on the same query.
  // The first round interaction checks IsSettingSupported with no responses
  // sent back and ends normally (will be recorded as kUnspecified), and
  // settings modification proto along with any text/voice responses would
  // be sent back in the second round (recorded as kDeviceAction).
  void RecordQueryResponseTypeUMA();

  void UpdateMediaState();

  State state_ = State::STOPPED;
  std::unique_ptr<AssistantMediaSession> media_session_;
  std::unique_ptr<PlatformApiImpl> platform_api_;
  std::unique_ptr<action::CrosActionModule> action_module_;
  ChromiumApiDelegate chromium_api_delegate_;
  // NOTE: |display_connection_| is used by |assistant_manager_| and must be
  // declared before so it will be destructed after.
  std::unique_ptr<CrosDisplayConnection> display_connection_;
  std::unique_ptr<assistant_client::AssistantManager> assistant_manager_;
  std::unique_ptr<AssistantSettingsManagerImpl> assistant_settings_manager_;
  // |new_asssistant_manager_| is created on |background_thread_| then posted to
  // main thread to finish initialization then move to |assistant_manager_|.
  std::unique_ptr<assistant_client::AssistantManager> new_assistant_manager_;
  base::Lock new_assistant_manager_lock_;
  // same ownership as assistant_manager_.
  assistant_client::AssistantManagerInternal* assistant_manager_internal_ =
      nullptr;
  mojo::InterfacePtrSet<mojom::AssistantInteractionSubscriber>
      interaction_subscribers_;
  media_session::mojom::MediaControllerPtr media_controller_;

  Service* service_;  // unowned.

  bool spoken_feedback_enabled_ = false;

  ax::mojom::AssistantExtraPtr assistant_extra_;
  std::unique_ptr<ui::AssistantTree> assistant_tree_;
  std::vector<uint8_t> assistant_screenshot_;
  std::string last_search_source_;
  base::Lock last_search_source_lock_;
  base::TimeTicks started_time_;

  base::Thread background_thread_;

  bool receive_modify_settings_proto_response_ = false;
  bool receive_inline_response_ = false;
  std::string receive_url_response_;

  bool is_first_client_discourse_context_query_ = true;

  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      media_controller_observer_receiver_{this};

  // Info associated to the active media session.
  media_session::mojom::MediaSessionInfoPtr media_session_info_ptr_;
  // The metadata for the active media session. It can be null to be reset, e.g.
  // the media that was being played has been stopped.
  base::Optional<media_session::MediaMetadata> media_metadata_ = base::nullopt;

  bool start_finished_ = false;

  mojo::Binding<mojom::AppListEventSubscriber> app_list_subscriber_binding_;

  base::WeakPtrFactory<AssistantManagerServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssistantManagerServiceImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_IMPL_H_
