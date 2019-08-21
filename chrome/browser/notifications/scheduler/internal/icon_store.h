// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_ICON_STORE_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_ICON_STORE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/proto/icon.pb.h"
#include "chrome/browser/notifications/scheduler/internal/icon_converter.h"
#include "chrome/browser/notifications/scheduler/internal/icon_entry.h"
#include "chrome/browser/notifications/scheduler/public/notification_data.h"
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
  using IconsMap = std::map<std::string /*icons_uuid*/, SkBitmap>;
  using LoadIconsCallback = base::OnceCallback<void(bool, IconsMap)>;
  using InitCallback = base::OnceCallback<void(bool)>;
  using UpdateCallback = base::OnceCallback<void(bool)>;
  using AddCallback =
      base::OnceCallback<void(std::vector<std::string> /*icons_uuid*/, bool)>;

  // Initializes the storage.
  virtual void Init(InitCallback callback) = 0;

  // Loads multiple icons.
  virtual void LoadIcons(const std::vector<std::string>& keys,
                         LoadIconsCallback callback) = 0;

  // Adds multiple icons to storage.
  virtual void AddIcons(std::vector<SkBitmap>, AddCallback callback) = 0;

  // Deletes multiple icons.
  virtual void DeleteIcons(const std::vector<std::string>& keys,
                           UpdateCallback callback) = 0;

  IconStore() = default;
  virtual ~IconStore() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(IconStore);
};

// IconStore implementation backed by a proto database.
class IconProtoDbStore : public IconStore {
 public:
  explicit IconProtoDbStore(
      std::unique_ptr<leveldb_proto::ProtoDatabase<proto::Icon, IconEntry>> db,
      std::unique_ptr<IconConverter> icon_converter);
  ~IconProtoDbStore() override;

 private:
  // IconStore implementation.
  void Init(InitCallback callback) override;
  void LoadIcons(const std::vector<std::string>& keys,
                 LoadIconsCallback callback) override;
  void AddIcons(std::vector<SkBitmap> icons, AddCallback callback) override;
  void DeleteIcons(const std::vector<std::string>& keys,
                   UpdateCallback callback) override;

  // Called when the proto database is initialized.
  void OnDbInitialized(InitCallback callback,
                       leveldb_proto::Enums::InitStatus status);

  // Called when the icon is retrieved from the database.
  void OnIconEntriesLoaded(
      LoadIconsCallback callback,
      bool success,
      std::unique_ptr<std::map<std::string, IconEntry>> icon_entries);

  // Called when the icons are encoded.
  void OnIconsEncoded(AddCallback callback,
                      std::vector<std::string> icons_uuid,
                      std::vector<std::string> encoded_icons_data);

  // Called when the encoded data are decoded.
  void OnIconsDecoded(LoadIconsCallback callback,
                      std::vector<std::string> icons_uuid,
                      std::vector<SkBitmap> decoded_icons);

  // The proto database instance that persists data.
  std::unique_ptr<leveldb_proto::ProtoDatabase<proto::Icon, IconEntry>> db_;

  // Help serializing icons to disk and deserializing encoded data to icons.
  std::unique_ptr<IconConverter> icon_converter_;

  base::WeakPtrFactory<IconProtoDbStore> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IconProtoDbStore);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_ICON_STORE_H_
