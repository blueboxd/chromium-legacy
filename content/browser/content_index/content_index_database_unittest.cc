// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/content_index/content_index_database.h"

#include <string>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/public/browser/content_index_provider.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/origin.h"

namespace content {
namespace {

using ::testing::_;

class MockContentIndexProvider : public ContentIndexProvider {
 public:
  MOCK_METHOD1(OnContentAdded, void(ContentIndexEntry entry));
  MOCK_METHOD3(OnContentDeleted,
               void(int64_t service_Worker_registration_id,
                    const url::Origin& origin,
                    const std::string& description_id));
};

class ContentIndexTestBrowserContext : public TestBrowserContext {
 public:
  ContentIndexTestBrowserContext()
      : delegate_(std::make_unique<MockContentIndexProvider>()) {}
  ~ContentIndexTestBrowserContext() override = default;

  MockContentIndexProvider* GetContentIndexProvider() override {
    return delegate_.get();
  }

 private:
  std::unique_ptr<MockContentIndexProvider> delegate_;
};

void DidRegisterServiceWorker(int64_t* out_service_worker_registration_id,
                              base::OnceClosure quit_closure,
                              blink::ServiceWorkerStatusCode status,
                              const std::string& status_message,
                              int64_t service_worker_registration_id) {
  DCHECK(out_service_worker_registration_id);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status) << status_message;

  *out_service_worker_registration_id = service_worker_registration_id;

  std::move(quit_closure).Run();
}

void DidFindServiceWorkerRegistration(
    scoped_refptr<ServiceWorkerRegistration>* out_service_worker_registration,
    base::OnceClosure quit_closure,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK(out_service_worker_registration);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status)
      << blink::ServiceWorkerStatusToString(status);

  *out_service_worker_registration = service_worker_registration;

  std::move(quit_closure).Run();
}

void DatabaseErrorCallback(base::OnceClosure quit_closure,
                           blink::mojom::ContentIndexError* out_error,
                           blink::mojom::ContentIndexError error) {
  *out_error = error;
  std::move(quit_closure).Run();
}

void GetDescriptionsCallback(
    base::OnceClosure quit_closure,
    blink::mojom::ContentIndexError* out_error,
    std::vector<blink::mojom::ContentDescriptionPtr>* out_descriptions,
    blink::mojom::ContentIndexError error,
    std::vector<blink::mojom::ContentDescriptionPtr> descriptions) {
  if (out_error)
    *out_error = error;
  DCHECK(out_descriptions);
  *out_descriptions = std::move(descriptions);
  std::move(quit_closure).Run();
}

SkBitmap CreateTestIcon() {
  SkBitmap icon;
  icon.allocN32Pixels(42, 42);
  return icon;
}

}  // namespace

class ContentIndexDatabaseTest : public ::testing::Test {
 public:
  ContentIndexDatabaseTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        embedded_worker_test_helper_(base::FilePath() /* in memory */) {}

  ~ContentIndexDatabaseTest() override = default;

  void SetUp() override {
    // Register Service Worker.
    service_worker_registration_id_ = RegisterServiceWorker();
    ASSERT_NE(service_worker_registration_id_,
              blink::mojom::kInvalidServiceWorkerRegistrationId);
    database_ = std::make_unique<ContentIndexDatabase>(
        &browser_context_, embedded_worker_test_helper_.context_wrapper());
  }

  blink::mojom::ContentDescriptionPtr CreateDescription(const std::string& id) {
    return blink::mojom::ContentDescription::New(
        id, "title", "description", blink::mojom::ContentCategory::HOME_PAGE,
        "https://example.com", "https://example.com");
  }

  blink::mojom::ContentIndexError AddEntry(
      blink::mojom::ContentDescriptionPtr description) {
    base::RunLoop run_loop;
    blink::mojom::ContentIndexError error;
    database_->AddEntry(
        service_worker_registration_id_, origin_, std::move(description),
        CreateTestIcon(), launch_url(),
        base::BindOnce(&DatabaseErrorCallback, run_loop.QuitClosure(), &error));
    run_loop.Run();

    return error;
  }

