import streamlit as st
import pandas as pd
import numpy as np
import os
from pathlib import Path

st.set_page_config(page_title="短视频推荐系统", layout="wide")

PROJECT_ROOT = Path(__file__).parent

TOPIC_NAME_MAP = {
    0: "影视剪辑",
    1: "科技数码",
    2: "野生技能协会",
    3: "美食制作",
    4: "日常",
    5: "动画综合",
    6: "亲子",
    7: "出行",
    8: "音乐综合",
    9: "健身",
    10: "校园学习",
    11: "小剧场",
}

GROUP_NAME_MAP = {
    0: "影视剪辑用户",
    1: "科技数码用户",
    2: "野生技能用户",
    3: "美食制作用户",
    4: "日常分享用户",
    5: "动画综合用户",
    6: "亲子教育用户",
    7: "出行旅游用户",
    8: "音乐欣赏用户",
    9: "健身运动用户",
    10: "校园学习用户",
    11: "小剧场用户",
}

@st.cache_data
def load_user_interest_vectors():
    file_path = PROJECT_ROOT / "data" / "outputs" / "user_interest_vectors.csv"
    df = pd.read_csv(file_path)
    df['interest_vector'] = df['interest_vector'].apply(
        lambda x: np.array([float(v) for v in x.replace('"', '').split(';')])
    )
    return df

@st.cache_data
def load_video_feature_vectors():
    file_path = PROJECT_ROOT / "data" / "outputs" / "video_feature_vectors.csv"
    df = pd.read_csv(file_path)
    df['feature_vector'] = df['feature_vector'].apply(
        lambda x: np.array([float(v) for v in x.replace('"', '').split(';')])
    )
    return df

@st.cache_data
def load_users():
    file_path = PROJECT_ROOT / "data" / "simulated" / "users.csv"
    return pd.read_csv(file_path)

@st.cache_data
def load_videos():
    file_path = PROJECT_ROOT / "data" / "processed" / "videos.csv"
    return pd.read_csv(file_path)

@st.cache_data
def load_events():
    file_path = PROJECT_ROOT / "data" / "simulated" / "events.csv"
    return pd.read_csv(file_path)

@st.cache_data
def load_popularity_predictions():
    file_path = PROJECT_ROOT / "data" / "outputs" / "popularity_predictions.csv"
    df = pd.read_csv(file_path)
    df['daily_views_list'] = df['daily_views'].apply(
        lambda x: np.array([float(v) for v in str(x).split(';')])
    )
    df['smooth_values_list'] = df['smooth_values'].apply(
        lambda x: np.array([float(v) for v in str(x).split(';')])
    )
    df['predicted_views_list'] = df['predicted_views'].apply(
        lambda x: np.array([float(v) for v in str(x).split(';')])
    )
    return df

@st.cache_data
def load_video_title_map():
    videos_df = load_videos()
    return dict(zip(videos_df['video_id'], videos_df['title']))

def get_video_title(video_id, title_map):
    title = title_map.get(video_id, "未知")
    return title[:25] + "..." if len(title) > 25 else title

def cosine_similarity(a, b):
    a = np.array(a)
    b = np.array(b)
    norm_a = np.linalg.norm(a)
    norm_b = np.linalg.norm(b)
    if norm_a == 0 or norm_b == 0:
        return 0.0
    return np.dot(a, b) / (norm_a * norm_b)

def dot_product(a, b):
    a = np.array(a)
    b = np.array(b)
    return np.dot(a, b)

def find_similar_users(user_id, user_vectors_df, top_k=10):
    target_user = user_vectors_df[user_vectors_df['user_id'] == user_id]
    if target_user.empty:
        return None
    target_vector = target_user.iloc[0]['interest_vector']
    similarities = []
    for idx, row in user_vectors_df.iterrows():
        if row['user_id'] != user_id:
            sim = cosine_similarity(target_vector, row['interest_vector'])
            similarities.append((row['user_id'], sim, row['group_id'], row['event_count']))
    similarities.sort(key=lambda x: x[1], reverse=True)
    return similarities[:top_k]

