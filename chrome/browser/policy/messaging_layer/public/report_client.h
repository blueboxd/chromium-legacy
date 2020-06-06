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
#include "chrome/browser/policy/messaging_layer/util/statusor.h"

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
  ReportingClient();

  static StatusOr<ReportingClient*> GetInstance();

  // ReportingClient is not meant to be used directly.
  static StatusOr<std::unique_ptr<ReportingClient>> Create();

  scoped_refptr<StorageModule> storage_;
  scoped_refptr<EncryptionModule> encryption_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_H_
