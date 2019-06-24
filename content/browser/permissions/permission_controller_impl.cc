// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PERMISSIONS_PERMISSION_CONTROLLER_IMPL_CC_
#define CONTENT_BROWSER_PERMISSIONS_PERMISSION_CONTROLLER_IMPL_CC_

#include "content/browser/permissions/permission_controller_impl.h"
#include "base/bind.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

class GURL;

namespace content {

namespace {

blink::mojom::PermissionStatus GetPermissionOverrideStatus(
    const PermissionControllerImpl::PermissionOverrides& permission_overrides,
    const PermissionType& permission) {
  if (permission_overrides.find(permission) == permission_overrides.end())
    return blink::mojom::PermissionStatus::DENIED;
  return blink::mojom::PermissionStatus::GRANTED;
}

base::Optional<blink::scheduler::WebSchedulerTrackedFeature>
PermissionToSchedulingFeature(PermissionType permission_name) {
  switch (permission_name) {
    case PermissionType::GEOLOCATION:
      return blink::scheduler::WebSchedulerTrackedFeature::
          kRequestedGeolocationPermission;
    case PermissionType::NOTIFICATIONS:
      return blink::scheduler::WebSchedulerTrackedFeature::
          kRequestedNotificationsPermission;
    case PermissionType::MIDI:
    case PermissionType::MIDI_SYSEX:
      return blink::scheduler::WebSchedulerTrackedFeature::
          kRequestedMIDIPermission;
    case PermissionType::AUDIO_CAPTURE:
      return blink::scheduler::WebSchedulerTrackedFeature::
          kRequestedAudioCapturePermission;
    case PermissionType::VIDEO_CAPTURE:
      return blink::scheduler::WebSchedulerTrackedFeature::
          kRequestedVideoCapturePermission;
    case PermissionType::SENSORS:
      return blink::scheduler::WebSchedulerTrackedFeature::
          kRequestedSensorsPermission;
    case PermissionType::BACKGROUND_SYNC:
    case PermissionType::BACKGROUND_FETCH:
    case PermissionType::PERIODIC_BACKGROUND_SYNC:
      return blink::scheduler::WebSchedulerTrackedFeature::
          kRequestedBackgroundWorkPermission;
    case PermissionType::PROTECTED_MEDIA_IDENTIFIER:
    case PermissionType::DURABLE_STORAGE:
    case PermissionType::FLASH:
    case PermissionType::ACCESSIBILITY_EVENTS:
    case PermissionType::CLIPBOARD_READ:
    case PermissionType::CLIPBOARD_WRITE:
    case PermissionType::PAYMENT_HANDLER:
    case PermissionType::IDLE_DETECTION:
    case PermissionType::WAKE_LOCK_SCREEN:
    case PermissionType::WAKE_LOCK_SYSTEM:
    case PermissionType::NUM:
      return base::nullopt;
  }
}

void NotifySchedulerAboutPermissionRequest(RenderFrameHost* render_frame_host,
                                           PermissionType permission_name) {
  DCHECK(render_frame_host);

  base::Optional<blink::scheduler::WebSchedulerTrackedFeature> feature =
      PermissionToSchedulingFeature(permission_name);

  if (!feature)
    return;

  static_cast<RenderFrameHostImpl*>(render_frame_host)
      ->OnSchedulerTrackedFeatureUsed(feature.value());
}

}  // namespace

PermissionControllerImpl::PermissionControllerImpl(
    BrowserContext* browser_context)
    : browser_context_(browser_context) {}

// static
PermissionControllerImpl* PermissionControllerImpl::FromBrowserContext(
    BrowserContext* browser_context) {
  return static_cast<PermissionControllerImpl*>(
      BrowserContext::GetPermissionController(browser_context));
}

struct PermissionControllerImpl::Subscription {
  PermissionType permission;
  GURL requesting_origin;
  GURL embedding_origin;
  int render_frame_id = -1;
  int render_process_id = -1;
  base::Callback<void(blink::mojom::PermissionStatus)> callback;
  int delegate_subscription_id;
};

PermissionControllerImpl::~PermissionControllerImpl() {
  // Ideally we need to unsubscribe from delegate subscriptions here,
  // but browser_context_ is already destroyed by this point, so
  // we can't fetch our delegate.
}

blink::mojom::PermissionStatus
PermissionControllerImpl::GetSubscriptionCurrentValue(
    const Subscription& subscription) {
  // The RFH may be null if the request is for a worker.
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      subscription.render_process_id, subscription.render_frame_id);
  if (rfh) {
    return GetPermissionStatusForFrame(subscription.permission, rfh,
                                       subscription.requesting_origin);
  }
  return GetPermissionStatus(subscription.permission,
                             subscription.requesting_origin,
                             subscription.embedding_origin);
}

