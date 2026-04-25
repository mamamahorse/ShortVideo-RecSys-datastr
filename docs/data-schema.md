# 数据结构与数据表设计

本文说明项目中核心 CSV 文件、内存结构和后续 F3-F7 分析之间的关系。当前数据流分为两层：

- F1 数据构建：读取原始视频元数据，输出 `data/processed/videos.csv`。
- F2 行为模拟：读取清洗后的视频表，输出 `data/simulated/users.csv` 和 `data/simulated/events.csv`。

三张核心表可以简单理解为：

```text
videos.csv：有哪些视频、视频属于什么主题、质量如何。
users.csv：有哪些用户、用户偏好什么主题、活跃度如何。
events.csv：哪个用户在什么时间看了哪个视频、看了多久、有没有点赞收藏投币分享。
```

## 原始视频表 `raw_videos`

当前原始文件位于：

```text
data/raw/bilibili_10w_pro.csv
```

原始文件来自 B 站视频元数据，是 F1 数据构建的输入。代码中会把这些字段读入 `RawVideo` 结构，再清洗成 `ModeledVideo`。

| 原字段 | 建议类型 | 说明 | 为什么需要 |
| --- | --- | --- | --- |
| `aid` | long long | 视频数字 ID。 | 作为项目内的 `video_id` 主键，后续行为表通过它关联视频。 |
| `bvid` | string | B 站视频 BV 号。 | 用于展示、排查和回到原始视频页面，算法主要不依赖它。 |
| `title` | string | 视频标题。 | 标题可能包含主题关键词，可辅助生成 `topic_vector`。 |
| `category` | string | 视频分区。 | 分类比标签更稳定，适合作为视频主题的主信号。 |
| `author` | string | 作者名。 | 当前核心流程暂不使用，可供后续作者聚合或展示。 |
| `duration` | int | 视频时长，单位秒。 | 用于生成观看秒数、观看比例和完播判断。 |
| `pubdate` | long long | 发布时间，Unix 时间戳。 | 行为时间必须晚于发布时间，也支撑 F5 热度预测。 |
| `view_count` | long long | 原始播放量。 | 用于计算 `quality_score` 中的播放量部分。 |
| `favorite` | long long | 原始收藏数。 | 收藏是较强正反馈，用于计算质量分和模拟互动概率。 |
| `coin` | long long | 原始投币数。 | 投币是更强认可行为，用于计算质量分和模拟互动概率。 |
| `share` | long long | 原始分享数。 | 分享代表传播能力，用于计算质量分和模拟分享概率。 |
| `like` | long long | 原始点赞数。 | 点赞代表基础正反馈，用于计算质量分和模拟点赞概率。 |
| `tag` | string | 原始标签字段。 | 拆分后映射为 `tag_ids`，用于主题修正和倒排索引。 |

## 规范化视频表 `data/processed/videos.csv`

这是 F1 的输出文件，由 `src/data_builder/build_video_catalog.cpp` 生成。它不是原始视频表，而是后续模拟和分析更容易使用的规范化视频表。

当前表头为：

```text
video_id,bvid,title,category_id,category_name,tag_ids,duration_sec,publish_ts,raw_view_count,raw_like,raw_favorite,raw_coin,raw_share,quality_score,topic_id,topic_vector
```

