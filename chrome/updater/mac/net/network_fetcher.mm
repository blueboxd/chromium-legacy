// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/net/network_fetcher.h"

#import <Foundation/Foundation.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/updater/mac/net/network.h"
#import "net/base/mac/url_conversions.h"
#include "url/gurl.h"

const NSString* kHeaderEtag = @"ETag";
const NSString* kHeaderXRetryAfter = @"X-Retry-After";

using ResponseStartedCallback =
    update_client::NetworkFetcher::ResponseStartedCallback;
using ProgressCallback = update_client::NetworkFetcher::ProgressCallback;
using PostRequestCompleteCallback =
    update_client::NetworkFetcher::PostRequestCompleteCallback;
using DownloadToFileCompleteCallback =
    update_client::NetworkFetcher::DownloadToFileCompleteCallback;

@interface CRUUpdaterNetworkController : NSObject <NSURLSessionDelegate>
- (instancetype)initWithResponseStartedCallback:
                    (ResponseStartedCallback)responseStartedCallback
                               progressCallback:
                                   (ProgressCallback)progressCallback;
@end

@implementation CRUUpdaterNetworkController {
 @protected
  ResponseStartedCallback _responseStartedCallback;
  ProgressCallback _progressCallback;
  scoped_refptr<base::SingleThreadTaskRunner> _callbackRunner;
}

- (instancetype)initWithResponseStartedCallback:
                    (ResponseStartedCallback)responseStartedCallback
                               progressCallback:
                                   (ProgressCallback)progressCallback {
  if (self = [super init]) {
    _responseStartedCallback = std::move(responseStartedCallback);
    _progressCallback = progressCallback;
    _callbackRunner = base::ThreadTaskRunnerHandle::Get();
  }
  return self;
}

#pragma mark - NSURLSessionDelegate

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  if (error) {
    DLOG(ERROR) << "NSURLSession error: " << error
                << ". NSURLSession: " << session
                << ". NSURLSessionTask: " << task;
  }
}
@end

@interface CRUUpdaterNetworkDataDelegate
    : CRUUpdaterNetworkController <NSURLSessionDataDelegate>
- (instancetype)
    initWithResponseStartedCallback:
        (ResponseStartedCallback)responseStartedCallback
                   progressCallback:(ProgressCallback)progressCallback
        postRequestCompleteCallback:
            (PostRequestCompleteCallback)postRequestCompleteCallback;
@end

@implementation CRUUpdaterNetworkDataDelegate {
  PostRequestCompleteCallback _postRequestCompleteCallback;
  base::scoped_nsobject<NSMutableData> _downloadedData;
}

- (instancetype)
    initWithResponseStartedCallback:
        (ResponseStartedCallback)responseStartedCallback
                   progressCallback:(ProgressCallback)progressCallback
        postRequestCompleteCallback:
            (PostRequestCompleteCallback)postRequestCompleteCallback {
  if (self = [super
          initWithResponseStartedCallback:std::move(responseStartedCallback)
                         progressCallback:progressCallback]) {
    _postRequestCompleteCallback = std::move(postRequestCompleteCallback);
  }
  return self;
}

#pragma mark - NSURLSessionDataDelegate

- (void)URLSession:(NSURLSession*)session
          dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveData:(NSData*)data {
  if (_downloadedData == nil) {
    _downloadedData.reset([[NSMutableData alloc] init]);
  }
  [_downloadedData appendData:data];

  int64_t current = 0;

  if (dataTask.countOfBytesExpectedToReceive > 0) {
    current = (dataTask.countOfBytesReceived * 100) /
              dataTask.countOfBytesExpectedToReceive;
  } else {
    current = 100;
  }
  _callbackRunner->PostTask(FROM_HERE,
                            base::BindOnce(_progressCallback, current));
  [dataTask resume];
}

// Tells the delegate that the data task received the initial reply (headers)
// from the server.
- (void)URLSession:(NSURLSession*)session
              dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveResponse:(NSURLResponse*)response
     completionHandler:
         (void (^)(NSURLSessionResponseDisposition))completionHandler {
  _callbackRunner->PostTask(
      FROM_HERE, base::BindOnce(std::move(_responseStartedCallback),
                                [(NSHTTPURLResponse*)response statusCode],
                                dataTask.countOfBytesExpectedToReceive));
  if (completionHandler) {
    completionHandler(NSURLSessionResponseAllow);
  }
  [dataTask resume];
}

#pragma mark - NSURLSessionDelegate

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  [super URLSession:session task:task didCompleteWithError:error];

  NSHTTPURLResponse* response = (NSHTTPURLResponse*)task.response;
  NSDictionary* headers = response.allHeaderFields;
  NSString* etag = @"";
  if ([headers objectForKey:kHeaderEtag]) {
    etag = [headers objectForKey:kHeaderEtag];
  }
  int64_t retryAfterResult = -1;
  NSString* xRetryAfter = [headers objectForKey:kHeaderXRetryAfter];
  if (xRetryAfter) {
    retryAfterResult = [xRetryAfter intValue];
  }

  _callbackRunner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(_postRequestCompleteCallback),
                     std::make_unique<std::string>(
                         base::SysNSStringToUTF8(response.description)),
                     error.code, std::string(base::SysNSStringToUTF8(etag)),
                     retryAfterResult));
}

@end

@interface CRUUpdaterNetworkDownloadDelegate
    : CRUUpdaterNetworkController <NSURLSessionDownloadDelegate>
