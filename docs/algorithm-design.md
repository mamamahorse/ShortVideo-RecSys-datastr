# 算法与数据结构设计

## 实现原则

由于本项目是数据结构大作业，核心算法应尽量自己实现底层原理：

- 可以使用 C++ 标准库的 `vector`、`string`、`unordered_map`、`priority_queue`、`sort`、`fstream` 等基础工具。
- 不使用 `sklearn`、`pandas`、`numpy` 等库直接完成推荐、聚类、相似度和预测。
- 聚类、Top-K、余弦相似度、倒排索引、滑动窗口、指数平滑等核心步骤需要自己写。

## F1 数据构建

**输入：** `data/bilibili_10w_pro.csv`

**核心结构：**

- `vector<Video>` 存储视频表。
- `unordered_map<int, int>` 建立 `video_id` 到下标的映射。
- `unordered_map<string, int>` 建立分类、标签到编号的映射。
- `unordered_map<int, vector<int>> tagToVideos` 建立标签倒排索引。

**算法步骤：**

1. 逐行读取 CSV，解析视频字段。
2. 将 `category` 和 `tag` 映射为整数编号。
3. 计算视频质量分 `quality_score`。
4. 根据分类和标签生成主题向量 `topic_vector`。
5. 写出规范化视频表。

**质量分建议：**

```text
view_part = log(1 + view_count)
like_rate = like / (view_count + smooth)
fav_rate = favorite / (view_count + smooth)
coin_rate = coin / (view_count + smooth)
share_rate = share / (view_count + smooth)

quality_score = normalize(
    0.45 * view_part
  + 0.20 * like_rate
  + 0.15 * fav_rate
  + 0.15 * coin_rate
  + 0.05 * share_rate
)
```

加入 `smooth` 是为了避免低播放视频因为偶然几个互动而得到虚高比例。

## F2 行为模拟

详细方案见 `docs/simulation-plan.md`。这里给出算法定位。

**核心结构：**

- `vector<User>`：用户画像。
- `vector<Event>`：行为日志。
- `vector<vector<int>> topicToVideos`：主题到视频候选池。
- `priority_queue` 或加权采样数组：选择高质量视频。

**基本思路：**

1. 视频先被映射到 K 个隐性主题。
2. 用户被分配到 M 个兴趣群体，并生成兴趣向量。
3. 用户活跃度服从长尾分布，高活用户产生更多行为。
4. 用户每次行为先从偏好主题中召回候选视频，再按匹配分、质量分和时间分生成点击。
5. 点击后通过行为漏斗生成观看时长、完播、点赞、收藏、投币、分享。

**为什么这样做：**

完全随机行为无法支持 F3-F7。引入兴趣向量、视频主题和质量分后，用户与视频之间会形成稳定的群体结构，从而让相似用户、推荐和聚类有可解释结果。

## F3 相似用户分析

**推荐算法：** 基于用户兴趣向量的余弦相似度。

**核心结构：**

- 稀疏兴趣向量：`vector<pair<int, double>>` 或定长 `vector<double>`。
- 用户倒排索引：`topic/tag -> users`。
- 小顶堆：保留 Top-K 相似用户。

**算法步骤：**

1. 根据用户历史行为计算兴趣向量。
2. 对观看比例、点赞、收藏、投币、分享赋予不同权重。
3. 使用倒排索引找到候选相似用户，而不是扫描全部用户。
4. 计算目标用户与候选用户的余弦相似度。
5. 用小顶堆维护 Top-K。

**兴趣增量建议：**

```text
feedback_score =
    1.0 * watch_ratio
  + 1.0 * is_finish
  + 2.0 * is_like
  + 3.0 * is_favorite
  + 3.0 * is_coin
  + 2.5 * is_share
```

这让强反馈行为比普通点击更能代表兴趣。

## F4 视频推荐

**推荐算法：** 可解释混合推荐。

**候选来源：**

- 用户历史高分标签对应的视频。
- 相似用户喜欢但目标用户未看过的视频。
- 与用户主兴趣主题相同的视频。
- 少量近期热门或高质量视频，用于保持新鲜度。

**排序分数：**

```text
recommend_score =
    0.45 * interest_match
  + 0.25 * collaborative_score
  + 0.20 * quality_score
  + 0.10 * freshness_score   冷启动
```

**核心结构：**

- 标签倒排索引召回候选。
- 用户历史集合过滤已看视频。
- Top-N 小根堆维护推荐列表。

**为什么适合课程项目：**

该方案不依赖黑箱模型，每个分数都可以解释，并且能展示倒排索引、集合过滤、相似度和堆排序。

## F5 热度预测

**推荐算法：** 时间窗口统计 + 指数平滑 + 趋势修正。

**核心结构：**

- `vector<int> dailyViews`：按天统计观看量。
- 滑动窗口队列：维护最近 W 天观看量。
- 时间序列数组：保存历史和预测值。

**算法步骤：**

1. 从 `videoEvents` 中取出某视频所有行为。
2. 按天或按小时分桶统计观看量。
3. 使用最近窗口计算移动平均。
4. 使用指数平滑更新下一期预测。
5. 加入最近趋势项，判断上升、平稳或下降。

**预测公式：**

```text
smooth[t] = alpha * views[t] + (1 - alpha) * smooth[t - 1]
trend = average(last_half_window) - average(first_half_window)
pred[t + 1] = max(0, smooth[t] + beta * trend)
```

**为什么这样做：**

它比复杂时间序列模型更适合课程设计，因为数组、队列、滑动窗口和递推公式都容易解释。

## F6 视频聚类

**推荐算法：** 自实现 KMeans，输入为视频受众向量或主题向量。

**视频向量可选：**

- 内容向量：由分类和标签生成。
- 受众向量：统计观看该视频的用户兴趣群体分布。
- 混合向量：内容向量和受众向量加权拼接。

**核心结构：**

- `vector<vector<double>> videoVectors`
- `vector<vector<double>> centers`
- `vector<int> clusterId`

**算法步骤：**

1. 初始化 K 个中心。
2. 对每个视频计算到各中心的距离。
3. 分配到最近中心。
4. 重新计算每个簇中心。
5. 重复直到迭代次数达到上限或中心变化很小。

**解释重点：**

如果两个视频被相似用户群体观看，它们的受众向量接近，因此会聚到同一簇。

## F7 用户聚类

**推荐算法：** 自实现 KMeans，输入为用户兴趣向量。

**核心结构：**

- `vector<vector<double>> userVectors`
- `vector<vector<double>> centers`
- `vector<int> clusterId`
- `unordered_map<int, vector<int>> clusterToUsers`

**算法步骤：**

1. 使用用户兴趣向量作为聚类输入。
2. 初始化 M 个中心，M 可以与模拟兴趣群体数接近。
3. 反复执行“分配用户到最近中心”和“更新中心”。
4. 输出每个簇的人数、主兴趣主题和代表用户。

**验证方式：**

由于模拟数据中保存了 `group_id`，可以比较聚类结果和真实兴趣群体是否大致一致。这能证明行为模拟不是随机噪声。

## 答辩表达模板

每个功能按下面顺序讲：

1. 输入是什么。
2. 输出是什么。
3. 使用了什么数据结构。
4. 算法步骤是什么。
5. 时间复杂度和空间复杂度是什么。
6. 为什么没有直接调用库。
7. 为什么这个算法适合课程项目。
