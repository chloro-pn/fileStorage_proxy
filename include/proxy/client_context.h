#ifndef CLIENT_STATE_H
#define CLIENT_STATE_H

#include "common/md5_info.h"
#include "common/asio_wrapper/tcp_connection.h"
#include "spdlog/spdlog.h"
#include <vector>
#include <string>
#include <memory>

class ClientContext {
public:
  enum class state { init,
                     waiting_upload_block,
                     have_uploaded_all_blocks,
                     file_storage_succ };

  ClientContext(uint64_t flow_id, std::shared_ptr<spdlog::logger> logger):
                  flow_id_(flow_id),
                  state_(state::init),
                  succ_storages_(0),
                  fail_storages_(0),
                  logger_(logger) {

  }

  state getState() const {
    return state_;
  }

  void setState(state s) {
    state_ = s;
  }

  void succFailZero() {
    succ_storages_ = 0;
    fail_storages_ = 0;
  }

  int getSuccStorages() const {
    return succ_storages_;
  }

  void addSuccStorages() {
    ++succ_storages_;
  }

  int getFailStorages() const {
    return fail_storages_;
  }

  void addFailStorages() {
    ++fail_storages_;
  }

  bool readyToReplyClient() const {
    return succ_storages_ + fail_storages_ == 1;
  }

  void setUploadingBlockMd5s(const std::vector<Md5Info>& md5s) {
    uploading_block_md5s_ = md5s;
  }

  const std::vector<Md5Info>& getUploadingBlockMd5s() const {
    return uploading_block_md5s_;
  }

  void pushTransferingStorageServers(std::weak_ptr<TcpConnection> storage) {
    transfering_storage_servers_.push_back(storage);
  }

  std::vector<std::weak_ptr<TcpConnection>> getTransferingStorageServers() {
    return transfering_storage_servers_;
  }

  void transferingSetzero() {
    transfering_storage_servers_.clear();
  }

  uint64_t getFlowId() const {
    return flow_id_;
  }

private:
  uint64_t flow_id_;
  state state_;
  std::vector<Md5Info> uploading_block_md5s_;
  std::vector<std::weak_ptr<TcpConnection>> transfering_storage_servers_;

  int succ_storages_;
  int fail_storages_;
  std::shared_ptr<spdlog::logger> logger_;
};

#endif // CLIENT_STATE_H
