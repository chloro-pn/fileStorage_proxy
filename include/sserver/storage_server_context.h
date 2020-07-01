#ifndef STORAGE_SERVER_CONTEXT_H
#define STORAGE_SERVER_CONTEXT_H

#include "path_storage.h"
#include <map>
#include <vector>

class StorageServerContext {
public:
  enum class state {init, transfering_block_set, working };

  StorageServerContext();

  state getState() const {
      return state_;
  }

  void setState(state s) {
      state_ = s;
  }

  std::vector<Md5Info>& transferingMd5s() {
      return transfering_md5s_;
  }

  size_t& nextToTransferMd5Index() {
      return next_to_transfer_md5_index_;
  }

  std::map<Md5Info, std::string>& uploadingMd5s() {
      return uploading_md5s_;
  }

  std::map<Md5Info, uint64_t>& uploadingFlowIds() {
    return uploading_flow_ids_;
  }

  std::string getBlockFilePath(const Md5Info& md5) {
      return base_path_ + md5.getMd5Value();
  }

private:
  state state_;
  size_t next_to_transfer_md5_index_;
  std::vector<Md5Info> transfering_md5s_;

  std::map<Md5Info, std::string> uploading_md5s_;
  std::map<Md5Info, uint64_t> uploading_flow_ids_;
  std::string base_path_;
};

#endif // STORAGE_SERVER_CONTEXT_H
