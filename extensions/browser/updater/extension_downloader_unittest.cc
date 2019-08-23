// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/extension_downloader.h"

#include "base/sequenced_task_runner.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/updater/extension_downloader_test_helper.h"
#include "extensions/common/extension.h"

using testing::_;
using testing::AnyNumber;
using testing::DoAll;
using testing::Return;
using testing::Sequence;
using testing::SetArgPointee;

namespace extensions {

namespace {

const char kTestExtensionId[] = "test_app";

}  // namespace

class ExtensionDownloaderTest : public ExtensionsTest {
 protected:
  ExtensionDownloaderTest() {}

  std::unique_ptr<ManifestFetchData> CreateManifestFetchData(
      const GURL& update_url,
      ManifestFetchData::FetchPriority fetch_priority =
          ManifestFetchData::FetchPriority::BACKGROUND) {
    return std::make_unique<ManifestFetchData>(
        update_url, 0, "", "", ManifestFetchData::PING, fetch_priority);
  }

  std::unique_ptr<ManifestFetchData> CreateTestAppFetchData() {
    GURL kUpdateUrl("http://localhost/manifest1");
    std::unique_ptr<ManifestFetchData> fetch(
        CreateManifestFetchData(kUpdateUrl));
    ManifestFetchData::PingData zero_days(0, 0, true, 0);
    fetch->AddExtension(kTestExtensionId, "1.0", &zero_days, "", "", "",
                        ManifestFetchData::FetchPriority::BACKGROUND);
    return fetch;
  }

  void AddFetchDataToDownloader(ExtensionDownloaderTestHelper* helper,
                                std::unique_ptr<ManifestFetchData> fetch) {
    helper->downloader().StartUpdateCheck(std::move(fetch));
  }
};

// Several tests checking that OnExtensionDownloadStageChanged callback is
// called correctly.
TEST_F(ExtensionDownloaderTest, TestStageChanges) {
  ExtensionDownloaderTestHelper helper;

  std::unique_ptr<ManifestFetchData> fetch(CreateTestAppFetchData());
  GURL fetch_url = fetch->full_url();

  const std::string kManifest =
      "<?xml version='1.0' encoding='UTF-8'?>"
      "<gupdate xmlns='http://www.google.com/update2/response'"
      "                protocol='2.0'>"
      " <app appid='" +
      std::string(kTestExtensionId) +
      "'>"
      "  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
      "               version='1.1' prodversionmin='1.1' />"
      " </app>"
      "</gupdate>";
  helper.test_url_loader_factory().AddResponse(fetch_url.spec(), kManifest,
                                               net::HTTP_OK);
  GURL kCrxUrl("http://example.com/extension_1.2.3.4.crx");
  helper.test_url_loader_factory().AddResponse(kCrxUrl.spec(), "no data",
                                               net::HTTP_OK);

  MockExtensionDownloaderDelegate& delegate = helper.delegate();
  EXPECT_CALL(delegate, IsExtensionPending(kTestExtensionId))
      .WillOnce(Return(true));
  Sequence sequence;
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            kTestExtensionId,
                            ExtensionDownloaderDelegate::QUEUED_FOR_MANIFEST))
      .Times(AnyNumber());
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            kTestExtensionId,
                            ExtensionDownloaderDelegate::QUEUED_FOR_CRX))
      .Times(AnyNumber());
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            kTestExtensionId,
                            ExtensionDownloaderDelegate::DOWNLOADING_MANIFEST))
      .InSequence(sequence);
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            kTestExtensionId,
                            ExtensionDownloaderDelegate::PARSING_MANIFEST))
      .InSequence(sequence);
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            kTestExtensionId,
                            ExtensionDownloaderDelegate::MANIFEST_LOADED))
      .InSequence(sequence);
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            kTestExtensionId,
                            ExtensionDownloaderDelegate::DOWNLOADING_CRX))
      .InSequence(sequence);
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  kTestExtensionId, ExtensionDownloaderDelegate::FINISHED))
      .InSequence(sequence);

  AddFetchDataToDownloader(&helper, std::move(fetch));

  content::RunAllTasksUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&delegate);
}

