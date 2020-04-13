// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "content/browser/service_worker/service_worker_storage_control_impl.h"

#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/service_worker/navigation_preload_state.mojom.h"

namespace content {

using DatabaseStatus = storage::mojom::ServiceWorkerDatabaseStatus;
using FindRegistrationResult =
    storage::mojom::ServiceWorkerFindRegistrationResultPtr;

namespace {

int ReadResponseHead(storage::mojom::ServiceWorkerResourceReader* reader,
                     network::mojom::URLResponseHeadPtr& out_response_head,
                     base::Optional<mojo_base::BigBuffer>& out_metadata) {
  int return_value;
  base::RunLoop loop;
  reader->ReadResponseHead(base::BindLambdaForTesting(
      [&](int result, network::mojom::URLResponseHeadPtr response_head,
          base::Optional<mojo_base::BigBuffer> metadata) {
        return_value = result;
        out_response_head = std::move(response_head);
        out_metadata = std::move(metadata);
        loop.Quit();
      }));
  loop.Run();
  return return_value;
}

std::string ReadResponseData(
    storage::mojom::ServiceWorkerResourceReader* reader,
    int data_size) {
  mojo::ScopedDataPipeConsumerHandle data_consumer;
  base::RunLoop loop;
  reader->ReadData(data_size, base::BindLambdaForTesting(
                                  [&](mojo::ScopedDataPipeConsumerHandle pipe) {
                                    data_consumer = std::move(pipe);
                                    loop.Quit();
                                  }));
  loop.Run();

  return ReadDataPipe(std::move(data_consumer));
}

int WriteResponseHead(storage::mojom::ServiceWorkerResourceWriter* writer,
                      network::mojom::URLResponseHeadPtr response_head) {
  int return_value;
  base::RunLoop loop;
  writer->WriteResponseHead(std::move(response_head),
                            base::BindLambdaForTesting([&](int result) {
                              return_value = result;
                              loop.Quit();
                            }));
  loop.Run();
  return return_value;
}

int WriteResponseData(storage::mojom::ServiceWorkerResourceWriter* writer,
                      mojo_base::BigBuffer data) {
  int return_value;
  base::RunLoop loop;
  writer->WriteData(std::move(data),
                    base::BindLambdaForTesting([&](int result) {
                      return_value = result;
                      loop.Quit();
                    }));
  loop.Run();
  return return_value;
}

int WriteResponseMetadata(
    storage::mojom::ServiceWorkerResourceMetadataWriter* writer,
    mojo_base::BigBuffer metadata) {
  int return_value;
  base::RunLoop loop;
  writer->WriteMetadata(std::move(metadata),
                        base::BindLambdaForTesting([&](int result) {
                          return_value = result;
                          loop.Quit();
                        }));
  loop.Run();
  return return_value;
}

}  // namespace

class ServiceWorkerStorageControlImplTest : public testing::Test {
 public:
  ServiceWorkerStorageControlImplTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    ASSERT_TRUE(user_data_directory_.CreateUniqueTempDir());

    auto storage = ServiceWorkerStorage::Create(
        user_data_directory_.GetPath(),
        /*database_task_runner=*/base::ThreadTaskRunnerHandle::Get(),
        /*quota_manager_proxy=*/nullptr);
    storage_impl_ =
        std::make_unique<ServiceWorkerStorageControlImpl>(std::move(storage));
  }

  void TearDown() override {
    storage_impl_.reset();
    disk_cache::FlushCacheThreadForTesting();
    content::RunAllTasksUntilIdle();
  }

  storage::mojom::ServiceWorkerStorageControl* storage() {
    return storage_impl_.get();
  }

  void LazyInitializeForTest() { storage_impl_->LazyInitializeForTest(); }

