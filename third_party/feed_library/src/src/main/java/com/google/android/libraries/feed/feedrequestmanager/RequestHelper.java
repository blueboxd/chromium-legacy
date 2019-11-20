// Copyright 2019 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.android.libraries.feed.feedrequestmanager;

import android.net.Uri;
import android.util.Base64;
import com.google.android.libraries.feed.api.host.network.HttpRequest;
import com.google.android.libraries.feed.api.host.network.HttpRequest.HttpMethod;
import com.google.protobuf.CodedInputStream;
import java.io.IOException;
import java.util.Collections;

/** A class that helps build and sent requests to the server */
public final class RequestHelper {
  public static final String MOTHERSHIP_PARAM_PAYLOAD = "reqpld";
  public static final String LOCALE_PARAM = "hl";
  private static final String MOTHERSHIP_PARAM_FORMAT = "fmt";
  private static final String MOTHERSHIP_VALUE_BINARY = "bin";

  private RequestHelper() {}

  /**
   * Returns the first length-prefixed value from {@code input}. The first bytes of input are
   * assumed to be a varint32 encoding the length of the rest of the message. If input contains more
   * than one message, only the first message is returned.i w
   *
   * @throws IOException if input cannot be parsed
   */
  static byte[] getLengthPrefixedValue(byte[] input) throws IOException {
    CodedInputStream codedInputStream = CodedInputStream.newInstance(input);
    if (codedInputStream.isAtEnd()) {
      throw new IOException("Empty length-prefixed response");
    } else {
      int length = codedInputStream.readRawVarint32();
      return codedInputStream.readRawBytes(length);
    }
  }

  static HttpRequest buildHttpRequest(
      String httpMethod, byte[] bytes, String endpoint, String locale) {
    boolean isPostMethod = HttpMethod.POST.equals(httpMethod);
    Uri.Builder uriBuilder = Uri.parse(endpoint).buildUpon();
    if (!isPostMethod) {
      uriBuilder.appendQueryParameter(
          MOTHERSHIP_PARAM_PAYLOAD, Base64.encodeToString(bytes, Base64.URL_SAFE | Base64.NO_WRAP));
    }

    uriBuilder.appendQueryParameter(MOTHERSHIP_PARAM_FORMAT, MOTHERSHIP_VALUE_BINARY);
    if (!locale.isEmpty()) {
      uriBuilder.appendQueryParameter(LOCALE_PARAM, locale);
    }

    return new HttpRequest(
        uriBuilder.build(),
        httpMethod,
        Collections.emptyList(),
        isPostMethod ? bytes : new byte[] {});
  }
}
