import asyncio
from bilibili_api import search, video, request_settings
import pandas as pd
import random
import re
from pathlib import Path

USER_AGENTS = [
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36",
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:133.0) Gecko/20100101 Firefox/133.0",
    "Mozilla/5.0 (iPhone; CPU iPhone OS 17_5 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.5 Mobile/15E148 Safari/604.1"
]

def update_mask():
    """随机切换请求头伪装"""
    ua = random.choice(USER_AGENTS)
    request_settings.set("headers", {
        "User-Agent": ua,
        "Referer": "https://www.bilibili.com/",
        "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8"
    })


# 伪装成真实的浏览器指纹
request_settings.set("impersonate", "chrome131")
# 初始化时先设置一次 Header
update_mask()
# =====================================
TARGET_COUNT = 120000           
MIN_DURATION = 15                    
MAX_DURATION = 300                   
CONCURRENCY_LIMIT = 5  # 限制concurrency，防止被封
SAVE_EVERY = 100
OUTPUT_FILE = Path(__file__).resolve().parent / "bilibili_10w_pro.csv"

KEYWORDS_POOL = {
    "游戏": [
        "英雄联盟", "原神", "绝地求生", "我的世界", "单机游戏", "主机游戏", "王者荣耀", "黑神话悟空", "怀旧游戏", "电竞高光",
        "steam游戏", "独立游戏", "游戏实况", "速通游戏", "RPG游戏", "开放世界游戏", "沙盒游戏", "恐怖游戏", "国产单机",
        "联机游戏", "卡牌游戏", "塔防游戏", "射击游戏", "魂类游戏", "游戏混剪", "游戏推荐", "游戏测评", "游戏攻略",
        "游戏剧情", "游戏整活", "主播高光", "电竞比赛", "LOL集锦", "王者操作", "原神抽卡", "MC建筑", "我的世界生存",
        "黑神话试玩", "FPS高光", "搞笑游戏"
    ],

    "动画": [
        "动漫剪辑", "名场面", "热血番", "治愈系动漫", "二次元", "配音整活", "燃向AMV", "新番推荐",
        "动漫混剪", "国漫推荐", "日漫推荐", "番剧解说", "动漫台词", "催泪动漫", "高燃动漫", "恋爱番", "校园番",
        "机战动漫", "少年漫", "少女漫", "动漫盘点", "动漫人物", "番剧吐槽", "动漫神作", "动画电影", "动漫音乐",
        "配音秀", "MAD视频", "AMV混剪", "动漫名梗"
    ],

    "科技": [
        "数码测评", "手机推荐", "装机教程", "人工智能", "自动驾驶", "黑科技", "程序员", "极客", "物理实验", "航天工程",
        "芯片", "显卡评测", "笔记本推荐", "平板测评", "耳机测评", "键盘测评", "机械键盘", "显示器评测", "相机评测",
        "无人机", "机器人", "AI工具", "大模型", "编程教学", "Python教程", "前端开发", "后端开发", "操作系统",
        "网络安全", "开源项目", "服务器", "NAS搭建", "DIY电脑", "科技资讯", "数码开箱", "性能测试", "手机摄影",
        "电脑技巧", "电子制作", "单片机"
    ],

    "知识": [
        "科普", "历史冷知识", "地理百科", "心理学", "硬核知识", "大国重器", "纪录片", "法律案例", "生物多样性",
        "哲学", "经济学", "社会学", "医学科普", "化学实验", "物理科普", "数学之美", "天文学", "宇宙探索", "考古发现",
        "国际关系", "刑侦故事", "案件解析", "新闻解读", "知识盘点", "冷门知识", "未解之谜", "古代历史", "近代史",
        "世界史", "中国史", "科学实验", "自然奇观", "地缘政治", "商业思维", "认知提升", "思维模型", "历史人物"
    ],

    "生活": [
        "生活Vlog", "日常整活", "手工DIY", "极简生活", "收纳整理", "萌宠日常", "猫片", "修驴蹄", "赶海", "农村生活",
        "城市生活", "独居生活", "租房改造", "卧室改造", "清洁整理", "断舍离", "周末日常", "情侣日常", "家庭日常",
        "上班日常", "治愈日常", "慢生活", "生活技巧", "省钱妙招", "宿舍日常", "大学生日常", "真实记录", "沉浸式做家务",
        "家居好物", "生活碎片", "一个人的生活", "摆摊日常", "乡村生活", "早起vlog", "晚间routine"
    ],

    "美食": [
        "美食教程", "探店", "街头小吃", "深夜食堂", "大口吃肉", "懒人食谱", "神仙饮品", "黑暗料理", "复刻美食",
        "下饭菜", "家常菜", "早餐教程", "夜宵", "烘焙", "甜品制作", "蛋糕教程", "饮品制作", "火锅", "烧烤", "海鲜",
        "地方小吃", "美食测评", "餐厅推荐", "厨艺教学", "学生党做饭", "空气炸锅食谱", "减脂餐", "便当", "办公室零食",
        "沉浸式做饭", "做菜vlog", "美食开箱", "吃播", "必吃榜", "宵夜推荐"
    ],

    "音乐": [
        "周杰伦", "翻唱", "钢琴演奏", "吉他教学", "电音", "纯音乐", "古风音乐", "演唱会现场", "车载音乐",
        "流行音乐", "民谣", "说唱", "RAP", "摇滚", "国风歌曲", "神级现场", "高音挑战", "音乐推荐", "伤感情歌",
        "经典老歌", "90后回忆", "00后歌单", "BGM推荐", "音乐混剪", "钢琴cover", "吉他cover", "小提琴演奏",
        "乐器演奏", "音乐现场", "live版", "KTV必点", "热门歌曲", "洗脑神曲", "宝藏音乐", "听歌向"
    ],

    "舞蹈": [
        "宅舞", "韩舞", "街舞", "中国舞", "卡点舞", "变装", "广场舞", "芭蕾",
        "爵士舞", "女团舞", "男团舞", "舞蹈教学", "编舞", "镜面教学", "舞蹈翻跳", "kpop舞蹈", "古典舞", "民族舞",
        "现代舞", "breaking", "locking", "popping", "舞蹈卡点", "舞蹈混剪", "高颜值舞蹈", "热门舞蹈"
    ],

    "影视": [
        "影视解说", "电影推荐", "高分神剧", "经典对白", "预告片", "港片回忆", "好莱坞大片",
        "电影混剪", "悬疑电影", "犯罪电影", "动作片", "爱情片", "喜剧片", "恐怖片", "科幻片", "国产剧", "美剧",
        "韩剧", "日剧", "神级演技", "封神片段", "催泪电影", "影视盘点", "电影细节", "剧情解析", "反转电影",
        "下饭神剧", "高能片段", "老电影", "院线新片"
    ],

    "运动": [
        "NBA", "欧冠", "健身房", "极限运动", "滑板", "羽毛球", "垂钓", "马拉松", "武术",
        "足球集锦", "篮球教学", "健身教学", "增肌", "减脂", "跑步训练", "拳击", "散打", "搏击", "游泳",
        "乒乓球", "网球", "台球", "排球", "自行车", "骑行", "徒步", "登山", "滑雪", "冲浪", "钓鱼",
        "体能训练", "居家健身", "徒手训练", "核心训练", "拉伸放松", "羽毛球教学", "篮球过人", "足球技巧"
    ],

    "汽车": [
        "豪车测评", "越野视频", "五菱宏光", "摩托车", "自动泊车", "赛车高光", "房车旅行",
        "新能源车", "电动车测评", "比亚迪", "特斯拉", "理想汽车", "小米汽车", "汽车试驾", "汽车评测", "二手车",
        "改装车", "性能车", "SUV推荐", "家用车推荐", "自驾游", "房车生活", "骑行摩托", "机车文化", "越野穿越",
        "赛车比赛", "漂移", "卡丁车", "汽车知识"
    ],

    "时尚": [
        "穿搭技巧", "美妆教程", "沉浸式护肤", "模特走秀", "开箱视频", "汉服",
        "男生穿搭", "女生穿搭", "春季穿搭", "夏季穿搭", "秋季穿搭", "冬季穿搭", "平价穿搭", "通勤穿搭",
        "校园穿搭", "妆容教程", "伪素颜", "护肤分享", "香水推荐", "饰品推荐", "发型教程", "日系穿搭", "韩系穿搭",
        "复古穿搭", "国风穿搭", "好物分享", "衣橱整理"
    ],

    "娱乐": [
        "明星八卦", "综艺名场面", "脱口秀", "相声", "德云社", "情景剧",
        "综艺混剪", "搞笑综艺", "明星采访", "娱乐盘点", "饭圈趣事", "颁奖典礼", "红毯造型", "爆笑片段",
        "综艺封神场面", "段子合集", "喜剧人", "吐槽大会", "搞笑视频", "尴尬名场面"
    ],

    "鬼畜": [
        "全明星", "人力VOCALOID", "鬼畜调教", "经典素材", "哲学", "魔性洗脑",
        "鬼畜区", "洗脑循环", "抽象整活", "经典鬼畜", "名场面鬼畜", "魔性剪辑", "神曲鬼畜", "鬼畜配音",
        "整活视频", "抽象视频"
    ],

    "学习教育": [
        "高数", "线性代数", "概率论", "大学物理", "C语言", "Python入门", "数据结构", "算法讲解", "考研数学",
        "英语四六级", "雅思", "托福", "学习方法", "高效学习", "自律打卡", "课堂笔记", "考试技巧", "高考复习",
        "大学生学习", "编程学习", "面试题", "Java教程", "C++教程", "操作系统课程", "计算机网络"
    ],

    "职场商业": [
        "求职面试", "简历修改", "职场沟通", "打工人日常", "副业", "创业", "商业分析", "品牌营销", "销售技巧",
        "运营思维", "产品经理", "程序员面试", "互联网职场", "远程办公", "效率工具", "时间管理", "办公技巧",
        "PPT技巧", "Excel技巧", "职场成长", "职业规划", "跳槽经验"
    ],

    "旅行户外": [
        "旅行vlog", "自驾旅行", "穷游", "露营", "徒步旅行", "城市漫步", "海边旅行", "雪山徒步", "景点攻略",
        "旅行记录", "出国旅行", "国内旅行", "周边游", "小众旅行地", "露营装备", "房车旅行", "背包旅行",
        "公路旅行", "日照金山", "无人区穿越", "旅行攻略", "酒店测评"
    ],

    "摄影剪辑": [
        "摄影教程", "手机摄影", "相机推荐", "镜头评测", "调色教程", "视频剪辑", "PR教程", "AE教程", "达芬奇调色",
        "运镜教学", "拍照姿势", "人像摄影", "风光摄影", "vlog拍摄", "短视频剪辑", "剪辑思路", "构图技巧",
        "后期修图", "LR调色", "拍摄花絮"
    ],

    "宠物": [
        "猫咪", "狗狗", "萌宠", "宠物日常", "猫猫日常", "狗狗日常", "猫粮测评", "养猫经验", "养狗经验",
        "宠物搞笑", "萌宠合集", "流浪动物", "宠物救助", "训狗教学", "猫咪行为", "撸猫", "吸猫", "宠物医疗",
        "小宠物", "仓鼠", "鹦鹉"
    ],

    "手工文创": [
        "手工制作", "黏土", "刺绣", "木工", "皮具", "编织", "手账", "文具开箱", "模型制作", "乐高",
        "折纸", "滴胶", "陶艺", "手作饰品", "DIY改造", "国风手工", "文创好物", "治愈手工"
    ],

    "家居装修": [
        "装修避坑", "家装设计", "旧房改造", "软装搭配", "家居收纳", "小户型装修", "租房改造", "家具推荐",
        "智能家居", "家电测评", "厨房收纳", "卫生间改造", "客厅设计", "卧室设计", "装修日记", "全屋定制",
        "灯光设计", "装修灵感"
    ],

    "财经观察": [
        "财经", "经济观察", "商业新闻", "股市", "基金", "理财入门", "消费趋势", "互联网商业", "品牌分析",
        "公司研究", "行业分析", "财报解读", "创业故事", "商业案例", "市场热点"
    ],

    "军事历史": [
        "军事", "军武", "历史战役", "兵器", "航母", "战斗机", "坦克", "国际局势", "冷战历史", "二战历史",
        "战争纪录片", "武器装备", "军事科普", "历史讲解", "名将故事"
    ],

    "搞笑整活": [
        "搞笑", "爆笑", "整活", "离谱瞬间", "社死现场", "抽象", "神操作", "迷惑行为", "整蛊", "搞笑配音",
        "搞笑合集", "翻车现场", "段子", "沙雕视频", "魔性视频", "欢乐日常"
    ],

    "亲子母婴": [
        "带娃日常", "育儿经验", "宝宝辅食", "亲子互动", "早教", "儿童绘本", "母婴好物", "家庭教育",
        "新手妈妈", "亲子vlog", "宝宝成长", "育儿知识"
    ],

    "三农自然": [
        "三农", "农村赶集", "乡村美食", "种植", "养殖", "果园", "菜园", "农机", "丰收", "山野生活",
        "野外生存", "自然观察", "昆虫世界", "动植物记录", "乡村纪实"
    ]
}

