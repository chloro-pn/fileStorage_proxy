#ifndef TCP_CONNECTION_H
#define TCP_CONNECTION_H

#include <string>
#include <functional>
#include <memory>

class Session;

class TcpConnection {
public:
  friend class Session;

  TcpConnection(Session& sess);

  void get_next_message();

  void send(const std::string& str);

  void send(std::string&& str);

  void shutdownw();

  void run_after(size_t ms, std::function<void(std::shared_ptr<TcpConnection>)> self);

  const char* message_data() const;

  size_t message_length() const;

  void force_close();

  bool should_close() const;

  const std::string& iport() const;

  enum class state { goOn,
               readZero,
               readError,
               writeError,
               shutdownError,
               forceClose,
               outOfData,
               lengthError
               };

  state get_state() const;

  const char* get_state_str() const ;

  void set_state(state s);

  const std::string& what() const;

  void set_context(std::shared_ptr<void> cxt);

  template<typename T>
  T* get_context() {
    return static_cast<T*>(context_.get());
  }

private:
  void setError(const std::string err) {
      error_ = err;
  }

  Session& session_;
  std::string error_;
  std::string iport_;
  state state_;
  std::shared_ptr<void> context_;
};


#endif // TCP_CONNECTION_H
