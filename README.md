## TODO：

1.client上传文件，位置拼接可能有bug。[done]

2.MD5值的统一，字符串而不是二进制。[done]

3.json包不能发二进制数据，不是utf8字节会产生异常。

4.完成选择合适的存储服务器函数。[done]

5.proxy和sserver周期性的通信

6.可能发生向同一个sserver发送同一个md5值的block的piece，这样会导致
记录混乱，考虑加上flow号区分不同块的piece，丢弃不需要的，或者
新命令（优化）。

