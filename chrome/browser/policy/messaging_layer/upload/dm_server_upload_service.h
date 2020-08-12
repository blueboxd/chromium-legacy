// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_DM_SERVER_UPLOAD_SERVICE_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_DM_SERVER_UPLOAD_SERVICE_H_

#include <memory>
#include <vector>

#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/status_macros.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "chrome/browser/policy/messaging_layer/util/task_runner_context.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/proto/record.pb.h"
#include "components/policy/proto/record_constants.pb.h"
#include "net/base/backoff_entry.h"

namespace reporting {

// DmServerUploadService uploads events to the DMServer. It does not manage
// sequence information, instead reporting the highest sequence number for each
// generation id and priority.
//
// DmServerUploadService relies on DmServerUploader for uploading. A
// DmServerUploader is provided with RecordHandlers for each Destination. An
// |EnqueueUpload| call creates a DmServerUploader and provides it with the
// records for upload, and the RecordHandlers.  DmServerUploader uses the
// RecordHandlers to upload each record.
class DmServerUploadService {
 public:
  // ReportSuccessfulUploadCallback is used to pass server responses back to
  // the owner of |this|.
  using ReportSuccessfulUploadCallback =
      base::RepeatingCallback<void(SequencingInformation)>;

  using CompletionResponse = StatusOr<std::vector<SequencingInformation>>;

  using CompletionCallback = base::OnceCallback<void(CompletionResponse)>;

  // Since DmServer records need to be sorted prior to sending, we need handlers
  // for each type of record.
  class RecordHandler {
   public:
    explicit RecordHandler(policy::CloudPolicyClient* client);
    virtual ~RecordHandler();

    virtual Status HandleRecord(Record record) = 0;

   protected:
    policy::CloudPolicyClient* GetClient() { return client_; }

   private:
    policy::CloudPolicyClient* client_;
  };

  class DmServerUploader : public TaskRunnerContext<CompletionResponse> {
   public:
    DmServerUploader(
        std::unique_ptr<std::vector<EncryptedRecord>> records,
        std::vector<std::unique_ptr<RecordHandler>>* handlers,
        CompletionCallback completion_cb,
        scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
        base::TimeDelta max_delay);

   private:
    struct RecordInfo {
      Record record;
      SequencingInformation sequencing_information;
    };

    ~DmServerUploader() override;

    // OnStart calls ProcessRecords to start the upload.
    void OnStart() override;

    // ProcessRecords verifies that the records provided are parseable and sets
    // the |Record|s up for handling by the |RecordHandler|s. On completion,
    // ProcessRecords |Schedule|s |HandleRecords| with an initial delay value of
    // 1 second.
    void ProcessRecords();

    // HandleRecords sends the records to the |record_handlers_|, allowing them
    // to upload to DmServer. |delay| is used to schedule the next call if the
    // server is currently unavailable. |delay| is doubled on each call, and if
    // it grows larger than |max_delay_| HandleRecords will abort trying to
    // upload records and report completion for any records it was able to
    // upload.
    void HandleRecords();

    // Complete evaluates if any records were successfully uploaded.  If no
    // records were successfully uploaded and |status| is not ok - it calls
    // |Response| with the provided |status|. Otherwise it calls |Response| with
    // the list of successful uploads (even if some were not successful).
    void Complete(Status status);

    void AddSuccessfulUpload(SequencingInformation sequencing_information);

    base::TimeDelta GetNextDelay();
    void ResetDelay();

    std::unique_ptr<std::vector<EncryptedRecord>> encrypted_records_;
    std::vector<std::unique_ptr<RecordHandler>>* handlers_;

    std::vector<RecordInfo> record_infos_;
    base::flat_map<uint64_t, SequencingInformation> successful_uploads_;

    base::TimeDelta max_delay_;
    std::unique_ptr<::net::BackoffEntry> backoff_entry_;
  };

  // Will create a DMServerUploadService with handlers.
  // On successful completion returns a DMServerUploadService.
  // If |client| is null, will return error::INVALID_ARGUMENT.
  // If any handlers fail to create, or the policy::CloudPolicyClient is null,
  // will return error::UNAVAILABLE.
  //
  // |client| must not be null.
  // |completion_cb| should report back to the holder of the created object
  // whenever a record set is successfully uploaded.
  static StatusOr<std::unique_ptr<DmServerUploadService>> Create(
      std::unique_ptr<policy::CloudPolicyClient> client,
      ReportSuccessfulUploadCallback completion_cb);
  ~DmServerUploadService();

  Status EnqueueUpload(std::unique_ptr<std::vector<EncryptedRecord>> record);

 private:
  DmServerUploadService(std::unique_ptr<policy::CloudPolicyClient> client,
                        ReportSuccessfulUploadCallback completion_cb);

  Status InitRecordHandlers();

  void UploadCompletion(StatusOr<std::vector<SequencingInformation>>) const;

  policy::CloudPolicyClient* GetClient();

  std::unique_ptr<policy::CloudPolicyClient> client_;
  ReportSuccessfulUploadCallback upload_cb_;
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  std::vector<std::unique_ptr<RecordHandler>> record_handlers_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_DM_SERVER_UPLOAD_SERVICE_H_
