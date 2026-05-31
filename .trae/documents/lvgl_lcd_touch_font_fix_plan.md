# LVGL LCD 触摸与字体问题修复计划

## 已确认问题

1. 字体文件重新生成后，`lv_font_source_han_sans_sc_16_cjk.c` 的头文件引用不适配当前工程，需要改回工程已有的 `lvgl.h` 引用方式。
2. 导航栏无法切换的主要风险点在触摸链路，而不是 `lv_tabview` 本身。
3. `ft6336.c` 中 `event` 解析存在明显错误：
   - 当前使用 `buf[base + 2]` 解析事件位。
   - FT6336 第一个触点的事件位应来自 `P1_XH`，也就是 `buf[base]` 的高 2 位。
   - 这会导致按下、抬起、持续接触状态判断异常。
4. `lvgl_lcd_demo.c` 中触摸坐标映射当前使用固定旋转公式，但尚未和当前 LCD 方向完全校准，存在点击位置错位风险。

## 实施步骤

1. 修复字体文件头部 include
   - 修改 `open_source/lvgl/src/font/lv_font_source_han_sans_sc_16_cjk.c`
   - 将 `#include "lvgl/lvgl.h"` 改为工程兼容写法

2. 修复 FT6336 触摸事件解析
   - 修改 `application/ws63/lvgl_lcd/ft6336.c`
   - 将 `touch_data->points[i].event` 的解析来源从 `buf[base + 2]` 改为 `buf[base]`
   - 保持 `x/y/weight/area` 解析逻辑与寄存器布局一致

3. 调整 LVGL 输入回调
   - 修改 `application/ws63/lvgl_lcd/lvgl_lcd_demo.c`
   - 在 `indev_read_cb` 中保留最后一次有效坐标
   - 基于正确的 `event` 值上报 `PRESSED/RELEASED`
   - 重新校准 `raw_x/raw_y` 到 `LCD_W/LCD_H` 的坐标映射

4. 校验底部导航切换
   - 重点验证 `lv_tabview` 底部 tab bar 区域是否能触发点击
   - 若点击区域仍偏移，则继续微调坐标交换和镜像关系

5. 校验文本颜色
   - 修改 `watchcontrol_ui.c` 中 `COLOR_TEXT` 与 `COLOR_TEXT_SEC`
   - 确保普通文本为黑色，同时保留状态色逻辑

## 预期结果

1. 字体文件恢复可编译。
2. 触摸按下、抬起、滑动状态正常上报。
3. 底部导航栏可切换。
4. 普通文本显示为黑色。
