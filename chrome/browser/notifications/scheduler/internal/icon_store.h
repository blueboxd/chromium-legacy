// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_ICON_STORE_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_ICON_STORE_H_

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/proto/icon.pb.h"
#include "chrome/browser/notifications/scheduler/internal/icon_entry.h"
#include "components/leveldb_proto/public/proto_database.h"

// Forward declaration for proto conversion.
namespace leveldb_proto {
void DataToProto(notifications::IconEntry* icon_entry,
                 notifications::proto::Icon* proto);

void ProtoToData(notifications::proto::Icon* proto,
                 notifications::IconEntry* icon_entry);
}  // namespace leveldb_proto

namespace notifications {

// Storage interface used to read/write icon data, each time only one icon can
// be loaded into memory.
class IconStore {
 public:
  using LoadCallback =
      base::OnceCallback<void(bool, std::unique_ptr<IconEntry>)>;
  using InitCallback = base::OnceCallback<void(bool)>;
  using UpdateCallback = base::OnceCallback<void(bool)>;

  // Initializes the storage.
  virtual void Init(InitCallback callback) = 0;

  // Loads one icon.
  virtual void Load(const std::string& key, LoadCallback callback) = 0;

  // Adds one icon to storage.
  virtual void Add(const std::string& key,
                   std::unique_ptr<IconEntry> entry,
                   UpdateCallback callback) = 0;

  // Deletes an icon.
  virtual void Delete(const std::string& key, UpdateCallback callback) = 0;

  IconStore() = default;
  virtual ~IconStore() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(IconStore);
};

// IconStore implementation backed by a proto database.
class IconProtoDbStore : public IconStore {
 public:
  explicit IconProtoDbStore(
      std::unique_ptr<leveldb_proto::ProtoDatabase<proto::Icon, IconEntry>> db);
  ~IconProtoDbStore() override;

 private:
  // IconStore implementation.
  void Init(InitCallback callback) override;
  void Load(const std::string& key, LoadCallback callback) override;
  void Add(const std::string& key,
           std::unique_ptr<IconEntry> entry,
           UpdateCallback callback) override;
  void Delete(const std::string& key, UpdateCallback callback) override;

  // Called when the proto database is initialized.
  void OnDbInitialized(InitCallback callback,
                       leveldb_proto::Enums::InitStatus status);

  // Called when the icon is retrieved from the database.
  void OnIconEntryLoaded(LoadCallback callback,
                         bool success,
                         std::unique_ptr<IconEntry> icon_entry);

  // The proto database instance that persists data.
  std::unique_ptr<leveldb_proto::ProtoDatabase<proto::Icon, IconEntry>> db_;

  base::WeakPtrFactory<IconProtoDbStore> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IconProtoDbStore);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_ICON_STORE_H_
