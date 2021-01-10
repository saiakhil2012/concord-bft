// Concord
//
// Copyright (c) 2018-2020 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0
// License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#include "internalCommandsHandler.hpp"
#include "OpenTracing.hpp"
#include "assertUtils.hpp"
#include "sliver.hpp"
#include "setup.hpp"
#include "kv_types.hpp"
#include "block_metadata.hpp"
#include "httplib.h"
#include <jsoncons/json.hpp>
#include <unistd.h>
#include <algorithm>

using namespace BasicRandomTests;
using namespace bftEngine;
using namespace httplib;
using namespace std;
using namespace jsoncons;

using concordUtils::Status;
using concordUtils::Sliver;
using concord::kvbc::BlockId;
using concord::kvbc::KeyValuePair;
using concord::storage::SetOfKeyValuePairs;

//const uint64_t LONG_EXEC_CMD_TIME_IN_SEC = 11;

int InternalCommandsHandler::execute(uint16_t clientId,
                                     uint64_t sequenceNum,
                                     uint8_t flags,
                                     uint32_t requestSize,
                                     const char *request,
                                     uint32_t maxReplySize,
                                     char *outReply,
                                     uint32_t &outActualReplySize,
                                     uint32_t &outActualReplicaSpecificInfoSize,
                                     concordUtils::SpanWrapper &span) {
  // ReplicaSpecificInfo is not currently used in the TesterReplica
  outActualReplicaSpecificInfoSize = 0;
  int res;
  if (requestSize < sizeof(SimpleRequest)) {
    LOG_ERROR(
        m_logger,
        "The message is too small: requestSize is " << requestSize << ", required size is " << sizeof(SimpleRequest));
    return -1;
  }
  bool readOnly = flags & MsgFlag::READ_ONLY_FLAG;
  if (readOnly) {
    res = executeReadOnlyCommand(
        requestSize, request, maxReplySize, outReply, outActualReplySize, outActualReplicaSpecificInfoSize);
  } else {
    res = executeWriteCommand(requestSize, request, sequenceNum, flags, maxReplySize, outReply, outActualReplySize);
  }
  if (!res) LOG_ERROR(m_logger, "Command execution failed!");
  return res ? 0 : -1;
}

void InternalCommandsHandler::execute(InternalCommandsHandler::ExecutionRequestsQueue &requests,
                                      const std::string &batchCid,
                                      concordUtils::SpanWrapper &parent_span) {
  for (auto &req : requests) {
    // ReplicaSpecificInfo is not currently used in the TesterReplica
    if (req.outExecutionStatus != 1) continue;
    req.outReplicaSpecificInfoSize = 0;
    int res;
    if (req.request.size() < sizeof(SimpleRequest)) {
      LOG_ERROR(m_logger,
                "The message is too small: requestSize is " << req.request.size() << ", required size is "
                                                            << sizeof(SimpleRequest));
      req.outExecutionStatus = -1;
    }
    bool readOnly = req.flags & MsgFlag::READ_ONLY_FLAG;
    if (readOnly) {
      res = executeReadOnlyCommand(req.request.size(),
                                   req.request.c_str(),
                                   req.outReply.size(),
                                   req.outReply.data(),
                                   req.outActualReplySize,
                                   req.outReplicaSpecificInfoSize);
    } else {
      res = executeWriteCommand(req.request.size(),
                                req.request.c_str(),
                                req.executionSequenceNum,
                                req.flags,
                                req.outReply.size(),
                                req.outReply.data(),
                                req.outActualReplySize);
    }
    if (!res) LOG_ERROR(m_logger, "Command execution failed!");
    req.outExecutionStatus = res ? 0 : -1;
  }
}

void InternalCommandsHandler::addMetadataKeyValue(SetOfKeyValuePairs &updates, uint64_t sequenceNum) const {
  Sliver metadataKey = m_blockMetadata->getKey();
  Sliver metadataValue = m_blockMetadata->serialize(sequenceNum);
  updates.insert(KeyValuePair(metadataKey, metadataValue));
}

Sliver InternalCommandsHandler::buildSliverFromStaticBuf(char *buf) {
  char *newBuf = new char[KV_LEN];
  memcpy(newBuf, buf, KV_LEN);
  return Sliver(newBuf, KV_LEN);
}

