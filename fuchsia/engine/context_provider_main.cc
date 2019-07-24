// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/context_provider_main.h"

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/service_directory.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "fuchsia/base/lifecycle_impl.h"
#include "fuchsia/engine/context_provider_impl.h"

int ContextProviderMain() {
  base::SingleThreadTaskExecutor main_task_executor(
      base::MessagePump::Type::UI);
  base::fuchsia::ServiceDirectory* const directory =
      base::fuchsia::ServiceDirectory::GetDefault();

  ContextProviderImpl context_provider;
  base::fuchsia::ScopedServiceBinding<fuchsia::web::ContextProvider> binding(
      directory, &context_provider);

  base::fuchsia::ScopedServiceBinding<fuchsia::web::Debug> debug_binding(
      directory->outgoing_directory()->debug_dir(), &context_provider);

  base::RunLoop run_loop;
  cr_fuchsia::LifecycleImpl lifecycle(directory, run_loop.QuitClosure());

  run_loop.Run();

  return 0;
}
