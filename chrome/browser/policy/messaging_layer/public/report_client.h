// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_H_

#include <memory>
#include <utility>

#include "chrome/browser/policy/messaging_layer/encryption/encryption_module.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue_configuration.h"
#include "chrome/browser/policy/messaging_layer/storage/storage_module.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "chrome/browser/policy/messaging_layer/util/task_runner_context.h"

namespace reporting {

// ReportingClient acts a single point for creating |reporting::ReportQueue|s.
// It ensures that all ReportQueues are created with the same storage and
// encryption settings.
//
// Example Usage:
// Status SendMessage(google::protobuf::ImportantMessage important_message,
//                    base::OnceCallback<void(Status)> callback) {
//   ASSIGN_OR_RETURN(std::unique_ptr<ReportQueueConfiguration> config,
//                  ReportQueueConfiguration::Create(...));
//   ASSIGN_OR_RETURN(std::unique_ptr<ReportQueue> report_queue,
//                  ReportingClient::CreateReportQueue(config));
//   return report_queue->Enqueue(important_message, callback);
// }
class ReportingClient {
 public:
  // Uploader is passed to Storage in order to upload messages using the
  // UploadClient.
  class Uploader : public Storage::UploaderInterface {
   public:
    using UploadCallback = base::OnceCallback<Status(
        std::unique_ptr<std::vector<EncryptedRecord>>)>;

    static StatusOr<std::unique_ptr<Uploader>> Create(
        UploadCallback upload_callback);

    ~Uploader() override;
    Uploader(const Uploader& other) = delete;
    Uploader& operator=(const Uploader& other) = delete;

    // TODO(chromium:1078512) Priority is unused, remove it.
    void ProcessBlob(Priority priority,
                     StatusOr<base::span<const uint8_t>> data,
                     base::OnceCallback<void(bool)> processed_cb) override;

    // TODO(chromium:1078512) Priority is unused, remove it.
    void Completed(Priority priority, Status final_status) override;

   private:
    explicit Uploader(UploadCallback upload_callback_);

    void RunUpload();

    UploadCallback upload_callback_;

    bool completed_;
    std::unique_ptr<std::vector<EncryptedRecord>> encrypted_records_;
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  };

  ~ReportingClient();
  ReportingClient(const ReportingClient& other) = delete;
  ReportingClient& operator=(const ReportingClient& other) = delete;

  // Allows a user to synchronously create a |ReportQueue|. Will create an
  // underlying ReportingClient if it doesn't exists. This call can fail if
  // |storage_| or |encryption_| cannot be instantiated for any reason.
  //
  // TODO(chromium:1078512): Once the StorageModule is ready, update this
  // comment with concrete failure conditions.
  // TODO(chromium:1078512): Once the EncryptionModule is ready, update this
  // comment with concrete failure conditions.
  static StatusOr<std::unique_ptr<ReportQueue>> CreateReportQueue(
      std::unique_ptr<ReportQueueConfiguration> config);

 private:
  explicit ReportingClient(scoped_refptr<StorageModule> storage);

  static StatusOr<ReportingClient*> GetInstance();

  // ReportingClient is not meant to be used directly.
  static StatusOr<std::unique_ptr<ReportingClient>> Create();

  // TODO(chromium:1078512) Priority is unused, remove it.
  static StatusOr<std::unique_ptr<Storage::UploaderInterface>> BuildUploader(
      Priority priority);

  scoped_refptr<StorageModule> storage_;
  scoped_refptr<EncryptionModule> encryption_;
  std::unique_ptr<UploadClient> upload_client_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_H_
