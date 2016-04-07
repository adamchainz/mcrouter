/*
 *  Copyright (c) 2016, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#include <gtest/gtest.h>

#include <folly/experimental/fibers/FiberManager.h>

#include "mcrouter/lib/network/test/TestClientServerUtil.h"
#include "mcrouter/lib/network/ThriftMessageList.h"
#include "mcrouter/lib/network/ThriftMsgDispatcher.h"

using namespace facebook::memcache;
using namespace facebook::memcache::test;

namespace {

class TypedServerOnRequest
    : public ThriftMsgDispatcher<TRequestList,
                                 TypedServerOnRequest,
                                 McServerRequestContext&&>,
      public TestServerOnRequest {

 public:
  TypedServerOnRequest(std::atomic<bool>& shutdown, bool outOfOrder)
      : TestServerOnRequest(shutdown, outOfOrder) {}

  void onTypedMessage(TypedThriftRequest<cpp2::McGetRequest>&& req,
                      McServerRequestContext&& ctx) {
    onRequest(std::move(ctx), std::move(req));
  }

  void onTypedMessage(TypedThriftRequest<cpp2::McSetRequest>&&,
                      McServerRequestContext&& ctx) {
    processReply(
        std::move(ctx), TypedThriftReply<cpp2::McSetReply>(mc_res_stored));
  }

  template <class Unsupported>
  void onTypedMessage(TypedThriftMessage<Unsupported>&&,
                      McServerRequestContext&& ctx) {
    processReply(std::move(ctx), McReply(mc_res_remote_error));
  }
};

void umbrellaCaretTest(SSLContextProvider ssl) {
  auto server = TestServer::create<TypedServerOnRequest>(true, ssl != noSsl());
  TestClient client("localhost",
                    server->getListenPort(),
                    200,
                    mc_caret_protocol,
                    ssl,
                    0,
                    0);
  client.sendGet("test1", mc_res_found);
  client.sendGet("test2", mc_res_found);
  client.sendGet("empty", mc_res_found);
  client.sendGet("hold", mc_res_found);
  client.sendGet("test3", mc_res_found);
  client.sendGet("test4", mc_res_found);
  client.sendGet("value_size:4096", mc_res_found);
  client.sendGet("value_size:8192", mc_res_found);
  client.sendGet("value_size:16384", mc_res_found);
  client.waitForReplies(3);
  client.sendGet("shutdown", mc_res_notfound);
  client.waitForReplies();
  server->join();
  EXPECT_EQ(1, server->getAcceptedConns());
}

} // anonymous

TEST(AsyncMcClientCaret, basic) {
  umbrellaCaretTest(noSsl());
}

TEST(AsyncMcClientCaret, basicSsl) {
  umbrellaCaretTest(validSsl());
}