  FindRegistrationResult FindRegistrationForClientUrl(const GURL& client_url) {
    FindRegistrationResult return_value;
    base::RunLoop loop;
    storage()->FindRegistrationForClientUrl(
        client_url,
        base::BindLambdaForTesting([&](FindRegistrationResult result) {
          return_value = result.Clone();
          loop.Quit();
        }));
    loop.Run();
    return return_value;
  }

  FindRegistrationResult FindRegistrationForScope(const GURL& scope) {
    FindRegistrationResult return_value;
    base::RunLoop loop;
    storage()->FindRegistrationForScope(
        scope, base::BindLambdaForTesting([&](FindRegistrationResult result) {
          return_value = result.Clone();
          loop.Quit();
        }));
    loop.Run();
    return return_value;
  }

  FindRegistrationResult FindRegistrationForId(int64_t registration_id,
                                               const GURL& origin) {
    FindRegistrationResult return_value;
    base::RunLoop loop;
    storage()->FindRegistrationForId(
        registration_id, origin,
        base::BindLambdaForTesting([&](FindRegistrationResult result) {
          return_value = result.Clone();
          loop.Quit();
        }));
    loop.Run();
    return return_value;
  }

  void GetRegistrationsForOrigin(
      const GURL& origin,
      DatabaseStatus& out_status,
      std::vector<storage::mojom::SerializedServiceWorkerRegistrationPtr>&
          out_registrations) {
    base::RunLoop loop;
    storage()->GetRegistrationsForOrigin(
        origin,
        base::BindLambdaForTesting(
            [&](DatabaseStatus status,
                std::vector<
                    storage::mojom::SerializedServiceWorkerRegistrationPtr>
                    registrations) {
              out_status = status;
              out_registrations = std::move(registrations);
              loop.Quit();
            }));
    loop.Run();
  }

  void StoreRegistration(
      storage::mojom::ServiceWorkerRegistrationDataPtr registration,
      std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources,
      DatabaseStatus& out_status) {
    base::RunLoop loop;
    storage()->StoreRegistration(
        std::move(registration), std::move(resources),
        base::BindLambdaForTesting([&](DatabaseStatus status) {
          out_status = status;
          loop.Quit();
        }));
    loop.Run();
  }

  void DeleteRegistration(
      int64_t registration_id,
      const GURL& origin,
      DatabaseStatus& out_status,
      storage::mojom::ServiceWorkerStorageOriginState& out_origin_state) {
    base::RunLoop loop;
    storage()->DeleteRegistration(
        registration_id, origin,
        base::BindLambdaForTesting(
            [&](DatabaseStatus status,
                storage::mojom::ServiceWorkerStorageOriginState origin_state) {
              out_status = status;
              out_origin_state = origin_state;
              loop.Quit();
            }));
    loop.Run();
  }

  int64_t GetNewRegistrationId() {
    int64_t return_value;
    base::RunLoop loop;
    storage()->GetNewRegistrationId(
        base::BindLambdaForTesting([&](int64_t registration_id) {
          return_value = registration_id;
          loop.Quit();
        }));
    loop.Run();
    return return_value;
  }

  int64_t GetNewResourceId() {
    int64_t return_value;
    base::RunLoop loop;
    storage()->GetNewResourceId(
        base::BindLambdaForTesting([&](int64_t resource_id) {
          return_value = resource_id;
          loop.Quit();
        }));
    loop.Run();
    return return_value;
  }

  void GetUserData(int64_t registration_id,
                   const std::vector<std::string>& keys,
                   DatabaseStatus& out_status,
                   std::vector<std::string>& out_values) {
    base::RunLoop loop;
    storage()->GetUserData(
        registration_id, keys,
        base::BindLambdaForTesting(
            [&](DatabaseStatus status, const std::vector<std::string>& values) {
              out_status = status;
              out_values = values;
              loop.Quit();
            }));
    loop.Run();
  }