def recommend_videos(user_id, user_vectors_df, video_vectors_df, users_df, top_n=20):
    target_user = user_vectors_df[user_vectors_df['user_id'] == user_id]
    if target_user.empty:
        return None
    target_vector = target_user.iloc[0]['interest_vector']
    user_info = users_df[users_df['user_id'] == user_id]
    if user_info.empty:
        return None
    user_group = user_info.iloc[0]['group_id']
    scores = []
    for idx, row in video_vectors_df.iterrows():
        video_vector = row['feature_vector']
        video_topic_vector = video_vector[:12]
        interest_score = dot_product(target_vector, video_topic_vector)
        group_bonus = 0.1 if row['topic_id'] == user_group else 0.0
        total_score = interest_score + group_bonus
        scores.append((row['video_id'], total_score, row['topic_id'], row['event_count']))
    scores.sort(key=lambda x: x[1], reverse=True)
    return scores[:top_n]

st.title("🎬 短视频推荐系统 - 数据可视化平台")

st.sidebar.title("功能导航")
page = st.sidebar.radio("选择功能", ["📊 数据概览", "👥 相似用户分析", "🎥 视频推荐", "📈 用户聚类分析", "🎯 视频聚类分析", "🔥 热度预测"])

if page == "📊 数据概览":
    st.header("📊 数据概览")
    
    col1, col2, col3 = st.columns(3)
    
    with col1:
        st.subheader("用户数据")
        users_df = load_users()
        st.metric("总用户数", len(users_df))
        st.caption(f"Group分布: {users_df['group_id'].nunique()} 个群体")
    
    with col2:
        st.subheader("视频数据")
        videos_df = load_videos()
        st.metric("总视频数", len(videos_df))
    
    with col3:
        st.subheader("行为数据")
        events_df = load_events()
        st.metric("总行为数", len(events_df))
    
    st.divider()
    
    col1, col2 = st.columns(2)
    
    with col1:
        st.subheader("用户兴趣向量示例")
        user_vectors_df = load_user_interest_vectors()
        st.dataframe(user_vectors_df.head(10), use_container_width=True)
    
    with col2:
        st.subheader("视频特征向量示例")
        video_vectors_df = load_video_feature_vectors()
        display_df = video_vectors_df.head(10).copy()
        display_df['feature_vector'] = display_df['feature_vector'].apply(lambda x: f"维度: {len(x)}")
        st.dataframe(display_df, use_container_width=True)
    
    st.divider()
    
    col1, col2 = st.columns(2)
    
    with col1:
        st.subheader("用户群体分布")
        group_counts = users_df['group_id'].value_counts().sort_index()
        group_counts.index = group_counts.index.map(lambda x: GROUP_NAME_MAP.get(x, f"群体{x}"))
        st.bar_chart(group_counts)
    
    with col2:
        st.subheader("视频Topic分布")
        video_vectors_df = load_video_feature_vectors()
        topic_counts = video_vectors_df['topic_id'].value_counts().sort_index()
        topic_counts.index = topic_counts.index.map(lambda x: TOPIC_NAME_MAP.get(x, f"主题{x}"))
        st.bar_chart(topic_counts)

elif page == "👥 相似用户分析":
    st.header("👥 相似用户分析")
    st.caption("基于用户兴趣向量的余弦相似度计算")
    
    user_vectors_df = load_user_interest_vectors()
    users_df = load_users()
    
    col1, col2 = st.columns([1, 3])
    
    with col1:
        user_options = users_df['user_id'].tolist()
        user_id = st.selectbox(
            "选择用户",
            options=user_options,
            key="select_user_similar"
        )
        top_k = st.slider("返回数量", min_value=5, max_value=50, value=10)
    
    with col2:
        current_user = users_df[users_df['user_id'] == user_id]
        if not current_user.empty:
            info = current_user.iloc[0]
            st.info(f"用户 {user_id}: {GROUP_NAME_MAP.get(info['group_id'])}")
        
        similar_users = find_similar_users(user_id, user_vectors_df, top_k)
        if similar_users is None:
            st.error(f"未找到用户ID {user_id}")
        else:
            result_df = pd.DataFrame(
                similar_users,
                columns=["用户ID", "相似度", "群体ID", "行为数"]
            )
            result_df["相似度"] = result_df["相似度"].apply(lambda x: f"{x:.4f}")
            result_df["群体"] = result_df["群体ID"].apply(lambda x: GROUP_NAME_MAP.get(x, f"群体{x}"))
            result_df = result_df[["用户ID", "相似度", "群体", "行为数"]]
            st.success(f"找到 {len(similar_users)} 个相似用户")
            st.dataframe(result_df, use_container_width=True)
    
    st.divider()
    
    st.subheader("用户向量可视化")
    target_user = user_vectors_df[user_vectors_df['user_id'] == user_id].iloc[0]
    vector = target_user['interest_vector']
    
    chart_data = pd.DataFrame({
        'Topic': [TOPIC_NAME_MAP.get(i, f"Topic {i}") for i in range(len(vector))],
        '兴趣值': vector
    })
    st.bar_chart(chart_data.set_index('Topic'))

