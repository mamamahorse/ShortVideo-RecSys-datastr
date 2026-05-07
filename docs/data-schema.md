# 数据结构与数据表设计

本文说明项目中核心 CSV 文件、内存结构和后续 F3-F7 分析之间的关系。当前数据流分为三层：

- F1 数据构建：读取原始视频元数据，输出 `data/processed/videos.csv`。
- F2 行为模拟：读取清洗后的视频表，输出 `data/simulated/users.csv` 和 `data/simulated/events.csv`。
- 向量构建：读取 `videos.csv`、`users.csv`、`events.csv`，输出 `data/outputs` 下的用户兴趣向量、视频受众向量和统一视频特征向量。

前三张基础表可以简单理解为：

```text
videos.csv：有哪些视频、视频属于什么主题、质量如何。
users.csv：有哪些用户、用户偏好什么主题、活跃度如何。
events.csv：哪个用户在什么时间看了哪个视频、看了多久、有没有点赞收藏投币分享。
## 用户在模拟的时候被预先编入12个group，有观看记录的视频至少拥有十条观看记录。
```

在这三张基础表之上，当前又新增了三张向量表：

```text
user_interest_vectors.csv：每个用户对 12 个主题的实际兴趣分布。
video_audience_vectors.csv：每个视频被 12 个用户群体喜欢的程度。
video_feature_vectors.csv：用于后续推荐和聚类的统一视频特征向量。
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
| `category_name` | string | 原始分类名。 
| `tag_ids` | string | 标签编号集合，当前 CSV 中用 `;` 分隔。 | 标签是推荐召回的重要依据，编号后可建立 `tagToVideos` 倒排索引。 |
| `duration_sec` | int | 视频时长，单位秒。 | 用于生成 `watch_sec`，并计算 `watch_ratio` 和 `is_finish`。 |
| `publish_ts` | long long | 发布时间，Unix 时间戳。 | 保证行为时间不早于视频发布时间，并支持按时间窗口做热度预测。 |
| `raw_view_count` | long long | 原始播放量。 
| `raw_like` | long long | 原始点赞数。 
| `raw_favorite` | long long | 原始收藏数。
| `raw_coin` | long long | 原始投币数。 
| `raw_share` | long long | 原始分享数。 
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
| `group_id` | int | 模拟时分配的兴趣群体。 | 作为用户聚类的“模拟真值”，便于后续验证 F7 的聚类效果, ****注意group_id和primary_topic是同一个**** |
| `primary_topic` | int | 用户最偏好的主题。 | 让用户行为具有稳定主兴趣，避免行为完全随机。 |
| `secondary_topic` | int | 用户次偏好的主题。 | 让用户兴趣更接近真实情况，不局限于单一主题。 |
| `activity_level` | double | 用户活跃度。 | 用于描述用户行为强度，高活跃用户通常产生更多行为。 |
| `planned_events` | int | 计划为该用户生成的行为数量。 | 直接控制该用户在 `events.csv` 中贡献多少条行为，形成长尾活跃度分布。 |

### 用户兴趣向量说明


现在项目把“基于真实行为反推的用户兴趣向量”生成为：

```text
data/outputs/user_interest_vectors.csv
```

当前表头为：

```text
user_id,group_id,event_count,interest_vector
```

其中 `interest_vector` 是一个 12 维向量，按下面的方式生成：

```text
user_interest[topic] += event.feedback_score * video.topic_vector[topic]
```

最后对 12 个维度按总和归一化，使其更适合做 F3 相似用户分析、F4 个性化推荐和 F7 用户聚类。

用户兴趣向量的基本设计是：

```text
primary_topic: 0.55 - 0.75 
secondary_topic: 0.15 - 0.30
other_topics: share remaining weight（均分）
```

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


## 视频受众向量与统一特征向量

在 `videos.csv` 和 `events.csv` 的基础上，当前项目又生成了两类和视频相关的向量。

### 视频受众向量 `data/outputs/video_audience_vectors.csv`

当前表头为：

```text
video_id,event_count,has_audience,audience_vector
```

其中：

- `event_count` 表示该视频在 `events.csv` 中出现了多少次。
- `has_audience` 表示该视频是否有足够行为样本来可靠使用受众向量。
- `audience_vector` 是一个 12 维向量，12 个维度对应 12 个 `group_id`。

它的生成方式是：

```text
video_audience[user.group_id] += event.feedback_score
```

最后按总和归一化，使其表示“这个视频主要被哪类用户喜欢”。

当前使用的门槛是：

```text
event_count >= 10 -> has_audience = 1
event_count < 10  -> has_audience = 0
```

这样做的原因是：行为太少时，受众结构容易被偶然点击污染，不适合直接参与 F6 视频聚类或相似视频推荐。

### 统一视频特征向量 `data/outputs/video_feature_vectors.csv`

当前表头为：

```text
video_id,topic_id,event_count,has_audience,feature_vector
```

`feature_vector` 是一个 26 维向量，由以下部分组成：

```text
12 维 topic_vector
12 维 audience_vector
1 维 quality_score
1 维 duration_score
```

其中：

- 前 12 维是视频内容主题。
- 中间 12 维是视频受众分布。
- 倒数第 2 维是 `quality_score`。
- 倒数第 1 维是对 `duration_sec` 做归一化后的时长特征。

当前使用两套权重：

```text
有足够行为时：
topic_weight = 0.60
audience_weight = 0.25
quality_weight = 0.10
duration_weight = 0.05

