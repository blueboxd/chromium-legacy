// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_database.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/model_error.h"

namespace web_app {

WebAppDatabase::WebAppDatabase(AbstractWebAppDatabaseFactory* database_factory)
    : database_factory_(database_factory) {
  DCHECK(database_factory_);
}

WebAppDatabase::~WebAppDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WebAppDatabase::OpenDatabase(OnceRegistryOpenedCallback callback) {
  DCHECK(!store_);

  syncer::OnceModelTypeStoreFactory store_factory =
      database_factory_->GetStoreFactory();

  // CreateStore then ReadRegistry.
  CreateStore(
      std::move(store_factory),
      base::BindOnce(&WebAppDatabase::ReadRegistry,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebAppDatabase::WriteWebApp(const WebApp& web_app) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(opened_);

  BeginTransaction();

  auto proto = CreateWebAppProto(web_app);
  write_batch_->WriteData(proto->app_id(), proto->SerializeAsString());

  CommitTransaction();
}

void WebAppDatabase::DeleteWebApps(std::vector<AppId> app_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(opened_);

  BeginTransaction();
  for (auto& app_id : app_ids)
    write_batch_->DeleteData(app_id);
  CommitTransaction();
}

// static
std::unique_ptr<WebAppProto> WebAppDatabase::CreateWebAppProto(
    const WebApp& web_app) {
  auto proto = std::make_unique<WebAppProto>();

  // Required fields:
  DCHECK(!web_app.app_id().empty());
  proto->set_app_id(web_app.app_id());

  DCHECK(!web_app.launch_url().is_empty() && web_app.launch_url().is_valid());
  proto->set_launch_url(web_app.launch_url().spec());

  proto->set_name(web_app.name());

  // Optional fields:
  proto->set_description(web_app.description());
  if (!web_app.scope().is_empty())
    proto->set_scope(web_app.scope().spec());
  if (web_app.theme_color().has_value())
    proto->set_theme_color(web_app.theme_color().value());

  for (const WebApp::IconInfo& icon : web_app.icons()) {
    WebAppIconInfoProto* icon_proto = proto->add_icons();
    icon_proto->set_url(icon.url.spec());
    icon_proto->set_size_in_px(icon.size_in_px);
  }

  return proto;
}

// static
std::unique_ptr<WebApp> WebAppDatabase::CreateWebApp(const WebAppProto& proto) {
  auto web_app = std::make_unique<WebApp>(proto.app_id());

  // Required fields:
  GURL launch_url(proto.launch_url());
  if (launch_url.is_empty() || !launch_url.is_valid()) {
    DLOG(ERROR) << "WebApp proto launch_url parse error: "
                << launch_url.possibly_invalid_spec();
    return nullptr;
  }
  web_app->SetLaunchUrl(launch_url);

  if (!proto.has_name()) {
    DLOG(ERROR) << "WebApp proto parse error: no name field";
    return nullptr;
  }
  web_app->SetName(proto.name());

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

  if (proto.has_theme_color())
    web_app->SetThemeColor(proto.theme_color());

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

void WebAppDatabase::ReadRegistry(OnceRegistryOpenedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(store_);
  store_->ReadAllData(base::BindOnce(&WebAppDatabase::OnAllDataRead,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(callback)));
}

void WebAppDatabase::CreateStore(
    syncer::OnceModelTypeStoreFactory store_factory,
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(store_factory)
      .Run(syncer::WEB_APPS,
           base::BindOnce(&WebAppDatabase::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr(), std::move(closure)));
}

void WebAppDatabase::OnStoreCreated(
    base::OnceClosure closure,
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    DLOG(ERROR) << "WebApps LevelDB opening error: " << error->ToString();
    return;
  }

  store_ = std::move(store);
  std::move(closure).Run();
}

void WebAppDatabase::OnAllDataRead(
    OnceRegistryOpenedCallback callback,
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
    const base::Optional<syncer::ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error)
    DLOG(ERROR) << "WebApps LevelDB write error: " << error->ToString();
}

// static
std::unique_ptr<WebApp> WebAppDatabase::ParseWebApp(const AppId& app_id,
                                                    const std::string& value) {
  WebAppProto proto;
  const bool parsed = proto.ParseFromString(value);
  if (!parsed || proto.app_id() != app_id) {
    DLOG(ERROR) << "WebApps LevelDB parse error (unknown).";
    return nullptr;
  }

  return CreateWebApp(proto);
}

void WebAppDatabase::BeginTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!write_batch_);
  write_batch_ = store_->CreateWriteBatch();
}

void WebAppDatabase::CommitTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(write_batch_);

  store_->CommitWriteBatch(std::move(write_batch_),
                           base::BindOnce(&WebAppDatabase::OnDataWritten,
                                          weak_ptr_factory_.GetWeakPtr()));
  write_batch_.reset();
}

}  // namespace web_app