void PermissionControllerImpl::SetPermissionOverridesForDevTools(
    const GURL& origin,
    const PermissionOverrides& overrides) {
  std::vector<base::Closure> callbacks;
  for (SubscriptionsMap::iterator iter(&subscriptions_); !iter.IsAtEnd();
       iter.Advance()) {
    Subscription* subscription = iter.GetCurrentValue();
    if (subscription->requesting_origin != origin)
      continue;
    blink::mojom::PermissionStatus current_value =
        GetSubscriptionCurrentValue(*subscription);
    blink::mojom::PermissionStatus new_value =
        GetPermissionOverrideStatus(overrides, subscription->permission);
    if (current_value != new_value)
      callbacks.push_back(base::Bind(subscription->callback, new_value));
  }
  devtools_permission_overrides_[origin] = overrides;
  for (const auto& callback : callbacks)
    callback.Run();
}

void PermissionControllerImpl::ResetPermissionOverridesForDevTools() {
  std::map<GURL, PermissionOverrides> old_overrides;
  old_overrides.swap(devtools_permission_overrides_);
  std::vector<base::Closure> callbacks;
  for (SubscriptionsMap::iterator iter(&subscriptions_); !iter.IsAtEnd();
       iter.Advance()) {
    Subscription* subscription = iter.GetCurrentValue();
    auto overrides_it = old_overrides.find(subscription->requesting_origin);
    if (overrides_it == old_overrides.end())
      continue;
    blink::mojom::PermissionStatus current_value = GetPermissionOverrideStatus(
        overrides_it->second, subscription->permission);
    blink::mojom::PermissionStatus new_value =
        GetSubscriptionCurrentValue(*subscription);
    if (current_value != new_value)
      callbacks.push_back(base::Bind(subscription->callback, new_value));
  }
  for (const auto& callback : callbacks)
    callback.Run();
}

int PermissionControllerImpl::RequestPermission(
    PermissionType permission,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    const base::Callback<void(blink::mojom::PermissionStatus)>& callback) {
  NotifySchedulerAboutPermissionRequest(render_frame_host, permission);

  auto it = devtools_permission_overrides_.find(requesting_origin.GetOrigin());
  if (it != devtools_permission_overrides_.end()) {
    callback.Run(GetPermissionOverrideStatus(it->second, permission));
    return kNoPendingOperation;
  }

  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate) {
    callback.Run(blink::mojom::PermissionStatus::DENIED);
    return kNoPendingOperation;
  }
  return delegate->RequestPermission(permission, render_frame_host,
                                     requesting_origin, user_gesture, callback);
}

int PermissionControllerImpl::RequestPermissions(
    const std::vector<PermissionType>& permissions,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    const base::Callback<
        void(const std::vector<blink::mojom::PermissionStatus>&)>& callback) {
  for (PermissionType permission : permissions)
    NotifySchedulerAboutPermissionRequest(render_frame_host, permission);

  auto it = devtools_permission_overrides_.find(requesting_origin.GetOrigin());
  if (it != devtools_permission_overrides_.end()) {
    std::vector<blink::mojom::PermissionStatus> result;
    for (auto& permission : permissions)
      result.push_back(GetPermissionOverrideStatus(it->second, permission));
    callback.Run(result);
    return kNoPendingOperation;
  }

  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate) {
    std::vector<blink::mojom::PermissionStatus> result(
        permissions.size(), blink::mojom::PermissionStatus::DENIED);
    callback.Run(result);
    return kNoPendingOperation;
  }
  return delegate->RequestPermissions(permissions, render_frame_host,
                                      requesting_origin, user_gesture,
                                      callback);
}

