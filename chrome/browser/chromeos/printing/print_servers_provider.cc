// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_servers_provider.h"

#include <memory>
#include <set>
#include <vector>

#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/values.h"
#include "chrome/browser/chromeos/printing/print_server.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

constexpr int kMaxRecords = 16;

struct TaskResults {
  int task_id;
  std::vector<PrintServer> servers;
};

// Parses |data|, a JSON blob, into a vector of PrintServers.  If |data| cannot
// be parsed, returns data with empty list of servers.
// This needs to run on a sequence that may block as it can be very slow.
TaskResults ParseData(int task_id, std::unique_ptr<std::string> data) {
  TaskResults task_data;
  task_data.task_id = task_id;

  if (!data) {
    LOG(WARNING) << "Received null data";
    return task_data;
  }

  // This could be really slow.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::JSONReader::ValueWithError value_with_error =
      base::JSONReader::ReadAndReturnValueWithError(
          *data, base::JSONParserOptions::JSON_PARSE_RFC);
  if (value_with_error.error_code != base::JSONReader::JSON_NO_ERROR) {
    LOG(WARNING) << "Failed to parse print servers policy ("
                 << value_with_error.error_message << ") on line "
                 << value_with_error.error_line << " at position "
                 << value_with_error.error_column;
    return task_data;
  }

  base::Value& json_blob = value_with_error.value.value();
  if (!json_blob.is_list()) {
    LOG(WARNING) << "Failed to parse print servers policy "
                 << "(an array was expected)";
    return task_data;
  }

  base::Value::ListStorage& json_list = json_blob.GetList();
  if (json_list.size() > kMaxRecords) {
    LOG(WARNING) << "Too many records in print servers policy: "
                 << json_list.size() << ". Only the first " << kMaxRecords
                 << " records will be read.";
    json_list.resize(kMaxRecords);
  }

  std::set<GURL> print_server_urls;
  task_data.servers.reserve(json_list.size());
  for (const base::Value& val : json_list) {
    if (!val.is_dict()) {
      LOG(WARNING) << "Entry in print servers policy skipped. "
                   << "Not a dictionary.";
      continue;
    }
    const std::string* url = val.FindStringKey("url");
    const std::string* name = val.FindStringKey("display_name");
    if (url == nullptr || name == nullptr) {
      LOG(WARNING) << "Entry in print servers policy skipped. The following "
                   << "fields are required: url, display_name.";
      continue;
    }
    GURL gurl(*url);
    if (!gurl.is_valid()) {
      LOG(WARNING) << "Entry in print servers policy skipped. "
                   << "The following URL is invalid: " << *url;
      continue;
    }
    if (!gurl.SchemeIsHTTPOrHTTPS() && !gurl.SchemeIs("ipp") &&
        !gurl.SchemeIs("ipps")) {
      LOG(WARNING) << "Entry in print servers policy skipped. "
                   << "URL has unsupported scheme. Only the following "
                   << "schemes are supported: http, https, ipp, ipps";
      continue;
    }
    // Replaces ipp/ipps by http/https. IPP standard describes protocol built
    // on top of HTTP, so both types of addresses have the same meaning in the
    // context of IPP interface. Moreover, the URL must have http/https scheme
    // to pass IsStandard() test from GURL library (see "Validation of the URL
    // address" below).
    if (gurl.SchemeIs("ipp")) {
      gurl = GURL("http" + url->substr(url->find_first_of(':')));
    } else if (gurl.SchemeIs("ipps")) {
      gurl = GURL("https" + url->substr(url->find_first_of(':')));
    }
    // Validation of the URL address.
    if (!gurl.is_valid()) {
      LOG(WARNING) << "Entry in print servers policy skipped. "
                   << "The following URL is invalid: " << *url;
      continue;
    }
    // Checks if a set of URLs contains this URL. If not, the URL is added to
    // the set. Otherwise, a warning is emitted and the record is skipped.
    if (!print_server_urls.insert(gurl).second) {
      // The set already contained this URL. It means that a duplicate record
      // was found.
      LOG(WARNING) << "Entry in print servers policy skipped. There is "
                   << "already a record with the same URL: " << gurl.spec();
      continue;
    }
    task_data.servers.emplace_back(gurl, *name);
  }

  return task_data;
}

class PrintServersProviderImpl : public PrintServersProvider {
 public:
  PrintServersProviderImpl()
      : task_runner_(base::CreateSequencedTaskRunnerWithTraits(
            {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
        weak_ptr_factory_(this) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  ~PrintServersProviderImpl() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  void AddObserver(PrintServersProvider::Observer* observer) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    observers_.AddObserver(observer);
    observer->OnServersChanged(IsCompleted(), servers_);
  }

  void RemoveObserver(PrintServersProvider::Observer* observer) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    observers_.RemoveObserver(observer);
  }

  void ClearData() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    const bool was_complete = IsCompleted();
    const bool was_empty = servers_.empty();
    last_processed_task_ = ++last_received_task_;
    servers_.clear();
    if (!(was_complete && was_empty)) {
      // Notify observers.
      for (auto& observer : observers_)
        observer.OnServersChanged(true, servers_);
    }
  }

  void SetData(std::unique_ptr<std::string> data) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    const bool was_complete = IsCompleted();
    base::PostTaskAndReplyWithResult(
        task_runner_.get(), FROM_HERE,
        base::BindOnce(&ParseData, ++last_received_task_, std::move(data)),
        base::BindOnce(&PrintServersProviderImpl::OnComputationComplete,
                       weak_ptr_factory_.GetWeakPtr()));
    if (was_complete) {
      // Notify observers.
      for (auto& observer : observers_)
        observer.OnServersChanged(false, servers_);
    }
  }

 private:
  // Returns true <=> there is no tasks being processed and there was at least
  // one call to SetData(...) or ClearData(...).
  bool IsCompleted() const {
    // The case when there is no calls to SetData(...) or ClearData(...).
    if (last_received_task_ == 0)
      return false;
    // The case when there is at least one unfinished task.
    if (last_processed_task_ != last_received_task_)
      return false;
    return true;
  }

  // Called on computation completion. |task_data| corresponds to finalized
  // task.
  void OnComputationComplete(TaskResults&& task_data) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (task_data.task_id <= last_processed_task_) {
      // The task is outdated (e.g.: ClearData() was called in the meantime).
      return;
    }
    last_processed_task_ = task_data.task_id;
    const bool is_complete = IsCompleted();
    if (!is_complete && servers_ == task_data.servers) {
      // No changes in the object's state.
      return;
    }
    servers_ = std::move(task_data.servers);
    // Notifies observers about changes.
    for (auto& observer : observers_)
      observer.OnServersChanged(is_complete, servers_);
  }

  // The sequence used for parsing JSON and computing the list of servers.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Id of the last scheduled task.
  int last_received_task_ = 0;
  // Id of the last completed task.
  int last_processed_task_ = 0;
  // The current list of servers.
  std::vector<PrintServer> servers_;

  base::ObserverList<PrintServersProvider::Observer>::Unchecked observers_;
  base::WeakPtrFactory<PrintServersProviderImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrintServersProviderImpl);
};

}  // namespace

// static
std::unique_ptr<PrintServersProvider> PrintServersProvider::Create() {
  return std::make_unique<PrintServersProviderImpl>();
}

}  // namespace chromeos
