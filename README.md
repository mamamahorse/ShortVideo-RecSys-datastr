# 短视频推荐系统 — 数据结构课程大作业

基于 C++17 和 Python Streamlit 的短视频用户行为分析与推荐系统。包含数据构建、行为模拟、相似用户分析、视频推荐、热度预测、视频聚类和用户聚类七大功能模块。

## 运行
安装所需要的依赖，运行python run app.py
## 数据规模

| 指标 | 数值 |
|------|------|
| 视频数 | 109,948 |
| 用户数 | 12,000 |
| 用户群体数 | 12 |
| 行为日志数 | ~2,600,000 |
| 用户兴趣向量维度 | 12 |
| 视频特征向量维度 | 26 |


## 技术栈

| 层 | 技术 | 用途 |
|---|------|------|
| 核心算法 | C++17 (g++) | F1 数据构建、F2 行为模拟、向量构建、F5 热度预测 |
| 前端可视化 | Python Streamlit | F3 相似用户分析、F4 视频推荐、F6/F7 聚类分析、数据仪表板 |
| 数据处理 | Pandas, NumPy | CSV 读写、向量计算 |
| 数据采集 | bilibili_api (Python) | 从 B 站采集 10 万+ 条视频元数据 |
| 打包分发 | PyInstaller | 生成独立 Windows 可执行文件 |

## 项目结构

```
ShortVideo-RecSys-datastr/
├── src/                         # C++ 源代码
│   ├── data_builder/            # F1 数据构建
│   │   ├── build_video_catalog.cpp
│   │   ├── video_cleaner.cpp
│   │   └── video_cleaner.hpp
│   ├── simulator/               # F2 行为模拟
│   │   ├── generate_behavior_data.cpp
│   │   ├── behavior_simulator.cpp
│   │   └── behavior_simulator.hpp
│   ├── common/                  # 向量构建
│   │   ├── generate_vectors.cpp
│   │   ├── vector_builder.cpp
│   │   └── vector_builder.hpp
│   └── popularity/              # F5 热度预测
│       ├── predict_popularity.cpp
│       ├── popularity_predictor.cpp
│       └── popularity_predictor.hpp
├── datacollector/               # 数据采集脚本
│   ├── seeds_collector_2.py     # 大规模并发采集
│   └── seeds_collector_3.py     # 保守稳健采集
├── tests/                       # C++ 单元测试
├── docs/                        # 设计文档与课程报告
├── data/                        # 数据文件（运行时依赖）
│   ├── raw/                     # 原始视频数据
│   ├── processed/               # F1 输出：规范化视频表
│   ├── simulated/               # F2 输出：用户表 + 行为日志
│   └── outputs/                 # 向量表 + 预测结果
├── app.py                       # Streamlit 前端主程序（6 页面）
├── launcher.py                  # PyInstaller 启动器
├── build.ps1                    # C++ 编译脚本
├── ShortVideoRecSys.spec        # PyInstaller 打包配置
└── README.md                    # 本文件