TEST_F(ExtensionDownloaderTest, TestStageChangesNoUpdates) {
  ExtensionDownloaderTestHelper helper;

  std::unique_ptr<ManifestFetchData> fetch(CreateTestAppFetchData());
  GURL fetch_url = fetch->full_url();

  const std::string kManifest =
      "<?xml version='1.0' encoding='UTF-8'?>"
      "<gupdate xmlns='http://www.google.com/update2/response'"
      "                protocol='2.0'>"
      " <app appid='" +
      std::string(kTestExtensionId) +
      "'>"
      "  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
      "               version='1.1' prodversionmin='1.1' />"
      " </app>"
      "</gupdate>";
  helper.test_url_loader_factory().AddResponse(fetch_url.spec(), kManifest,
                                               net::HTTP_OK);
  GURL kCrxUrl("http://example.com/extension_1.2.3.4.crx");
  helper.test_url_loader_factory().AddResponse(kCrxUrl.spec(), "no data",
                                               net::HTTP_OK);

  MockExtensionDownloaderDelegate& delegate = helper.delegate();
  EXPECT_CALL(delegate, IsExtensionPending(kTestExtensionId))
      .WillOnce(Return(false));
  EXPECT_CALL(delegate, GetExtensionExistingVersion(kTestExtensionId, _))
      .WillOnce(DoAll(SetArgPointee<1>("1.1"), Return(true)));
  Sequence sequence;
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            kTestExtensionId,
                            ExtensionDownloaderDelegate::QUEUED_FOR_MANIFEST))
      .Times(AnyNumber());
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            kTestExtensionId,
                            ExtensionDownloaderDelegate::DOWNLOADING_MANIFEST))
      .InSequence(sequence);
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            kTestExtensionId,
                            ExtensionDownloaderDelegate::PARSING_MANIFEST))
      .InSequence(sequence);
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            kTestExtensionId,
                            ExtensionDownloaderDelegate::MANIFEST_LOADED))
      .InSequence(sequence);
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            kTestExtensionId,
                            ExtensionDownloaderDelegate::DOWNLOADING_CRX))
      .Times(0);
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  kTestExtensionId, ExtensionDownloaderDelegate::FINISHED))
      .InSequence(sequence);

  AddFetchDataToDownloader(&helper, std::move(fetch));

  content::RunAllTasksUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&delegate);
}

TEST_F(ExtensionDownloaderTest, TestStageChangesBadManifest) {
  ExtensionDownloaderTestHelper helper;

  std::unique_ptr<ManifestFetchData> fetch(CreateTestAppFetchData());
  GURL fetch_url = fetch->full_url();

  const std::string kManifest = "invalid xml";
  helper.test_url_loader_factory().AddResponse(fetch_url.spec(), kManifest,
                                               net::HTTP_OK);

  MockExtensionDownloaderDelegate& delegate = helper.delegate();
  Sequence sequence;
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            kTestExtensionId,
                            ExtensionDownloaderDelegate::QUEUED_FOR_MANIFEST))
      .Times(AnyNumber());
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            kTestExtensionId,
                            ExtensionDownloaderDelegate::DOWNLOADING_MANIFEST))
      .InSequence(sequence);
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            kTestExtensionId,
                            ExtensionDownloaderDelegate::PARSING_MANIFEST))
      .InSequence(sequence);
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            kTestExtensionId,
                            ExtensionDownloaderDelegate::MANIFEST_LOADED))
      .Times(0);
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  kTestExtensionId, ExtensionDownloaderDelegate::FINISHED))
      .InSequence(sequence);

  AddFetchDataToDownloader(&helper, std::move(fetch));

  content::RunAllTasksUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&delegate);
}

TEST_F(ExtensionDownloaderTest, TestStageChangesBadQuery) {
  ExtensionDownloaderTestHelper helper;

  std::unique_ptr<ManifestFetchData> fetch(CreateTestAppFetchData());
  GURL fetch_url = fetch->full_url();

  helper.test_url_loader_factory().AddResponse(fetch_url.spec(), "",
                                               net::HTTP_BAD_REQUEST);

  MockExtensionDownloaderDelegate& delegate = helper.delegate();
  Sequence sequence;
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            kTestExtensionId,
                            ExtensionDownloaderDelegate::QUEUED_FOR_MANIFEST))
      .Times(AnyNumber());
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            kTestExtensionId,
                            ExtensionDownloaderDelegate::DOWNLOADING_MANIFEST))
      .InSequence(sequence);
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            kTestExtensionId,
                            ExtensionDownloaderDelegate::PARSING_MANIFEST))
      .Times(0);
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  kTestExtensionId, ExtensionDownloaderDelegate::FINISHED))
      .InSequence(sequence);

  AddFetchDataToDownloader(&helper, std::move(fetch));

  content::RunAllTasksUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&delegate);
}

// Test that failure callback was actually called in case of empty answer from
// the update server. Regression for problem described/fixed in
// crbug.com/938265.
TEST_F(ExtensionDownloaderTest, TestNoUpdatesManifestReports) {
  ExtensionDownloaderTestHelper helper;

  std::unique_ptr<ManifestFetchData> fetch(CreateTestAppFetchData());
  GURL fetch_url = fetch->full_url();

  const std::string kManifest =
      "<?xml version='1.0' encoding='UTF-8'?>"
      "<gupdate xmlns='http://www.google.com/update2/response'"
      "                protocol='2.0'>"
      " <app appid='" +
      std::string(kTestExtensionId) +
      "'>"
      "  <updatecheck info='bandwidth limit' status='noupdate' />"
      " </app>"
      "</gupdate>";
  helper.test_url_loader_factory().AddResponse(fetch_url.spec(), kManifest,
                                               net::HTTP_OK);

  MockExtensionDownloaderDelegate& delegate = helper.delegate();
  EXPECT_CALL(delegate, IsExtensionPending(kTestExtensionId))
      .WillOnce(Return(true));
  // TODO(burunduk) Also check error (second argument). By now we return
  // CRX_FETCH_FAILED, but probably we may want to make another one.
  EXPECT_CALL(delegate, OnExtensionDownloadFailed(kTestExtensionId, _, _, _));

  AddFetchDataToDownloader(&helper, std::move(fetch));

  content::RunAllTasksUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&delegate);
}

}  // namespace extensions
