#ifndef STORAGE_CONTEXT_H
#define STORAGE_CONTEXT_H

#include "common/md5_info.h"
#include <set>
#include <map>
#include <vector>
#include <functional>

class StorageContext {
public:
  StorageContext():state_(state::init) {

  }

  bool find(const Md5Info& md5) const {
    return storage_md5s_.find(md5) != storage_md5s_.end();
  }

  void pushStorageMd5(const Md5Info& md5) {
    storage_md5s_.insert(md5);
  }

  void subBlockAckEvent(const Md5Info& block, std::function<void(const Md5Info&, bool succ)> cb) {
    block_ack_callbacks_[block].push_back(cb);
  }

  void pubBlockAckEvent(const Md5Info& block, bool succ) {
    if(block_ack_callbacks_.find(block) == block_ack_callbacks_.end()) {
      return;
    }
    for(auto& each_cb : block_ack_callbacks_[block]) {
      each_cb(block, succ);
    }
    block_ack_callbacks_.erase(block);
  }

  void handleRemainEvent() {
    for(auto& each_cbs : block_ack_callbacks_) {
      for(auto& each_cb : each_cbs.second) {
        each_cb(each_cbs.first, false);
      }
    }
  }

  enum class state { init, waiting_block_set, working };

private:
  std::set<Md5Info> storage_md5s_;
  std::map<Md5Info, std::vector<std::function<void(const Md5Info&, bool)>>> block_ack_callbacks_;
  state state_;

public:
  state getState() const {
    return state_;
  }

  void setState(state s) {
    state_ = s;
  }
};

#endif // STORAGE_CONTEXT_H
