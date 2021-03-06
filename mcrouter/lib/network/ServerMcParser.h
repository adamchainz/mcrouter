/*
 *  Copyright (c) 2016, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#pragma once

#include "mcrouter/lib/network/McAsciiParser.h"
#include "mcrouter/lib/network/McParser.h"

namespace facebook { namespace memcache {

template <class Callback>
class ServerMcParser : private McParser::ParserCallback {
 public:
  ServerMcParser(Callback& cb, size_t minBufferSize, size_t maxBufferSize);

  ~ServerMcParser();

  /**
   * TAsyncTransport-style getReadBuffer().
   *
   * @return  a buffer pointer and its size that should be safe to read into.
   *
   * The caller might use less than the whole buffer, and will call
   * readDataAvailable(n) giving the actual number of bytes used from
   * the beginning of this buffer.
   */
  std::pair<void*, size_t> getReadBuffer();

  /**
   * Feeds the new data into the parser.
   * @return false  On any parse error.
   */
  bool readDataAvailable(size_t len);

  mc_protocol_t protocol() const {
    return parser_.protocol();
  }

  bool outOfOrder() const {
    return parser_.outOfOrder();
  }

  /**
   * @return error message from ascii parser about parsing error.
   */
  folly::StringPiece getUnderlyingAsciiParserError() const {
    return asciiParser_.getErrorDescription();
  }

 private:
  McParser parser_;
  McServerAsciiParser asciiParser_;

  Callback& callback_;

  template <class ThriftType>
  void requestReadyHelper(TypedThriftRequest<ThriftType>&& req,
                          uint64_t reqid);

  /* McParser callbacks */
  bool umMessageReady(const UmbrellaMessageInfo& info,
                      const folly::IOBuf& buffer) override final;
  bool caretMessageReady(const UmbrellaMessageInfo& headerInfo,
                         const folly::IOBuf& buffer) override final;
  void handleAscii(folly::IOBuf& readBuffer) override final;
  void parseError(mc_res_t result, folly::StringPiece reason) override final;
  bool shouldReadToAsciiBuffer() const;

  template <class Request>
  void onRequest(Request&&, bool noreply);

  void multiOpEnd();

  // McServerAsciiParser callback wrapper.
  template <class C, class ReqsList>
  friend class detail::CallbackWrapper;
};

}}  // facebook::memcache

#include "ServerMcParser-inl.h"