elif page == "🎥 视频推荐":
    st.header("🎥 视频推荐")
    st.caption("基于用户兴趣向量和视频特征向量的混合推荐")

    user_vectors_df = load_user_interest_vectors()
    video_vectors_df = load_video_feature_vectors()
    users_df = load_users()
    video_title_map = load_video_title_map()

    col1, col2 = st.columns([1, 3])

    with col1:
        user_options = users_df['user_id'].tolist()
        user_id = st.selectbox(
            "选择用户",
            options=user_options,
            key="select_user_recommend"
        )
        top_n = st.slider("推荐数量", min_value=5, max_value=50, value=20)

    with col2:
        current_user = users_df[users_df['user_id'] == user_id]
        if not current_user.empty:
            info = current_user.iloc[0]
            st.info(f"用户 {user_id}: {GROUP_NAME_MAP.get(info['group_id'])}")
        
        recommendations = recommend_videos(user_id, user_vectors_df, video_vectors_df, users_df, top_n)
        if recommendations is None:
            st.error(f"未找到用户ID {user_id}")
        else:
            video_ids = [r[0] for r in recommendations]
            video_titles = [get_video_title(vid, video_title_map) for vid in video_ids]

            result_df = pd.DataFrame(
                recommendations,
                columns=["视频ID", "推荐得分", "主题ID", "事件数"]
            )
            result_df.insert(2, "视频标题", video_titles)
            result_df["推荐得分"] = result_df["推荐得分"].apply(lambda x: f"{x:.4f}")
            result_df["主题"] = result_df["主题ID"].apply(lambda x: TOPIC_NAME_MAP.get(x, f"主题{x}"))
            result_df = result_df[["视频ID", "视频标题", "主题", "推荐得分", "事件数"]]
            result_df.insert(0, "排名", range(1, len(result_df) + 1))
            st.success(f"为用户 {user_id} 生成 {len(recommendations)} 条推荐")
            st.dataframe(result_df, use_container_width=True)
    
    st.divider()
    
    st.subheader("推荐分布统计")
    user_info = users_df[users_df['user_id'] == user_id].iloc[0]
    col_a, col_b, col_c = st.columns(3)
    with col_a:
        st.metric("用户群体", GROUP_NAME_MAP.get(user_info['group_id'], f"群体{int(user_info['group_id'])}"))
    with col_b:
        st.metric("主要Topic", TOPIC_NAME_MAP.get(user_info['primary_topic'], f"主题{int(user_info['primary_topic'])}"))
    with col_c:
        st.metric("活跃度", f"{user_info['activity_level']:.2f}")