KEYWORDS = [item for sublist in KEYWORDS_POOL.values() for item in sublist] #平铺，之后可以shuffle
# ============================================

def parse_duration(duration_str):  # 把时间转换为c++喜欢的数字串
    parts = str(duration_str).split(':')
    try:
        if len(parts) == 2: return int(parts[0]) * 60 + int(parts[1])
        if len(parts) == 3: return int(parts[0]) * 3600 + int(parts[1]) * 60 + int(parts[2])
    except: pass
    return 0

def clean_html(text):
    return re.sub(r'<[^>]+>', '', str(text))  #清洗html标签

CSV_COLUMNS = [
    "aid", "bvid", "title", "category", "author", "duration", "pubdate",
    "view_count", "danmaku", "reply", "favorite", "coin", "share", "like",
    "now_rank", "his_rank", "tag"
]

def load_existing_bvids(csv_path):
    if not csv_path.exists():
        print(f"未发现目标文件：{csv_path.name}，将从空库开始采集")
        return set(), 0

    old_df = pd.read_csv(csv_path, usecols=["bvid"])
    seen_bvids = set(old_df["bvid"].dropna().astype(str))
    print(f"已加载历史数据：{csv_path.name}，已有 {len(seen_bvids)} 条去重记录")
    return seen_bvids, len(seen_bvids)

