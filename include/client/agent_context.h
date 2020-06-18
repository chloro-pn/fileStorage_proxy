#ifndef CLIENT_CONTEXT_H
#define CLIENT_CONTEXT_H

class AgentContext {
public:
  enum class state {init,
                    waiting_upload_response,
                    uploading_blocks,
                    waiting_block_ack,
                    waiting_final_result,
                    succ,
                    fail};

  AgentContext();

  state getState() const {
      return state_;
  }

  void setState(state s) {
      state_ = s;
  }

private:
  state state_;
};

#endif // CLIENT_CONTEXT_H
