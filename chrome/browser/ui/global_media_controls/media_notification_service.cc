// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_notification_service.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/global_media_controls/media_dialog_delegate.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_impl.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_message_center/media_notification_util.h"
#include "components/media_message_center/media_session_notification_item.h"
#include "content/public/browser/media_session.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

// Here we check to see if the WebContents is focused. Note that since Session
// is a WebContentsObserver, we could in theory listen for
// |OnWebContentsFocused()| and |OnWebContentsLostFocus()|. However, this won't
// actually work since focusing the MediaDialogView causes the WebContents to
// "lose focus", so we'd never be focused.
bool IsWebContentsFocused(content::WebContents* web_contents) {
  DCHECK(web_contents);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return false;

  // If the given WebContents is not in the focused window, then it's not
  // focused. Note that we know a Browser is focused because otherwise the user
  // could not interact with the MediaDialogView.
  if (BrowserList::GetInstance()->GetLastActive() != browser)
    return false;

  return browser->tab_strip_model()->GetActiveWebContents() == web_contents;
}

}  // anonymous namespace

MediaNotificationService::Session::Session(
    MediaNotificationService* owner,
    const std::string& id,
    std::unique_ptr<media_message_center::MediaSessionNotificationItem> item,
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      owner_(owner),
      id_(id),
      item_(std::move(item)) {
  DCHECK(owner_);
  DCHECK(item_);
}

MediaNotificationService::Session::~Session() = default;

void MediaNotificationService::Session::WebContentsDestroyed() {
  // If the WebContents is destroyed, then we should just remove the item
  // instead of freezing it.
  owner_->RemoveItem(id_);
}

MediaNotificationService::MediaNotificationService(
    Profile* profile,
    service_manager::Connector* connector)
    : connector_(connector) {
  if (base::FeatureList::IsEnabled(media::kGlobalMediaControlsForCast) &&
      media_router::MediaRouterEnabled(profile)) {
    cast_notification_provider_ =
        std::make_unique<CastMediaNotificationProvider>(
            profile, this,
            base::BindRepeating(
                &MediaNotificationService::OnCastNotificationsChanged,
                base::Unretained(this)));
  }

  // |connector_| can be null in tests.
  if (!connector_)
    return;

  const base::UnguessableToken& source_id =
      content::MediaSession::GetSourceId(profile);

  // Connect to the controller manager so we can create media controllers for
  // media sessions.
  connector_->Connect(media_session::mojom::kServiceName,
                      controller_manager_remote_.BindNewPipeAndPassReceiver());

  // Connect to receive audio focus events.
  connector_->Connect(media_session::mojom::kServiceName,
                      audio_focus_remote_.BindNewPipeAndPassReceiver());
  audio_focus_remote_->AddSourceObserver(
      source_id, audio_focus_observer_receiver_.BindNewPipeAndPassRemote());

  audio_focus_remote_->GetSourceFocusRequests(
      source_id,
      base::BindOnce(&MediaNotificationService::OnReceivedAudioFocusRequests,
                     weak_ptr_factory_.GetWeakPtr()));
}

MediaNotificationService::~MediaNotificationService() {
  for (auto container_pair : observed_containers_)
    container_pair.second->RemoveObserver(this);
}

void MediaNotificationService::AddObserver(
    MediaNotificationServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaNotificationService::RemoveObserver(
    MediaNotificationServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MediaNotificationService::OnFocusGained(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  const std::string id = session->request_id->ToString();

  // If we have an existing unfrozen item then this is a duplicate call and
  // we should ignore it.
  auto it = sessions_.find(id);
  if (it != sessions_.end() && !it->second.item()->frozen())
    return;

  mojo::Remote<media_session::mojom::MediaController> controller;

  // |controller_manager_remote_| may be null in tests where connector is
  // unavailable.
  if (controller_manager_remote_) {
    controller_manager_remote_->CreateMediaControllerForSession(
        controller.BindNewPipeAndPassReceiver(), *session->request_id);
  }

  if (it != sessions_.end()) {
    // If the notification was previously frozen then we should reset the
    // controller because the mojo pipe would have been reset.
    it->second.item()->SetController(std::move(controller),
                                     std::move(session->session_info));
    active_controllable_session_ids_.insert(id);
    frozen_session_ids_.erase(id);
    for (auto& observer : observers_)
      observer.OnNotificationListChanged();
  } else {
    sessions_.emplace(
        std::piecewise_construct, std::forward_as_tuple(id),
        std::forward_as_tuple(
            this, id,
            std::make_unique<
                media_message_center::MediaSessionNotificationItem>(
                this, id, session->source_name.value_or(std::string()),
                std::move(controller), std::move(session->session_info)),
            content::MediaSession::GetWebContentsFromRequestId(
                *session->request_id)));
  }
}

void MediaNotificationService::OnFocusLost(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  const std::string id = session->request_id->ToString();

  auto it = sessions_.find(id);
  if (it == sessions_.end())
    return;

  it->second.item()->Freeze();
  active_controllable_session_ids_.erase(id);
  frozen_session_ids_.insert(id);
  for (auto& observer : observers_)
    observer.OnNotificationListChanged();
}

void MediaNotificationService::ShowNotification(const std::string& id) {
  active_controllable_session_ids_.insert(id);
  for (auto& observer : observers_)
    observer.OnNotificationListChanged();

  if (!dialog_delegate_)
    return;

  base::WeakPtr<media_message_center::MediaNotificationItem> item =
      GetNotificationItem(id);
  MediaNotificationContainerImpl* container =
      dialog_delegate_->ShowMediaSession(id, item);

  // Observe the container for dismissal.
  if (container) {
    container->AddObserver(this);
    observed_containers_[id] = container;
  }
}

void MediaNotificationService::HideNotification(const std::string& id) {
  active_controllable_session_ids_.erase(id);
  frozen_session_ids_.erase(id);
  for (auto& observer : observers_)
    observer.OnNotificationListChanged();

  if (!dialog_delegate_)
    return;

  dialog_delegate_->HideMediaSession(id);
}

scoped_refptr<base::SequencedTaskRunner>
MediaNotificationService::GetTaskRunner() const {
  return nullptr;
}

void MediaNotificationService::RemoveItem(const std::string& id) {
  active_controllable_session_ids_.erase(id);
  frozen_session_ids_.erase(id);
  sessions_.erase(id);

  for (auto& observer : observers_)
    observer.OnNotificationListChanged();
}

void MediaNotificationService::LogMediaSessionActionButtonPressed(
    const std::string& id) {
  auto it = sessions_.find(id);
  if (it == sessions_.end())
    return;

  content::WebContents* web_contents = it->second.web_contents();
  if (!web_contents)
    return;

  base::UmaHistogramBoolean("Media.GlobalMediaControls.UserActionFocus",
                            IsWebContentsFocused(web_contents));
}

void MediaNotificationService::OnContainerClicked(const std::string& id) {
  auto it = sessions_.find(id);
  if (it == sessions_.end())
    return;

  content::WebContents* web_contents = it->second.web_contents();
  if (!web_contents)
    return;

  content::WebContentsDelegate* delegate = web_contents->GetDelegate();
  if (!delegate)
    return;

  delegate->ActivateContents(web_contents);
}

void MediaNotificationService::OnContainerDismissed(const std::string& id) {
  auto it = sessions_.find(id);
  if (it != sessions_.end())
    it->second.item()->Dismiss();
}

void MediaNotificationService::OnContainerDestroyed(const std::string& id) {
  auto iter = observed_containers_.find(id);
  DCHECK(iter != observed_containers_.end());

  iter->second->RemoveObserver(this);
  observed_containers_.erase(iter);
}

void MediaNotificationService::OnCastNotificationsChanged() {
  for (auto& observer : observers_)
    observer.OnNotificationListChanged();
}

void MediaNotificationService::SetDialogDelegate(
    MediaDialogDelegate* delegate) {
  DCHECK(!delegate || !dialog_delegate_);
  dialog_delegate_ = delegate;

  for (auto& observer : observers_)
    observer.OnMediaDialogOpenedOrClosed();

  if (!dialog_delegate_)
    return;

  for (const std::string& id : active_controllable_session_ids_) {
    base::WeakPtr<media_message_center::MediaNotificationItem> item =
        GetNotificationItem(id);
    MediaNotificationContainerImpl* container =
        dialog_delegate_->ShowMediaSession(id, item);

    // Observe the container for dismissal.
    if (container) {
      container->AddObserver(this);
      observed_containers_[id] = container;
    }
  }

  media_message_center::RecordConcurrentNotificationCount(
      active_controllable_session_ids_.size());
}

bool MediaNotificationService::HasActiveNotifications() const {
  return !active_controllable_session_ids_.empty() ||
         (cast_notification_provider_ &&
          cast_notification_provider_->HasItems());
}

bool MediaNotificationService::HasFrozenNotifications() const {
  return !frozen_session_ids_.empty();
}

bool MediaNotificationService::HasOpenDialog() const {
  return !!dialog_delegate_;
}

void MediaNotificationService::OnReceivedAudioFocusRequests(
    std::vector<media_session::mojom::AudioFocusRequestStatePtr> sessions) {
  for (auto& session : sessions)
    OnFocusGained(std::move(session));
}

base::WeakPtr<media_message_center::MediaNotificationItem>
MediaNotificationService::GetNotificationItem(const std::string& id) {
  auto it = sessions_.find(id);
  if (it != sessions_.end()) {
    return it->second.item()->GetWeakPtr();
  } else if (cast_notification_provider_) {
    return cast_notification_provider_->GetNotificationItem(id);
  }
  return nullptr;
}
