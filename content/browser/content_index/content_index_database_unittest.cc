// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/content_index/content_index_database.h"

#include "base/run_loop.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

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

void GetEntriesCallback(
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
        embedded_worker_test_helper_.context_wrapper());
  }

  blink::mojom::ContentDescriptionPtr CreateDescription(const std::string& id) {
    return blink::mojom::ContentDescription::New(
        id, "title", "description", blink::mojom::ContentCategory::HOME_PAGE,
        GURL("https://example.com"), GURL("https://example.com"));
  }

  blink::mojom::ContentIndexError AddEntry(
      blink::mojom::ContentDescriptionPtr description) {
    base::RunLoop run_loop;
    blink::mojom::ContentIndexError error;
    database_->AddEntry(
        service_worker_registration_id_, origin_, std::move(description),
        SkBitmap(),
        base::BindOnce(&DatabaseErrorCallback, run_loop.QuitClosure(), &error));
    run_loop.Run();

    return error;
  }

  blink::mojom::ContentIndexError DeleteEntry(const std::string& id) {
    base::RunLoop run_loop;
    blink::mojom::ContentIndexError error;
    database_->DeleteEntry(
        service_worker_registration_id_, id,
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
        base::BindOnce(&GetEntriesCallback, run_loop.QuitClosure(), out_error,
                       &descriptions));
    run_loop.Run();
    return descriptions;
  }

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

}  // namespace content