| 字段 | 类型 | 说明 | 为什么要设置 |
| --- | --- | --- | --- |
| `video_id` | long long | 视频数字 ID，来自原始 `aid`。 | 作为视频主键，`events.csv` 通过它关联视频。使用 `long long` 是因为 B 站数字 ID 可能较大。 |
| `bvid` | string | B 站 BV 号。 | 保留可读、可追溯的视频标识，方便展示和人工检查。 |
| `title` | string | 视频标题。 | 既方便展示，也能在清洗阶段辅助主题识别。 |
| `category_id` | int | 分类字符串映射后的整数编号。 | 字符串不适合频繁计算，转成整数后可作为数组、哈希表或向量维度的索引。 |
| `category_name` | string | 原始分类名。 | 保留可读分类，便于报告、答辩和结果解释。 |
| `tag_ids` | string | 标签编号集合，当前 CSV 中用 `;` 分隔。 | 标签是推荐召回的重要依据，编号后可建立 `tagToVideos` 倒排索引。 |
| `duration_sec` | int | 视频时长，单位秒。 | 用于生成 `watch_sec`，并计算 `watch_ratio` 和 `is_finish`。 |
| `publish_ts` | long long | 发布时间，Unix 时间戳。 | 保证行为时间不早于视频发布时间，并支持按时间窗口做热度预测。 |
| `raw_view_count` | long long | 原始播放量。 | 作为视频质量分和基础曝光倾向的重要来源。 |
| `raw_like` | long long | 原始点赞数。 | 用于计算点赞率，也影响模拟点赞概率。 |
| `raw_favorite` | long long | 原始收藏数。 | 收藏比点赞更能体现内容价值，因此参与质量分和反馈概率。 |
| `raw_coin` | long long | 原始投币数。 | 投币是高强度正反馈，用于体现视频被认可程度。 |
| `raw_share` | long long | 原始分享数。 | 分享代表传播意愿和扩散能力。 |
| `quality_score` | double | 归一化后的视频质量分，范围约为 0 到 1。 | 用来控制基础曝光，质量高的视频在模拟推荐时更容易被选中。 |
| `topic_id` | int | 视频主主题编号。 | 方便把视频放入主题候选池，避免每次模拟都扫描全量视频。 |
| `topic_vector` | string | 视频主题向量，当前 CSV 中用 `;` 分隔多个浮点数。 | 用于和用户兴趣向量做点积，得到 `match_score`；也可作为 F6 视频聚类输入。 |

### 视频字段设计原因

`category_id`、`tag_ids`、`topic_id` 和 `topic_vector` 的作用不同：

| 字段 | 主要解决的问题 |
| --- | --- |
| `category_id` | 把稳定但粗粒度的分类转成整数，便于计算和存储。 |
| `tag_ids` | 保存细粒度标签，支持标签倒排索引和候选召回。 |
| `topic_id` | 保存主主题，便于快速把视频放入主题池。 |
| `topic_vector` | 保存多主题权重，支持兴趣匹配、相似度计算和聚类。 |

`quality_score` 的计算思想是综合播放量和互动率：

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

加入 `smooth` 是为了避免低播放视频因为偶然几个互动而得到虚高比例。使用 `log(1 + view_count)` 是为了降低头部视频播放量过大的压制效应。

## 模拟用户表 `data/simulated/users.csv`

这是 F2 的用户画像输出。它描述“用户是谁、属于哪个兴趣群体、主要偏好什么、活跃度如何”。

当前表头为：

```text
user_id,group_id,primary_topic,secondary_topic,activity_level,planned_events
```

| 字段 | 类型 | 说明 | 为什么要设置 |
| --- | --- | --- | --- |
| `user_id` | int | 用户唯一编号。 | 作为用户主键，`events.csv` 通过它关联用户。 |
| `group_id` | int | 模拟时分配的兴趣群体。 | 作为用户聚类的“模拟真值”，便于后续验证 F7 的聚类效果。 |
| `primary_topic` | int | 用户最偏好的主题。 | 让用户行为具有稳定主兴趣，避免行为完全随机。 |
| `secondary_topic` | int | 用户次偏好的主题。 | 让用户兴趣更接近真实情况，不局限于单一主题。 |
| `activity_level` | double | 用户活跃度。 | 用于描述用户行为强度，高活跃用户通常产生更多行为。 |
| `planned_events` | int | 计划为该用户生成的行为数量。 | 直接控制该用户在 `events.csv` 中贡献多少条行为，形成长尾活跃度分布。 |

### 用户兴趣向量说明