blink::mojom::PermissionStatus PermissionControllerImpl::GetPermissionStatus(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  auto it = devtools_permission_overrides_.find(requesting_origin.GetOrigin());
  if (it != devtools_permission_overrides_.end())
    return GetPermissionOverrideStatus(it->second, permission);

  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate)
    return blink::mojom::PermissionStatus::DENIED;
  return delegate->GetPermissionStatus(permission, requesting_origin,
                                       embedding_origin);
}

blink::mojom::PermissionStatus
PermissionControllerImpl::GetPermissionStatusForFrame(
    PermissionType permission,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin) {
  auto it = devtools_permission_overrides_.find(requesting_origin.GetOrigin());
  if (it != devtools_permission_overrides_.end())
    return GetPermissionOverrideStatus(it->second, permission);

  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate)
    return blink::mojom::PermissionStatus::DENIED;
  return delegate->GetPermissionStatusForFrame(permission, render_frame_host,
                                               requesting_origin);
}

void PermissionControllerImpl::ResetPermission(PermissionType permission,
                                               const GURL& requesting_origin,
                                               const GURL& embedding_origin) {
  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (!delegate)
    return;
  delegate->ResetPermission(permission, requesting_origin, embedding_origin);
}

void PermissionControllerImpl::OnDelegatePermissionStatusChange(
    Subscription* subscription,
    blink::mojom::PermissionStatus status) {
  auto overrides_it =
      devtools_permission_overrides_.find(subscription->requesting_origin);
  if (overrides_it == devtools_permission_overrides_.end())
    subscription->callback.Run(status);
}

int PermissionControllerImpl::SubscribePermissionStatusChange(
    PermissionType permission,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const base::Callback<void(blink::mojom::PermissionStatus)>& callback) {
  auto subscription = std::make_unique<Subscription>();
  subscription->permission = permission;
  subscription->callback = callback;
  subscription->requesting_origin = requesting_origin;

  // The RFH may be null if the request is for a worker.
  if (render_frame_host) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(render_frame_host);
    subscription->embedding_origin =
        web_contents->GetLastCommittedURL().GetOrigin();
    subscription->render_frame_id = render_frame_host->GetRoutingID();
    subscription->render_process_id = render_frame_host->GetProcess()->GetID();
  } else {
    subscription->embedding_origin = requesting_origin;
    subscription->render_frame_id = -1;
    subscription->render_process_id = -1;
  }

  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (delegate) {
    subscription->delegate_subscription_id =
        delegate->SubscribePermissionStatusChange(
            permission, render_frame_host, requesting_origin,
            base::Bind(
                &PermissionControllerImpl::OnDelegatePermissionStatusChange,
                base::Unretained(this), subscription.get()));
  } else {
    subscription->delegate_subscription_id = kNoPendingOperation;
  }
  return subscriptions_.Add(std::move(subscription));
}

void PermissionControllerImpl::UnsubscribePermissionStatusChange(
    int subscription_id) {
  Subscription* subscription = subscriptions_.Lookup(subscription_id);
  if (!subscription)
    return;
  PermissionControllerDelegate* delegate =
      browser_context_->GetPermissionControllerDelegate();
  if (delegate &&
      subscription->delegate_subscription_id != kNoPendingOperation) {
    delegate->UnsubscribePermissionStatusChange(
        subscription->delegate_subscription_id);
  }
  subscriptions_.Remove(subscription_id);
}

}  // namespace content

#endif  // CONTENT_BROWSER_PERMISSIONS_PERMISSION_CONTROLLER_IMPL_CC_
