// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_database.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

namespace web_app {

WebAppDatabase::WebAppDatabase(AbstractWebAppDatabaseFactory* database_factory,
                               ReportErrorCallback error_callback)
    : database_factory_(database_factory),
      error_callback_(std::move(error_callback)) {
  DCHECK(database_factory_);
}

WebAppDatabase::~WebAppDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WebAppDatabase::OpenDatabase(RegistryOpenedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!store_);

  syncer::OnceModelTypeStoreFactory store_factory =
      database_factory_->GetStoreFactory();

  std::move(store_factory)
      .Run(syncer::WEB_APPS,
           base::BindOnce(&WebAppDatabase::OnDatabaseOpened,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebAppDatabase::Write(
    const RegistryUpdateData& update_data,
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    CompletionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(opened_);

  DCHECK(!update_data.IsEmpty());

  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  write_batch->TakeMetadataChangesFrom(std::move(metadata_change_list));

  for (const std::unique_ptr<WebApp>& web_app : update_data.apps_to_create) {
    auto proto = CreateWebAppProto(*web_app);
    write_batch->WriteData(web_app->app_id(), proto->SerializeAsString());
  }

  for (const std::unique_ptr<WebApp>& web_app : update_data.apps_to_update) {
    auto proto = CreateWebAppProto(*web_app);
    write_batch->WriteData(web_app->app_id(), proto->SerializeAsString());
  }

  for (const AppId& app_id : update_data.apps_to_delete)
    write_batch->DeleteData(app_id);

  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&WebAppDatabase::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

// static
std::unique_ptr<WebAppProto> WebAppDatabase::CreateWebAppProto(
    const WebApp& web_app) {
  auto local_data = std::make_unique<WebAppProto>();

  // Required fields:
  const GURL launch_url = web_app.launch_url();
  DCHECK(!launch_url.is_empty() && launch_url.is_valid());

  DCHECK(!web_app.app_id().empty());
  DCHECK_EQ(web_app.app_id(), GenerateAppIdFromURL(launch_url));

  sync_pb::WebAppSpecifics* sync_data = local_data->mutable_sync_data();

  sync_data->set_launch_url(launch_url.spec());

  local_data->set_name(web_app.name());

  sync_data->set_display_mode(
      ToWebAppSpecificsDisplayMode(web_app.display_mode()));

  DCHECK(web_app.sources_.any());
  local_data->mutable_sources()->set_system(web_app.sources_[Source::kSystem]);
  local_data->mutable_sources()->set_policy(web_app.sources_[Source::kPolicy]);
  local_data->mutable_sources()->set_web_app_store(
      web_app.sources_[Source::kWebAppStore]);
  local_data->mutable_sources()->set_sync(web_app.sources_[Source::kSync]);
  local_data->mutable_sources()->set_default_(
      web_app.sources_[Source::kDefault]);

  local_data->set_is_locally_installed(web_app.is_locally_installed());

  // Optional fields:
  local_data->set_description(web_app.description());
  if (!web_app.scope().is_empty())
    local_data->set_scope(web_app.scope().spec());
  if (web_app.theme_color().has_value())
    local_data->set_theme_color(web_app.theme_color().value());

  local_data->set_is_in_sync_install(web_app.is_in_sync_install());

  // Set sync_data to sync proto.
  sync_data->set_name(web_app.sync_data().name);
  if (web_app.sync_data().theme_color.has_value())
    sync_data->set_theme_color(web_app.sync_data().theme_color.value());

  for (const WebApp::IconInfo& icon : web_app.icons()) {
    WebAppIconInfoProto* icon_proto = local_data->add_icons();
    icon_proto->set_url(icon.url.spec());
    icon_proto->set_size_in_px(icon.size_in_px);
  }

  return local_data;
}

// static
std::unique_ptr<WebApp> WebAppDatabase::CreateWebApp(
    const WebAppProto& local_data) {
  if (!local_data.has_sync_data()) {
    DLOG(ERROR) << "WebApp proto parse error: no sync_data field";
    return nullptr;
  }

  const sync_pb::WebAppSpecifics& sync_data = local_data.sync_data();

  // AppId is a hash of launch_url. Read launch_url first:
  GURL launch_url(sync_data.launch_url());
  if (launch_url.is_empty() || !launch_url.is_valid()) {
    DLOG(ERROR) << "WebApp proto launch_url parse error: "
                << launch_url.possibly_invalid_spec();
    return nullptr;
  }

  const AppId app_id = GenerateAppIdFromURL(launch_url);

  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->SetLaunchUrl(launch_url);

  // Required fields:
  if (!local_data.has_sources()) {
    DLOG(ERROR) << "WebApp proto parse error: no sources field";
    return nullptr;
  }

  WebApp::Sources sources;
  sources[Source::kSystem] = local_data.sources().system();
  sources[Source::kPolicy] = local_data.sources().policy();
  sources[Source::kWebAppStore] = local_data.sources().web_app_store();
  sources[Source::kSync] = local_data.sources().sync();
  sources[Source::kDefault] = local_data.sources().default_();
  if (!sources.any()) {
    DLOG(ERROR) << "WebApp proto parse error: no any source in sources field";
    return nullptr;
  }
  web_app->sources_ = sources;

  if (!local_data.has_name()) {
    DLOG(ERROR) << "WebApp proto parse error: no name field";
    return nullptr;
  }
  web_app->SetName(local_data.name());

  if (!sync_data.has_display_mode()) {
    DLOG(ERROR) << "WebApp proto parse error: no display_mode field";
    return nullptr;
  }
  web_app->SetDisplayMode(ToMojomDisplayMode(sync_data.display_mode()));

  if (!local_data.has_is_locally_installed()) {
    DLOG(ERROR) << "WebApp proto parse error: no is_locally_installed field";
    return nullptr;
  }
  web_app->SetIsLocallyInstalled(local_data.is_locally_installed());

  // Optional fields:
  if (local_data.has_description())
    web_app->SetDescription(local_data.description());

  if (local_data.has_scope()) {
    GURL scope(local_data.scope());
    if (scope.is_empty() || !scope.is_valid()) {
      DLOG(ERROR) << "WebApp proto scope parse error: "
                  << scope.possibly_invalid_spec();
      return nullptr;
    }
    web_app->SetScope(scope);
  }

  if (local_data.has_theme_color())
    web_app->SetThemeColor(local_data.theme_color());

  if (local_data.has_is_in_sync_install())
    web_app->SetIsInSyncInstall(local_data.is_in_sync_install());

  // Parse sync_data from sync proto.
  WebApp::SyncData parsed_sync_data;
  if (sync_data.has_name())
    parsed_sync_data.name = sync_data.name();
  if (sync_data.has_theme_color())
    parsed_sync_data.theme_color = sync_data.theme_color();
  web_app->SetSyncData(std::move(parsed_sync_data));

  WebApp::Icons icons;
  for (int i = 0; i < local_data.icons_size(); ++i) {
    const WebAppIconInfoProto& icon_proto = local_data.icons(i);

    GURL icon_url(icon_proto.url());
    if (icon_url.is_empty() || !icon_url.is_valid()) {
      DLOG(ERROR) << "WebApp IconInfo proto url parse error: "
                  << icon_url.possibly_invalid_spec();
      return nullptr;
    }

    icons.push_back({icon_url, icon_proto.size_in_px()});
  }
  web_app->SetIcons(std::move(icons));

  return web_app;
}

void WebAppDatabase::OnDatabaseOpened(
    RegistryOpenedCallback callback,
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    error_callback_.Run(*error);
    DLOG(ERROR) << "WebApps LevelDB opening error: " << error->ToString();
    return;
  }

  store_ = std::move(store);
  // TODO(loyso): Use ReadAllDataAndPreprocess to parse protos in the background
  // sequence.
  store_->ReadAllData(base::BindOnce(&WebAppDatabase::OnAllDataRead,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(callback)));
}

void WebAppDatabase::OnAllDataRead(
    RegistryOpenedCallback callback,
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore::RecordList> data_records) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    error_callback_.Run(*error);
    DLOG(ERROR) << "WebApps LevelDB data read error: " << error->ToString();
    return;
  }

  store_->ReadAllMetadata(base::BindOnce(
      &WebAppDatabase::OnAllMetadataRead, weak_ptr_factory_.GetWeakPtr(),
      std::move(data_records), std::move(callback)));
}

void WebAppDatabase::OnAllMetadataRead(
    std::unique_ptr<syncer::ModelTypeStore::RecordList> data_records,
    RegistryOpenedCallback callback,
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    error_callback_.Run(*error);
    DLOG(ERROR) << "WebApps LevelDB metadata read error: " << error->ToString();
    return;
  }