bool InternalCommandsHandler::verifyWriteCommand(uint32_t requestSize,
                                                 const SimpleCondWriteRequest &request,
                                                 size_t maxReplySize,
                                                 uint32_t &outReplySize) const {
  if (requestSize < sizeof(SimpleCondWriteRequest)) {
    LOG_ERROR(m_logger,
              "The message is too small: requestSize is " << requestSize << ", required size is "
                                                          << sizeof(SimpleCondWriteRequest));
    return false;
  }
  if (requestSize < sizeof(request)) {
    LOG_ERROR(m_logger,
              "The message is too small: requestSize is " << requestSize << ", required size is " << sizeof(request));
    return false;
  }
  if (maxReplySize < outReplySize) {
    LOG_ERROR(m_logger, "replySize is too big: replySize=" << outReplySize << ", maxReplySize=" << maxReplySize);
    return false;
  }
  return true;
}

bool InternalCommandsHandler::executeWriteCommand(uint32_t requestSize,
                                                  const char *request,
                                                  uint64_t sequenceNum,
                                                  uint8_t flags,
                                                  size_t maxReplySize,
                                                  char *outReply,
                                                  uint32_t &outReplySize) {
  auto *writeReq = (SimpleCondWriteRequest *)request;
  LOG_DEBUG(m_logger,
           "Execute WRITE command:"
               << ", executionEngineId=" << (int)writeReq->header.executionEngineId << " type=" << writeReq->header.type
               << " seqNum=" << sequenceNum << " numOfWrites=" << writeReq->numOfWrites
               << " numOfKeysInReadSet=" << writeReq->numOfKeysInReadSet << " readVersion=" << writeReq->readVersion
               << " READ_ONLY_FLAG=" << ((flags & MsgFlag::READ_ONLY_FLAG) != 0 ? "true" : "false")
               << " PRE_PROCESS_FLAG=" << ((flags & MsgFlag::PRE_PROCESS_FLAG) != 0 ? "true" : "false")
               << " HAS_PRE_PROCESSED_FLAG=" << ((flags & MsgFlag::HAS_PRE_PROCESSED_FLAG) != 0 ? "true" : "false"));

  LOG_DEBUG(m_logger, "Caling a GET on Execution Engine");
  Client cli("172.17.0.1", 8080);

  /*auto res = cli.Get("/test");
  LOG_DEBUG(m_logger, "Test Status is " << res->status);
  LOG_DEBUG(m_logger, "Test Body is " << res->body);*/

  bool wroteKVSuccessfully = true;
  for (size_t i = 0; i < writeReq->numOfWrites; i++) {
      SimpleKV *keyValArray = writeReq->keyValueArray();
      KeyValuePair keyValue(buildSliverFromStaticBuf(keyValArray[i].simpleKey.key),
                            buildSliverFromStaticBuf(keyValArray[i].simpleValue.value));

      std::string k1(keyValArray[i].simpleKey.key);
      std::string v1(keyValArray[i].simpleValue.value);

      LOG_DEBUG(m_logger, "(WRITE) Key is " << k1);
      LOG_DEBUG(m_logger, "(WRITE) Value is " << v1);

      json body;
      body["command"] = "add";
      body["key"] = k1;
      body["value"] = v1;

      std::stringstream buffer;
      buffer << body << std::endl;

      LOG_DEBUG(m_logger, "(WRITE) JSON object is " << buffer.str());

      if (isSecure == true) {
        auto res1 = cli.Post("/ee/secured/execute", buffer.str(), "application/json");
        LOG_DEBUG(m_logger, "(WRITE) Status is " << res1->status);
        LOG_DEBUG(m_logger, "(WRITE) Body is " << res1->body);
        LOG_DEBUG(m_logger, "(WRITE) Number of Writes: " << ++numWrites);

        if(res1->body.length() == 0) {
          wroteKVSuccessfully = false;
        }
      } else {
        auto res1 = cli.Post("/ee/execute", buffer.str(), "application/json");
        LOG_DEBUG(m_logger, "(WRITE) Status is " << res1->status);
        LOG_DEBUG(m_logger, "(WRITE) Body is " << res1->body);
        LOG_DEBUG(m_logger, "(WRITE) Number of Writes: " << ++numWrites);

        if(res1->body.length() == 0) {
          wroteKVSuccessfully = false;
        }
      }
  }


  /*if (writeReq->header.type == WEDGE) {
    LOG_DEBUG(m_logger, "A wedge command has been called" << KVLOG(sequenceNum));
    controlStateManager_->setStopAtNextCheckpoint(sequenceNum);
  }
  if (writeReq->header.type == ADD_REMOVE_NODE) {
    LOG_DEBUG(m_logger, "An add_remove_node command has been called" << KVLOG(sequenceNum));
    controlStateManager_->setStopAtNextCheckpoint(sequenceNum);
    controlStateManager_->setEraseMetadataFlag(sequenceNum);
  }

  if (!(flags & MsgFlag::HAS_PRE_PROCESSED_FLAG)) {
    bool result = verifyWriteCommand(requestSize, *writeReq, maxReplySize, outReplySize);
    if (!result) ConcordAssert(0);
    if (flags & MsgFlag::PRE_PROCESS_FLAG) {
      if (writeReq->header.type == LONG_EXEC_COND_WRITE) sleep(LONG_EXEC_CMD_TIME_IN_SEC);
      outReplySize = requestSize;
      memcpy(outReply, request, requestSize);
      return result;
    }
  }

  SimpleKey *readSetArray = writeReq->readSetArray();
  BlockId currBlock = m_storage->getLastBlock();

  // Look for conflicts
  bool hasConflict = false;
  for (size_t i = 0; !hasConflict && i < writeReq->numOfKeysInReadSet; i++) {
    m_storage->mayHaveConflictBetween(
        buildSliverFromStaticBuf(readSetArray[i].key), writeReq->readVersion + 1, currBlock, hasConflict);
  }

  if (!hasConflict) {
    SimpleKV *keyValArray = writeReq->keyValueArray();
    SetOfKeyValuePairs updates;
    for (size_t i = 0; i < writeReq->numOfWrites; i++) {
      KeyValuePair keyValue(buildSliverFromStaticBuf(keyValArray[i].simpleKey.key),
                            buildSliverFromStaticBuf(keyValArray[i].simpleValue.value));
      updates.insert(keyValue);
    }
    addMetadataKeyValue(updates, sequenceNum);
    BlockId newBlockId = 0;
    Status addSuccess = m_blocksAppender->addBlock(updates, newBlockId);
    ConcordAssert(addSuccess.isOK());
    ConcordAssert(newBlockId == currBlock + 1);
  }

  ConcordAssert(sizeof(SimpleReply_ConditionalWrite) <= maxReplySize);*/
  
  
  
  auto *reply = (SimpleReply_ConditionalWrite *)outReply;
  reply->header.type = COND_WRITE;
  reply->success = wroteKVSuccessfully;
  /*if (wroteKVSuccessfully)
    reply->latestBlock = currBlock + 1;
  else
    reply->latestBlock = currBlock;*/

  outReplySize = sizeof(SimpleReply_ConditionalWrite);
  ++m_writesCounter;
  LOG_DEBUG(
      m_logger,
      "ConditionalWrite message handled; writesCounter=" << m_writesCounter << " currBlock=" << reply->latestBlock);
  return true;
}

