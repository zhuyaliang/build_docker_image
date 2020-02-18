# 跨平台制作docker镜像工具

## 说明
仿照docker build 实现跨平台制作docker 镜像，例如在x86平台下制作mips64、sw64平台下的docker 镜像。可以很好的兼容Dockerfile语法。

## 支持的操作
```FROM```
 
```COPY```

```EXPOSE```

```CMD```

```WORKDIR```

```VOLUME```

Dockerfile
```
FROM new:test
WORKDIR /home
VOLUME /home /home
COPY main.c /home
EXPOSE 9090 8080
CMD ls
```
## 编译
make

## 使用
sudo ./mkdimage Build  ```imagename```:```tag```