elif page == "📈 用户聚类分析":
    st.header("📈 用户聚类分析")
    st.caption("基于用户兴趣向量的KMeans聚类结果")

    users_df = load_users()
    user_vectors_df = load_user_interest_vectors()

    col1, col2 = st.columns(2)

    with col1:
        st.subheader("各群体用户数量")
        group_counts = users_df['group_id'].value_counts().sort_index()
        st.bar_chart(group_counts)

    with col2:
        st.subheader("群体行为数分布")
        group_events = users_df.groupby('group_id')['planned_events'].mean()
        st.bar_chart(group_events)

    st.divider()

    st.subheader("群体详情")
    group_stats = users_df.groupby('group_id').agg({
        'user_id': 'count',
        'activity_level': 'mean',
        'planned_events': 'mean'
    }).rename(columns={
        'user_id': '用户数',
        'activity_level': '平均活跃度',
        'planned_events': '平均行为数'
    })
    st.dataframe(group_stats, use_container_width=True)

    st.divider()

    st.subheader("用户兴趣向量查询")
    col_a, col_b = st.columns([1, 3])

    with col_a:
        user_options = users_df['user_id'].tolist()
        query_user_id = st.selectbox(
            "选择用户",
            options=user_options,
            key="select_user_cluster"
        )
        selected_group = st.selectbox(
            "选择群体查看",
            options=sorted(users_df['group_id'].unique()),
            format_func=lambda x: GROUP_NAME_MAP.get(x, f"群体{x}"),
            key="select_group"
        )

    with col_b:
        user_info = users_df[users_df['user_id'] == query_user_id]
        if not user_info.empty:
            info = user_info.iloc[0]
            col1, col2, col3, col4 = st.columns(4)
            with col1:
                st.metric("用户ID", int(info['user_id']))
            with col2:
                st.metric("所属群体", GROUP_NAME_MAP.get(info['group_id'], f"群体{int(info['group_id'])}"))
            with col3:
                st.metric("主要兴趣", TOPIC_NAME_MAP.get(info['primary_topic'], f"主题{int(info['primary_topic'])}"))
            with col4:
                st.metric("活跃度", f"{info['activity_level']:.2f}")

            user_vector = user_vectors_df[user_vectors_df['user_id'] == query_user_id].iloc[0]['interest_vector']
            chart_data = pd.DataFrame({
                'Topic': [TOPIC_NAME_MAP.get(i, f"Topic {i}") for i in range(len(user_vector))],
                '兴趣值': user_vector
            })
            st.bar_chart(chart_data.set_index('Topic'))

    st.divider()

    st.subheader("群体代表用户兴趣分布")
    group_users = user_vectors_df[user_vectors_df['group_id'] == selected_group]
    if not group_users.empty:
        sample_user = group_users.iloc[0]
        vector = sample_user['interest_vector']

        chart_data = pd.DataFrame({
            'Topic': [TOPIC_NAME_MAP.get(i, f"Topic {i}") for i in range(len(vector))],
            '兴趣值': vector
        })
        group_name = GROUP_NAME_MAP.get(selected_group, f"群体{selected_group}")
        st.bar_chart(chart_data.set_index('Topic'))
        st.caption(f"群体 {group_name} 代表用户 {int(sample_user['user_id'])} 的兴趣分布")

elif page == "🎯 视频聚类分析":
    st.header("🎯 视频聚类分析")
    st.caption("基于视频特征向量的KMeans聚类结果")

    video_vectors_df = load_video_feature_vectors()
    video_title_map = load_video_title_map()

    col1, col2 = st.columns(2)

    with col1:
        st.subheader("各Topic视频数量")
        topic_counts = video_vectors_df['topic_id'].value_counts().sort_index()
        st.bar_chart(topic_counts)

    with col2:
        st.subheader("视频受众覆盖分布")
        has_audience = video_vectors_df['has_audience'].value_counts()
        audience_data = pd.DataFrame({
            '类型': ['有受众数据', '无受众数据'],
            '数量': [has_audience.get(1, 0), has_audience.get(0, 0)]
        })
        st.bar_chart(audience_data.set_index('类型'))

    st.divider()

    st.subheader("Topic详情")
    topic_stats = video_vectors_df.groupby('topic_id').agg({
        'video_id': 'count',
        'event_count': ['mean', 'sum']
    })
    topic_stats.columns = ['视频数', '平均事件数', '总事件数']
    topic_stats.index = topic_stats.index.map(lambda x: TOPIC_NAME_MAP.get(x, f"主题{x}"))
    st.dataframe(topic_stats, use_container_width=True)

    st.divider()

    st.subheader("视频特征向量查询")
    col_a, col_b = st.columns([1, 3])

    with col_a:
        video_ids = video_vectors_df['video_id'].tolist()
        video_display = [f"{vid} - {get_video_title(vid, video_title_map)}" for vid in video_ids]
        
        video_id_map = {display: vid for display, vid in zip(video_display, video_ids)}
        selected_display = st.selectbox(
            "选择视频",
            options=video_display,
            key="select_video"
        )
        query_video_id = video_id_map[selected_display]
        
        selected_topic = st.selectbox(
            "选择Topic查看",
            options=sorted(video_vectors_df['topic_id'].unique()),
            format_func=lambda x: TOPIC_NAME_MAP.get(x, f"主题{x}"),
            key="select_topic"
        )

    with col_b:
        video_info = video_vectors_df[video_vectors_df['video_id'] == query_video_id]
        if not video_info.empty:
            info = video_info.iloc[0]
            video_title = get_video_title(query_video_id, video_title_map)

            col1, col2, col3, col4 = st.columns(4)
            with col1:
                st.metric("视频ID", int(info['video_id']))
            with col2:
                st.metric("视频标题", video_title[:20] + "..." if len(video_title) > 20 else video_title)
            with col3:
                st.metric("Topic", TOPIC_NAME_MAP.get(info['topic_id'], f"主题{info['topic_id']}"))
            with col4:
                st.metric("向量维度", len(info['feature_vector']))

            vector = info['feature_vector']
            chart_data = pd.DataFrame({
                '维度': [f"D{i}" for i in range(len(vector))],
                '特征值': vector
            })
            st.bar_chart(chart_data.set_index('维度'))

    st.divider()

    st.subheader("Topic代表视频特征")
    topic_videos = video_vectors_df[video_vectors_df['topic_id'] == selected_topic]
    if not topic_videos.empty:
        sample_video = topic_videos.iloc[0]
        video_title = get_video_title(sample_video['video_id'], video_title_map)

        vector = sample_video['feature_vector']
        chart_data = pd.DataFrame({
            '维度': [f"D{i}" for i in range(len(vector))],
            '特征值': vector
        })
        st.bar_chart(chart_data.set_index('维度'))
        topic_name = TOPIC_NAME_MAP.get(selected_topic, f"主题{selected_topic}")
        st.caption(f"Topic {topic_name} 代表视频 {int(sample_video['video_id'])}: {video_title[:30]}..." if len(video_title) > 30 else f"Topic {topic_name} 代表视频 {int(sample_video['video_id'])}: {video_title}")

