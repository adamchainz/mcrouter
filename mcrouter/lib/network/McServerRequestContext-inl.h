/*
 *  Copyright (c) 2016, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#include "mcrouter/lib/McOperation.h"
#include "mcrouter/lib/network/McServerSession.h"
#include "mcrouter/lib/network/TypedThriftMessage.h"
#include "mcrouter/lib/network/WriteBuffer.h"

namespace facebook { namespace memcache {

template <class ThriftType>
void McServerRequestContext::reply(McServerRequestContext&& ctx,
                                   TypedThriftReply<ThriftType>&& reply) {
  static constexpr size_t typeId = IdFromType<ThriftType, TReplyList>::value;
  McServerRequestContext::reply(std::move(ctx), std::move(reply), typeId);
}

template <class Reply>
void McServerRequestContext::reply(McServerRequestContext&& ctx,
                                   Reply&& reply,
                                   size_t typeId) {
  ctx.replied_ = true;

  // On error, multi-op parent may assume responsiblity of replying
  if (ctx.moveReplyToParent(
        reply.result(), reply.appSpecificErrorCode(),
        std::move(reply->message))) {
    replyImpl(std::move(ctx), Reply(), typeId);
  } else {
    replyImpl(std::move(ctx), std::move(reply), typeId);
  }
}

template <class Reply>
void McServerRequestContext::replyImpl(McServerRequestContext&& ctx,
                                       Reply&& reply,
                                       size_t typeId) {
  auto session = ctx.session_;

  if (ctx.noReply(reply.result())) {
    session->reply(nullptr, ctx.reqid_);
    return;
  }

  if (!session->ensureWriteBufs()) {
    return;
  }

  uint64_t reqid = ctx.reqid_;
  auto wb = session->writeBufs_->get();
  if (!wb->prepareTyped(std::move(ctx), std::move(reply), typeId)) {
    session->transport_->close();
    return;
  }
  session->reply(std::move(wb), reqid);
}

template <class OnRequest>
void McServerOnRequestWrapper<OnRequest, List<>>::requestReady(
  McServerRequestContext&& ctx, McRequest&& req, mc_op_t operation) {

  switch (operation) {
#define MC_OP(MC_OPERATION)                                             \
    case MC_OPERATION::mc_op:                                           \
      onRequest_.onRequest(                                             \
          std::move(ctx),                                               \
          McRequestWithOp<MC_OPERATION>(std::move(req)));               \
      break;
#include "mcrouter/lib/McOpList.h"

    case mc_op_unknown:
    case mc_op_servererr:
    case mc_nops:
      CHECK(false) << "internal operation type passed to requestReady()";
      break;
  }
}

template <class T, class Enable = void>
struct HasDispatchTypedRequest {
  static constexpr std::false_type value{};
};

template <class T>
struct HasDispatchTypedRequest<
  T,
  typename std::enable_if<
    std::is_same<
      decltype(std::declval<T>().dispatchTypedRequest(
                 size_t(0),
                 std::declval<folly::IOBuf>(),
                 std::declval<McServerRequestContext>())),
      bool>::value>::type> {
  static constexpr std::true_type value{};
};

template <class OnRequest>
void McServerOnRequestWrapper<OnRequest, List<>>::typedRequestReady(
    uint32_t typeId,
    const folly::IOBuf& reqBody,
    McServerRequestContext&& ctx) {

  dispatchTypedRequestIfDefined(
    typeId, reqBody, std::move(ctx),
    HasDispatchTypedRequest<OnRequest>::value);
}

}}  // facebook::memcache