  blink::mojom::ContentIndexError DeleteEntry(const std::string& id) {
    base::RunLoop run_loop;
    blink::mojom::ContentIndexError error;
    database_->DeleteEntry(
        service_worker_registration_id_, origin_, id,
        base::BindOnce(&DatabaseErrorCallback, run_loop.QuitClosure(), &error));
    run_loop.Run();

    return error;
  }

  std::vector<blink::mojom::ContentDescriptionPtr> GetDescriptions(
      blink::mojom::ContentIndexError* out_error = nullptr) {
    base::RunLoop run_loop;
    std::vector<blink::mojom::ContentDescriptionPtr> descriptions;
    database_->GetDescriptions(
        service_worker_registration_id_,
        base::BindOnce(&GetDescriptionsCallback, run_loop.QuitClosure(),
                       out_error, &descriptions));
    run_loop.Run();
    return descriptions;
  }

  SkBitmap GetIcon(const std::string& id) {
    base::RunLoop run_loop;
    SkBitmap out_icon;
    database_->GetIcon(service_worker_registration_id_, id,
                       base::BindLambdaForTesting([&](SkBitmap icon) {
                         out_icon = std::move(icon);
                         run_loop.Quit();
                       }));
    run_loop.Run();
    return out_icon;
  }

  std::vector<ContentIndexEntry> GetAllEntries() {
    base::RunLoop run_loop;
    std::vector<ContentIndexEntry> out_entries;
    database_->GetAllEntries(
        base::BindLambdaForTesting([&](blink::mojom::ContentIndexError error,
                                       std::vector<ContentIndexEntry> entries) {
          ASSERT_EQ(error, blink::mojom::ContentIndexError::NONE);
          out_entries = std::move(entries);
          run_loop.Quit();
        }));
    run_loop.Run();
    return out_entries;
  }

  std::unique_ptr<ContentIndexEntry> GetEntry(
      const std::string& description_id) {
    base::RunLoop run_loop;
    std::unique_ptr<ContentIndexEntry> out_entry;
    database_->GetEntry(service_worker_registration_id_, description_id,
                        base::BindLambdaForTesting(
                            [&](base::Optional<ContentIndexEntry> entry) {
                              if (entry)
                                out_entry = std::make_unique<ContentIndexEntry>(
                                    std::move(*entry));
                              run_loop.Quit();
                            }));
    run_loop.Run();
    return out_entry;
  }

  MockContentIndexProvider* provider() {
    return browser_context_.GetContentIndexProvider();
  }

  int64_t service_worker_registration_id() {
    return service_worker_registration_id_;
  }

  ContentIndexDatabase* database() { return database_.get(); }

  TestBrowserThreadBundle& thread_bundle() { return thread_bundle_; }

  const url::Origin& origin() { return origin_; }

  GURL launch_url() { return origin_.GetURL(); }

 private:
  int64_t RegisterServiceWorker() {
    GURL script_url(origin_.GetURL().spec() + "sw.js");
    int64_t service_worker_registration_id =
        blink::mojom::kInvalidServiceWorkerRegistrationId;

    {
      blink::mojom::ServiceWorkerRegistrationOptions options;
      options.scope = origin_.GetURL();
      base::RunLoop run_loop;
      embedded_worker_test_helper_.context()->RegisterServiceWorker(
          script_url, options,
          base::BindOnce(&DidRegisterServiceWorker,
                         &service_worker_registration_id,
                         run_loop.QuitClosure()));

      run_loop.Run();
    }

    if (service_worker_registration_id ==
        blink::mojom::kInvalidServiceWorkerRegistrationId) {
      ADD_FAILURE() << "Could not obtain a valid Service Worker registration";
      return blink::mojom::kInvalidServiceWorkerRegistrationId;
    }

    {
      base::RunLoop run_loop;
      embedded_worker_test_helper_.context()->storage()->FindRegistrationForId(
          service_worker_registration_id, origin_.GetURL(),
          base::BindOnce(&DidFindServiceWorkerRegistration,
                         &service_worker_registration_,
                         run_loop.QuitClosure()));
      run_loop.Run();
    }

    // Wait for the worker to be activated.
    base::RunLoop().RunUntilIdle();

    if (!service_worker_registration_) {
      ADD_FAILURE() << "Could not find the new Service Worker registration.";
      return blink::mojom::kInvalidServiceWorkerRegistrationId;
    }

    return service_worker_registration_id;
  }

