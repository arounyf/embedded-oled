# SSD1306 OLED 驱动（i2c-gpio 方案）

为 **RK3566/OEC Turbo** 设备适配的 OLED 驱动，基于内核 `i2c-gpio` 模块实现硬件 I2C。

## 背景

RK3566 片上 I2C 控制器默认全部 disabled，且 GPIO0_D0/D1（pin 24/25）不支持 mux 为硬件 I2C。通过内核 `i2c-gpio` 驱动将这两个 GPIO 注册为标准 I2C 总线（`/dev/i2c-*`），由内核处理时序，用户态 `write()` 即可驱动 OLED，CPU 占用接近零。

## 关键问题与解决

| 问题 | 根因 | 解决 |
|------|------|------|
| 页面不轮播 | `signal()` 在 ARM64 上只触发一次 | 改用 `sigaction()` |
| 无法使用硬件 I2C | RK3566 I2C 控制器不可用 pin24/25 | DTB 添加 `i2c-gpio` 节点 |
| 软件 I2C CPU 过高 | busy-wait 消耗大量 CPU | 改用内核 i2c-gpio 驱动 |

## 文件结构

```
├── README.md
├── Makefile
├── config.json                      # OLED 显示内容 JSON 配置
├── I2C_Library/
│   ├── I2C.c                        # Linux I2C 接口（open/ioctl/write）
│   └── I2C.h
├── SSD1306_OLED_Library/
│   ├── SSD1306_OLED.c               # SSD1306 驱动
│   ├── SSD1306_OLED.h
│   └── gfxfont.h
├── Main/
│   ├── Main.c                       # 定时器改用 sigaction
│   ├── dataapi.c                    # JSON 解析与页面调度
│   └── dataapi.h
├── cJSON/
│   ├── cJSON.c
│   └── cJSON.h
```

## GPIO 接线

| 信号 | GPIO | 物理引脚 |
|------|------|----------|
| SDA | gpio0 pin 24 | UART6 RX |
| SCL | gpio0 pin 25 | UART6 TX |

## DTB 修改

需要两处修改，建议直接修改当前使用的 `.dtb`：

### 1. USB OTG（One-KVM 必需）

将 `usbdrd` 节点的 `dr_mode` 从 `"host"` 改为 `"peripheral"`：

```bash
cd /boot/dtb/rockchip/
# 备份
cp your-device.dtb your-device.dtb.orig
# 解包
dtc -I dtb -O dts your-device.dtb -o tmp.dts
# 编辑：找到 usbdrd 下的 usb@fcc00000，将 dr_mode = "host" 改为 dr_mode = "peripheral"
# （注意不要改 usbhost 下的那个）
# 打包
dtc -I dts -O dtb tmp.dts -o your-device.dtb
```

### 2. i2c-gpio（OLED 所需）

在根节点内添加 `i2c-gpio-0` 节点，将 GPIO24/25 注册为 I2C 总线：

```dts
i2c-gpio-0 {
    compatible = "i2c-gpio";
    sda-gpios = <&gpio0 24 0>;
    scl-gpios = <&gpio0 25 0>;
    i2c-gpio,delay-us = <2>;
    #address-cells = <1>;
    #size-cells = <0>;
    status = "okay";
};
```

> 确保内核已加载 `i2c-gpio` 模块：`echo i2c-gpio > /etc/modules-load.d/i2c-gpio.conf`
>
> 重启后检查：`ls /dev/i2c-*`，更新 `config.json` 中 `"dev"` 为对应设备路径。

## 部署步骤

### 1. 编译

```bash
cd /opt/ssd1306_oled
make clean
make
```

> 无需额外依赖，使用内核头文件 `<linux/i2c-dev.h>` 即可。

### 2. 安装服务

```bash
mkdir -p /etc/oled
cp config.json /etc/oled/config.json
# 按需修改 /etc/oled/config.json 中的 "dev" 指向正确的 /dev/i2c-*

cat > /etc/systemd/system/ssd-oled.service << 'EOF'
[Unit]
Description=SSD1306 OLED Display Driver
After=network.target

[Service]
Type=simple
ExecStart=/opt/ssd1306_oled/ssd -c /etc/oled/config.json
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable --now ssd-oled
```

### 3. 运行

```bash
./ssd                          # 使用默认配置 /etc/oled/config.json
./ssd -c /path/to/config.json  # 使用自定义配置
```

## 定时器修复 (Main.c)

```c
// 原代码：
signal(SIGALRM, timerHandler);

// 改为：
struct sigaction sa;
memset(&sa, 0, sizeof(sa));
sa.sa_handler = timerHandler;
sigaction(SIGALRM, &sa, NULL);
```

## 配置文件说明

`config.json` 支持多页面轮播：

```json
{
  "seting": {
    "pixel": 12864,
    "dev": "/dev/i2c-6",
    "addr": 60
  },
  "PageName": {
    "seting": {
      "cycle": 3,
      "time": 20,
      "page": 1
    },
    "display": [ ... ]
  }
}
```

页面序号必须从 1 开始连续递增，否则轮播中断。