  DatabaseStatus StoreUserData(
      int64_t registration_id,
      const GURL& origin,
      std::vector<storage::mojom::ServiceWorkerUserDataPtr> user_data) {
    DatabaseStatus return_value;
    base::RunLoop loop;
    storage()->StoreUserData(
        registration_id, origin, std::move(user_data),
        base::BindLambdaForTesting([&](DatabaseStatus status) {
          return_value = status;
          loop.Quit();
        }));
    loop.Run();
    return return_value;
  }

  DatabaseStatus ClearUserData(int64_t registration_id,
                               const std::vector<std::string>& keys) {
    DatabaseStatus return_value;
    base::RunLoop loop;
    storage()->ClearUserData(
        registration_id, keys,
        base::BindLambdaForTesting([&](DatabaseStatus status) {
          return_value = status;
          loop.Quit();
        }));
    loop.Run();
    return return_value;
  }

  // Create a registration with a single resource and stores the registration.
  DatabaseStatus CreateAndStoreRegistration(int64_t registration_id,
                                            const GURL& scope,
                                            const GURL& script_url,
                                            int64_t script_size) {
    std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources;
    resources.push_back(storage::mojom::ServiceWorkerResourceRecord::New(
        registration_id, script_url, script_size));

    auto data = storage::mojom::ServiceWorkerRegistrationData::New();
    data->registration_id = registration_id;
    data->scope = scope;
    data->script = script_url;
    data->navigation_preload_state =
        blink::mojom::NavigationPreloadState::New();

    int64_t resources_total_size_bytes = 0;
    for (auto& resource : resources) {
      resources_total_size_bytes += resource->size_bytes;
    }
    data->resources_total_size_bytes = resources_total_size_bytes;

    DatabaseStatus status;
    StoreRegistration(std::move(data), std::move(resources), status);
    return status;
  }

  mojo::Remote<storage::mojom::ServiceWorkerResourceReader>
  CreateResourceReader(int64_t resource_id) {
    mojo::Remote<storage::mojom::ServiceWorkerResourceReader> reader;
    storage()->CreateResourceReader(resource_id,
                                    reader.BindNewPipeAndPassReceiver());
    return reader;
  }

  mojo::Remote<storage::mojom::ServiceWorkerResourceWriter>
  CreateResourceWriter(int64_t resource_id) {
    mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> writer;
    storage()->CreateResourceWriter(resource_id,
                                    writer.BindNewPipeAndPassReceiver());
    return writer;
  }

  mojo::Remote<storage::mojom::ServiceWorkerResourceMetadataWriter>
  CreateResourceMetadataWriter(int64_t resource_id) {
    mojo::Remote<storage::mojom::ServiceWorkerResourceMetadataWriter> writer;
    storage()->CreateResourceMetadataWriter(
        resource_id, writer.BindNewPipeAndPassReceiver());
    return writer;
  }

 private:
  base::ScopedTempDir user_data_directory_;
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ServiceWorkerStorageControlImpl> storage_impl_;
};

// Tests that FindRegistration methods don't find anything without having stored
// anything.
TEST_F(ServiceWorkerStorageControlImplTest, FindRegistration_NoRegistration) {
  const GURL kScope("https://www.example.com/scope/");
  const GURL kClientUrl("https://www.example.com/scope/document.html");
  const int64_t kRegistrationId = 0;

  LazyInitializeForTest();

  {
    FindRegistrationResult result = FindRegistrationForClientUrl(kClientUrl);
    EXPECT_EQ(result->status, DatabaseStatus::kErrorNotFound);
  }

  {
    FindRegistrationResult result = FindRegistrationForScope(kScope);
    EXPECT_EQ(result->status, DatabaseStatus::kErrorNotFound);
  }

  {
    FindRegistrationResult result =
        FindRegistrationForId(kRegistrationId, kScope.GetOrigin());
    EXPECT_EQ(result->status, DatabaseStatus::kErrorNotFound);
  }
}

