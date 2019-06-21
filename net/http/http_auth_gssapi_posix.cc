// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_gssapi_posix.h"

#include <limits>
#include <string>

#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/hex_utils.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth_gssapi_posix.h"
#include "net/http/http_auth_multi_round_parse.h"
#include "net/net_buildflags.h"

namespace net {

using DelegationType = HttpAuth::DelegationType;

// Exported mechanism for GSSAPI. We always use SPNEGO:

// iso.org.dod.internet.security.mechanism.snego (1.3.6.1.5.5.2)
gss_OID_desc CHROME_GSS_SPNEGO_MECH_OID_DESC_VAL = {
  6,
  const_cast<char*>("\x2b\x06\x01\x05\x05\x02")
};

gss_OID CHROME_GSS_SPNEGO_MECH_OID_DESC =
    &CHROME_GSS_SPNEGO_MECH_OID_DESC_VAL;

// Debugging helpers.
namespace {

OM_uint32 DelegationTypeToFlag(DelegationType delegation_type) {
  switch (delegation_type) {
    case DelegationType::kNone:
      return 0;
    case DelegationType::kByKdcPolicy:
      return GSS_C_DELEG_POLICY_FLAG;
    case DelegationType::kUnconstrained:
      return GSS_C_DELEG_FLAG;
  }
}

// ScopedBuffer releases a gss_buffer_t when it goes out of scope.
class ScopedBuffer {
 public:
  ScopedBuffer(gss_buffer_t buffer, GSSAPILibrary* gssapi_lib)
      : buffer_(buffer), gssapi_lib_(gssapi_lib) {
    DCHECK(gssapi_lib_);
  }

  ~ScopedBuffer() {
    if (buffer_ != GSS_C_NO_BUFFER) {
      OM_uint32 minor_status = 0;
      OM_uint32 major_status =
          gssapi_lib_->release_buffer(&minor_status, buffer_);
      if (major_status != GSS_S_COMPLETE) {
        DLOG(WARNING) << "Problem releasing buffer. major=" << major_status
                      << ", minor=" << minor_status;
      }
      buffer_ = GSS_C_NO_BUFFER;
    }
  }

 private:
  gss_buffer_t buffer_;
  GSSAPILibrary* gssapi_lib_;

  DISALLOW_COPY_AND_ASSIGN(ScopedBuffer);
};

// ScopedName releases a gss_name_t when it goes out of scope.
class ScopedName {
 public:
  ScopedName(gss_name_t name, GSSAPILibrary* gssapi_lib)
      : name_(name), gssapi_lib_(gssapi_lib) {
    DCHECK(gssapi_lib_);
  }

  ~ScopedName() {
    if (name_ != GSS_C_NO_NAME) {
      OM_uint32 minor_status = 0;
      OM_uint32 major_status = gssapi_lib_->release_name(&minor_status, &name_);
      if (major_status != GSS_S_COMPLETE) {
        DLOG(WARNING) << "Problem releasing name. "
                      << GetGssStatusValue(nullptr, "gss_release_name",
                                           major_status, minor_status);
      }
      name_ = GSS_C_NO_NAME;
    }
  }

 private:
  gss_name_t name_;
  GSSAPILibrary* gssapi_lib_;

