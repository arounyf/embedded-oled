# SSD1306 OLED 驱动（GPIO Busy-Wait 方案）

基于 [SSD1306_OLED_json](https://github.com/arounyf/SSD1306_OLED_json)，为 **RK3566/OEC Turbo** 设备适配的软件 I2C 方案。

## 背景

RK3566 片上 6 路 I2C 控制器默认全部 disabled，且 GPIO0_D0/D1（pin 24/25）不支持 mux 为硬件 I2C 功能。本方案通过 GPIO 软件模拟 I2C 驱动 SSD1306 OLED。

## 关键问题与解决

| 问题 | 根因 | 解决 |
|------|------|------|
| 页面不轮播 | `signal()` 在 ARM64 上只触发一次 | 改用 `sigaction()` |
| OLED 逐行刷新极慢 | 内核 `CONFIG_HZ=300`，`usleep()` 最少 3.3ms | 改用 busy-wait 延迟 |
| 每次 GPIO 操作过慢 | libgpiod `set_value` 是 ioctl，开销 50+us | libgpiod 实测 ~0.1us，瓶颈全在 usleep |
| mmap `/dev/mem` 写不生效 | `CONFIG_STRICT_DEVMEM=y` 阻止用户态写设备寄存器 | 放弃 mmap，用 libgpiod + busy-wait |

## I2C 时序

```
I2C_HD = 1us  (半位延迟，busy-wait)
I2C 时钟 ≈ 333kHz  (SSD1306 最大支持 400kHz)
整帧刷新 ≈ 46ms   (~22fps)
```

## 文件结构

```
├── README.md
├── Makefile                         # 编译文件，链接 -lgpiod
├── config.json                      # OLED 显示内容 JSON 配置
├── I2C_Library/
│   ├── I2C.c                        # 软件 I2C 实现（libgpiod + busy-wait）
│   └── I2C.h
├── SSD1306_OLED_Library/
│   ├── SSD1306_OLED.c               # SSD1306 驱动（原版，未改）
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

## 部署步骤

### 1. 安装依赖

```bash
apt-get install libgpiod-dev
```

### 2. 编译

```bash
cd /opt/ssd1306_oled
make clean
make
```

### 3. 运行前释放 GPIO

```bash
echo 24 > /sys/class/gpio/unexport 2>/dev/null
echo 25 > /sys/class/gpio/unexport 2>/dev/null
```

### 4. 运行

```bash
./ssd                      # 使用默认配置 /etc/oled/config.json
./ssd -c /path/to/config.json  # 使用自定义配置
```

## I2C.c 核心实现

```c
// GPIO 速度：libgpiod set_value 实测 ~0.1us/次
// 瓶颈在于 usleep() — CONFIG_HZ=300 导致最小睡眠 3.3ms
// 解决方案：用 NOP 循环做 busy-wait

#define BUSY_LOOPS_PER_US  2500   // 基于 CPU 频率校准
#define I2C_HD            1       // 半位延迟 (us)
#define I2C_FREQ_KHZ      (1000 / (3 * I2C_HD))  // ~333kHz

static void busy_udelay(int us)
{
    if (us <= 0) return;
    volatile unsigned int n = us * BUSY_LOOPS_PER_US;
    while (n--) __asm__ __volatile__("nop");
}

static void i2c_write_byte_fast(unsigned char byte)
{
    for (int i = 7; i >= 0; i--) {
        sda_set((byte >> i) & 1);
        busy_udelay(I2C_HD);
        scl_set(1); busy_udelay(I2C_HD);
        scl_set(0); busy_udelay(I2C_HD);
    }
    // ACK bit — SSD1306 always ACKs, skip read
    sda_set(1); busy_udelay(I2C_HD);
    scl_set(1); busy_udelay(I2C_HD);
    scl_set(0); busy_udelay(I2C_HD);
}
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

## BUSY_LOOPS_PER_US 校准

不同 CPU 频率需调节此值。在目标设备上运行：

```c
#include <time.h>
long now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000L + ts.tv_nsec;
}

int main() {
    volatile unsigned int n;
    #define TEST_LOOPS 100000
    long t0 = now_ns();
    for (volatile int i = 0; i < TEST_LOOPS; i++) {
        n = BUSY_LOOPS_PER_US;
        while (n--) __asm__("nop");
    }
    long t1 = now_ns();
    printf("Target: %ld ns/loop, Actual: %.1f ns\n",
           1000L, (double)(t1 - t0) / TEST_LOOPS);
    return 0;
}
```

调整 `BUSY_LOOPS_PER_US` 使实际值接近 1000ns。

## 配置文件说明

`config.json` 支持多页面轮播：

```json
{
  "seting": {                           // 全局设置
    "pixel": 12864,                     // 128x64 分辨率
    "dev": "/dev/i2c-3",               // 占位（实际用 GPIO）
    "addr": 60                          // I2C 地址 0x3C
  },
  "PageName": {                         // 每个页面一个 key
    "seting": {
      "cycle": 3,                       // 刷新次数
      "time": 20,                       // 显示时间 (×100ms)
      "page": 1                         // 页面序号（从 1 起连续）
    },
    "display": [ ... ]                  // 显示元素列表
  }
}
```

页面序号必须从 1 开始连续递增，否则轮播中断。