  Registry registry;
  for (const syncer::ModelTypeStore::Record& record : *data_records) {
    const AppId app_id = record.id;
    std::unique_ptr<WebApp> web_app = ParseWebApp(app_id, record.value);
    if (web_app)
      registry.emplace(app_id, std::move(web_app));
  }

  std::move(callback).Run(std::move(registry), std::move(metadata_batch));
  opened_ = true;
}

void WebAppDatabase::OnDataWritten(
    CompletionCallback callback,
    const base::Optional<syncer::ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    error_callback_.Run(*error);
    DLOG(ERROR) << "WebApps LevelDB write error: " << error->ToString();
  }

  std::move(callback).Run(!error);
}

// static
std::unique_ptr<WebApp> WebAppDatabase::ParseWebApp(const AppId& app_id,
                                                    const std::string& value) {
  WebAppProto proto;
  const bool parsed = proto.ParseFromString(value);
  if (!parsed) {
    DLOG(ERROR) << "WebApps LevelDB parse error: can't parse proto.";
    return nullptr;
  }

  auto web_app = CreateWebApp(proto);
  if (web_app->app_id() != app_id) {
    DLOG(ERROR) << "WebApps LevelDB error: app_id doesn't match storage key";
    return nullptr;
  }

  return web_app;
}

blink::mojom::DisplayMode ToMojomDisplayMode(
    ::sync_pb::WebAppSpecifics::DisplayMode display_mode) {
  switch (display_mode) {
    case ::sync_pb::WebAppSpecifics::BROWSER:
      return blink::mojom::DisplayMode::kBrowser;
    case ::sync_pb::WebAppSpecifics::MINIMAL_UI:
      return blink::mojom::DisplayMode::kMinimalUi;
    case ::sync_pb::WebAppSpecifics::STANDALONE:
      return blink::mojom::DisplayMode::kStandalone;
  }
}

::sync_pb::WebAppSpecifics::DisplayMode ToWebAppSpecificsDisplayMode(
    blink::mojom::DisplayMode display_mode) {
  switch (display_mode) {
    case blink::mojom::DisplayMode::kBrowser:
      return ::sync_pb::WebAppSpecifics::BROWSER;
    case blink::mojom::DisplayMode::kMinimalUi:
      return ::sync_pb::WebAppSpecifics::MINIMAL_UI;
    case blink::mojom::DisplayMode::kUndefined:
      NOTREACHED();
      FALLTHROUGH;
    case blink::mojom::DisplayMode::kFullscreen:
    case blink::mojom::DisplayMode::kStandalone:
      // We do not persist kFullscreen - see crbug.com/850465.
      return ::sync_pb::WebAppSpecifics::STANDALONE;
  }
}

}  // namespace web_app
