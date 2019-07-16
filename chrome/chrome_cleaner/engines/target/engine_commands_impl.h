// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_TARGET_ENGINE_COMMANDS_IMPL_H_
#define CHROME_CHROME_CLEANER_ENGINES_TARGET_ENGINE_COMMANDS_IMPL_H_

#include <stdint.h>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "chrome/chrome_cleaner/engines/target/engine_delegate.h"
#include "chrome/chrome_cleaner/mojom/cleaner_engine_requests.mojom.h"
#include "chrome/chrome_cleaner/mojom/engine_requests.mojom.h"
#include "chrome/chrome_cleaner/mojom/engine_sandbox.mojom.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace chrome_cleaner {

class EngineCommandsImpl : public mojom::EngineCommands {
 public:
  EngineCommandsImpl(scoped_refptr<EngineDelegate> engine_delegate,
                     mojom::EngineCommandsRequest request,
                     scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                     base::OnceClosure error_handler);
  ~EngineCommandsImpl() override;

  // mojom::EngineCommands

  void Initialize(mojom::EngineFileRequestsAssociatedPtrInfo file_requests,
                  const base::FilePath& log_directory,
                  InitializeCallback callback) override;
  void StartScan(
      const std::vector<UwSId>& enabled_uws,
      const std::vector<UwS::TraceLocation>& enabled_trace_locations,
      bool include_details,
      mojom::EngineFileRequestsAssociatedPtrInfo file_requests,
      mojom::EngineRequestsAssociatedPtrInfo sandboxed_engine_requests,
      mojom::EngineScanResultsAssociatedPtrInfo scan_results_info,
      StartScanCallback callback) override;
  void StartCleanup(
      const std::vector<UwSId>& enabled_uws,
      mojom::EngineFileRequestsAssociatedPtrInfo file_requests,
      mojom::EngineRequestsAssociatedPtrInfo sandboxed_engine_requests,
      mojom::CleanerEngineRequestsAssociatedPtrInfo
          sandboxed_cleaner_engine_requests,
      mojom::EngineCleanupResultsAssociatedPtrInfo cleanup_results_info,
      StartCleanupCallback callback) override;
  void Finalize(FinalizeCallback callback) override;

 private:
  // Invokes |callback(result_code)| on the sequence of |task_runner_|.
  void PostInitializeCallback(
      mojom::EngineCommands::InitializeCallback callback,
      uint32_t result_code);

  scoped_refptr<EngineDelegate> engine_delegate_;
  mojo::Binding<mojom::EngineCommands> binding_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_TARGET_ENGINE_COMMANDS_IMPL_H_