bool InternalCommandsHandler::executeGetBlockDataCommand(
    uint32_t requestSize, const char *request, size_t maxReplySize, char *outReply, uint32_t &outReplySize) {
  auto *req = (SimpleGetBlockDataRequest *)request;
  LOG_DEBUG(m_logger, "Execute GET_BLOCK_DATA command: type=" << req->h.type << ", BlockId=" << req->block_id);

  auto minRequestSize = std::max(sizeof(SimpleGetBlockDataRequest), req->size());
  if (requestSize < minRequestSize) {
    LOG_ERROR(m_logger,
              "The message is too small: requestSize=" << requestSize << ", minRequestSize=" << minRequestSize);
    return false;
  }

  auto block_id = req->block_id;
  SetOfKeyValuePairs outBlockData;
  if (!m_storage->getBlockData(block_id, outBlockData).isOK()) {
    LOG_ERROR(m_logger, "GetBlockData: Failed to retrieve block %" << block_id);
    return false;
  }

  // Each block contains a single metadata key holding the sequence number
  const int numMetadataKeys = 1;
  auto numOfElements = outBlockData.size() - numMetadataKeys;
  size_t replySize = SimpleReply_Read::getSize(numOfElements);
  LOG_DEBUG(m_logger, "NUM OF ELEMENTS IN BLOCK = " << numOfElements);
  if (maxReplySize < replySize) {
    LOG_ERROR(m_logger, "replySize is too big: replySize=" << replySize << ", maxReplySize=" << maxReplySize);
    return false;
  }

  SimpleReply_Read *pReply = (SimpleReply_Read *)(outReply);
  outReplySize = replySize;
  memset(pReply, 0, replySize);
  pReply->header.type = READ;
  pReply->numOfItems = numOfElements;

  const Sliver metadataKey = m_blockMetadata->getKey();

  auto i = 0;
  for (const auto &kv : outBlockData) {
    if (kv.first != metadataKey) {
      memcpy(pReply->items[i].simpleKey.key, kv.first.data(), KV_LEN);
      memcpy(pReply->items[i].simpleValue.value, kv.second.data(), KV_LEN);
      ++i;
    }
  }
  return true;
}

