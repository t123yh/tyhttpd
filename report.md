# 计算机网络实验报告

## HTTPS 实现

`HttpsStream`基于`MyStream`结构体实现，具体拓扑如下：

```C
struct MyStream {
    void *priv => 
        struct ssl_stream_priv{
            SSL_CTX *ssl;
            int fd;
        };
    void *userdata => NULL | SSL *ssl (after accept) ;
    read_function read;
  	write_function write;
  	destroy_function destroy;
}
```

具体实现中，当负责`https`的`pthread`接收到新的`tcp`请求后，会将对应的`file descriptor`传递给`InitTlsStream()`进行`ssl`上下文的初始化。

初始化过程中，程序首先会申请内存空间，设定`fd`，并储存函数指针。接下来程序会加载`SSL`库，设定上下文只接受`TLSv1-TLSv1.3`版本的请求，并验证上下文创建成功后加载证书文件。

初始化完成后，该函数返回一个`MyStream`结构体，并将该结构体传递给新创建的`pthread`进行处理。

在处理过程中，对`Stream`的首次读取会创建一个`SSL`会话，创建时会检查`MyStream`是否存在已绑定的会话。如果有，则返回已有对话，如果没有，则调用`SSL_accpet()`函数进行验证及握手流程，并将该会话储存在`MyStream`的`userdata`字段。

实际上，`read`,`write`,`destory`都会首先调用一次`accept`以确保完成了握手，避免未预期的行为。如果在握手过程中遇到客户端拒绝握手或者连接中断的问题（例如`curl`和`chrome`默认拒绝与自签名证书握手），`accept`会返回空指针，需要针对这种情况做特殊处理。

在`tls_accept()`返回空指针后，`tls_read()`和`tls_write()`均会返回`-1`。调用该部分的函数（以`ParseRequest()`为例）在得到非正常结果后，会终止执行，清空会话及上下文，释放内存空间并返回空指针。最终最上层的`ConnectionHandler`得到空指针后，会认为链接出现问题，直接进行流的销毁与清理。至此，异常处理完成。如果不对握手异常进行处理，可能造成程序异常循环或崩溃。

## 301 重定向实现

在进行重定向检查之前，程序已经完成了基本的`HTTP header`解析。而如果是`HTTPS`请求，也代表程序已经完成了`SSL`握手，`stream->userdata`也指向了`SSL`会话。

那么我们只需要检查`stream->userdata`是否为空，即可确定是否是`HTTPS`请求，是否需要重定向。同时如果`SSL`会话失效，程序会在解析`header`时就结束链接，因此不会出现因会话失效导致`stream->userdata`为空的情况。

如果确定为`HTTP`请求，只需要依据`header`中的`$host`及`$uri`拼接成`https://$host$uri`进行重定向即可完成。

## MinNet测试