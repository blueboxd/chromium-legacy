// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/crostini_file_tasks.h"

#include "base/files/file_path.h"
#include "base/values.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/guest_os/guest_os_pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/entry_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace file_tasks {

class CrostiniFileTasksTest : public testing::Test {
 public:
  CrostiniFileTasksTest() {}

 protected:
  void AddApp(const std::string& id,
              const std::string& name,
              const std::string& mime) {
    // crostini.registry {<id>: {container_name: "penguin", name: {"": <name>},
    //                           mime_types: [<mime>,], vm_name: "termina"}}
    DictionaryPrefUpdate update(profile_.GetPrefs(),
                                guest_os::prefs::kGuestOsRegistry);
    base::DictionaryValue* registry = update.Get();
    base::Value app(base::Value::Type::DICTIONARY);
    app.SetKey("container_name", base::Value("penguin"));
    base::Value mimes(base::Value::Type::LIST);
    mimes.Append(mime);
    app.SetKey("mime_types", std::move(mimes));
    base::Value name_dict(base::Value::Type::DICTIONARY);
    name_dict.SetKey("", base::Value(name));
    app.SetKey("name", std::move(name_dict));
    app.SetKey("vm_name", base::Value("termina"));
    registry->SetKey(id, std::move(app));
  }

  void AddEntry(const std::string& path, const std::string& mime) {
    entries_.push_back(
        extensions::EntryInfo(base::FilePath(path), mime, false));
  }

  void AddMime(const std::string& file_ext, const std::string& mime) {
    // crostini.mime_types {<termina/penguin/<file_ext>:
    // {container_name: "penguin", mime_type: <mime>, vm_name: "termina"}}
    DictionaryPrefUpdate update(profile_.GetPrefs(),
                                crostini::prefs::kCrostiniMimeTypes);
    base::DictionaryValue* mimes = update.Get();
    base::Value mime_dict(base::Value::Type::DICTIONARY);
    mime_dict.SetKey("container_name", base::Value("penguin"));
    mime_dict.SetKey("mime_type", base::Value(mime));
    mime_dict.SetKey("vm_name", base::Value("termina"));
    mimes->SetKey("termina/penguin/" + file_ext, std::move(mime_dict));
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::vector<extensions::EntryInfo> entries_;
  std::vector<std::string> app_ids_;
  std::vector<std::string> app_names_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniFileTasksTest);
};

TEST_F(CrostiniFileTasksTest, NoApps) {
  AddApp("app1", "name1", "test/mime1");
  AddEntry("entry.txt", "test/mime2");
  FindCrostiniApps(&profile_, entries_, &app_ids_, &app_names_);
  EXPECT_THAT(app_ids_, testing::IsEmpty());
  EXPECT_THAT(app_names_, testing::IsEmpty());
}

TEST_F(CrostiniFileTasksTest, AppRegisteredForMime) {
  AddApp("app1", "name1", "test/mime1");
  AddEntry("entry.txt", "test/mime1");
  FindCrostiniApps(&profile_, entries_, &app_ids_, &app_names_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1"));
  EXPECT_THAT(app_names_, testing::ElementsAre("name1"));
}

TEST_F(CrostiniFileTasksTest, NotAllEntries) {
  AddApp("app1", "name1", "test/mime1");
  AddApp("app2", "name2", "test/mime2");
  AddEntry("entry1.txt", "test/mime1");
  AddEntry("entry2.txt", "test/mime2");
  FindCrostiniApps(&profile_, entries_, &app_ids_, &app_names_);
  EXPECT_THAT(app_ids_, testing::IsEmpty());
  EXPECT_THAT(app_names_, testing::IsEmpty());
}

TEST_F(CrostiniFileTasksTest, MultipleAppsRegistered) {
  AddApp("app1", "name1", "test/mime1");
  AddApp("app2", "name2", "test/mime1");
  AddEntry("entry.txt", "test/mime1");
  FindCrostiniApps(&profile_, entries_, &app_ids_, &app_names_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1", "app2"));
  EXPECT_THAT(app_names_, testing::ElementsAre("name1", "name2"));
}

TEST_F(CrostiniFileTasksTest, AppRegisteredForTextPlain) {
  AddApp("app1", "name1", "text/plain");
  AddEntry("entry.js", "text/javascript");
  FindCrostiniApps(&profile_, entries_, &app_ids_, &app_names_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1"));
  EXPECT_THAT(app_names_, testing::ElementsAre("name1"));
}

TEST_F(CrostiniFileTasksTest, MimeServiceForTextPlain) {
  AddApp("app1", "name1", "test/mime1");
  AddEntry("entry.unknown", "text/plain");
  AddMime("unknown", "test/mime1");
  FindCrostiniApps(&profile_, entries_, &app_ids_, &app_names_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1"));
  EXPECT_THAT(app_names_, testing::ElementsAre("name1"));
}

TEST_F(CrostiniFileTasksTest, MimeServiceForApplicationOctetStream) {
  AddApp("app1", "name1", "test/mime1");
  AddEntry("entry.unknown", "application/octet-stream");
  AddMime("unknown", "test/mime1");
  FindCrostiniApps(&profile_, entries_, &app_ids_, &app_names_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1"));
  EXPECT_THAT(app_names_, testing::ElementsAre("name1"));
}

}  // namespace file_tasks
}  // namespace file_manager
