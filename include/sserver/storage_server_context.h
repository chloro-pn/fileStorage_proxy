#ifndef STORAGE_SERVER_CONTEXT_H
#define STORAGE_SERVER_CONTEXT_H

#include "path_storage.h"
#include <map>
#include <vector>
#include <tuple>

class StorageServerContext {
public:
  struct upload_context {
    uint64_t flow_id;
    Md5Info uploading_md5_;
    std::string content;
  };

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

  std::map<uint64_t, upload_context>& uploadingContext() {
    return uploading_context_;
  }

  std::string getBlockFilePath(const Md5Info& md5) {
      return base_path_ + md5.getMd5Value();
  }

  std::map<Md5Info, std::pair<uint32_t, std::string>>& downloadingMd5s() {
    return downloading_md5s_;
  }

  std::map<uint64_t, std::tuple<Md5Info, size_t, uint32_t>>& downloadingContext() {
    return downloading_context_;
  }

private:
  state state_;
  size_t next_to_transfer_md5_index_;
  std::vector<Md5Info> transfering_md5s_;

  std::map<uint64_t, upload_context> uploading_context_;

  //md5, ref and file_block.
  std::map<Md5Info, std::pair<uint32_t, std::string>> downloading_md5s_;
  //flow_id, md5 and current_offset
  std::map<uint64_t, std::tuple<Md5Info, size_t, uint32_t>> downloading_context_;
  std::string base_path_;
};

#endif // STORAGE_SERVER_CONTEXT_H
