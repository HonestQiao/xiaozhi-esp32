# 编译配置命令

**配置编译目标为 ESP32：**

```bash
idf.py set-target esp32
```

**复制 PSRAM 配置（必须）：**

```bash
cp main/boards/m5stack-m5paper/sdkconfig.m5paper sdkconfig
```

**打开 menuconfig：**

```bash
idf.py menuconfig
```

**选择板子：**

```
Xiaozhi Assistant -> Board Type -> M5Stack M5Paper
```

**编译：**

```bash
idf.py build
```