// Tests that storing/finding/deleting a registration work.
TEST_F(ServiceWorkerStorageControlImplTest, StoreAndDeleteRegistration) {
  const GURL kScope("https://www.example.com/scope/");
  const GURL kScriptUrl("https://www.example.com/scope/sw.js");
  const GURL kClientUrl("https://www.example.com/scope/document.html");
  const int64_t kRegistrationId = 0;
  const int64_t kScriptSize = 10;

  LazyInitializeForTest();

  // Create a registration data with a single resource.
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources;
  resources.push_back(storage::mojom::ServiceWorkerResourceRecord::New(
      kRegistrationId, kScriptUrl, kScriptSize));

  auto data = storage::mojom::ServiceWorkerRegistrationData::New();
  data->registration_id = kRegistrationId;
  data->scope = kScope;
  data->script = kScriptUrl;
  data->navigation_preload_state = blink::mojom::NavigationPreloadState::New();

  int64_t resources_total_size_bytes = 0;
  for (auto& resource : resources) {
    resources_total_size_bytes += resource->size_bytes;
  }
  data->resources_total_size_bytes = resources_total_size_bytes;

  // Store the registration data.
  {
    DatabaseStatus status;
    StoreRegistration(std::move(data), std::move(resources), status);
    ASSERT_EQ(status, DatabaseStatus::kOk);
  }

  // Find the registration. Find operations should succeed.
  {
    FindRegistrationResult result = FindRegistrationForClientUrl(kClientUrl);
    ASSERT_EQ(result->status, DatabaseStatus::kOk);
    EXPECT_EQ(result->registration->registration_id, kRegistrationId);
    EXPECT_EQ(result->registration->scope, kScope);
    EXPECT_EQ(result->registration->script, kScriptUrl);
    EXPECT_EQ(result->registration->resources_total_size_bytes,
              resources_total_size_bytes);
    EXPECT_EQ(result->resources.size(), 1UL);

    result = FindRegistrationForScope(kScope);
    EXPECT_EQ(result->status, DatabaseStatus::kOk);
    result = FindRegistrationForId(kRegistrationId, kScope.GetOrigin());
    EXPECT_EQ(result->status, DatabaseStatus::kOk);
  }

  // Delete the registration.
  {
    DatabaseStatus status;
    storage::mojom::ServiceWorkerStorageOriginState origin_state;
    DeleteRegistration(kRegistrationId, kScope.GetOrigin(), status,
                       origin_state);
    ASSERT_EQ(status, DatabaseStatus::kOk);
    EXPECT_EQ(origin_state,
              storage::mojom::ServiceWorkerStorageOriginState::kDelete);
  }

  // Try to find the deleted registration. These operation should result in
  // kErrorNotFound.
  {
    FindRegistrationResult result = FindRegistrationForClientUrl(kClientUrl);
    EXPECT_EQ(result->status, DatabaseStatus::kErrorNotFound);
    result = FindRegistrationForScope(kScope);
    EXPECT_EQ(result->status, DatabaseStatus::kErrorNotFound);
    result = FindRegistrationForId(kRegistrationId, kScope.GetOrigin());
    EXPECT_EQ(result->status, DatabaseStatus::kErrorNotFound);
  }
}