def append_rows(csv_path, rows):
    if not rows:
        return

    df = pd.DataFrame(rows, columns=CSV_COLUMNS)
    file_exists = csv_path.exists()
    df.to_csv(
        csv_path,
        mode="a",
        index=False,
        header=not file_exists,
        encoding="utf-8-sig"
    )

# 核心：带有信号量限制的信息抓取器
async def fetch_deep_info(bvid, sem):
    async with sem: # 获取通行证，限制并发数
        try:
            if random.random() < 0.2:  #切换身份
                update_mask()
            # 随机休眠 0.3 到 1 秒，模拟人类点击
            await asyncio.sleep(random.uniform(0.3, 1.0))
            v = video.Video(bvid=bvid)
            info = await v.get_info()
            return bvid, info
        except Exception:
            return bvid, None # 抓取失败则返回 None，容错处理

async def collect_10k():
    all_videos = []
    seen_bvids, effective_count = load_existing_bvids(OUTPUT_FILE)

    if effective_count >= TARGET_COUNT:
        print(f"当前已有 {effective_count} 条，已经达到目标 {TARGET_COUNT} 条，无需继续采集")
        return

    sem = asyncio.Semaphore(CONCURRENCY_LIMIT)

    print(f"开启数据采集，当前进度 {effective_count} / {TARGET_COUNT}，输出文件：{OUTPUT_FILE.name}")
    random.shuffle(KEYWORDS)

    for kw in KEYWORDS:
        if effective_count >= TARGET_COUNT:
            print("数据采集达标，结束采集")
            break
        update_mask()

        print(f"\n📡 正在扫描维度: 【{kw}】")
        page = 1

        while page <= 25 and effective_count < TARGET_COUNT:
            try:
                res = await search.search_by_type(
                    keyword=kw,
                    search_type=search.SearchObjectType.VIDEO,
                    page=page
                )
                items = res.get('result', [])
                if not items:
                    break

                # 1.浅层过滤（挑选合格的短视频）
                valid_items = []
                for item in items:
                    bvid = item.get('bvid')
                    if not bvid or bvid in seen_bvids:
                        continue

                    seconds = parse_duration(item.get('duration', '0:0'))
                    if MIN_DURATION <= seconds <= MAX_DURATION:
                        valid_items.append((bvid, item, seconds))

                if not valid_items:
                    page += 1
                    continue

                #2. 并发抓取（只对合格的短视频发请求）
                print(f"   发现 {len(valid_items)} 个候选视频，正在获取视频元数据...")
                tasks = [fetch_deep_info(bvid, sem) for bvid, _, _ in valid_items]
                deep_results = await asyncio.gather(*tasks)

                # 将结果转换为字典方便匹配
                deep_info_map = {res[0]: res[1] for res in deep_results if res[1] is not None}

                page_found = 0
                for bvid, item, seconds in valid_items:
                    if effective_count >= TARGET_COUNT:
                        break

                    deep_info = deep_info_map.get(bvid)
                    if not deep_info:
                        continue

                    stat = deep_info.get('stat', {})

                    # 第三步：抓取视频元数据
                    video_data = {
                        "aid": stat.get('aid', 0),
                        "bvid": bvid,
                        "title": clean_html(item.get('title', '')),
                        "category": item.get('typename', '未知'),
                        "author": clean_html(item.get('author', '未知')),
                        "duration": seconds,
                        "pubdate": deep_info.get('pubdate', 0),
                        "view_count": stat.get('view', item.get('play', 0)),
                        "danmaku": stat.get('danmaku', item.get('danmaku', 0)),
                        "reply": stat.get('reply', 0),
                        "favorite": stat.get('favorite', 0),
                        "coin": stat.get('coin', 0),
                        "share": stat.get('share', 0),
                        "like": stat.get('like', 0),
                        "now_rank": stat.get('now_rank', 0),
                        "his_rank": stat.get('his_rank', 0),
                        "tag": kw
                    }

                    all_videos.append(video_data)
                    seen_bvids.add(bvid)
                    effective_count += 1
                    page_found += 1

                    if len(all_videos) >= SAVE_EVERY:
                        append_rows(OUTPUT_FILE, all_videos)
                        print(f"   已追加保存 {len(all_videos)} 条到 {OUTPUT_FILE.name}")
                        all_videos.clear()

                print(f"   第 {page} 页解析完成，新增{page_found} 条，累计 {effective_count} / {TARGET_COUNT}")

            except Exception as e:
                print(f"   ❌ 搜索接口异常: {e}")
                update_mask()
                await asyncio.sleep(5)
                break

            page += 1
            await asyncio.sleep(random.uniform(1.2, 2.5))

    if all_videos:
        append_rows(OUTPUT_FILE, all_videos)
        print(f"已追加保存最后 {len(all_videos)} 条到 {OUTPUT_FILE.name}")

    print(f"采集结束，当前总条数 {effective_count}，文件位置：{OUTPUT_FILE}")
if __name__ == "__main__":
    asyncio.run(collect_10k())
