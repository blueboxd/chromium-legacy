// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This macro is used in <wrl/module.h>. Since only the COM functionality is
// used here (while WinRT is not being used), define this macro to optimize
// compilation of <wrl/module.h> for COM-only.
#ifndef __WRL_CLASSIC_COM_STRICT__
#define __WRL_CLASSIC_COM_STRICT__
#endif  // __WRL_CLASSIC_COM_STRICT__

#include "chrome/updater/server/win/com_classes.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/win/scoped_bstr.h"
#include "chrome/updater/server/win/server.h"
#include "chrome/updater/update_service.h"

namespace updater {

STDMETHODIMP CompleteStatusImpl::get_statusCode(LONG* code) {
  DCHECK(code);

  *code = code_;
  return S_OK;
}

STDMETHODIMP CompleteStatusImpl::get_statusMessage(BSTR* message) {
  DCHECK(message);

  *message = base::win::ScopedBstr(message_).Release();
  return S_OK;
}

HRESULT UpdaterImpl::CheckForUpdate(const base::char16* app_id) {
  return E_NOTIMPL;
}

HRESULT UpdaterImpl::Register(const base::char16* app_id,
                              const base::char16* brand_code,
                              const base::char16* tag,
                              const base::char16* version,
                              const base::char16* existence_checker_path) {
  return E_NOTIMPL;
}

HRESULT UpdaterImpl::Update(const base::char16* app_id) {
  return E_NOTIMPL;
}

// Called by the COM RPC runtime on one of its threads.
HRESULT UpdaterImpl::UpdateAll(IUpdaterObserver* observer) {
  using IUpdaterObserverPtr = Microsoft::WRL::ComPtr<IUpdaterObserver>;

  // Invoke the in-process |update_service| on the main sequence.
  auto com_server = ComServerApp::Instance();
  com_server->main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<UpdateService> update_service,
             IUpdaterObserverPtr observer) {
            update_service->UpdateAll(
                base::DoNothing(),
                base::BindOnce(
                    [](IUpdaterObserverPtr observer,
                       UpdateService::Result result) {
                      // The COM RPC outgoing call blocks and it must be posted
                      // through the thread pool.
                      base::ThreadPool::PostTaskAndReplyWithResult(
                          FROM_HERE, {base::MayBlock()},
                          base::BindOnce(
                              &IUpdaterObserver::OnComplete, observer,
                              Microsoft::WRL::Make<CompleteStatusImpl>(
                                  static_cast<int>(result), L"Test")),
                          base::BindOnce([](HRESULT hr) {
                            VLOG(2) << "IUpdaterObserver::OnComplete returned "
                                    << std::hex << hr;
                          }));
                    },
                    observer));
          },
          com_server->service(), IUpdaterObserverPtr(observer)));

  return S_OK;
}

}  // namespace updater
