# MemoryPool

A C++ Memory Pool for studing tcmalloc design.

## Docs

内存池是用于解决malloc/new在多线程并发场景和大内存管理时的效率问题的。即便是使用最普通的定长内存池，也能获得巨大的效率提升，在对性能要求比较极致的场景下，这一点效率提升还是挺重要的。

```
new cost time:195
memory pool cost time:100
```

项目开发记录在我的个人博客：[https://blog.musnow.top/posts/4231483511/](https://blog.musnow.top/posts/4231483511/)，欢迎查阅和交流。
