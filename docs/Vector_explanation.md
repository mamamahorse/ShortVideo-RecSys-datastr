# 向量生成逻辑说明

本文说明项目当前新增的三类向量：

```text
UserInterestVector
VideoAudienceVector
VideoFeatureVector
```

其中 `VideoTopicVector` 已经存在于 `data/processed/videos.csv` 的 `topic_vector` 字段中，不需要重复生成。

## 1. 已有向量：VideoTopicVector

`VideoTopicVector` 来源于：

```text
data/processed/videos.csv.topic_vector
```

它是一个 12 维向量，表示视频属于 12 个主题的程度。例如：

```text
1.000000;0.000000;0.000000;...
```

表示这个视频主要属于第 0 个主题。

这个向量在 F1 视频清洗阶段已经生成，生成时综合了：

- `category_name`
- `tag_ids` 对应的原始标签
- `title`

为什么要保留它：

- 它把不同视频统一映射到 12 维主题空间。
- 用户兴趣向量可以用同一套主题维度表示。
- 后续 F3、F4、F6、F7 都可以直接基于 12 维主题做匹配、推荐和聚类。

## 2. UserInterestVector

### 输入

```text
data/simulated/users.csv
data/simulated/events.csv
data/processed/videos.csv
```

### 输出

```text
data/outputs/user_interest_vectors.csv
```

字段为：

```text
user_id,group_id,event_count,interest_vector
```

### 生成逻辑

每个用户生成一个 12 维兴趣向量。初始时所有维度都是 0。

遍历行为日志 `events.csv`，对每一条行为：

```text
user_interest[topic] += event.feedback_score * video.topic_vector[topic]
```

也就是说：

- 用户看了某个视频，就把这个视频的主题权重加到用户兴趣里。
- 用户反馈越强，增加得越多。
- `feedback_score` 已经综合了观看比例、完播、点赞、收藏、投币、分享。

最后对每个用户的兴趣向量做按总和归一化，使 12 个维度之和约为 1。

### 为什么要这么做

`users.csv` 里只有 `primary_topic` 和 `secondary_topic`，它们是模拟时生成的用户画像。真正做 F3/F4/F7 时，更应该从用户实际行为中反推兴趣。

这样做改变了什么：

- 原来只能说“这个用户被模拟为喜欢 topic 3”。
- 现在可以说“这个用户实际高反馈行为主要集中在 topic 3”。

这个向量用于：

- F3 相似用户分析
- F4 个性化推荐
- F7 用户聚类

## 3. VideoAudienceVector

### 输入

```text
data/simulated/users.csv
data/simulated/events.csv
```

### 输出

```text
data/outputs/video_audience_vectors.csv
```

字段为：

```text
video_id,event_count,has_audience,audience_vector
```

### 生成逻辑

每个视频生成一个 12 维受众向量。这里的 12 维对应 12 个用户兴趣群体 `group_id`。

遍历行为日志 `events.csv`，对每一条行为：

```text
video_audience[user.group_id] += event.feedback_score
```

也就是说：

- 如果 group 2 的用户对某视频有强反馈，这个视频的第 2 维就增加。
- 如果多个 group 都喜欢同一个视频，它的受众向量会呈现多群体分布。

最后对每个视频的受众向量做按总和归一化，使 12 个维度之和约为 1。

### has_audience 判断

当前使用：

```text
audience_min_events = 10
```

如果一个视频的行为数不少于 10：

```text
has_audience = 1
```

否则：

```text
has_audience = 0
```

为什么要设置门槛：

- 行为太少时，受众分布容易被偶然行为影响。
- 至少 10 条行为后，受众向量才更适合参与视频聚类和相似视频推荐。

这个向量用于：

- F6 视频聚类
- 相似视频推荐
- 协同过滤类推荐信号

## 4. VideoFeatureVector

### 输入

```text
data/processed/videos.csv
data/outputs/video_audience_vectors.csv
```

### 输出

```text
data/outputs/video_feature_vectors.csv
```

字段为：

```text
video_id,topic_id,event_count,has_audience,feature_vector
```

### 生成逻辑

`VideoFeatureVector` 是后续 F4/F6 主要使用的统一视频特征向量。

它的维度为：

```text
12 维 topic
+ 12 维 audience
+ 1 维 quality_score
+ 1 维 duration_score
= 26 维
```

其中：

- 前 12 维来自 `topic_vector`。
- 中间 12 维来自 `VideoAudienceVector`。
- 第 25 维来自 `quality_score`。
- 第 26 维来自归一化后的 `duration_sec`。

### 有 audience 的视频

如果：

```text
event_count >= 10
```

则使用以下权重：

```text
topic_weight = 0.60
audience_weight = 0.25
quality_weight = 0.10
duration_weight = 0.05
```

含义是：

- 主题仍然是主要依据。
- 受众行为作为重要补充。
- 质量分帮助推荐更可靠的视频。
- 时长只作为轻量辅助，避免长短视频完全混在一起。

### 没有 audience 的视频

如果：

```text
event_count < 10
```

则不使用 audience 部分：

```text
topic_weight = 0.85
audience_weight = 0.00
quality_weight = 0.10
duration_weight = 0.05
```

为什么要这样做：

- 没有足够行为的视频不能可靠判断受众。
- 但它仍然可以依靠内容主题、质量分和时长参与推荐与聚类。
- 这解决了冷启动视频无法使用行为向量的问题。

最后对整个 26 维 `feature_vector` 做 L2 归一化。

## 5. 为什么不在后续算法中反复计算 similarity

如果每次推荐或聚类都临时计算：

```text
topic_similarity
category_similarity
tag_similarity
audience_similarity
quality_similarity
duration_similarity
```

后续算法会反复做大量拆分、查表和加权计算。特别是全量视频约 12 万条，两两比较会非常慢。

现在的做法是预先生成 `VideoFeatureVector`：

```text
先把 topic、audience、quality、duration 编码成统一数值向量
后续只做点积或距离计算
```

因为 `VideoFeatureVector` 已经做了 L2 归一化，所以后续计算余弦相似度时可以直接做点积：

```text
cosine_similarity(a, b) = dot(a.feature_vector, b.feature_vector)
```

这样做改变了什么：

- 原来：每次算法运行都临时计算多个相似度函数。
- 现在：向量预计算一次，推荐和聚类只做统一向量运算。

这更适合后续实现：

- Top-K 相似视频
- F4 视频推荐
- F6 KMeans 视频聚类

## 6. category 和 tag 的位置

当前没有把 `category_id` 和 `tag_ids` 放入 26 维主特征向量。

原因是：

- 当前每个视频只有 1 个 `tag_id`，标签信号偏弱。
- `category_id` 和 `tag_ids` 更适合做候选召回索引，而不是放进主向量反复计算。

推荐后续做法：

```text
候选召回：
  同 topic_id 的视频
  同 category_id 的视频
  同 tag_id 的视频

候选排序：
  使用 VideoFeatureVector 点积
```

这样可以避免全量扫描所有视频，也能让推荐逻辑更清晰。

## 7. 当前生成结果

当前全量生成结果为：

```text
user_interest_vectors = 12000
video_audience_vectors = 119999
video_feature_vectors = 119999
videos_with_audience = 16929
audience_min_events = 10
```

说明：

- 所有用户都有 `UserInterestVector`。
- 所有视频都有 `VideoAudienceVector` 和 `VideoFeatureVector`。
- 其中 16929 个视频有足够行为数据，特征向量融合了 audience 部分。
- 其他视频仍然可以依靠 topic、quality、duration 参与冷启动推荐和聚类。
