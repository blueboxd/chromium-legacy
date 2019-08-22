// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_contents_network_client.h"

#include <map>
#include <memory>
#include <utility>

#include "android_webview/browser/input_stream.h"
#include "android_webview/browser/network_service/aw_web_resource_request.h"
#include "android_webview/browser/network_service/aw_web_resource_response.h"
#include "android_webview/common/devtools_instrumentation.h"
#include "android_webview/native_jni/AwContentsBackgroundThreadClient_jni.h"
#include "android_webview/native_jni/AwContentsNetworkClient_jni.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/lock.h"
#include "base/threading/scoped_blocking_call.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/data_url.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/resource_request.h"

using base::LazyInstance;
using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using content::BrowserThread;
using content::RenderFrameHost;
using content::ResourceType;
using content::WebContents;
using std::map;
using std::pair;
using std::string;

namespace android_webview {

namespace {

struct NetworkClientData {
  bool pending_association;
  JavaObjectWeakGlobalRef network_client;

  NetworkClientData();
};

NetworkClientData::NetworkClientData() : pending_association(false) {}

typedef map<pair<int, int>, NetworkClientData>
    RenderFrameHostToNetworkClientType;

typedef pair<base::flat_set<RenderFrameHost*>, NetworkClientData>
    HostsAndClientDataPair;

// When browser side navigation is enabled, RenderFrameIDs do not have
// valid render process host and render frame ids for frame navigations.
// We need to identify these by using FrameTreeNodeIds. Furthermore, we need
// to keep track of which RenderFrameHosts are associated with each
// FrameTreeNodeId, so we know when the last RenderFrameHost is deleted (and
// therefore the FrameTreeNodeId should be removed).
typedef map<int, HostsAndClientDataPair> FrameTreeNodeToNetworkClientType;

static pair<int, int> GetRenderFrameHostIdPair(RenderFrameHost* rfh) {
  return pair<int, int>(rfh->GetProcess()->GetID(), rfh->GetRoutingID());
}

// RfhToNetworkClientMap -----------------------------------------------------
class RfhToNetworkClientMap {
 public:
  static RfhToNetworkClientMap* GetInstance();
  void Set(pair<int, int> rfh_id, const NetworkClientData& client);
  bool Get(pair<int, int> rfh_id, NetworkClientData* client);

  bool Get(int frame_tree_node_id, NetworkClientData* client);

  // Prefer to call these when RenderFrameHost* is available, because they
  // update both maps at the same time.
  void Set(RenderFrameHost* rfh, const NetworkClientData& client);
  void Erase(RenderFrameHost* rfh);

