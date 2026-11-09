# 训练大师

[English](README.md) | **简体中文**

训练大师是一款 SDVX 训练模组，提供丰富的练习功能。

> [!WARNING]
> 请勿用于排行榜作弊。

> [!NOTE]
> 支持 EXCEED GEAR FINAL（2025120900），NABLA 及以上版本。

## 下载

前往 Actions 页面下载最新构建版本。

## 使用

将 `trainingmaster.dll` 和 `trainingmaster.ini` 放入 contents 根目录或 modules 文件夹，然后使用 spice2x 注入 hook。

启动游戏后按 `Insert` 打开界面，或访问 http://127.0.0.1:27182/

## 功能

> [!NOTE]
> 启用这些功能时，成绩默认会上传并记录。如需避免上传，请修改选项中的「Autoplay & Upload Policy」，或在 WebUI 中设置「Autoplay 与上传策略」（三档可选）。

| 功能 | 说明 |
| --- | --- |
| 播放控制 | 定位、暂停、继续或倒放 |
| 播放倍率 | 将速度调整为 0.25x 至 2.00x，并提供预设按钮 |
| 循环 | 标记 IN 和 OUT 点，可连续循环或仅循环一次 |
| 循环缓冲 | 设置选中循环区域前后增加的时间 |
| 谱面预览 | 查看 Note 和旋钮轨迹，并编辑循环点 |
| 自动演奏 | 启用游戏原生自动演奏：全部、仅按钮或仅旋钮 |
| 谱面流速 | 固定谱面流速 |
| 镜头 | 阻止镜头移动 |
| 隐藏歌曲演出（实验性） | 练习时隐藏额外贴图动画、画面特效和 Live2D；部分效果无法移除 |
| 血条 | 覆盖血条类型和初始值 |

![alt text](QQ_1794197750947.png)

## 第三方组件

- [ImGui](https://github.com/ocornut/imgui)
- [MinHook](https://github.com/tsudakageyu/minhook)

## AI

所有代码均由 AI 编写，逆向工程由 AI 完成  
界面由人类设计
