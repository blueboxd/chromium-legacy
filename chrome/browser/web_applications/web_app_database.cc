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
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/web_app_specifics.pb.h"

namespace web_app {

WebAppDatabase::WebAppDatabase(AbstractWebAppDatabaseFactory* database_factory)
    : database_factory_(database_factory) {
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

void WebAppDatabase::BeginTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(opened_);

  DCHECK(!write_batch_);
  write_batch_ = store_->CreateWriteBatch();
}

void WebAppDatabase::CommitTransaction(const RegistryUpdateData& update_data,
                                       CompletionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(opened_);

  DCHECK(write_batch_);
  DCHECK(!update_data.IsEmpty());

  for (const std::unique_ptr<WebApp>& web_app : update_data.apps_to_create) {
    auto proto = CreateWebAppProto(*web_app);
    write_batch_->WriteData(web_app->app_id(), proto->SerializeAsString());
  }

  for (const AppId& app_id : update_data.apps_to_delete)
    write_batch_->DeleteData(app_id);

  for (const WebApp* web_app : update_data.apps_to_update) {
    auto proto = CreateWebAppProto(*web_app);
    write_batch_->WriteData(web_app->app_id(), proto->SerializeAsString());
  }

  store_->CommitWriteBatch(
      std::move(write_batch_),
      base::BindOnce(&WebAppDatabase::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  write_batch_.reset();
}

void WebAppDatabase::CancelTransaction() {
  DCHECK(write_batch_);
  write_batch_.reset();
}

// static
std::unique_ptr<WebAppProto> WebAppDatabase::CreateWebAppProto(
    const WebApp& web_app) {
  auto proto = std::make_unique<WebAppProto>();

  // Required fields:
  const GURL launch_url = web_app.launch_url();
  DCHECK(!launch_url.is_empty() && launch_url.is_valid());

  DCHECK(!web_app.app_id().empty());
  DCHECK_EQ(web_app.app_id(), GenerateAppIdFromURL(launch_url));

  sync_pb::WebAppSpecifics* specifics = proto->mutable_specifics();

  specifics->set_launch_url(launch_url.spec());

  specifics->set_name(web_app.name());

  DCHECK_NE(LaunchContainer::kDefault, web_app.launch_container());
  specifics->set_launch_container(web_app.launch_container() ==
                                          LaunchContainer::kWindow
                                      ? sync_pb::WebAppSpecifics::WINDOW
                                      : sync_pb::WebAppSpecifics::TAB);

  DCHECK(web_app.sources_.any());
  proto->mutable_sources()->set_system(web_app.sources_[Source::kSystem]);
  proto->mutable_sources()->set_policy(web_app.sources_[Source::kPolicy]);
  proto->mutable_sources()->set_web_app_store(
      web_app.sources_[Source::kWebAppStore]);
  proto->mutable_sources()->set_sync(web_app.sources_[Source::kSync]);
  proto->mutable_sources()->set_default_(web_app.sources_[Source::kDefault]);

  proto->set_is_locally_installed(web_app.is_locally_installed());

  // Optional fields:
  proto->set_description(web_app.description());
  if (!web_app.scope().is_empty())
    proto->set_scope(web_app.scope().spec());
  if (web_app.theme_color().has_value())
    specifics->set_theme_color(web_app.theme_color().value());

  for (const WebApp::IconInfo& icon : web_app.icons()) {
    WebAppIconInfoProto* icon_proto = proto->add_icons();
    icon_proto->set_url(icon.url.spec());
    icon_proto->set_size_in_px(icon.size_in_px);
  }

  return proto;
}

// static
std::unique_ptr<WebApp> WebAppDatabase::CreateWebApp(const WebAppProto& proto) {
  if (!proto.has_specifics()) {
    DLOG(ERROR) << "WebApp proto parse error: no specifics field";
    return nullptr;
  }

  const sync_pb::WebAppSpecifics& specifics = proto.specifics();

  // AppId is a hash of launch_url. Read launch_url first:
  GURL launch_url(specifics.launch_url());
  if (launch_url.is_empty() || !launch_url.is_valid()) {
    DLOG(ERROR) << "WebApp proto launch_url parse error: "
                << launch_url.possibly_invalid_spec();
    return nullptr;
  }

  const AppId app_id = GenerateAppIdFromURL(launch_url);

  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->SetLaunchUrl(launch_url);

  // Required fields:
  if (!proto.has_sources()) {
    DLOG(ERROR) << "WebApp proto parse error: no sources field";
    return nullptr;
  }

  WebApp::Sources sources;
  sources[Source::kSystem] = proto.sources().system();
  sources[Source::kPolicy] = proto.sources().policy();
  sources[Source::kWebAppStore] = proto.sources().web_app_store();
  sources[Source::kSync] = proto.sources().sync();
  sources[Source::kDefault] = proto.sources().default_();
  if (!sources.any()) {
    DLOG(ERROR) << "WebApp proto parse error: no any source in sources field";
    return nullptr;
  }
  web_app->sources_ = sources;

  if (!specifics.has_name()) {
    DLOG(ERROR) << "WebApp proto parse error: no name field";
    return nullptr;
  }
  web_app->SetName(specifics.name());

  if (!specifics.has_launch_container()) {
    DLOG(ERROR) << "WebApp proto parse error: no launch_container field";
    return nullptr;
  }
  web_app->SetLaunchContainer(specifics.launch_container() ==
                                      sync_pb::WebAppSpecifics::WINDOW
                                  ? LaunchContainer::kWindow
                                  : LaunchContainer::kTab);

  if (!proto.has_is_locally_installed()) {
    DLOG(ERROR) << "WebApp proto parse error: no is_locally_installed field";
    return nullptr;
  }
  web_app->SetIsLocallyInstalled(proto.is_locally_installed());

  // Optional fields:
  if (proto.has_description())
    web_app->SetDescription(proto.description());

  if (proto.has_scope()) {
    GURL scope(proto.scope());
    if (scope.is_empty() || !scope.is_valid()) {
      DLOG(ERROR) << "WebApp proto scope parse error: "
                  << scope.possibly_invalid_spec();
      return nullptr;
    }
    web_app->SetScope(scope);
  }

  if (specifics.has_theme_color())
    web_app->SetThemeColor(specifics.theme_color());

  WebApp::Icons icons;
  for (int i = 0; i < proto.icons_size(); ++i) {
    const WebAppIconInfoProto& icon_proto = proto.icons(i);

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
    DLOG(ERROR) << "WebApps LevelDB opening error: " << error->ToString();
    return;
  }

  store_ = std::move(store);
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
    DLOG(ERROR) << "WebApps LevelDB read error: " << error->ToString();
    return;
  }

  Registry registry;
  for (const syncer::ModelTypeStore::Record& record : *data_records) {
    const AppId app_id = record.id;
    std::unique_ptr<WebApp> web_app = ParseWebApp(app_id, record.value);
    if (web_app)
      registry.emplace(app_id, std::move(web_app));
  }

  std::move(callback).Run(std::move(registry));
  opened_ = true;
}

void WebAppDatabase::OnDataWritten(
    CompletionCallback callback,
    const base::Optional<syncer::ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error)
    DLOG(ERROR) << "WebApps LevelDB write error: " << error->ToString();

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

}  // namespace web_app