 private:
  base::Lock map_lock_;
  // We maintain two maps simultaneously so that we can always get the correct
  // NetworkClientData, even when only HostIdPair or FrameTreeNodeId is
  // available.
  RenderFrameHostToNetworkClientType rfh_to_network_client_;
  FrameTreeNodeToNetworkClientType frame_tree_node_to_network_client_;
};

// static
LazyInstance<RfhToNetworkClientMap>::DestructorAtExit g_instance_ =
    LAZY_INSTANCE_INITIALIZER;

// static
LazyInstance<JavaObjectWeakGlobalRef>::DestructorAtExit g_sw_instance_ =
    LAZY_INSTANCE_INITIALIZER;

// static
RfhToNetworkClientMap* RfhToNetworkClientMap::GetInstance() {
  return g_instance_.Pointer();
}

void RfhToNetworkClientMap::Set(pair<int, int> rfh_id,
                                const NetworkClientData& client) {
  base::AutoLock lock(map_lock_);
  rfh_to_network_client_[rfh_id] = client;
}

bool RfhToNetworkClientMap::Get(pair<int, int> rfh_id,
                                NetworkClientData* client) {
  base::AutoLock lock(map_lock_);
  RenderFrameHostToNetworkClientType::iterator iterator =
      rfh_to_network_client_.find(rfh_id);
  if (iterator == rfh_to_network_client_.end())
    return false;

  *client = iterator->second;
  return true;
}

bool RfhToNetworkClientMap::Get(int frame_tree_node_id,
                                NetworkClientData* client) {
  base::AutoLock lock(map_lock_);
  FrameTreeNodeToNetworkClientType::iterator iterator =
      frame_tree_node_to_network_client_.find(frame_tree_node_id);
  if (iterator == frame_tree_node_to_network_client_.end())
    return false;

  *client = iterator->second.second;
  return true;
}

void RfhToNetworkClientMap::Set(RenderFrameHost* rfh,
                                const NetworkClientData& client) {
  int frame_tree_node_id = rfh->GetFrameTreeNodeId();
  pair<int, int> rfh_id = GetRenderFrameHostIdPair(rfh);
  base::AutoLock lock(map_lock_);

  // If this FrameTreeNodeId already has an associated NetworkClientData, add
  // this RenderFrameHost to the hosts set (it's harmless to overwrite the
  // NetworkClientData). Otherwise, operator[] creates a new map entry and we
  // add this RenderFrameHost to the hosts set and insert |client| in the pair.
  HostsAndClientDataPair& current_entry =
      frame_tree_node_to_network_client_[frame_tree_node_id];
  current_entry.second = client;
  current_entry.first.insert(rfh);

  // Always add the entry to the HostIdPair map, since entries are 1:1 with
  // RenderFrameHosts.
  rfh_to_network_client_[rfh_id] = client;
}

void RfhToNetworkClientMap::Erase(RenderFrameHost* rfh) {
  int frame_tree_node_id = rfh->GetFrameTreeNodeId();
  pair<int, int> rfh_id = GetRenderFrameHostIdPair(rfh);
  base::AutoLock lock(map_lock_);
  HostsAndClientDataPair& current_entry =
      frame_tree_node_to_network_client_[frame_tree_node_id];
  size_t num_erased = current_entry.first.erase(rfh);
  DCHECK(num_erased == 1);
  // Only remove this entry from the FrameTreeNodeId map if there are no more
  // live RenderFrameHosts.
  if (current_entry.first.empty()) {
    frame_tree_node_to_network_client_.erase(frame_tree_node_id);
  }

  // Always safe to remove the entry from the HostIdPair map, since entries are
  // 1:1 with RenderFrameHosts.
  rfh_to_network_client_.erase(rfh_id);
}

// ClientMapEntryUpdater ------------------------------------------------------

class ClientMapEntryUpdater : public content::WebContentsObserver {
 public:
  ClientMapEntryUpdater(JNIEnv* env,
                        WebContents* web_contents,
                        jobject jdelegate);

  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;

 private:
  JavaObjectWeakGlobalRef jdelegate_;
};

ClientMapEntryUpdater::ClientMapEntryUpdater(JNIEnv* env,
                                             WebContents* web_contents,
                                             jobject jdelegate)
    : content::WebContentsObserver(web_contents), jdelegate_(env, jdelegate) {
  DCHECK(web_contents);
  DCHECK(jdelegate);

  if (web_contents->GetMainFrame())
    RenderFrameCreated(web_contents->GetMainFrame());
}

void ClientMapEntryUpdater::RenderFrameCreated(RenderFrameHost* rfh) {
  NetworkClientData client_data;
  client_data.network_client = jdelegate_;
  client_data.pending_association = false;
  RfhToNetworkClientMap::GetInstance()->Set(rfh, client_data);
}

void ClientMapEntryUpdater::RenderFrameDeleted(RenderFrameHost* rfh) {
  RfhToNetworkClientMap::GetInstance()->Erase(rfh);
}

void ClientMapEntryUpdater::WebContentsDestroyed() {
  delete this;
}

}  // namespace

// AwContentsNetworkClient -----------------------------------------------

// static
std::unique_ptr<AwContentsNetworkClient> AwContentsNetworkClient::FromID(
    int render_process_id,
    int render_frame_id) {
  pair<int, int> rfh_id(render_process_id, render_frame_id);
  NetworkClientData client_data;
  if (!RfhToNetworkClientMap::GetInstance()->Get(rfh_id, &client_data))
    return nullptr;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_delegate =
      client_data.network_client.get(env);
  DCHECK(!client_data.pending_association || java_delegate.is_null());
  return std::make_unique<AwContentsNetworkClient>(
      client_data.pending_association, java_delegate);
}

std::unique_ptr<AwContentsNetworkClient> AwContentsNetworkClient::FromID(
    int frame_tree_node_id) {
  NetworkClientData client_data;
  if (!RfhToNetworkClientMap::GetInstance()->Get(frame_tree_node_id,
                                                 &client_data))
    return nullptr;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_delegate =
      client_data.network_client.get(env);
  DCHECK(!client_data.pending_association || java_delegate.is_null());
  return std::make_unique<AwContentsNetworkClient>(
      client_data.pending_association, java_delegate);
}

// static
void AwContentsNetworkClient::SubFrameCreated(int render_process_id,
                                              int parent_render_frame_id,
                                              int child_render_frame_id) {
  pair<int, int> parent_rfh_id(render_process_id, parent_render_frame_id);
  pair<int, int> child_rfh_id(render_process_id, child_render_frame_id);
  NetworkClientData client_data;
  if (!RfhToNetworkClientMap::GetInstance()->Get(parent_rfh_id, &client_data)) {
    NOTREACHED();
    return;
  }

  RfhToNetworkClientMap::GetInstance()->Set(child_rfh_id, client_data);
}

// static
void AwContentsNetworkClient::RegisterPendingContents(
    WebContents* web_contents) {
  NetworkClientData client_data;
  client_data.pending_association = true;
  RfhToNetworkClientMap::GetInstance()->Set(
      GetRenderFrameHostIdPair(web_contents->GetMainFrame()), client_data);
}

// static
void AwContentsNetworkClient::Associate(WebContents* web_contents,
                                        const JavaRef<jobject>& jclient) {
  JNIEnv* env = AttachCurrentThread();
  // The ClientMapEntryUpdater lifespan is tied to the WebContents.
  new ClientMapEntryUpdater(env, web_contents, jclient.obj());
}

// static
void AwContentsNetworkClient::SetServiceWorkerNetworkClient(
    const base::android::JavaRef<jobject>& jclient,
    const base::android::JavaRef<jobject>& browser_context) {
  // TODO: currently there is only one browser context so it is ok to
  // store in a global variable, in the future use browser_context to
  // obtain the correct instance.
  JavaObjectWeakGlobalRef temp(AttachCurrentThread(), jclient.obj());
  g_sw_instance_.Get() = temp;
}

// static
std::unique_ptr<AwContentsNetworkClient>
AwContentsNetworkClient::GetServiceWorkerNetworkClient() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_delegate = g_sw_instance_.Get().get(env);