// Tests that getting registrations works.
TEST_F(ServiceWorkerStorageControlImplTest, GetRegistrationsForOrigin) {
  const GURL kScope1("https://www.example.com/foo/");
  const GURL kScriptUrl1("https://www.example.com/foo/sw.js");
  const GURL kScope2("https://www.example.com/bar/");
  const GURL kScriptUrl2("https://www.example.com/bar/sw.js");
  const int64_t kScriptSize = 10;

  LazyInitializeForTest();

  // Store two registrations which have the same origin.
  DatabaseStatus status;
  const int64_t registration_id1 = GetNewRegistrationId();
  status = CreateAndStoreRegistration(registration_id1, kScope1, kScriptUrl1,
                                      kScriptSize);
  ASSERT_EQ(status, DatabaseStatus::kOk);
  const int64_t registration_id2 = GetNewRegistrationId();
  status = CreateAndStoreRegistration(registration_id2, kScope2, kScriptUrl2,
                                      kScriptSize);
  ASSERT_EQ(status, DatabaseStatus::kOk);

  // Get registrations for the origin.
  {
    const GURL& origin = kScope1.GetOrigin();
    std::vector<storage::mojom::SerializedServiceWorkerRegistrationPtr>
        registrations;

    GetRegistrationsForOrigin(origin, status, registrations);
    ASSERT_EQ(status, DatabaseStatus::kOk);
    EXPECT_EQ(registrations.size(), 2UL);

    for (auto& registration : registrations) {
      EXPECT_EQ(registration->registration_data->scope.GetOrigin(), origin);
      EXPECT_EQ(registration->registration_data->resources_total_size_bytes,
                kScriptSize);
    }
  }

  // Getting registrations for another origin should succeed but shouldn't find
  // anything.
  {
    const GURL& origin = GURL("https://www.example.test/");
    std::vector<storage::mojom::SerializedServiceWorkerRegistrationPtr>
        registrations;

    GetRegistrationsForOrigin(origin, status, registrations);
    ASSERT_EQ(status, DatabaseStatus::kOk);
    EXPECT_EQ(registrations.size(), 0UL);
  }
}

// Tests that writing/reading a service worker script succeed.
TEST_F(ServiceWorkerStorageControlImplTest, WriteAndReadResource) {
  LazyInitializeForTest();

  // Create a SSLInfo to write/read.
  net::SSLInfo ssl_info = net::SSLInfo();
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(ssl_info.is_valid());

  int64_t resource_id = GetNewResourceId();

  mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> writer =
      CreateResourceWriter(resource_id);

  // Write a response head.
  {
    auto response_head = network::mojom::URLResponseHead::New();
    response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(
            "HTTP/1.1 200 OK\n"
            "Content-Type: application/javascript\n"));
    response_head->headers->GetMimeType(&response_head->mime_type);
    response_head->ssl_info = ssl_info;

    int result = WriteResponseHead(writer.get(), std::move(response_head));
    ASSERT_GT(result, 0);
  }

  const std::string kData("/* script body */");
  int data_size = kData.size();

  // Write content.
  {
    mojo_base::BigBuffer data(base::as_bytes(base::make_span(kData)));
    int data_size = data.size();

    int result = WriteResponseData(writer.get(), std::move(data));
    ASSERT_EQ(data_size, result);
  }

  mojo::Remote<storage::mojom::ServiceWorkerResourceReader> reader =
      CreateResourceReader(resource_id);

  // Read the response head, metadata and the content.
  {
    network::mojom::URLResponseHeadPtr response_head;
    base::Optional<mojo_base::BigBuffer> response_metadata;
    int result =
        ReadResponseHead(reader.get(), response_head, response_metadata);
    ASSERT_GT(result, 0);

    EXPECT_EQ(response_head->mime_type, "application/javascript");
    EXPECT_EQ(response_head->content_length, data_size);
    EXPECT_TRUE(response_head->ssl_info->is_valid());
    EXPECT_EQ(response_head->ssl_info->cert->serial_number(),
              ssl_info.cert->serial_number());
    EXPECT_EQ(response_metadata, base::nullopt);

    std::string data = ReadResponseData(reader.get(), data_size);
    EXPECT_EQ(data, kData);
  }

  const auto kMetadata = base::as_bytes(base::make_span("metadata"));
  int metadata_size = kMetadata.size();

  // Write metadata.
  {
    mojo::Remote<storage::mojom::ServiceWorkerResourceMetadataWriter>
        metadata_writer = CreateResourceMetadataWriter(resource_id);
    int result = WriteResponseMetadata(metadata_writer.get(),
                                       mojo_base::BigBuffer(kMetadata));
    ASSERT_EQ(result, metadata_size);
  }

  // Read the response head again. This time metadata should be read.
  {
    network::mojom::URLResponseHeadPtr response_head;
    base::Optional<mojo_base::BigBuffer> response_metadata;
    int result =
        ReadResponseHead(reader.get(), response_head, response_metadata);
    ASSERT_GT(result, 0);
    ASSERT_TRUE(response_metadata.has_value());
    EXPECT_EQ(response_metadata->size(), kMetadata.size());
    EXPECT_EQ(
        memcmp(response_metadata->data(), kMetadata.data(), kMetadata.size()),
        0);
  }
}