  TestBrowserThreadBundle thread_bundle_;  // Must be first member.
  ContentIndexTestBrowserContext browser_context_;
  url::Origin origin_ = url::Origin::Create(GURL("https://example.com"));
  int64_t service_worker_registration_id_ =
      blink::mojom::kInvalidServiceWorkerRegistrationId;
  EmbeddedWorkerTestHelper embedded_worker_test_helper_;
  scoped_refptr<ServiceWorkerRegistration> service_worker_registration_;
  std::unique_ptr<ContentIndexDatabase> database_;

  DISALLOW_COPY_AND_ASSIGN(ContentIndexDatabaseTest);
};

TEST_F(ContentIndexDatabaseTest, DatabaseOperations) {
  // Initially database will be empty.
  {
    blink::mojom::ContentIndexError error;
    auto descriptions = GetDescriptions(&error);
    EXPECT_TRUE(descriptions.empty());
    EXPECT_EQ(error, blink::mojom::ContentIndexError::NONE);
  }

  // Insert entries and expect to find them.
  EXPECT_EQ(AddEntry(CreateDescription("id1")),
            blink::mojom::ContentIndexError::NONE);
  EXPECT_EQ(AddEntry(CreateDescription("id2")),
            blink::mojom::ContentIndexError::NONE);
  EXPECT_EQ(GetDescriptions().size(), 2u);

  // Remove an entry.
  EXPECT_EQ(DeleteEntry("id2"), blink::mojom::ContentIndexError::NONE);

  // Inspect the last remaining element.
  auto descriptions = GetDescriptions();
  ASSERT_EQ(descriptions.size(), 1u);
  auto expected_description = CreateDescription("id1");
  EXPECT_TRUE(descriptions[0]->Equals(*expected_description));
}

TEST_F(ContentIndexDatabaseTest, AddDuplicateIdWillOverwrite) {
  auto description1 = CreateDescription("id");
  description1->title = "title1";
  auto description2 = CreateDescription("id");
  description2->title = "title2";

  EXPECT_EQ(AddEntry(std::move(description1)),
            blink::mojom::ContentIndexError::NONE);
  EXPECT_EQ(AddEntry(std::move(description2)),
            blink::mojom::ContentIndexError::NONE);

  auto descriptions = GetDescriptions();
  ASSERT_EQ(descriptions.size(), 1u);
  EXPECT_EQ(descriptions[0]->id, "id");
  EXPECT_EQ(descriptions[0]->title, "title2");
}

TEST_F(ContentIndexDatabaseTest, DeleteNonExistentEntry) {
  auto descriptions = GetDescriptions();
  EXPECT_TRUE(descriptions.empty());

  EXPECT_EQ(DeleteEntry("id"), blink::mojom::ContentIndexError::NONE);
}