代码内部会为每个用户生成 `interest_vector`，但当前 `users.csv` 没有直接落盘该字段。当前 CSV 保留 `primary_topic` 和 `secondary_topic`，它们足以解释用户主兴趣和次兴趣；内存中的 `interest_vector` 用于模拟阶段计算用户和视频的匹配分。

用户兴趣向量的基本设计是：

```text
primary_topic: 0.55 - 0.75
secondary_topic: 0.15 - 0.30
other_topics: share remaining weight
```

这样做的原因是：用户既有明确偏好，又不会机械地只看一种视频。后续如果 F3/F7 需要直接从 CSV 复现完整兴趣向量，可以增加 `interest_vector` 字段，或根据行为日志重新统计用户兴趣。

### 活跃度设计原因

真实平台中，少量高活跃用户会贡献大量行为，大量普通用户只产生少量行为。因此模拟时采用长尾分布：

| 用户类型 | 占比 | 行为数范围 |
| --- | --- | --- |
| 低活用户 | 约 70% | 20-80 |
| 中活用户 | 约 25% | 80-250 |
| 高活用户 | 约 5% | 250-1000 |

如果每个用户行为数都相同，F3 相似用户和 F7 用户聚类会显得过于理想化，不像真实平台行为。

## 模拟行为表 `data/simulated/events.csv`

这是 F2 的核心输出。每一行表示一次用户对视频的点击观看及后续反馈。

当前表头为：

```text
event_id,user_id,video_id,timestamp,match_score,watch_sec,watch_ratio,is_finish,is_like,is_favorite,is_coin,is_share,feedback_score
```

| 字段 | 类型 | 说明 | 为什么要设置 |
| --- | --- | --- | --- |
| `event_id` | long long | 行为唯一编号。 | 作为行为日志主键，方便定位和排序。 |
| `user_id` | int | 产生该行为的用户编号。 | 关联 `users.csv`，用于分析用户历史和相似用户。 |
| `video_id` | long long | 被观看的视频编号。 | 关联 `videos.csv`，用于分析视频热度、受众和聚类。 |
| `timestamp` | long long | 行为发生时间，Unix 时间戳。 | 支持 F5 按天或小时统计热度，并要求 `timestamp >= publish_ts`。 |
| `match_score` | double | 用户兴趣向量和视频主题向量的匹配分。 | 解释用户为什么会看这个视频，也可作为推荐排序特征。 |
| `watch_sec` | int | 实际观看秒数。 | 表示用户真实观看时长，用于完播、满意度和热度分析。 |
| `watch_ratio` | double | 观看比例，通常为 `watch_sec / duration_sec`。 | 不同视频时长不同，比例比绝对秒数更适合比较兴趣强弱。 |
| `is_finish` | int | 是否完播，1 表示完播，0 表示未完播。 | 完播是强兴趣信号，可用于推荐和兴趣更新。 |
| `is_like` | int | 是否点赞。 | 点赞代表明确正反馈。 |
| `is_favorite` | int | 是否收藏。 | 收藏比点赞更强，表示用户认为内容有长期价值。 |
| `is_coin` | int | 是否投币。 | 投币是更高强度认可行为。 |
| `is_share` | int | 是否分享。 | 分享表示传播意愿，可用于判断视频扩散能力。 |
| `feedback_score` | double | 综合反馈分。 | 把观看、完播、点赞、收藏、投币、分享合成一个数，便于 F3/F4 使用。 |

### 行为漏斗设计原因

行为不是随机生成的，而是按漏斗关系产生：

```text
点击 -> 观看 -> 完播/点赞/收藏/投币/分享
```

强反馈行为依赖观看比例。观看比例较低时，点赞、收藏、投币、分享概率很低；观看比例较高时，强反馈概率才明显提高。这样可以避免出现“用户几乎没看却大量投币收藏”的不合理数据。

`feedback_score` 的推荐公式是：

```text
feedback_score =
    1.0 * watch_ratio
  + 1.0 * is_finish
  + 2.0 * is_like
  + 3.0 * is_favorite
  + 3.0 * is_coin
  + 2.5 * is_share
```