elif page == "🔥 热度预测":
    st.header("🔥 热度预测")
    st.caption("基于历史观看数据的指数平滑预测，分析视频未来热度趋势")

    pred_df = load_popularity_predictions()
    title_map = load_video_title_map()

    if pred_df.empty:
        st.error("暂无预测数据，请先运行 predict_popularity 生成 popularity_predictions.csv")
    else:
        # build display name: "video_id - title"
        pred_df_display = pred_df.copy()
        pred_df_display['display'] = pred_df_display['video_id'].apply(
            lambda vid: f"{int(vid)} - {get_video_title(int(vid), title_map)}"
        )

        col1, col2 = st.columns([1, 3])
        with col1:
            video_options = list(pred_df_display['display'])
            default_idx = 0
            selected_display = st.selectbox(
                "选择视频",
                video_options,
                index=default_idx,
                key="f5_video_select"
            )
            selected_idx = video_options.index(selected_display)

        selected = pred_df.iloc[selected_idx]
        daily_views = selected['daily_views_list']
        smooth_vals = selected['smooth_values_list']
        predicted = selected['predicted_views_list']

        with col2:
            metric_cols = st.columns(4)
            with metric_cols[0]:
                st.metric("总观看事件", int(selected['total_events']))
            with metric_cols[1]:
                st.metric("趋势方向", selected['trend_direction'])
            with metric_cols[2]:
                st.metric("趋势值", f"{selected['trend_value']:.4f}")
            with metric_cols[3]:
                st.metric("峰值日", f"第{int(selected['peak_day'])}天")

        st.divider()

        st.subheader("历史观看量与指数平滑")
        history_days = len(daily_views)
        predict_days = len(predicted)

        # use numeric indices so Streamlit auto-scales x-axis ticks cleanly
        chart_data = pd.DataFrame({
            '日观看量': daily_views,
            '指数平滑': smooth_vals
        })
        st.line_chart(chart_data, height=320)

        st.caption(
            f"共 {history_days} 天历史数据 | "
            f"趋势: {selected['trend_direction']}({selected['trend_value']:.3f}) | "
            f"峰值: 第{int(selected['peak_day'])}天({int(max(daily_views))}次)"
        )

        st.divider()

        st.subheader("未来热度预测")
        pred_chart = pd.DataFrame({
            '预测日观看量': predicted
        }, index=[f"D+{i+1}" for i in range(predict_days)])
        st.bar_chart(pred_chart, height=250)

        pred_detail = ' → '.join([f"D+{i+1}: {v:.1f}" for i, v in enumerate(predicted)])
        st.caption(f"预测详情: {pred_detail}")

        st.divider()

        st.subheader("视频详情")
        video_title = get_video_title(int(selected['video_id']), title_map)
        info_cols = st.columns(3)
        with info_cols[0]:
            st.metric("Video ID", int(selected['video_id']))
            st.metric("覆盖天数", int(selected['day_count']))
        with info_cols[1]:
            st.metric("最大单日观看", int(max(daily_views)))
            st.metric("最近单日观看", int(daily_views[-1]))
        with info_cols[2]:
            st.metric("趋势方向", selected['trend_direction'])
            st.metric("趋势强度", f"{selected['trend_value']:.4f}")
        st.caption(f"视频: {video_title}")
