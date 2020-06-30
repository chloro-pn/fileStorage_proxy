### TODO：

1.client上传文件，位置拼接可能有bug。[done]

2.MD5值的统一，字符串而不是二进制。[done]

#### 第三方库问题

3.json包不能发二进制数据，不是utf8字节会产生异常。[done]

2020-06-30 : json库的dump函数会检测数据的合法性（是否为utf8字节），如果不合法则会抛出异常，可以通过json::error_handler_t::ignore忽略。

注：这个参数的具体含义正在问库作者，不知道是忽略检测还是忽略不合法数据。。。

* 重要更新：为了支持用json库传输二进制文件，需要采用新的解析函数和序列化函数。

之前解析：

```c++
std::string message;
json j = json::parse(message);
```

现在解析：

```c++
json j = json::from_bson(ptr, n); // ptr为接收到的消息头部指针，n为消息长度。
```

之前序列化：
```c++
std::string message = j.dump();
```

现在序列化：
```c++
std::string message;
json::to_bson(j, message);
```
如果使用message头文件中的函数构造发送消息则不用管，已经更新，但是解析消息需要用新的方法。

更新版本目前新开了个分支bson_test，没有放到主分支，需要再测试下。

4.完成选择合适的存储服务器函数。[done]

5.proxy和sserver周期性的通信

6.可能发生向同一个sserver发送同一个md5值的block的piece，这样会导致
记录混乱，考虑加上flow号区分不同块的piece，丢弃不需要的，或者
新命令（优化）。[doing]

2020-06-23 : 在uploadblock消息中添加了flow_id字段，类型为uint64_t，客户端只要将该字段置为0即可，proxy设置该字段
并将该消息转发给存储端，因为在并发情况下可能会有多个客户端向同一个存储服务器上传同一个文件块（md5值相同），
因此存储端需要一种机制区分上传同一个文件块的不同客户端。例如：

```c++
std::map<Md5Info, std::pair<uint64_t, std::string>> uploading_blocks_;
...
Md5Info md5 = Message::getMd5FromUploadBlockMessage(message);
uint64_t flow_id = Message::getFlowIdFromUploadBlockMessage(message);
if(uploading_blocks_.find(md5) == uploading_blocks_.end()) {
  ...//log and exit.
}
else {
  auto& tmp = uploading_blocks_[md5];
  if(flow_id != tmp.first) {
    ...//不是同一个客户端上传的md5块，直接丢掉（后续可以添加流程控制消息，告诉proxy该flow_id标识的客户端不需要继续上传该块，已经有其他客户端正在上传了。）
  }
  else {
    ...//storage.
  }
}
```

这个问题在之后的文件下载功能也会出现，可能有不同的客户端同一时间向同一个存储服务器请求下载同一个块，存储服务器要根据flow_id分别
记录每个客户端当前的下载上下文。

2020-06-21 : 
由于json库不能传递二进制数据，因此消息包中的md5都采用字符串模式而不是二进制模式。

#### 第三方库问题
注：MD5库也许有bug，寻找新的md5计算库测试，如有问题及时替换。

2020-06-28 :

经初步测试md5计算没有问题。

注：不要对一个MD5类型的对象调用多次toStr()函数，会产生不一样的md5值。

2020-06-22 : 
添加makefile和脚本文件用于测试。

#### 第三方库使用说明

在根目录下运行run_example.sh脚本文件，会在在examples目录下会生成三个可执行文件，并启动执行，三个可执行文件分别代表客户端，代理端和存储端。
这个示例程序会模拟一次文件上传，并生成日志文件存储在*.txt文件中。

本项目使用第三方库redis，hiredis，spdlog，json，asio需要在src/makefile和examples/makefile中根据本机情况配置头文件路径和库文件路径。
redis作为服务程序启动，端口号默认，asio和spdlog是header-only库，json我直接放到项目中了，不用配置。

2020-06-29 : spdlog和asio，json等库都被内置于third_party文件夹下，无需配置。

### 在云服务器上配置并启动

1.安装redis服务并启动（经测试云服务器的环境无法编译redis 6.0以上版本，因此选择5.x版本）

2.下载,编译并install hiredis v0.14.1，见 https://github.com/redis/hiredis/releases 。
注：需要修改文件：/etc/ld.so.conf 添加共享库路径/usr/local/lib.

3.hiredis的头文件和库文件路径修改src/makefile和examples/makefile文件。

4.编译 make -j4

5.启动服务 ./run_example.sh

6.在examples目录下查看日志文件*.txt，可以看到本次模拟上传的细节，在examples/file_storage_dir目录下是分块的文件，名称为块的md5值。
说明：proxy对客户端开放的端口为12345，对存储服务器开放的端口为12346，可以在conf/proxy.conf中修改。

7.停止服务 ./stop_example.sh

注：下一步预计开配置文件接口，开始的时候为了方便很多都写死在程序中了，现在调试很不方便。[doing]

2020-06-27 :

添加新的消息类型：FILE_NOT_EXIST，当客户端向proxy发起文件下载请求后，如果proxy没有对应文件id的记录，则返回给客户端此类型的消息，并重新进入init状态。

添加新的消息处理函数：getFlowIdFromTransferBlockMessage()，用以从该类型的消息中获取flow_id号码。

注：关于文件id的计算方式，本项目将组成一个文件的块序列的md5值作为内容，对该内容再做一次md5计算，将结果md5值作为文件id。理由是：根据md5算法的特性可以
保证不同文件的id不会相同，自动保证了id的全局唯一性，即使扩展系统也不需要引入第三方命名服务。


