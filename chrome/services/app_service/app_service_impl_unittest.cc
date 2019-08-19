// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <sstream>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/services/app_service/app_service_impl.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class FakePublisher : public apps::mojom::Publisher {
 public:
  FakePublisher(AppServiceImpl* impl,
                apps::mojom::AppType app_type,
                std::vector<std::string> initial_app_ids)
      : app_type_(app_type), known_app_ids_(std::move(initial_app_ids)) {
    apps::mojom::PublisherPtr ptr;
    bindings_.AddBinding(this, mojo::MakeRequest(&ptr));
    impl->RegisterPublisher(std::move(ptr), app_type_);
  }

  void PublishMoreApps(std::vector<std::string> app_ids) {
    subscribers_.ForAllPtrs([this, &app_ids](auto* subscriber) {
      CallOnApps(subscriber, app_ids);
    });
    for (const auto& app_id : app_ids) {
      known_app_ids_.push_back(app_id);
    }
  }

  std::string load_icon_app_id;

 private:
  void Connect(apps::mojom::SubscriberPtr subscriber,
               apps::mojom::ConnectOptionsPtr opts) override {
    CallOnApps(subscriber.get(), known_app_ids_);
    subscribers_.AddPtr(std::move(subscriber));
  }

  void LoadIcon(const std::string& app_id,
                apps::mojom::IconKeyPtr icon_key,
                apps::mojom::IconCompression icon_compression,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                LoadIconCallback callback) override {
    load_icon_app_id = app_id;
    std::move(callback).Run(apps::mojom::IconValue::New());
  }

  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::mojom::LaunchSource launch_source,
              int64_t display_id) override {}

  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission) override {}

  void Uninstall(const std::string& app_id) override {}

  void OpenNativeSettings(const std::string& app_id) override {}

  void CallOnApps(apps::mojom::Subscriber* subscriber,
                  std::vector<std::string>& app_ids) {
    std::vector<apps::mojom::AppPtr> apps;
    for (const auto& app_id : app_ids) {
      auto app = apps::mojom::App::New();
      app->app_type = app_type_;
      app->app_id = app_id;
      apps.push_back(std::move(app));
    }
    subscriber->OnApps(std::move(apps));
  }

  apps::mojom::AppType app_type_;
  std::vector<std::string> known_app_ids_;
  mojo::BindingSet<apps::mojom::Publisher> bindings_;
  mojo::InterfacePtrSet<apps::mojom::Subscriber> subscribers_;
};

class FakeSubscriber : public apps::mojom::Subscriber {
 public:
  explicit FakeSubscriber(AppServiceImpl* impl) {
    apps::mojom::SubscriberPtr ptr;
    bindings_.AddBinding(this, mojo::MakeRequest(&ptr));
    impl->RegisterSubscriber(std::move(ptr), nullptr);
  }

  std::string AppIdsSeen() {
    std::stringstream ss;
    for (const auto& app_id : app_ids_seen_) {
      ss << app_id;
    }
    return ss.str();
  }

 private:
  void OnApps(std::vector<apps::mojom::AppPtr> deltas) override {
    for (const auto& delta : deltas) {
      app_ids_seen_.insert(delta->app_id);
    }
  }

  void Clone(apps::mojom::SubscriberRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  mojo::BindingSet<apps::mojom::Subscriber> bindings_;
  std::set<std::string> app_ids_seen_;
};

class AppServiceImplTest : public testing::Test {
 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(AppServiceImplTest, PubSub) {
  const int size_hint_in_dip = 64;

  AppServiceImpl impl;

  // Start with one subscriber.
  FakeSubscriber sub0(&impl);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("", sub0.AppIdsSeen());

  // Add one publisher.
  FakePublisher pub0(&impl, apps::mojom::AppType::kArc,
                     std::vector<std::string>{"A", "B"});
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("AB", sub0.AppIdsSeen());

  // Have that publisher publish more apps.
  pub0.PublishMoreApps(std::vector<std::string>{"C", "D", "E"});
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("ABCDE", sub0.AppIdsSeen());

  // Add a second publisher.
  FakePublisher pub1(&impl, apps::mojom::AppType::kBuiltIn,
                     std::vector<std::string>{"m"});
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("ABCDEm", sub0.AppIdsSeen());

  // Have both publishers publish more apps.
  pub0.PublishMoreApps(std::vector<std::string>{"F"});
  pub1.PublishMoreApps(std::vector<std::string>{"n"});
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("ABCDEFmn", sub0.AppIdsSeen());

  // Add a second subscriber.
  FakeSubscriber sub1(&impl);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("ABCDEFmn", sub0.AppIdsSeen());
  EXPECT_EQ("ABCDEFmn", sub1.AppIdsSeen());

  // Publish more apps.
  pub1.PublishMoreApps(std::vector<std::string>{"o", "p", "q"});
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("ABCDEFmnopq", sub0.AppIdsSeen());
  EXPECT_EQ("ABCDEFmnopq", sub1.AppIdsSeen());

  // Add a third publisher.
  FakePublisher pub2(&impl, apps::mojom::AppType::kCrostini,
                     std::vector<std::string>{"$"});
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("$ABCDEFmnopq", sub0.AppIdsSeen());
  EXPECT_EQ("$ABCDEFmnopq", sub1.AppIdsSeen());

  // Publish more apps.
  pub2.PublishMoreApps(std::vector<std::string>{"&"});
  pub1.PublishMoreApps(std::vector<std::string>{"r"});
  pub0.PublishMoreApps(std::vector<std::string>{"G"});
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("$&ABCDEFGmnopqr", sub0.AppIdsSeen());
  EXPECT_EQ("$&ABCDEFGmnopqr", sub1.AppIdsSeen());

  // Call LoadIcon on the impl twice.
  //
  // The first time (i == 0), it should be forwarded onto the AppType::kBuiltIn
  // publisher (which is pub1) and no other publisher.
  //
  // The second time (i == 1), passing AppType::kUnknown, none of the
  // publishers' LoadIcon's should fire, but the callback should still be run.
  for (int i = 0; i < 2; i++) {
    auto app_type = i == 0 ? apps::mojom::AppType::kBuiltIn
                           : apps::mojom::AppType::kUnknown;

    bool callback_ran = false;
    pub0.load_icon_app_id = "-";
    pub1.load_icon_app_id = "-";
    pub2.load_icon_app_id = "-";
    auto icon_key = apps::mojom::IconKey::New(0, 0, 0);
    constexpr bool allow_placeholder_icon = false;
    impl.LoadIcon(
        app_type, "o", std::move(icon_key),
        apps::mojom::IconCompression::kUncompressed, size_hint_in_dip,
        allow_placeholder_icon,
        base::BindOnce(
            [](bool* ran, apps::mojom::IconValuePtr iv) { *ran = true; },
            &callback_ran));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(callback_ran);
    EXPECT_EQ("-", pub0.load_icon_app_id);
    EXPECT_EQ(i == 0 ? "o" : "-", pub1.load_icon_app_id);
    EXPECT_EQ("-", pub2.load_icon_app_id);
  }
}

}  // namespace apps