bool InternalCommandsHandler::executeReadCommand(
    uint32_t requestSize, const char *request, size_t maxReplySize, char *outReply, uint32_t &outReplySize) {
  auto *readReq = (SimpleReadRequest *)request;
  LOG_DEBUG(m_logger,
           "Execute READ command: type=" << readReq->header.type << ", numberOfKeysToRead="
                                         << readReq->numberOfKeysToRead << ", readVersion=" << readReq->readVersion
                                         << ", executionEngineId=" << (int)readReq->header.executionEngineId);

  auto minRequestSize = std::max(sizeof(SimpleReadRequest), readReq->getSize());
  if (requestSize < minRequestSize) {
    LOG_ERROR(m_logger,
              "The message is too small: requestSize=" << requestSize << ", minRequestSize=" << minRequestSize);
    return false;
  }

  size_t numOfItems = readReq->numberOfKeysToRead;
  size_t replySize = SimpleReply_Read::getSize(numOfItems);

  if (maxReplySize < replySize) {
    LOG_ERROR(m_logger, "replySize is too big: replySize=" << replySize << ", maxReplySize=" << maxReplySize);
    return false;
  }

  auto *reply = (SimpleReply_Read *)(outReply);
  outReplySize = replySize;
  reply->header.type = READ;
  reply->numOfItems = numOfItems;

  LOG_DEBUG(m_logger, "Caling a GET on Execution Engine");
  Client cli("172.17.0.1", 8080);

  SimpleKey *readKeys = readReq->keys;
  SimpleKV *replyItems = reply->items;
  for (size_t i = 0; i < numOfItems; i++) {
    memcpy(replyItems[i].simpleKey.key, readKeys[i].key, KV_LEN);
    
    LOG_DEBUG(m_logger, "(READ) i num Read Item is: " << i);
    std::string k1(replyItems[i].simpleKey.key);

    LOG_DEBUG(m_logger, "(READ) Key is " << k1);
    LOG_DEBUG(m_logger, "(READ) Size of Key is " << k1.length());

    json body;
    body["command"] = "get";
    body["key"] = k1;

    std::stringstream buffer;
    buffer << body << std::endl;

    if (isSecure == true) {
      auto res1 = cli.Post("/ee/secured/execute", buffer.str(), "application/json");
      LOG_DEBUG(m_logger, "(READ) Status is " << res1->status);
      LOG_DEBUG(m_logger, "(READ) Size of Body is " << res1->body.length());
      LOG_DEBUG(m_logger, "(READ) Body is " << res1->body);
      LOG_DEBUG(m_logger, "(READ) Number of Reads: " << ++numReads);

      if (res1->body.length() > 0) {
        strcpy(replyItems[i].simpleValue.value, res1->body.c_str());
      } else {
        memset(replyItems[i].simpleValue.value, 0, KV_LEN);
      }
    } else {
      auto res1 = cli.Post("/ee/execute", buffer.str(), "application/json");
      LOG_DEBUG(m_logger, "(READ) Status is " << res1->status);
      LOG_DEBUG(m_logger, "(READ) Size of Body is " << res1->body.length());
      LOG_DEBUG(m_logger, "(READ) Body is " << res1->body);
      LOG_DEBUG(m_logger, "(READ) Number of Reads: " << ++numReads);

      if (res1->body.length() > 0) {
        strcpy(replyItems[i].simpleValue.value, res1->body.c_str());
      } else {
        memset(replyItems[i].simpleValue.value, 0, KV_LEN);
      }
    }
  }
  ++m_readsCounter;
  LOG_DEBUG(m_logger, "READ message handled; readsCounter=" << m_readsCounter);
  return true;
}

