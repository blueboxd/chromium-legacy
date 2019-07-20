// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_print_manager.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "components/printing/browser/print_manager_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"

namespace android_webview {

namespace {

int SaveDataToFd(int fd,
                 int page_count,
                 scoped_refptr<base::RefCountedSharedMemoryMapping> data) {
  bool result = fd > base::kInvalidFd &&
                base::IsValueInRangeForNumericType<int>(data->size());
  if (result) {
    int size = data->size();
    result = base::WriteFileDescriptor(fd, data->front_as<char>(), size);
  }
  return result ? page_count : 0;
}

}  // namespace

struct AwPrintManager::FrameDispatchHelper {
  AwPrintManager* manager;
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

// static
AwPrintManager* AwPrintManager::CreateForWebContents(
    content::WebContents* contents,
    const printing::PrintSettings& settings,
    int file_descriptor,
    PrintManager::PdfWritingDoneCallback callback) {
  AwPrintManager* print_manager = new AwPrintManager(
      contents, settings, file_descriptor, std::move(callback));
  contents->SetUserData(UserDataKey(), base::WrapUnique(print_manager));
  return print_manager;
}

AwPrintManager::AwPrintManager(content::WebContents* contents,
                               const printing::PrintSettings& settings,
                               int file_descriptor,
                               PdfWritingDoneCallback callback)
    : PrintManager(contents), settings_(settings), fd_(file_descriptor) {
  pdf_writing_done_callback_ = std::move(callback);
  cookie_ = 1;  // Set a valid dummy cookie value.
}

AwPrintManager::~AwPrintManager() = default;

void AwPrintManager::PdfWritingDone(int page_count) {
  if (pdf_writing_done_callback_)
    pdf_writing_done_callback_.Run(page_count);
  // Invalidate the file descriptor so it doesn't get reused.
  fd_ = -1;
}

bool AwPrintManager::PrintNow() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* rfh = web_contents()->GetMainFrame();
  return rfh->Send(new PrintMsg_PrintPages(rfh->GetRoutingID()));
}

bool AwPrintManager::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  FrameDispatchHelper helper = {this, render_frame_host};
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(AwPrintManager, message, render_frame_host)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidPrintDocument, OnDidPrintDocument)
    IPC_MESSAGE_FORWARD_DELAY_REPLY(
        PrintHostMsg_GetDefaultPrintSettings, &helper,
        FrameDispatchHelper::OnGetDefaultPrintSettings)
    IPC_MESSAGE_FORWARD_DELAY_REPLY(PrintHostMsg_ScriptedPrint, &helper,
                                    FrameDispatchHelper::OnScriptedPrint)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled || PrintManager::OnMessageReceived(message, render_frame_host);
}

void AwPrintManager::OnGetDefaultPrintSettings(
    content::RenderFrameHost* render_frame_host,
    IPC::Message* reply_msg) {
  // Unlike the printing_message_filter, we do process this in UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PrintMsg_Print_Params params;
  printing::RenderParamsFromPrintSettings(settings_, &params);
  params.document_cookie = cookie_;
  PrintHostMsg_GetDefaultPrintSettings::WriteReplyParams(reply_msg, params);
  render_frame_host->Send(reply_msg);
}

void AwPrintManager::OnScriptedPrint(
    content::RenderFrameHost* render_frame_host,
    const PrintHostMsg_ScriptedPrint_Params& scripted_params,
    IPC::Message* reply_msg) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PrintMsg_PrintPages_Params params;
  printing::RenderParamsFromPrintSettings(settings_, &params.params);
  params.params.document_cookie = scripted_params.cookie;
  params.pages = printing::PageRange::GetPages(settings_.ranges());
  PrintHostMsg_ScriptedPrint::WriteReplyParams(reply_msg, params);
  render_frame_host->Send(reply_msg);
}

void AwPrintManager::OnDidPrintDocument(
    content::RenderFrameHost* render_frame_host,
    const PrintHostMsg_DidPrintDocument_Params& params) {
  if (params.document_cookie != cookie_)
    return;

  const PrintHostMsg_DidPrintContent_Params& content = params.content;
  if (!content.metafile_data_region.IsValid()) {
    NOTREACHED() << "invalid memory handle";
    web_contents()->Stop();
    PdfWritingDone(0);
    return;
  }

  auto data = base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(
      content.metafile_data_region);
  if (!data) {
    NOTREACHED() << "couldn't map";
    web_contents()->Stop();
    PdfWritingDone(0);
    return;
  }

  base::PostTaskAndReplyWithResult(
      base::CreateTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})
          .get(),
      FROM_HERE, base::BindRepeating(&SaveDataToFd, fd_, number_pages_, data),
      pdf_writing_done_callback_);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AwPrintManager)

}  // namespace android_webview
