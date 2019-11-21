// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/app_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/token.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

const char kAppServicePreferredApps[] = "app_service.preferred_apps";

void Connect(apps::mojom::Publisher* publisher,
             apps::mojom::Subscriber* subscriber) {
  mojo::PendingRemote<apps::mojom::Subscriber> clone;
  subscriber->Clone(clone.InitWithNewPipeAndPassReceiver());
  // TODO: replace nullptr with a ConnectOptions.
  publisher->Connect(std::move(clone), nullptr);
}

}  // namespace

namespace apps {

AppServiceImpl::AppServiceImpl(PrefService* profile_prefs)
    : pref_service_(profile_prefs) {
  DCHECK(pref_service_);
  InitializePreferredApps();
}

AppServiceImpl::~AppServiceImpl() = default;

// static
void AppServiceImpl::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kAppServicePreferredApps);
}

void AppServiceImpl::BindReceiver(
    mojo::PendingReceiver<apps::mojom::AppService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AppServiceImpl::FlushMojoCallsForTesting() {
  subscribers_.FlushForTesting();
  receivers_.FlushForTesting();
}

void AppServiceImpl::RegisterPublisher(
    mojo::PendingRemote<apps::mojom::Publisher> publisher_remote,
    apps::mojom::AppType app_type) {
  mojo::Remote<apps::mojom::Publisher> publisher(std::move(publisher_remote));
  // Connect the new publisher with every registered subscriber.
  for (auto& subscriber : subscribers_) {
    ::Connect(publisher.get(), subscriber.get());
  }

  // Check that no previous publisher has registered for the same app_type.
  CHECK(publishers_.find(app_type) == publishers_.end());

  // Add the new publisher to the set.
  publisher.set_disconnect_handler(
      base::BindOnce(&AppServiceImpl::OnPublisherDisconnected,
                     base::Unretained(this), app_type));
  auto result = publishers_.emplace(app_type, std::move(publisher));
  CHECK(result.second);
}

void AppServiceImpl::RegisterSubscriber(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  // Connect the new subscriber with every registered publisher.
  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  for (const auto& iter : publishers_) {
    ::Connect(iter.second.get(), subscriber.get());
  }

  // TODO: store the opts somewhere.

  // Initialise the Preferred Apps in the Subscribers on register.
  if (preferred_apps_.IsInitialized()) {
    subscriber->InitializePreferredApps(preferred_apps_.GetValue());
  }

  // Add the new subscriber to the set.
  subscribers_.Add(std::move(subscriber));
}

void AppServiceImpl::LoadIcon(apps::mojom::AppType app_type,
                              const std::string& app_id,
                              apps::mojom::IconKeyPtr icon_key,
                              apps::mojom::IconCompression icon_compression,
                              int32_t size_hint_in_dip,
                              bool allow_placeholder_icon,
                              LoadIconCallback callback) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    std::move(callback).Run(apps::mojom::IconValue::New());
    return;
  }
  iter->second->LoadIcon(app_id, std::move(icon_key), icon_compression,
                         size_hint_in_dip, allow_placeholder_icon,
                         std::move(callback));
}

void AppServiceImpl::Launch(apps::mojom::AppType app_type,
                            const std::string& app_id,
                            int32_t event_flags,
                            apps::mojom::LaunchSource launch_source,
                            int64_t display_id) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->Launch(app_id, event_flags, launch_source, display_id);
}

void AppServiceImpl::LaunchAppWithIntent(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    int64_t display_id) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->LaunchAppWithIntent(app_id, std::move(intent), launch_source,
                                    display_id);
}

void AppServiceImpl::SetPermission(apps::mojom::AppType app_type,
                                   const std::string& app_id,
                                   apps::mojom::PermissionPtr permission) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->SetPermission(app_id, std::move(permission));
}

void AppServiceImpl::PromptUninstall(apps::mojom::AppType app_type,
                                     const std::string& app_id) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->PromptUninstall(app_id);
}

void AppServiceImpl::Uninstall(apps::mojom::AppType app_type,
                               const std::string& app_id,
                               bool clear_site_data,
                               bool report_abuse) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->Uninstall(app_id, clear_site_data, report_abuse);
}

void AppServiceImpl::PauseApp(apps::mojom::AppType app_type,
                              const std::string& app_id) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->PauseApp(app_id);
}

void AppServiceImpl::UnpauseApps(apps::mojom::AppType app_type,
                                 const std::string& app_id) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->UnpauseApps(app_id);
}

void AppServiceImpl::OpenNativeSettings(apps::mojom::AppType app_type,
                                        const std::string& app_id) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->OpenNativeSettings(app_id);
}

void AppServiceImpl::AddPreferredApp(apps::mojom::AppType app_type,
                                     const std::string& app_id,
                                     apps::mojom::IntentFilterPtr intent_filter,
                                     apps::mojom::IntentPtr intent,
                                     bool from_publisher) {
  DCHECK(preferred_apps_.IsInitialized());

  preferred_apps_.AddPreferredApp(app_id, intent_filter);

  DictionaryPrefUpdate update(pref_service_, kAppServicePreferredApps);
  DCHECK(PreferredApps::VerifyPreferredApps(update.Get()));
  PreferredApps::AddPreferredApp(app_id, intent_filter, update.Get());

  for (auto& subscriber : subscribers_) {
    subscriber->OnPreferredAppSet(app_id, intent_filter->Clone());
  }

  if (from_publisher || !intent) {
    return;
  }
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->OnPreferredAppSet(app_id, std::move(intent_filter),
                                  std::move(intent));
}

void AppServiceImpl::RemovePreferredApp(apps::mojom::AppType app_type,
                                        const std::string& app_id) {
  DCHECK(preferred_apps_.IsInitialized());

  preferred_apps_.DeleteAppId(app_id);

  DictionaryPrefUpdate update(pref_service_, kAppServicePreferredApps);
  DCHECK(PreferredApps::VerifyPreferredApps(update.Get()));
  PreferredApps::DeleteAppId(app_id, update.Get());
}

void AppServiceImpl::RemovePreferredAppForFilter(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::IntentFilterPtr intent_filter) {
  DCHECK(preferred_apps_.IsInitialized());

  preferred_apps_.DeletePreferredApp(app_id, intent_filter);

  DictionaryPrefUpdate update(pref_service_, kAppServicePreferredApps);
  DCHECK(PreferredApps::VerifyPreferredApps(update.Get()));
  PreferredApps::DeletePreferredApp(app_id, intent_filter, update.Get());

  for (auto& subscriber : subscribers_) {
    subscriber->OnPreferredAppRemoved(app_id, intent_filter->Clone());
  }
}

PreferredApps& AppServiceImpl::GetPreferredAppsForTesting() {
  return preferred_apps_;
}

void AppServiceImpl::OnPublisherDisconnected(apps::mojom::AppType app_type) {
  publishers_.erase(app_type);
}

void AppServiceImpl::InitializePreferredApps() {
  DCHECK(pref_service_);
  preferred_apps_.Init(
      pref_service_->GetDictionary(kAppServicePreferredApps)->CreateDeepCopy());
  for (auto& subscriber : subscribers_) {
    subscriber->InitializePreferredApps(preferred_apps_.GetValue());
  }
}

}  // namespace apps
