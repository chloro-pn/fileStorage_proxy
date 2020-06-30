#include "common/message.h"
#include "common/asio_wrapper/util.h"
#include "json.hpp"
#include <cstring>
#include <cassert>

using nlohmann::json;

namespace Message {
std::string getType(const json& j) {
  std::string type = j["type"].get<std::string>();
  return  type;
}

//UPLOAD_REQUEST
std::vector<Md5Info> getMd5FromUploadRequestMessage(const json& j) {
  std::vector<Md5Info> result;
  if(j.contains("md5s") == false) {
    return result;
  }
  for(json::const_iterator it = j["md5s"].begin(); it!= j["md5s"].end(); ++it) {
    std::string md5 = (*it).get<std::string>();
    result.push_back(Md5Info(md5));
  }
  return result;
}

std::string createUploadRequestMessage(const std::vector<Md5Info> &md5s) {
  json j;
  j["type"] = "upload_request";
  for(auto& each : md5s) {
    j["md5s"].push_back(each.getMd5Value());
  }
  return j.dump(-1, ' ', false, json::error_handler_t::ignore);
}

std::string createUploadResponseMessage(const std::vector<Md5Info>& md5s) {
  json j;
  j["type"] = "upload_response";
  for(auto& each : md5s) {
    j["md5s"].push_back(each.getMd5Value());
  }
  return j.dump(-1, ' ', false, json::error_handler_t::ignore);
}

std::vector<Md5Info> getMd5FromUploadResponseMessage(const json& j) {
  std::vector<Md5Info> result;
  if(j.contains("md5s") == false) {
    return result;
  }
  for(json::const_iterator it = j["md5s"].begin(); it != j["md5s"].end(); ++it) {
    result.push_back(Md5Info(it->get<std::string>()));
  }
  return result;
}

//UPLOAD_BLOCK
std::string createUploadBlockMessage(const Md5Info &md5, uint32_t index, bool eof, std::string&& content) {
  json j;
  j["type"] = "upload_block";
  j["md5"] = md5.getMd5Value();
  j["index"] = index;
  //client does not need to set it.
  j["flow_id"] = 0;
  j["eof"] = eof;
  j["content"] = std::move(content);
  return j.dump(-1, ' ', false, json::error_handler_t::ignore);
}

void setFlowIdToUploadBlockMessage(json& j, uint64_t flow_id) {
  j["flow_id"] = flow_id;
}

uint64_t getFlowIdFromUploadBlockMessage(const json& j) {
  return j["flow_id"].get<uint64_t>();
}

bool theFirstBlockPiece(const json& j) {
  return j["index"].get<uint32_t>() == 0;
}

bool theLastBlockPiece(const json &j) {
  return j["eof"].get<bool>() == true;
}

Md5Info getMd5FromUploadBlockMessage(const json& j) {
  return Md5Info(j["md5"].get<std::string>());
}

std::string getContentFromUploadBlockMessage(const json &j) {
  return j["content"].get<std::string>();
}

std::string constructUploadBlockAckMessage(const Md5Info& md5) {
  json j;
  j["type"] = "upload_block_ack";
  j["md5"] = md5.getMd5Value();
  return j.dump(-1, ' ', false, json::error_handler_t::ignore);
}

std::string constructUploadBlockFailMessage(const Md5Info& md5) {
  json j;
  j["type"] = "upload_block_fail";
  j["md5"] = md5.getMd5Value();
  return j.dump(-1, ' ', false, json::error_handler_t::ignore);
}

std::string constructFileStoreSuccMessage(Md5Info file_id) {
  json j;
  j["type"] = "file_storage_succ";
  j["file_id"] = file_id.getMd5Value();
  return j.dump(-1, ' ', false, json::error_handler_t::ignore);
}

std::string constructFileStoreFailMessage(Md5Info file_id) {
  json j;
  j["type"] = "file_storage_fail";
  j["file_id"] = file_id.getMd5Value();
  return j.dump(-1, ' ', false, json::error_handler_t::ignore);
}

std::vector<Md5Info> getMd5sFromTransferBlockSetMessage(const json& j) {
  std::vector<Md5Info> result;
  if(j.contains("md5s") == false) {
    return result;
  }
  for(json::const_iterator it = j["md5s"].begin(); it != j["md5s"].end(); ++it) {
    std::string md5 = (*it).get<std::string>();
    result.push_back(Md5Info(md5));
  }
  return result;
}

//UPLOAD_BLOCK_ACK
Md5Info getMd5FromUploadBlockAckMessage(const json& j) {
  return Md5Info(j["md5"].get<std::string>());
}

Md5Info getMd5FromUploadBlockFailMessage(const json &j) {
  return Md5Info(j["md5"].get<std::string>());
}

std::string constructUploadAllBlocksMessage() {
  json j;
  j["type"] = "upload_all_blocks";
  return j.dump(-1, ' ', false, json::error_handler_t::ignore);
}

std::string constructTransferBlockSetMessage(const std::vector<Md5Info>& md5s, bool eof) {
  json j;
  j["type"] = "transfer_block_set";
  for(auto& each : md5s) {
    j["md5s"].push_back(each.getMd5Value());
  }
  j["eof"] = eof;
  return j.dump(-1, ' ', false, json::error_handler_t::ignore);
}

bool theLastTransferBlockSet(const json &j) {
  return j["eof"] == true;
}

//DOWNLOAD_REQUEST
std::string constructDownLoadRequestMessage(Md5Info file_id) {
  json j;
  j["type"] = "download_request";
  j["file_id"] = file_id.getMd5Value();
  return j.dump(-1, ' ', false, json::error_handler_t::ignore);
}

Md5Info getFileIdFromDownLoadRequestMessage(const json& j) {
  return Md5Info(j["file_id"].get<std::string>());
}

std::string constructFileNotExistMessage(const Md5Info& md5) {
  json j;
  j["type"] = "file_not_exist";
  j["file_id"] = md5.getMd5Value();
  return j.dump(-1, ' ', false, json::error_handler_t::ignore);
}

//DOWNLOAD_BLOCK
std::string constructDownLoadBlockMessage(Md5Info block) {
  json j;
  j["type"] = "download_block";
  j["md5"] = block.getMd5Value();
  return j.dump(-1, ' ', false, json::error_handler_t::ignore);
}

Md5Info getMD5FromDownLoadBlockMessage(const json& j) {
  return Md5Info(j["md5"].get<std::string>());
}

//TRANSFER_BLOCK
std::string constructTransferBlockMessage(const Md5Info& md5, uint32_t index, bool eof, uint64_t flow_id, std::string&& content) {
  json j;
  j["type"] = "transfer_block";
  j["md5"] = md5.getMd5Value();
  j["index"] = index;
  j["flow_id"] = flow_id;
  j["eof"] = eof;
  j["content"] = std::move(content);
  return j.dump(-1, ' ', false, json::error_handler_t::ignore);
}

uint64_t getFlowIdFromTransferBlockMessage(const json& j) {
  return j["flow_id"].get<uint64_t>();
}

Md5Info getMd5FromTransferBlockMessage(const json& j) {
  return Md5Info(j["md5"].get<std::string>());
}

std::string getContentFromTransferBlockMessage(const json& j) {
  return j["content"].get<std::string>();
}

//TRANSFER_BLOCK_ACK
std::string constructTransferBlockAckMessage(const Md5Info& md5) {
  json j;
  j["type"] = "transfer_block_ack";
  j["md5"] = md5.getMd5Value();
  return j.dump(-1, ' ', false, json::error_handler_t::ignore);
}

Md5Info getMd5FromTransferBlockAckMessage(const json& j) {
  return Md5Info(j["md5"].get<std::string>());
}

//TRANSFER_ALL_BLOCKS
std::string constructTransferAllBlocksMessage() {
  json j;
  j["type"] = "transfer_all_blocks";
  return j.dump(-1, ' ', false, json::error_handler_t::ignore);
}
} //namespace Message
