# 修复字体显示和触控问题计划

## 问题分析

### 问题1：字体问题 - 个别中文字体显示不出来

**根因：** 当前使用的 `lv_font_source_han_sans_sc_16_cjk`（思源黑体 16px）仅包含约 1338 个 CJK 字符，UI 中有 18 个常用汉字缺失。

**精确验证结果**（通过脚本扫描字体 .c 文件中的 unicode_list_5 对照）：

| Unicode | 字符 | 所在文本 | 字体中? |
|---------|------|----------|---------|
| U+8FDE | 连 | 已连接/断开 | ❌ |
| U+5F00 | 开 | 断开 | ❌ |
| U+5E7F | 广 | 广播中 | ❌ |
| U+64AD | 播 | 广播中 | ❌ |
| U+8FD0 | 运 | 运行中 | ❌ |
| U+8BBE | 设 | 设备 | ❌ |
| U+5907 | 备 | 设备 | ❌ |
| U+95F2 | 闲 | 空闲 | ❌ |
| U+626B | 扫 | 扫描 | ❌ |
| U+5FD7 | 志 | 日志 | ❌ |
| U+52A1 | 务 | 服务名 | ❌ |
| U+641C | 搜 | 搜寻(备选) | ❌ |
| U+5BFB | 寻 | 搜寻(备选) | ❌ |
| U+88C5 | 装 | 装置(备选) | ❌ |
| U+674E | 李 | 李桂芳 | ❌ |
| U+6842 | 桂 | 李桂芳 | ❌ |
| U+82B3 | 芳 | 李桂芳 | ❌ |
| U+62A5 | 报 | 最近上报 | ❌ |

**修复方案：** 使用 lv_font_conv 重新生成字体文件，包含所有 UI 所需字符。环境已确认：
- Node.js v22.12.0 ✅
- lv_font_conv 已全局安装 ✅
- 需下载 SourceHanSansSC-Normal.otf 字体文件（从 GitHub 获取）

### 问题2：触控问题 - 无法进行切换、点击、下滑等操作

**根因：** FT6336 触摸驱动完全没有接入 LVGL 输入设备系统！

具体问题：
1. ❌ `ft6336_init()` 从未被调用（`lvgl_lcd_demo.c` 中缺少初始化调用）
2. ❌ 没有创建 `lv_indev_t`（未向 LVGL 注册触摸输入设备）
3. ❌ 没有实现 `indev_read_cb` 回调（触摸数据无法传递给 LVGL）
4. ❌ `ft6336.c` 未加入 `CMakeLists.txt` 的 SOURCES 列表

---

## 实施步骤

### 步骤1：重新生成 CJK 字体文件

1.1 下载 SourceHanSansSC-Normal.otf 字体文件到临时目录
1.2 提取现有字体生成命令中的 `--symbols` 字符串
1.3 在 `--symbols` 参数中追加缺失的 18 个字符：连接开广播运设备闲扫志务搜寻装置李桂芳报
1.4 同时追加 FontAwesome 图标字符（保留原有图标支持）
1.5 使用 lv_font_conv 重新生成 `lv_font_source_han_sans_sc_16_cjk.c`
1.6 替换项目中原有的字体文件

### 步骤2：将 FT6336 触摸驱动接入 LVGL

2.1 **修改 `CMakeLists.txt`**：
   - 在 SOURCES 列表中添加 `${CMAKE_CURRENT_SOURCE_DIR}/ft6336.c`

2.2 **修改 `lvgl_lcd_demo.c`**：
   - 添加 `#include "ft6336.h"`
   - 在 `lvgl_lcd_task()` 中，`lcd_init()` 之后调用 `ft6336_init()`
   - 实现 `indev_read_cb` 回调函数，读取 FT6336 触摸数据：
     - 检测触摸按下/释放状态（event 字段：0=按下, 1=释放, 2=接触中）
     - 传递触摸坐标给 LVGL
   - 创建并注册 `lv_indev_t`：
     ```c
     lv_indev_t * indev = lv_indev_create();
     lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
     lv_indev_set_read_cb(indev, indev_read_cb);
     ```

2.3 **触摸坐标适配**：
   - 屏幕竖屏 320x480
   - FT6336 返回坐标需与 LCD 坐标系一致
   - 如果方向不匹配，在回调中做 X/Y 变换

### 步骤3：验证

3.1 确认所有修改文件语法正确
3.2 确认 `ft6336.c` 已加入编译
3.3 确认 indev 回调逻辑正确
3.4 确认字体包含所有 UI 所需字符（运行验证脚本）

---

## 涉及修改的文件

| 文件 | 修改内容 |
|------|---------|
| `CMakeLists.txt` | SOURCES 中添加 `ft6336.c` |
| `lvgl_lcd_demo.c` | 添加 ft6336 头文件、初始化、indev 回调和注册 |
| `lv_font_source_han_sans_sc_16_cjk.c` | 重新生成，包含缺失字符 |