没有足够行为时：
topic_weight = 0.85
audience_weight = 0.00
quality_weight = 0.10
duration_weight = 0.05
```

最后整个 `feature_vector` 会做 L2 归一化。这样后续做余弦相似度时，可以直接用点积，减少每次推荐和聚类时重复计算多种 similarity 函数的开销。  
**注意category和tag没有直接加入向量中，topic本身就考虑了category，tag，title。** 后续的倒排索引仍然可以使用category和tag。

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

这些结构不一定全部直接落盘，但它们是后续 F3-F7 的核心数据结构。当前代码已经提供了三组可复用接口：视频目录构建接口、行为模拟接口、向量构建接口。后续实现 F3-F7 时，应优先复用这些结构，而不是重新解析同一批 CSV 字段。

| 结构 | 来源 | 用途 |
| --- | --- | --- |
| `RawVideo` | `data/raw/bilibili_10w_pro.csv` | 原始视频记录，保留 `aid`、`title`、`category`、`tag`、播放量和互动数，是 F1 清洗入口。 |
| `ModeledVideo` | `data/processed/videos.csv` | 规范化视频记录，保存 `category_id`、`tag_ids`、`quality_score`、`topic_id`、`topic_vector`，是 F4/F5/F6 的基础视频对象。 |
| `Catalog` | `load_processed_videos()` 或 `build_catalog()` | 视频目录对象，内部包含 `vector<ModeledVideo>`、`topic_to_video_indices`、`category_to_id`、`tag_to_id`。 |
| `vector<ModeledVideo>` | `Catalog.videos` | 连续存储全量视频，便于遍历、向量构建、热度统计和聚类输入构造。 |
| `vector<vector<int>> topic_to_video_indices` | `Catalog.topic_to_video_indices` | 主题到视频下标列表，可直接作为 F4 推荐的主题候选池，也可减少相似视频召回时的全量扫描。 |
| `unordered_map<string, int> category_to_id` | `Catalog.category_to_id` | 把分类名压缩成整数编号，后续可扩展为 `category_id -> video_ids` 候选召回索引。 |
| `unordered_map<string, int> tag_to_id` | `Catalog.tag_to_id` | 把标签名压缩成整数编号，后续可扩展为 `tag_id -> video_ids` 倒排索引。 |
| `UserProfile` | `generate_users()` 或 `users.csv` | 模拟用户画像，保存 `group_id`、`primary_topic`、`secondary_topic`、`activity_level` 和内存态 `interest_vector`。 |
| `Event` | `simulate_events()` 或 `events.csv` | 行为日志对象，保存 `user_id`、`video_id`、`timestamp`、观看比例、强反馈和 `feedback_score`。 |
| `unordered_map<int, vector<int>> userEvents` | 可由 `events.csv` 或 `vector<Event>` 构建 | 用户 ID 到行为下标列表，用于 F3 相似用户、F4 个性化推荐和 F7 用户聚类。 |
| `unordered_map<long long, vector<int>> videoEvents` | 可由 `events.csv` 或 `vector<Event>` 构建 | 视频 ID 到行为下标列表，用于 F5 热度预测和 F6 视频受众统计。 |
| `UserInterestVector` | `build_vectors()` 或 `user_interest_vectors.csv` | 用户 12 维兴趣向量，由 `feedback_score * topic_vector` 累积得到，是 F3/F4/F7 的主要输入。 |
| `VideoAudienceVector` | `build_vectors()` 或 `video_audience_vectors.csv` | 视频 12 维受众向量，由用户 `group_id` 上的反馈分布构成，是 F6 视频聚类和相似视频推荐的行为侧输入。 |
| `VideoFeatureVector` | `build_vectors()` 或 `video_feature_vectors.csv` | 26 维统一视频特征，融合主题、受众、质量分和时长，已做 L2 归一化，可用于 F4 排序和 F6 聚类。 |
| `VectorBuildResult` | `build_vectors()` | 一次性返回用户兴趣向量、视频受众向量和统一视频特征向量，适合作为 F3-F7 的共享预处理结果。 |
| `priority_queue` | 推荐、相似度、热度模块 | 维护 Top-K 相似用户、Top-N 推荐视频或热门视频。 |

当前可直接复用的关键接口如下：

| 接口 | 所在文件 | 输入 | 输出 | 对 F3-F7 的帮助 |
| --- | --- | --- | --- | --- |
| `split_tags(raw_tags)` | `src/data_builder/video_cleaner.hpp` | 原始标签字符串 | 标签字符串列表 | 支持 F4 建立标签倒排索引。 |
| `load_raw_videos(csv_path, max_rows)` | `src/data_builder/video_cleaner.hpp` | 原始视频 CSV | `vector<RawVideo>` | 支持重新生成 F1 视频目录。 |
| `build_catalog(raw_videos, config)` | `src/data_builder/video_cleaner.hpp` | 原始视频列表 | `Catalog` | 生成 `topic_vector`、`topic_id`、`tag_ids`、`quality_score`，是 F4/F6 的视频基础特征来源。 |
| `load_processed_videos(csv_path, config)` | `src/data_builder/video_cleaner.hpp` | `data/processed/videos.csv` | `Catalog` | 后续 F3-F7 推荐优先使用该接口读取已清洗视频，避免重复处理原始 CSV。 |
| `write_videos_csv(path, catalog)` | `src/data_builder/video_cleaner.hpp` | `Catalog` | `videos.csv` | 支持持久化 F1 清洗结果。 |
| `generate_users(config)` | `src/simulator/behavior_simulator.hpp` | `SimulationConfig` | `vector<UserProfile>` | 生成有 `group_id` 和兴趣向量的用户画像，可用于 F7 聚类验证。 |
| `simulate_events(catalog, users, config)` | `src/simulator/behavior_simulator.hpp` | 视频目录、用户画像、模拟配置 | `vector<Event>` | 生成带 `feedback_score` 的行为日志，是 F3/F4/F5/F6/F7 的行为数据来源。 |
| `write_users_csv(path, users)` | `src/simulator/behavior_simulator.hpp` | `vector<UserProfile>` | `users.csv` | 持久化用户画像和聚类真值 `group_id`。 |
| `write_events_csv(path, events)` | `src/simulator/behavior_simulator.hpp` | `vector<Event>` | `events.csv` | 持久化行为日志，供 F3-F7 重复使用。 |
| `load_vector_users_csv(path)` | `src/common/vector_builder.hpp` | `users.csv` | `vector<VectorUser>` | 读取 `user_id` 与 `group_id`，用于构建受众向量和聚类验证。 |
| `load_vector_events_csv(path)` | `src/common/vector_builder.hpp` | `events.csv` | `vector<VectorEvent>` | 读取 `user_id`、`video_id`、`feedback_score`，用于兴趣和受众向量构建。 |
| `build_vectors(catalog, users, events, config)` | `src/common/vector_builder.hpp` | `Catalog`、用户、行为、向量配置 | `VectorBuildResult` | 统一生成 `UserInterestVector`、`VideoAudienceVector`、`VideoFeatureVector`，是 F3/F4/F6/F7 的核心预处理接口。 |
| `write_user_interest_vectors_csv(path, vectors)` | `src/common/vector_builder.hpp` | 用户兴趣向量 | `user_interest_vectors.csv` | 为 F3/F4/F7 提供可复用的落盘输入。 |
| `write_video_audience_vectors_csv(path, vectors)` | `src/common/vector_builder.hpp` | 视频受众向量 | `video_audience_vectors.csv` | 为 F6 和相似视频推荐提供行为侧视频特征。 |
| `write_video_feature_vectors_csv(path, vectors)` | `src/common/vector_builder.hpp` | 视频统一特征向量 | `video_feature_vectors.csv` | 为 F4 推荐排序和 F6 KMeans 聚类提供预计算向量。 |

## 对 F3-F7 的支撑关系

| 功能 | 主要使用的数据 | 说明 |
| --- | --- | --- |
| F3 相似用户 | `user_interest_vectors.csv`、`events.csv` | 根据用户兴趣向量和行为反馈计算用户相似度。 |
| F4 视频推荐 | `videos.csv`、`user_interest_vectors.csv`、`video_feature_vectors.csv` | 用主题匹配、标签/分类召回、统一视频特征和历史反馈生成推荐。 |
| F5 热度预测 | `events.csv.timestamp`、`events.csv.video_id` | 按时间窗口统计视频观看量，预测未来趋势。 |
| F6 视频聚类 | `video_feature_vectors.csv`、`video_audience_vectors.csv` | 根据视频内容主题和观看用户群体做统一聚类。 |
| F7 用户聚类 | `user_interest_vectors.csv`、`events.csv` | 根据用户兴趣向量和行为偏好聚类用户。 |

## 输出文件

| 文件 | 内容 | 当前状态 |
| --- | --- | --- |
| `data/processed/videos.csv` | F1 规范化后的视频表。 | 已由数据清洗模块生成。 |
| `data/simulated/users.csv` | F2 模拟用户画像。 | 已由行为模拟模块生成。 |
| `data/simulated/events.csv` | F2 模拟行为日志。 | 已由行为模拟模块生成。 |
| `data/outputs/user_interest_vectors.csv` | 用户兴趣向量表。 | 已由向量构建模块生成。 |
| `data/outputs/video_audience_vectors.csv` | 视频受众向量表。 | 已由向量构建模块生成。 |
| `data/outputs/video_feature_vectors.csv` | 统一视频特征向量表。 | 已由向量构建模块生成。 |
| `data/outputs/recommendations.csv` | F4 推荐结果。 | 后续生成。 |
| `data/outputs/user_clusters.csv` | F7 用户聚类结果。 | 后续生成。 |
| `data/outputs/video_clusters.csv` | F6 视频聚类结果。 | 后续生成。 |
| `data/outputs/popularity_prediction.csv` | F5 热度预测结果。 | 后续生成。 |