  DISALLOW_COPY_AND_ASSIGN(ScopedName);
};

bool OidEquals(const gss_OID left, const gss_OID right) {
  if (left->length != right->length)
    return false;
  return 0 == memcmp(left->elements, right->elements, right->length);
}

}  // namespace

base::Value GetGssStatusCodeValue(GSSAPILibrary* gssapi_lib,
                                  OM_uint32 status,
                                  OM_uint32 status_code_type) {
  base::Value rv{base::Value::Type::DICTIONARY};

  rv.SetIntKey("status", status);

  // Message lookups aren't performed if there's no library or if the status
  // indicates success.
  if (!gssapi_lib || status == GSS_S_COMPLETE)
    return rv;

  // gss_display_status() can potentially return multiple strings by sending
  // each string on successive invocations. State is maintained across these
  // invocations in a caller supplied OM_uint32.  After each successful call,
  // the context is set to a non-zero value that should be passed as a message
  // context to each successive gss_display_status() call.  The initial and
  // terminal values of this context storage is 0.
  OM_uint32 message_context = 0;

  // To account for the off chance that gss_display_status() misbehaves and gets
  // into an infinite loop, we'll artificially limit the number of iterations to
  // |kMaxDisplayIterations|. This limit is arbitrary.
  constexpr size_t kMaxDisplayIterations = 8;
  size_t iterations = 0;

  // In addition, each message string is again arbitrarily limited to
  // |kMaxMsgLength|. There's no real documented limit to work with here.
  constexpr size_t kMaxMsgLength = 4096;

  base::Value messages{base::Value::Type::LIST};
  do {
    gss_buffer_desc_struct message_buffer = GSS_C_EMPTY_BUFFER;
    ScopedBuffer message_buffer_releaser(&message_buffer, gssapi_lib);

    OM_uint32 minor_status = 0;
    OM_uint32 major_status = gssapi_lib->display_status(
        &minor_status, status, status_code_type, GSS_C_NO_OID, &message_context,
        &message_buffer);

    if (major_status != GSS_S_COMPLETE || message_buffer.length == 0 ||
        !message_buffer.value) {
      continue;
    }

    base::StringPiece message_string{
        static_cast<const char*>(message_buffer.value),
        std::min(kMaxMsgLength, message_buffer.length)};

    // The returned string is almost assuredly ASCII, but be defensive.
    if (!base::IsStringUTF8(message_string))
      continue;

    messages.GetList().emplace_back(message_string);
  } while (message_context != 0 && ++iterations < kMaxDisplayIterations);

  if (messages.GetList().size() > 0)
    rv.SetKey("message", std::move(messages));
  return rv;
}

base::Value GetGssStatusValue(GSSAPILibrary* gssapi_lib,
                              base::StringPiece method,
                              OM_uint32 major_status,
                              OM_uint32 minor_status) {
  base::Value params{base::Value::Type::DICTIONARY};
  params.SetStringKey("function", method);
  params.SetKey("major_status", GetGssStatusCodeValue(gssapi_lib, major_status,
                                                      GSS_C_GSS_CODE));
  params.SetKey("minor_status", GetGssStatusCodeValue(gssapi_lib, minor_status,
                                                      GSS_C_MECH_CODE));
  return params;
}

base::Value OidToValue(gss_OID oid) {
  base::Value params(base::Value::Type::DICTIONARY);

  if (!oid || oid->length == 0) {
    params.SetStringKey("oid", "<Empty OID>");
    return params;
  }

  params.SetIntKey("length", oid->length);
  if (!oid->elements)
    return params;

  // Cap OID content at arbitrary limit 1k.
  constexpr OM_uint32 kMaxOidDataSize = 1024;
  const base::StringPiece oid_contents(reinterpret_cast<char*>(oid->elements),
                                       std::min(kMaxOidDataSize, oid->length));

  params.SetStringKey("bytes", HexDump(oid_contents));

  // Based on RFC 2744 Appendix A. Hardcoding the OIDs in the list below to
  // avoid having a static dependency on the library.
  static const struct {
    const char* symbolic_name;
    const gss_OID_desc oid_desc;
  } kWellKnownOIDs[] = {
      {"GSS_C_NT_USER_NAME",
       {10, const_cast<char*>("\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x01")}},
      {"GSS_C_NT_MACHINE_UID_NAME",
       {10, const_cast<char*>("\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x02")}},
      {"GSS_C_NT_STRING_UID_NAME",
       {10, const_cast<char*>("\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x03")}},
      {"GSS_C_NT_HOSTBASED_SERVICE_X",
       {6, const_cast<char*>("\x2b\x06\x01\x05\x06\x02")}},
      {"GSS_C_NT_HOSTBASED_SERVICE",
       {10, const_cast<char*>("\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x04")}},
      {"GSS_C_NT_ANONYMOUS", {6, const_cast<char*>("\x2b\x06\01\x05\x06\x03")}},
      {"GSS_C_NT_EXPORT_NAME",
       {6, const_cast<char*>("\x2b\x06\x01\x05\x06\x04")}}};

  for (auto& well_known_oid : kWellKnownOIDs) {
    if (OidEquals(oid, const_cast<const gss_OID>(&well_known_oid.oid_desc)))
      params.SetStringKey("oid", well_known_oid.symbolic_name);
  }

  return params;
}

base::Value GetDisplayNameValue(GSSAPILibrary* gssapi_lib,
                                const gss_name_t gss_name) {
  OM_uint32 major_status = 0;
  OM_uint32 minor_status = 0;
  gss_buffer_desc_struct name = GSS_C_EMPTY_BUFFER;
  gss_OID name_type = GSS_C_NO_OID;

  base::Value rv{base::Value::Type::DICTIONARY};
  major_status =
      gssapi_lib->display_name(&minor_status, gss_name, &name, &name_type);
  ScopedBuffer scoped_output_name(&name, gssapi_lib);
  if (major_status != GSS_S_COMPLETE) {
    rv.SetKey("error", GetGssStatusValue(gssapi_lib, "gss_display_name",
                                         major_status, minor_status));
    return rv;
  }
  auto name_string =
      base::StringPiece(reinterpret_cast<const char*>(name.value), name.length);
  rv.SetStringKey("name", base::IsStringUTF8(name_string)
                              ? name_string
                              : HexDump(name_string));
  rv.SetKey("type", OidToValue(name_type));
  return rv;
}

base::Value ContextFlagsToValue(OM_uint32 flags) {
  // TODO(asanka): This should break down known flags. At least the
  // GSS_C_DELEG_FLAG.
  return base::Value(base::StringPrintf("%08x", flags));
}

base::Value GetContextStateAsValue(GSSAPILibrary* gssapi_lib,
                                   const gss_ctx_id_t context_handle) {
  base::Value rv{base::Value::Type::DICTIONARY};
  if (context_handle == GSS_C_NO_CONTEXT) {
    rv.SetKey("error",
              GetGssStatusValue(nullptr, "<none>", GSS_S_NO_CONTEXT, 0));
    return rv;
  }

  OM_uint32 major_status = 0;
  OM_uint32 minor_status = 0;
  gss_name_t src_name = GSS_C_NO_NAME;
  gss_name_t targ_name = GSS_C_NO_NAME;
  OM_uint32 lifetime_rec = 0;
  gss_OID mech_type = GSS_C_NO_OID;
  OM_uint32 ctx_flags = 0;
  int locally_initiated = 0;
  int open = 0;
  major_status = gssapi_lib->inquire_context(&minor_status,
                                             context_handle,
                                             &src_name,
                                             &targ_name,
                                             &lifetime_rec,
                                             &mech_type,
                                             &ctx_flags,
                                             &locally_initiated,
                                             &open);
  if (major_status != GSS_S_COMPLETE) {
    rv.SetKey("error", GetGssStatusValue(gssapi_lib, "gss_inquire_context",
                                         major_status, minor_status));
    return rv;
  }
  ScopedName scoped_src_name(src_name, gssapi_lib);
  ScopedName scoped_targ_name(targ_name, gssapi_lib);

  rv.SetKey("source", GetDisplayNameValue(gssapi_lib, src_name));
  rv.SetKey("target", GetDisplayNameValue(gssapi_lib, targ_name));
  // lifetime_rec is a uint32, while base::Value only takes ints. On 32 bit
  // platforms uint32 doesn't fit on an int.
  rv.SetStringKey("lifetime", base::NumberToString(lifetime_rec));
  rv.SetKey("mechanism", OidToValue(mech_type));
  rv.SetKey("flags", ContextFlagsToValue(ctx_flags));
  rv.SetBoolKey("open", !!open);
  return rv;
}

GSSAPISharedLibrary::GSSAPISharedLibrary(const std::string& gssapi_library_name)
    : gssapi_library_name_(gssapi_library_name) {}

GSSAPISharedLibrary::~GSSAPISharedLibrary() {
  if (gssapi_library_) {
    base::UnloadNativeLibrary(gssapi_library_);
    gssapi_library_ = nullptr;
  }
}

bool GSSAPISharedLibrary::Init() {
  if (!initialized_)
    InitImpl();
  return initialized_;
}

bool GSSAPISharedLibrary::InitImpl() {
  DCHECK(!initialized_);
#if BUILDFLAG(DLOPEN_KERBEROS)
  gssapi_library_ = LoadSharedLibrary();
  if (gssapi_library_ == nullptr)
    return false;
#endif  // BUILDFLAG(DLOPEN_KERBEROS)
  initialized_ = true;
  return true;
}

base::NativeLibrary GSSAPISharedLibrary::LoadSharedLibrary() {
  const char* const* library_names;
  size_t num_lib_names;
  const char* user_specified_library[1];
  if (!gssapi_library_name_.empty()) {
    user_specified_library[0] = gssapi_library_name_.c_str();
    library_names = user_specified_library;
    num_lib_names = 1;
  } else {
    static const char* const kDefaultLibraryNames[] = {
#if defined(OS_MACOSX)
      "/System/Library/Frameworks/GSS.framework/GSS"
#elif defined(OS_OPENBSD)
      "libgssapi.so"          // Heimdal - OpenBSD
#else
      "libgssapi_krb5.so.2",  // MIT Kerberos - FC, Suse10, Debian
      "libgssapi.so.4",       // Heimdal - Suse10, MDK
      "libgssapi.so.2",       // Heimdal - Gentoo
      "libgssapi.so.1"        // Heimdal - Suse9, CITI - FC, MDK, Suse10
#endif
    };
    library_names = kDefaultLibraryNames;
    num_lib_names = base::size(kDefaultLibraryNames);
  }

  for (size_t i = 0; i < num_lib_names; ++i) {
    const char* library_name = library_names[i];
    base::FilePath file_path(library_name);

    // TODO(asanka): Move library loading to a separate thread.
    //               http://crbug.com/66702
    base::ThreadRestrictions::ScopedAllowIO allow_io_temporarily;
    base::NativeLibraryLoadError load_error;
    base::NativeLibrary lib = base::LoadNativeLibrary(file_path, &load_error);
    if (lib) {
      // Only return this library if we can bind the functions we need.
      if (BindMethods(lib))
        return lib;
      base::UnloadNativeLibrary(lib);
    } else {
      // If this is the only library available, log the reason for failure.
      DLOG_IF(WARNING, num_lib_names == 1) << load_error.ToString();
    }
  }
  DLOG(WARNING) << "Unable to find a compatible GSSAPI library";
  return nullptr;
}

#if BUILDFLAG(DLOPEN_KERBEROS)

namespace {

template <typename T>
bool BindGssMethod(base::NativeLibrary lib, const char* method, T* receiver) {
  *receiver = reinterpret_cast<T>(
      base::GetFunctionPointerFromNativeLibrary(lib, method));
  if (*receiver == nullptr) {
    DLOG(WARNING) << "Unable to bind function \"" << method << "\"";
    return false;
  }
  return true;
}

}  // namespace

bool GSSAPISharedLibrary::BindMethods(base::NativeLibrary lib) {
  bool rv = true;
  // It's unlikely for BindMethods() to fail if LoadNativeLibrary() succeeded.
  // A failure in this function indicates an interoperability issue whose
  // diagnosis requires knowing all the methods that are missing. Hence |rv| is
  // updated in a manner that prevents short-circuiting the BindGssMethod()
  // invocations.
  rv = BindGssMethod(lib, "gss_delete_sec_context", &delete_sec_context_) && rv;
  rv = BindGssMethod(lib, "gss_display_name", &display_name_) && rv;
  rv = BindGssMethod(lib, "gss_display_status", &display_status_) && rv;
  rv = BindGssMethod(lib, "gss_import_name", &import_name_) && rv;
  rv = BindGssMethod(lib, "gss_init_sec_context", &init_sec_context_) && rv;
  rv = BindGssMethod(lib, "gss_inquire_context", &inquire_context_) && rv;
  rv = BindGssMethod(lib, "gss_release_buffer", &release_buffer_) && rv;
  rv = BindGssMethod(lib, "gss_release_name", &release_name_) && rv;
  rv = BindGssMethod(lib, "gss_wrap_size_limit", &wrap_size_limit_) && rv;

  if (LIKELY(rv))
    return true;

  delete_sec_context_ = nullptr;
  display_name_ = nullptr;
  display_status_ = nullptr;
  import_name_ = nullptr;
  init_sec_context_ = nullptr;
  inquire_context_ = nullptr;
  release_buffer_ = nullptr;
  release_name_ = nullptr;
  wrap_size_limit_ = nullptr;
  return false;
}

#else  // DLOPEN_KERBEROS

bool GSSAPISharedLibrary::BindMethods(base::NativeLibrary lib) {
  // When not using dlopen(), statically bind to libgssapi methods.
  import_name_ = gss_import_name;
  release_name_ = gss_release_name;
  release_buffer_ = gss_release_buffer;
  display_name_ = gss_display_name;
  display_status_ = gss_display_status;
  init_sec_context_ = gss_init_sec_context;
  wrap_size_limit_ = gss_wrap_size_limit;
  delete_sec_context_ = gss_delete_sec_context;
  inquire_context_ = gss_inquire_context;
  return true;
}

#endif  // DLOPEN_KERBEROS

OM_uint32 GSSAPISharedLibrary::import_name(
    OM_uint32* minor_status,
    const gss_buffer_t input_name_buffer,
    const gss_OID input_name_type,
    gss_name_t* output_name) {
  DCHECK(initialized_);
  return import_name_(minor_status, input_name_buffer, input_name_type,
                      output_name);
}

OM_uint32 GSSAPISharedLibrary::release_name(
    OM_uint32* minor_status,
    gss_name_t* input_name) {
  DCHECK(initialized_);
  return release_name_(minor_status, input_name);
}

OM_uint32 GSSAPISharedLibrary::release_buffer(
    OM_uint32* minor_status,
    gss_buffer_t buffer) {
  DCHECK(initialized_);
  return release_buffer_(minor_status, buffer);
}

OM_uint32 GSSAPISharedLibrary::display_name(
    OM_uint32* minor_status,
    const gss_name_t input_name,
    gss_buffer_t output_name_buffer,
    gss_OID* output_name_type) {
  DCHECK(initialized_);
  return display_name_(minor_status,
                       input_name,
                       output_name_buffer,
                       output_name_type);
}

OM_uint32 GSSAPISharedLibrary::display_status(
    OM_uint32* minor_status,
    OM_uint32 status_value,
    int status_type,
    const gss_OID mech_type,
    OM_uint32* message_context,
    gss_buffer_t status_string) {
  DCHECK(initialized_);
  return display_status_(minor_status, status_value, status_type, mech_type,
                         message_context, status_string);
}

OM_uint32 GSSAPISharedLibrary::init_sec_context(
    OM_uint32* minor_status,
    const gss_cred_id_t initiator_cred_handle,
    gss_ctx_id_t* context_handle,
    const gss_name_t target_name,
    const gss_OID mech_type,
    OM_uint32 req_flags,
    OM_uint32 time_req,
    const gss_channel_bindings_t input_chan_bindings,
    const gss_buffer_t input_token,
    gss_OID* actual_mech_type,
    gss_buffer_t output_token,
    OM_uint32* ret_flags,
    OM_uint32* time_rec) {
  DCHECK(initialized_);
  return init_sec_context_(minor_status,
                           initiator_cred_handle,
                           context_handle,
                           target_name,
                           mech_type,
                           req_flags,
                           time_req,
                           input_chan_bindings,
                           input_token,
                           actual_mech_type,
                           output_token,
                           ret_flags,
                           time_rec);
}

OM_uint32 GSSAPISharedLibrary::wrap_size_limit(
    OM_uint32* minor_status,
    const gss_ctx_id_t context_handle,
    int conf_req_flag,
    gss_qop_t qop_req,
    OM_uint32 req_output_size,
    OM_uint32* max_input_size) {
  DCHECK(initialized_);
  return wrap_size_limit_(minor_status,
                          context_handle,
                          conf_req_flag,
                          qop_req,
                          req_output_size,
                          max_input_size);
}

OM_uint32 GSSAPISharedLibrary::delete_sec_context(
    OM_uint32* minor_status,
    gss_ctx_id_t* context_handle,
    gss_buffer_t output_token) {
  // This is called from the owner class' destructor, even if
  // Init() is not called, so we can't assume |initialized_|
  // is set.
  if (!initialized_)
    return 0;
  return delete_sec_context_(minor_status,
                             context_handle,
                             output_token);
}

OM_uint32 GSSAPISharedLibrary::inquire_context(
    OM_uint32* minor_status,
    const gss_ctx_id_t context_handle,
    gss_name_t* src_name,
    gss_name_t* targ_name,
    OM_uint32* lifetime_rec,
    gss_OID* mech_type,
    OM_uint32* ctx_flags,
    int* locally_initiated,
    int* open) {
  DCHECK(initialized_);
  return inquire_context_(minor_status,
                          context_handle,
                          src_name,
                          targ_name,
                          lifetime_rec,
                          mech_type,
                          ctx_flags,
                          locally_initiated,
                          open);
}

const std::string& GSSAPISharedLibrary::GetLibraryNameForTesting() {
  return gssapi_library_name_;
}

ScopedSecurityContext::ScopedSecurityContext(GSSAPILibrary* gssapi_lib)
    : security_context_(GSS_C_NO_CONTEXT),
      gssapi_lib_(gssapi_lib) {
  DCHECK(gssapi_lib_);
}

ScopedSecurityContext::~ScopedSecurityContext() {
  if (security_context_ != GSS_C_NO_CONTEXT) {
    gss_buffer_desc output_token = GSS_C_EMPTY_BUFFER;
    OM_uint32 minor_status = 0;
    OM_uint32 major_status = gssapi_lib_->delete_sec_context(
        &minor_status, &security_context_, &output_token);
    if (major_status != GSS_S_COMPLETE) {
      LOG(WARNING) << "Problem releasing security_context. "
                   << GetGssStatusValue(gssapi_lib_, "gss_delete_sec_context",
                                        major_status, minor_status);
    }
    security_context_ = GSS_C_NO_CONTEXT;
  }
}

HttpAuthGSSAPI::HttpAuthGSSAPI(GSSAPILibrary* library,
                               const std::string& scheme,
                               gss_OID gss_oid)
    : scheme_(scheme),
      gss_oid_(gss_oid),
      library_(library),
      scoped_sec_context_(library) {
  DCHECK(library_);
}

HttpAuthGSSAPI::~HttpAuthGSSAPI() = default;

bool HttpAuthGSSAPI::Init() {
  if (!library_)
    return false;
  return library_->Init();
}

bool HttpAuthGSSAPI::NeedsIdentity() const {
  return decoded_server_auth_token_.empty();
}

bool HttpAuthGSSAPI::AllowsExplicitCredentials() const {
  return false;
}

void HttpAuthGSSAPI::SetDelegation(DelegationType delegation_type) {
  delegation_type_ = delegation_type;
}

HttpAuth::AuthorizationResult HttpAuthGSSAPI::ParseChallenge(
    HttpAuthChallengeTokenizer* tok) {
  if (scoped_sec_context_.get() == GSS_C_NO_CONTEXT) {
    return net::ParseFirstRoundChallenge(scheme_, tok);
  }
  std::string encoded_auth_token;
  return net::ParseLaterRoundChallenge(scheme_, tok, &encoded_auth_token,
                                       &decoded_server_auth_token_);
}

int HttpAuthGSSAPI::GenerateAuthToken(const AuthCredentials* credentials,
                                      const std::string& spn,
                                      const std::string& channel_bindings,
                                      std::string* auth_token,
                                      CompletionOnceCallback /*callback*/) {
  DCHECK(auth_token);

  gss_buffer_desc input_token = GSS_C_EMPTY_BUFFER;
  input_token.length = decoded_server_auth_token_.length();
  input_token.value = (input_token.length > 0)
                          ? const_cast<char*>(decoded_server_auth_token_.data())
                          : nullptr;
  gss_buffer_desc output_token = GSS_C_EMPTY_BUFFER;
  ScopedBuffer scoped_output_token(&output_token, library_);
  int rv =
      GetNextSecurityToken(spn, channel_bindings, &input_token, &output_token);
  if (rv != OK)
    return rv;

  // Base64 encode data in output buffer and prepend the scheme.
  std::string encode_input(static_cast<char*>(output_token.value),
                           output_token.length);
  std::string encode_output;
  base::Base64Encode(encode_input, &encode_output);
  *auth_token = scheme_ + " " + encode_output;
  return OK;
}


namespace {

// GSSAPI status codes consist of a calling error (essentially, a programmer
// bug), a routine error (defined by the RFC), and supplementary information,
// all bitwise-or'ed together in different regions of the 32 bit return value.
// This means a simple switch on the return codes is not sufficient.

int MapImportNameStatusToError(OM_uint32 major_status) {
  VLOG(1) << "import_name returned 0x" << std::hex << major_status;
  if (major_status == GSS_S_COMPLETE)
    return OK;
  if (GSS_CALLING_ERROR(major_status) != 0)
    return ERR_UNEXPECTED;
  OM_uint32 routine_error = GSS_ROUTINE_ERROR(major_status);
  switch (routine_error) {
    case GSS_S_FAILURE:
      // Looking at the MIT Kerberos implementation, this typically is returned
      // when memory allocation fails. However, the API does not guarantee
      // that this is the case, so using ERR_UNEXPECTED rather than
      // ERR_OUT_OF_MEMORY.
      return ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS;
    case GSS_S_BAD_NAME:
    case GSS_S_BAD_NAMETYPE:
      return ERR_MALFORMED_IDENTITY;
    case GSS_S_DEFECTIVE_TOKEN:
      // Not mentioned in the API, but part of code.
      return ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS;
    case GSS_S_BAD_MECH:
      return ERR_UNSUPPORTED_AUTH_SCHEME;
    default:
      return ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS;
  }
}

int MapInitSecContextStatusToError(OM_uint32 major_status) {
  VLOG(1) << "init_sec_context returned 0x" << std::hex << major_status;
  // Although GSS_S_CONTINUE_NEEDED is an additional bit, it seems like
  // other code just checks if major_status is equivalent to it to indicate
  // that there are no other errors included.
  if (major_status == GSS_S_COMPLETE || major_status == GSS_S_CONTINUE_NEEDED)
    return OK;
  if (GSS_CALLING_ERROR(major_status) != 0)
    return ERR_UNEXPECTED;
  OM_uint32 routine_status = GSS_ROUTINE_ERROR(major_status);
  switch (routine_status) {
    case GSS_S_DEFECTIVE_TOKEN:
      return ERR_INVALID_RESPONSE;
    case GSS_S_DEFECTIVE_CREDENTIAL:
      // Not expected since this implementation uses the default credential.
      return ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS;
    case GSS_S_BAD_SIG:
      // Probably won't happen, but it's a bad response.
      return ERR_INVALID_RESPONSE;
    case GSS_S_NO_CRED:
      return ERR_INVALID_AUTH_CREDENTIALS;
    case GSS_S_CREDENTIALS_EXPIRED:
      return ERR_INVALID_AUTH_CREDENTIALS;
    case GSS_S_BAD_BINDINGS:
      // This only happens with mutual authentication.
      return ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS;
    case GSS_S_NO_CONTEXT:
      return ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS;
    case GSS_S_BAD_NAMETYPE:
      return ERR_UNSUPPORTED_AUTH_SCHEME;
    case GSS_S_BAD_NAME:
      return ERR_UNSUPPORTED_AUTH_SCHEME;
    case GSS_S_BAD_MECH:
      return ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS;
    case GSS_S_FAILURE:
      // This should be an "Unexpected Security Status" according to the
      // GSSAPI documentation, but it's typically used to indicate that
      // credentials are not correctly set up on a user machine, such
      // as a missing credential cache or hitting this after calling
      // kdestroy.
      // TODO(cbentzel): Use minor code for even better mapping?
      return ERR_MISSING_AUTH_CREDENTIALS;
    default:
      if (routine_status != 0)
        return ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS;
      break;
  }
  OM_uint32 supplemental_status = GSS_SUPPLEMENTARY_INFO(major_status);
  // Replays could indicate an attack.
  if (supplemental_status & (GSS_S_DUPLICATE_TOKEN | GSS_S_OLD_TOKEN |
                             GSS_S_UNSEQ_TOKEN | GSS_S_GAP_TOKEN))
    return ERR_INVALID_RESPONSE;

  // At this point, every documented status has been checked.
  return ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS;
}

}  // anonymous namespace

int HttpAuthGSSAPI::GetNextSecurityToken(const std::string& spn,
                                         const std::string& channel_bindings,
                                         gss_buffer_t in_token,
                                         gss_buffer_t out_token) {
  // GSSAPI header files, to this day, require OIDs passed in as non-const
  // pointers. Rather than const casting, let's just leave this as non-const.
  // Even if the OID pointer is const, the inner |elements| pointer is still
  // non-const.
  static gss_OID_desc kGSS_C_NT_HOSTBASED_SERVICE = {
      10, const_cast<char*>("\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x04")};

  // Create a name for the principal
  // TODO(cbentzel): Just do this on the first pass?
  std::string spn_principal = spn;
  gss_buffer_desc spn_buffer = GSS_C_EMPTY_BUFFER;
  spn_buffer.value = const_cast<char*>(spn_principal.c_str());
  spn_buffer.length = spn_principal.size() + 1;
  OM_uint32 minor_status = 0;
  gss_name_t principal_name = GSS_C_NO_NAME;
  OM_uint32 major_status =
      library_->import_name(&minor_status, &spn_buffer,
                            &kGSS_C_NT_HOSTBASED_SERVICE, &principal_name);
  int rv = MapImportNameStatusToError(major_status);
  if (rv != OK) {
    LOG(ERROR) << "Problem importing name from "
               << "spn \"" << spn_principal << "\"\n"
               << GetGssStatusValue(library_, "gss_import_name", major_status,
                                    minor_status);
    return rv;
  }
  ScopedName scoped_name(principal_name, library_);

  // Continue creating a security context.
  OM_uint32 req_flags = DelegationTypeToFlag(delegation_type_);
  major_status = library_->init_sec_context(
      &minor_status, GSS_C_NO_CREDENTIAL, scoped_sec_context_.receive(),
      principal_name, gss_oid_, req_flags, GSS_C_INDEFINITE,
      GSS_C_NO_CHANNEL_BINDINGS, in_token,
      nullptr,  // actual_mech_type
      out_token,
      nullptr,  // ret flags
      nullptr);
  rv = MapInitSecContextStatusToError(major_status);
  if (rv != OK) {
    LOG(ERROR) << "Problem initializing context. \n"
               << GetGssStatusValue(library_, "gss_init_sec_context",
                                    major_status, minor_status)
               << '\n'
               << GetContextStateAsValue(library_, scoped_sec_context_.get());
  }
  return rv;
}

}  // namespace net