这样设计的原因是：观看比例是基础兴趣，完播是更强兴趣，点赞是正反馈，收藏、投币、分享代表更高强度的认可或传播意愿。

## 三张表之间的关系

核心外键关系如下：

```text
users.user_id   -> events.user_id
videos.video_id -> events.video_id
```

也就是说：

```text
用户表说明“这个人是谁、喜欢什么”。
视频表说明“这个视频是什么、属于什么主题、质量如何”。
行为表说明“这个人在什么时候看了什么视频、反馈如何”。
```

## 分析中间结构

这些结构不一定全部直接落盘，但它们是后续 F3-F7 的核心数据结构。

| 结构 | 来源 | 用途 |
| --- | --- | --- |
| `vector<Video>` | `videos.csv` | 连续存储视频，便于遍历、随机采样和向量计算。 |
| `unordered_map<long long, int> videoIdToIndex` | `videos.csv` | 从 `video_id` 快速定位 `vector<Video>` 下标。 |
| `unordered_map<string, int> categoryToId` | 视频清洗阶段 | 把分类名压缩成整数编号。 |
| `unordered_map<string, int> tagToId` | 视频清洗阶段 | 把标签名压缩成整数编号。 |
| `unordered_map<int, vector<int>> tagToVideos` | `tag_ids` | 标签倒排索引，用于推荐候选召回。 |
| `vector<vector<int>> topicToVideos` | `topic_id` | 主题到视频候选池，避免模拟时扫描全量视频。 |
| `vector<User>` | `users.csv` | 连续存储用户画像。 |
| `vector<Event>` | `events.csv` | 行为日志主表。 |
| `unordered_map<int, vector<int>> userEvents` | `events.csv` | 用户 ID 到行为下标列表，用于 F3/F4/F7。 |
| `unordered_map<long long, vector<int>> videoEvents` | `events.csv` | 视频 ID 到行为下标列表，用于 F5/F6。 |
| `vector<vector<double>> userInterestMatrix` | `users.csv` 或 `events.csv` | 用户兴趣矩阵，用于相似用户和用户聚类。 |
| `vector<vector<double>> videoAudienceMatrix` | `events.csv` | 视频受众向量，用于视频聚类。 |
| `priority_queue` | 推荐、相似度、热度模块 | 维护 Top-K 相似用户、Top-N 推荐视频或热门视频。 |

## 对 F3-F7 的支撑关系

| 功能 | 主要使用的数据 | 说明 |
| --- | --- | --- |
| F3 相似用户 | `users.csv`、`events.csv` | 根据用户兴趣主题和行为反馈计算用户相似度。 |
| F4 视频推荐 | `videos.csv`、`events.csv` | 用主题匹配、标签倒排、视频质量分和历史反馈生成推荐。 |
| F5 热度预测 | `events.csv.timestamp`、`events.csv.video_id` | 按时间窗口统计视频观看量，预测未来趋势。 |
| F6 视频聚类 | `videos.csv.topic_vector`、`events.csv` | 根据内容主题或观看用户群体聚类视频。 |
| F7 用户聚类 | `users.csv`、`events.csv` | 根据用户兴趣主题和行为偏好聚类用户。 |

## 输出文件

| 文件 | 内容 | 当前状态 |
| --- | --- | --- |
| `data/processed/videos.csv` | F1 规范化后的视频表。 | 已由数据清洗模块生成。 |
| `data/simulated/users.csv` | F2 模拟用户画像。 | 已由行为模拟模块生成。 |
| `data/simulated/events.csv` | F2 模拟行为日志。 | 已由行为模拟模块生成。 |
| `data/outputs/recommendations.csv` | F4 推荐结果。 | 后续生成。 |
| `data/outputs/user_clusters.csv` | F7 用户聚类结果。 | 后续生成。 |
| `data/outputs/video_clusters.csv` | F6 视频聚类结果。 | 后续生成。 |
| `data/outputs/popularity_prediction.csv` | F5 热度预测结果。 | 后续生成。 |