TEST_F(ContentIndexDatabaseTest, ProviderUpdated) {
  {
    std::unique_ptr<ContentIndexEntry> out_entry;
    EXPECT_CALL(*provider(), OnContentAdded(_))
        .WillOnce(testing::Invoke([&](auto entry) {
          out_entry = std::make_unique<ContentIndexEntry>(std::move(entry));
        }));
    EXPECT_EQ(AddEntry(CreateDescription("id")),
              blink::mojom::ContentIndexError::NONE);

    // Wait for the provider to receive the OnContentAdded event.
    thread_bundle().RunUntilIdle();

    ASSERT_TRUE(out_entry);
    ASSERT_TRUE(out_entry->description);
    EXPECT_EQ(out_entry->service_worker_registration_id,
              service_worker_registration_id());
    EXPECT_EQ(out_entry->description->id, "id");
    EXPECT_EQ(out_entry->launch_url, launch_url());
    EXPECT_FALSE(out_entry->registration_time.is_null());
  }

  {
    EXPECT_CALL(*provider(), OnContentDeleted(service_worker_registration_id(),
                                              origin(), "id"));
    EXPECT_EQ(DeleteEntry("id"), blink::mojom::ContentIndexError::NONE);
    thread_bundle().RunUntilIdle();
  }
}

TEST_F(ContentIndexDatabaseTest, IconLifetimeTiedToEntry) {
  // Initially we don't have an icon.
  EXPECT_TRUE(GetIcon("id").isNull());

  EXPECT_EQ(AddEntry(CreateDescription("id")),
            blink::mojom::ContentIndexError::NONE);
  SkBitmap icon = GetIcon("id");
  EXPECT_FALSE(icon.isNull());
  EXPECT_FALSE(icon.drawsNothing());
  EXPECT_EQ(icon.width(), 42);
  EXPECT_EQ(icon.height(), 42);

  EXPECT_EQ(DeleteEntry("id"), blink::mojom::ContentIndexError::NONE);
  EXPECT_TRUE(GetIcon("id").isNull());
}

TEST_F(ContentIndexDatabaseTest, GetEntries) {
  // Initially there are no entries.
  EXPECT_FALSE(GetEntry("any-id"));
  EXPECT_TRUE(GetAllEntries().empty());

  std::unique_ptr<ContentIndexEntry> added_entry;
  {
    EXPECT_CALL(*provider(), OnContentAdded(_))
        .WillOnce(testing::Invoke([&](auto entry) {
          added_entry = std::make_unique<ContentIndexEntry>(std::move(entry));
        }));
    EXPECT_EQ(AddEntry(CreateDescription("id")),
              blink::mojom::ContentIndexError::NONE);
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(added_entry);
  }

  // Check the notified entries match the queried entries.
  {
    auto entry = GetEntry("id");
    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->description->Equals(*added_entry->description));

    auto entries = GetAllEntries();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_TRUE(entries[0].description->Equals(*added_entry->description));
  }

  // Add one more entry.
  {
    EXPECT_CALL(*provider(), OnContentAdded(_));
    EXPECT_EQ(AddEntry(CreateDescription("id-2")),
              blink::mojom::ContentIndexError::NONE);
    auto entries = GetAllEntries();
    EXPECT_EQ(entries.size(), 2u);
  }
}

TEST_F(ContentIndexDatabaseTest, BlockedOriginsCannotRegisterContent) {
  // Initially adding is fine.
  EXPECT_EQ(AddEntry(CreateDescription("id1")),
            blink::mojom::ContentIndexError::NONE);

  // Two delete events were dispatched.
  database()->BlockOrigin(origin());
  database()->BlockOrigin(origin());

  // Content can't be registered while the origin is blocked.
  EXPECT_EQ(AddEntry(CreateDescription("id2")),
            blink::mojom::ContentIndexError::STORAGE_ERROR);

  // First event dispatch completed.
  database()->UnblockOrigin(origin());

  // Content still can't be registered.
  EXPECT_EQ(AddEntry(CreateDescription("id3")),
            blink::mojom::ContentIndexError::STORAGE_ERROR);

  // Last event dispatch completed.
  database()->UnblockOrigin(origin());

  // Registering is OK now.
  EXPECT_EQ(AddEntry(CreateDescription("id4")),
            blink::mojom::ContentIndexError::NONE);
}

}  // namespace content