  if (java_delegate.is_null())
    return nullptr;

  return std::make_unique<AwContentsNetworkClient>(false, java_delegate);
}

AwContentsNetworkClient::AwContentsNetworkClient(bool pending_association,
                                                 const JavaRef<jobject>& obj)
    : pending_association_(pending_association), java_object_(obj) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

AwContentsNetworkClient::~AwContentsNetworkClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // explict, out-of-line destructor.
}

bool AwContentsNetworkClient::PendingAssociation() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pending_association_;
}

AwContentsNetworkClient::CacheMode AwContentsNetworkClient::GetCacheMode()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (java_object_.is_null())
    return AwContentsNetworkClient::LOAD_DEFAULT;

  JNIEnv* env = AttachCurrentThread();
  return static_cast<AwContentsNetworkClient::CacheMode>(
      Java_AwContentsNetworkClient_getCacheMode(env, java_object_));
}

namespace {
// Used to specify what kind of url was intercepted by the embedded
// using shouldIntercepterRequest callback.
// Note: these values are persisted in UMA logs, so they should never be
// renumbered nor reused.
enum class InterceptionType {
  kNoIntercept,
  kOther,
  kHTTP,
  kHTTPS,
  kFILE,
  kDATA,
  // Magic constant used by the histogram macros.
  kMaxValue = kDATA,
};

// Record UMA whether the request was intercepted and if so what kind of scheme.
void RecordInterceptedScheme(bool response_is_null, const std::string& url) {
  InterceptionType type = InterceptionType::kNoIntercept;
  if (!response_is_null) {
    GURL gurl(url);
    if (gurl.SchemeIs(url::kHttpScheme)) {
      type = InterceptionType::kHTTP;
    } else if (gurl.SchemeIs(url::kHttpsScheme)) {
      type = InterceptionType::kHTTPS;
    } else if (gurl.SchemeIs(url::kFileScheme)) {
      type = InterceptionType::kFILE;
    } else if (gurl.SchemeIs(url::kDataScheme)) {
      type = InterceptionType::kDATA;
    } else {
      type = InterceptionType::kOther;
    }
  }
  UMA_HISTOGRAM_ENUMERATION(
      "Android.WebView.ShouldInterceptRequest.InterceptionType", type);
}

// Record UMA for the custom response status code for the intercepted requests
// where input stream is null. UMA is recorded only when the status codes and
// reason phrases are actually valid.
void RecordResponseStatusCode(JNIEnv* env, AwWebResourceResponse* response) {
  DCHECK(response);
  DCHECK(!response->HasInputStream(env));

  int status_code;
  std::string reason_phrase;
  bool status_info_valid =
      response->GetStatusInfo(env, &status_code, &reason_phrase);

  if (!status_info_valid) {
    // Status code is not necessary set properly in the response,
    // e.g. Webview's WebResourceResponse(String, String, InputStream) [*]
    // does not actually set the status code or the reason phrase. In this case
    // we just record a zero status code.
    // The other constructor (long version) or the #setStatusCodeAndReasonPhrase
    // method does actually perform validity checks on status code and reason
    // phrase arguments.
    // [*]
    // https://developer.android.com/reference/android/webkit/WebResourceResponse.html
    status_code = 0;
  }

  base::UmaHistogramSparse(
      "Android.WebView.ShouldInterceptRequest.NullInputStream."
      "ResponseStatusCode",
      status_code);
}

std::unique_ptr<AwWebResourceResponse> RunShouldInterceptRequest(
    AwWebResourceRequest request,
    JavaObjectWeakGlobalRef ref) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = ref.get(env);
  if (obj.is_null())
    return nullptr;