// Tests that storing/getting user data works.
TEST_F(ServiceWorkerStorageControlImplTest, StoreAndGetUserData) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  const int64_t kScriptSize = 10;

  LazyInitializeForTest();

  const int64_t registration_id = GetNewRegistrationId();
  DatabaseStatus status;
  status = CreateAndStoreRegistration(registration_id, kScope, kScriptUrl,
                                      kScriptSize);
  ASSERT_EQ(status, DatabaseStatus::kOk);

  // Store user data with two entries.
  {
    std::vector<storage::mojom::ServiceWorkerUserDataPtr> user_data;
    user_data.push_back(
        storage::mojom::ServiceWorkerUserData::New("key1", "value1"));
    user_data.push_back(
        storage::mojom::ServiceWorkerUserData::New("key2", "value2"));

    status = StoreUserData(registration_id, kScope.GetOrigin(),
                           std::move(user_data));
    ASSERT_EQ(status, DatabaseStatus::kOk);
  }

  // Get user data.
  {
    std::vector<std::string> keys = {"key1", "key2"};
    std::vector<std::string> values;
    GetUserData(registration_id, keys, status, values);
    ASSERT_EQ(status, DatabaseStatus::kOk);
    EXPECT_EQ(values.size(), 2UL);
    EXPECT_EQ("value1", values[0]);
    EXPECT_EQ("value2", values[1]);
  }

  // Try to get user data with an unknown key should fail.
  {
    std::vector<std::string> keys = {"key1", "key2", "key3"};
    std::vector<std::string> values;
    GetUserData(registration_id, keys, status, values);
    ASSERT_EQ(status, DatabaseStatus::kErrorNotFound);
    EXPECT_EQ(values.size(), 0UL);
  }

  // Clear the first entry.
  {
    std::vector<std::string> keys = {"key1"};
    status = ClearUserData(registration_id, keys);
    ASSERT_EQ(status, DatabaseStatus::kOk);

    std::vector<std::string> values;
    GetUserData(registration_id, keys, status, values);
    ASSERT_EQ(status, DatabaseStatus::kErrorNotFound);
    EXPECT_EQ(values.size(), 0UL);
  }

  // Getting the second entry should succeed.
  {
    std::vector<std::string> keys = {"key2"};
    std::vector<std::string> values;
    GetUserData(registration_id, keys, status, values);
    ASSERT_EQ(status, DatabaseStatus::kOk);
    EXPECT_EQ(values.size(), 1UL);
    EXPECT_EQ("value2", values[0]);
  }

  // Delete the registration and store a new registration for the same
  // scope.
  const int64_t new_registration_id = GetNewRegistrationId();
  {
    storage::mojom::ServiceWorkerStorageOriginState origin_state;
    DeleteRegistration(registration_id, kScope.GetOrigin(), status,
                       origin_state);
    ASSERT_EQ(status, DatabaseStatus::kOk);

    status = CreateAndStoreRegistration(new_registration_id, kScope, kScriptUrl,
                                        kScriptSize);
    ASSERT_EQ(status, DatabaseStatus::kOk);
  }

  // Try to get user data stored for the previous registration should fail.
  {
    std::vector<std::string> keys = {"key2"};
    std::vector<std::string> values;
    GetUserData(new_registration_id, keys, status, values);
    ASSERT_EQ(status, DatabaseStatus::kErrorNotFound);
    EXPECT_EQ(values.size(), 0UL);
  }
}

}  // namespace content
