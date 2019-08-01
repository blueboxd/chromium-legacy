// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/print_manager.h"

#include "build/build_config.h"
#include "components/printing/common/print_messages.h"
#include "content/public/browser/render_frame_host.h"

namespace printing {

struct PrintManager::FrameDispatchHelper {
  PrintManager* manager;
  content::RenderFrameHost* render_frame_host;

  bool Send(IPC::Message* msg) { return render_frame_host->Send(msg); }

  void OnGetDefaultPrintSettings(IPC::Message* reply_msg) {
    manager->OnGetDefaultPrintSettings(render_frame_host, reply_msg);
  }

  void OnScriptedPrint(const PrintHostMsg_ScriptedPrint_Params& scripted_params,
                       IPC::Message* reply_msg) {
    manager->OnScriptedPrint(render_frame_host, scripted_params, reply_msg);
  }
};

PrintManager::PrintManager(content::WebContents* contents)
    : content::WebContentsObserver(contents) {}

PrintManager::~PrintManager() = default;

bool PrintManager::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(PrintManager, message, render_frame_host)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidPrintDocument, OnDidPrintDocument)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  if (handled)
    return true;

  handled = true;
  FrameDispatchHelper helper = {this, render_frame_host};
  IPC_BEGIN_MESSAGE_MAP(PrintManager, message)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidGetPrintedPagesCount,
                        OnDidGetPrintedPagesCount)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidGetDocumentCookie,
                        OnDidGetDocumentCookie)
    IPC_MESSAGE_FORWARD_DELAY_REPLY(
        PrintHostMsg_GetDefaultPrintSettings, &helper,
        FrameDispatchHelper::OnGetDefaultPrintSettings)
    IPC_MESSAGE_FORWARD_DELAY_REPLY(PrintHostMsg_ScriptedPrint, &helper,
                                    FrameDispatchHelper::OnScriptedPrint)

    IPC_MESSAGE_HANDLER(PrintHostMsg_PrintingFailed, OnPrintingFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PrintManager::OnDidGetPrintedPagesCount(int cookie,
                                             int number_pages) {
  DCHECK_GT(cookie, 0);
  DCHECK_GT(number_pages, 0);
  number_pages_ = number_pages;
}

void PrintManager::OnDidGetDocumentCookie(int cookie) {
  cookie_ = cookie;
}

void PrintManager::OnPrintingFailed(int cookie) {
  if (cookie != cookie_) {
    NOTREACHED();
    return;
  }
#if defined(OS_ANDROID)
  PdfWritingDone(0);
#endif
}

void PrintManager::PrintingRenderFrameDeleted() {
#if defined(OS_ANDROID)
  PdfWritingDone(0);
#endif
}

}  // namespace printing