  AwWebResourceRequest::AwJavaWebResourceRequest java_web_resource_request;
  AwWebResourceRequest::ConvertToJava(env, request, &java_web_resource_request);

  devtools_instrumentation::ScopedEmbedderCallbackTask embedder_callback(
      "shouldInterceptRequest");
  ScopedJavaLocalRef<jobject> ret =
      Java_AwContentsBackgroundThreadClient_shouldInterceptRequestFromNative(
          env, obj, java_web_resource_request.jurl, request.is_main_frame,
          request.has_user_gesture, java_web_resource_request.jmethod,
          java_web_resource_request.jheader_names,
          java_web_resource_request.jheader_values);

  RecordInterceptedScheme(ret.is_null(), request.url);

  if (ret.is_null())
    return nullptr;

  std::unique_ptr<AwWebResourceResponse> response =
      std::make_unique<AwWebResourceResponse>(ret);
  if (!response->HasInputStream(env)) {
    // Only record UMA for cases where the input stream is null (see
    // crbug.com/974273).
    RecordResponseStatusCode(env, response.get());
  }

  return response;
}

std::unique_ptr<AwWebResourceResponse> ReturnNull() {
  return nullptr;
}

}  // namespace

void AwContentsNetworkClient::ShouldInterceptRequestAsync(
    AwWebResourceRequest request,
    ShouldInterceptRequestResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::OnceCallback<std::unique_ptr<AwWebResourceResponse>()> get_response =
      base::BindOnce(&ReturnNull);
  JNIEnv* env = AttachCurrentThread();
  if (bg_thread_client_object_.is_null() && !java_object_.is_null()) {
    bg_thread_client_object_.Reset(
        Java_AwContentsNetworkClient_getBackgroundThreadClient(env,
                                                               java_object_));
  }
  if (!bg_thread_client_object_.is_null()) {
    get_response = base::BindOnce(
        &RunShouldInterceptRequest, std::move(request),
        JavaObjectWeakGlobalRef(env, bg_thread_client_object_.obj()));
  }
  base::PostTaskAndReplyWithResult(sequenced_task_runner_.get(), FROM_HERE,
                                   std::move(get_response),
                                   std::move(callback));
}

bool AwContentsNetworkClient::ShouldBlockContentUrls() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (java_object_.is_null())
    return false;

  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsNetworkClient_shouldBlockContentUrls(env, java_object_);
}

bool AwContentsNetworkClient::ShouldBlockFileUrls() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (java_object_.is_null())
    return false;

  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsNetworkClient_shouldBlockFileUrls(env, java_object_);
}

bool AwContentsNetworkClient::ShouldAcceptThirdPartyCookies() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (java_object_.is_null())
    return false;

  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsNetworkClient_shouldAcceptThirdPartyCookies(
      env, java_object_);
}

bool AwContentsNetworkClient::GetSafeBrowsingEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (java_object_.is_null())
    return false;

  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsNetworkClient_getSafeBrowsingEnabled(env, java_object_);
}

bool AwContentsNetworkClient::ShouldBlockNetworkLoads() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (java_object_.is_null())
    return false;

  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsNetworkClient_shouldBlockNetworkLoads(env,
                                                              java_object_);
}

}  // namespace android_webview
