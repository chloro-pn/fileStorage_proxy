## TODO：

1.client上传文件，位置拼接可能有bug。[done]

2.MD5值的统一，字符串而不是二进制。[done]

3.json包不能发二进制数据，不是utf8字节会产生异常。

4.完成选择合适的存储服务器函数。[done]

5.proxy和sserver周期性的通信

6.可能发生向同一个sserver发送同一个md5值的block的piece，这样会导致
记录混乱，考虑加上flow号区分不同块的piece，丢弃不需要的，或者
新命令（优化）。

2020-06-21 : 

由于json库不能传递二进制数据，因此消息包中的md5都采用字符串模式而不是二进制模式。

注：MD5库也许有bug，寻找新的md5计算库测试，如有问题及时替换

2020-06-22 : 
添加makefile和脚本文件用于测试。

在根目录下运行run_example.sh脚本文件，会在在examples目录下会生成三个可执行文件，并启动执行，三个可执行文件分别代表客户端，代理端和存储端。
这个示例程序会模拟一次文件上传，并生成日志文件存储在*.txt文件中。

本项目使用第三方库redis，hiredis，spdlog，json，asio需要在src/makefile和examples/makefile中根据本机情况配置头文件路径和库文件路径。
redis作为服务程序启动，端口号默认，asio和spdlog是header-only库，json我直接放到项目中了，不用配置。

注：下一步预计开配置文件接口，开始的时候为了方便很多都写死在程序中了，现在调试很不方便。