- (instancetype)
    initWithResponseStartedCallback:
        (ResponseStartedCallback)responseStartedCallback
                   progressCallback:(ProgressCallback)progressCallback
                           filePath:(const base::FilePath&)filePath
     downloadToFileCompleteCallback:
         (DownloadToFileCompleteCallback)downloadToFileCompleteCallback;
@end

@implementation CRUUpdaterNetworkDownloadDelegate {
  base::FilePath _filePath;
  DownloadToFileCompleteCallback _downloadToFileCompleteCallback;
}

- (instancetype)
    initWithResponseStartedCallback:
        (ResponseStartedCallback)responseStartedCallback
                   progressCallback:(ProgressCallback)progressCallback
                           filePath:(const base::FilePath&)filePath
     downloadToFileCompleteCallback:
         (DownloadToFileCompleteCallback)downloadToFileCompleteCallback {
  if (self = [super
          initWithResponseStartedCallback:std::move(responseStartedCallback)
                         progressCallback:progressCallback]) {
    _filePath = filePath;
    _downloadToFileCompleteCallback = std::move(downloadToFileCompleteCallback);
  }
  return self;
}

#pragma mark - NSURLSessionDownloadDelegate

- (void)URLSession:(NSURLSession*)session
             dataTask:(NSURLSessionDataTask*)dataTask
    willCacheResponse:(NSCachedURLResponse*)proposedResponse
    completionHandler:
        (void (^)(NSCachedURLResponse* _Nullable))completionHandler {
  completionHandler(NULL);
}

- (void)URLSession:(NSURLSession*)session
                 downloadTask:(NSURLSessionDownloadTask*)downloadTask
    didFinishDownloadingToURL:(NSURL*)location {
  if (!location)
    return;

  const base::FilePath tempPath =
      base::mac::NSStringToFilePath([location path]);
  base::File::Error fileError;
  if (!base::ReplaceFile(tempPath, _filePath, &fileError)) {
    DLOG(ERROR)
        << "Failed to move the downloaded file from the temporary location: "
        << tempPath << "to: " << _filePath
        << " Error: " << base::File::ErrorToString(fileError);
  }
}

#pragma mark - NSURLSessionDelegate

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  [super URLSession:session task:task didCompleteWithError:error];

  NSHTTPURLResponse* response = (NSHTTPURLResponse*)task.response;
  NSURL* destination = base::mac::FilePathToNSURL(_filePath);
  NSString* filePath = [destination path];
  NSDictionary<NSFileAttributeKey, id>* attributes =
      [[NSFileManager defaultManager] attributesOfItemAtPath:filePath
                                                       error:nil];
  NSNumber* fileSizeAttribute = attributes[NSFileSize];
  int64_t fileSize = [fileSizeAttribute integerValue];
  _callbackRunner->PostTask(
      FROM_HERE, base::BindOnce(std::move(_downloadToFileCompleteCallback),
                                response.statusCode, fileSize));
}

@end

namespace base {
class SingleThreadTaskRunner;
}

namespace updater {

NetworkFetcher::NetworkFetcher() = default;

NetworkFetcher::~NetworkFetcher() = default;

void NetworkFetcher::PostRequest(
    const GURL& url,
    const std::string& post_data,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    PostRequestCompleteCallback post_request_complete_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  base::scoped_nsobject<CRUUpdaterNetworkDataDelegate> delegate(
      [[CRUUpdaterNetworkDataDelegate alloc]
          initWithResponseStartedCallback:std::move(response_started_callback)
                         progressCallback:progress_callback
              postRequestCompleteCallback:std::move(
                                              post_request_complete_callback)]);

  NSURLSession* session =
      [NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration
                                                 defaultSessionConfiguration]
                                    delegate:delegate
                               delegateQueue:nil];

  base::scoped_nsobject<NSMutableURLRequest> urlRequest(
      [[NSMutableURLRequest alloc] initWithURL:net::NSURLWithGURL(url)]);
  [urlRequest setHTTPMethod:@"POST"];
  [urlRequest setHTTPBody:[base::SysUTF8ToNSString(post_data)
                              dataUsingEncoding:NSUTF8StringEncoding]];
  VLOG(1) << "Posting data: " << post_data.c_str();

  NSURLSessionDataTask* dataTask = [session dataTaskWithRequest:urlRequest];
  [dataTask resume];
}

void NetworkFetcher::DownloadToFile(
    const GURL& url,
    const base::FilePath& file_path,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    DownloadToFileCompleteCallback download_to_file_complete_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  base::scoped_nsobject<CRUUpdaterNetworkDownloadDelegate> delegate(
      [[CRUUpdaterNetworkDownloadDelegate alloc]
          initWithResponseStartedCallback:std::move(response_started_callback)
                         progressCallback:progress_callback
                                 filePath:file_path
           downloadToFileCompleteCallback:
               std::move(download_to_file_complete_callback)]);

  NSURLSession* session =
      [NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration
                                                 defaultSessionConfiguration]
                                    delegate:delegate
                               delegateQueue:nil];

  base::scoped_nsobject<NSMutableURLRequest> urlRequest(
      [[NSMutableURLRequest alloc] initWithURL:net::NSURLWithGURL(url)]);

  NSURLSessionDownloadTask* downloadTask =
      [session downloadTaskWithRequest:urlRequest];
  [downloadTask resume];
}

NetworkFetcherFactory::NetworkFetcherFactory() = default;
NetworkFetcherFactory::~NetworkFetcherFactory() = default;

std::unique_ptr<update_client::NetworkFetcher> NetworkFetcherFactory::Create()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return std::make_unique<NetworkFetcher>();
}

}  // namespace updater