bool InternalCommandsHandler::executeHaveYouStoppedReadCommand(uint32_t requestSize,
                                                               const char *request,
                                                               size_t maxReplySize,
                                                               char *outReply,
                                                               uint32_t &outReplySize,
                                                               uint32_t &specificReplicaInfoSize) {
  auto *readReq = (SimpleHaveYouStoppedRequest *)request;
  LOG_DEBUG(m_logger, "Execute HaveYouStopped command: type=" << readReq->header.type);

  specificReplicaInfoSize = sizeof(int64_t);
  outReplySize = sizeof(SimpleReply);
  outReplySize += specificReplicaInfoSize;
  if (maxReplySize < outReplySize) {
    LOG_ERROR(m_logger, "The message is too small: requestSize=" << requestSize << ", minRequestSize=" << outReplySize);
    return false;
  }
  auto *reply = (SimpleReply_HaveYouStopped *)(outReply);
  reply->header.type = WEDGE;
  reply->stopped = controlHandlers_->haveYouStopped(readReq->n_of_n_stop);
  LOG_DEBUG(m_logger, "HaveYouStopped message handled");
  return true;
}

bool InternalCommandsHandler::executeGetLastBlockCommand(uint32_t requestSize,
                                                         size_t maxReplySize,
                                                         char *outReply,
                                                         uint32_t &outReplySize) {
  LOG_DEBUG(m_logger, "GET LAST BLOCK!!!");

  if (requestSize < sizeof(SimpleGetLastBlockRequest)) {
    LOG_ERROR(m_logger,
              "The message is too small: requestSize is " << requestSize << ", required size is "
                                                          << sizeof(SimpleGetLastBlockRequest));
    return false;
  }

  outReplySize = sizeof(SimpleReply_GetLastBlock);
  if (maxReplySize < outReplySize) {
    LOG_ERROR(m_logger, "maxReplySize is too small: replySize=" << outReplySize << ", maxReplySize=" << maxReplySize);
    return false;
  }

  auto *reply = (SimpleReply_GetLastBlock *)(outReply);
  reply->header.type = GET_LAST_BLOCK;
  reply->latestBlock = m_storage->getLastBlock();
  ++m_getLastBlockCounter;
  LOG_DEBUG(m_logger,
           "GetLastBlock message handled; getLastBlockCounter=" << m_getLastBlockCounter
                                                                << ", latestBlock=" << reply->latestBlock);
  return true;
}

bool InternalCommandsHandler::executeReadOnlyCommand(uint32_t requestSize,
                                                     const char *request,
                                                     size_t maxReplySize,
                                                     char *outReply,
                                                     uint32_t &outReplySize,
                                                     uint32_t &specificReplicaInfoOutReplySize) {
  auto *requestHeader = (SimpleRequest *)request;
  if (requestHeader->type == READ) {
    return executeReadCommand(requestSize, request, maxReplySize, outReply, outReplySize);
  } else if (requestHeader->type == GET_LAST_BLOCK) {
    return executeGetLastBlockCommand(requestSize, maxReplySize, outReply, outReplySize);
  } else if (requestHeader->type == GET_BLOCK_DATA) {
    return executeGetBlockDataCommand(requestSize, request, maxReplySize, outReply, outReplySize);
  } else if (requestHeader->type == WEDGE) {
    return executeHaveYouStoppedReadCommand(
        requestSize, request, maxReplySize, outReply, outReplySize, specificReplicaInfoOutReplySize);
  } else {
    outReplySize = 0;
    LOG_ERROR(m_logger, "Illegal message received: requestHeader->type=" << requestHeader->type);
    return false;
  }
}
void InternalCommandsHandler::setControlStateManager(
    std::shared_ptr<bftEngine::ControlStateManager> controlStateManager) {
  controlStateManager_ = controlStateManager;
}
