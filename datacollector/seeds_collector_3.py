import asyncio
import os
import time
import random
import re
import pandas as pd

from bilibili_api import search, video

# ================== 动态配置 ==================
TARGET_COUNT = 100000

MIN_DURATION = 15
MAX_DURATION = 300

# 保守一点：不要页内爆发式并发
CONCURRENCY_LIMIT =8
# 全局节流：所有请求共用
GLOBAL_REQUEST_INTERVAL = (0.2, 1.0)   # 每次请求之间最少间隔
PAGE_SLEEP_RANGE = (5.0, 10.0)         # 每页之间额外暂停
KEYWORD_SLEEP_RANGE = (10.0, 20.0)     # 每个关键词之间额外暂停

MAX_RETRY = 4
SAVE_EVERY = 200
OUTPUT_FILE = "bilibili_100k_safe.csv"

KEYWORDS_POOL = {
    "游戏":["英雄联盟", "原神", "绝地求生", "我的世界", "单机游戏", "主机游戏", "王者荣耀", "黑神话悟空", "怀旧游戏", "电竞高光"],
    "动画":["动漫剪辑", "名场面", "热血番", "治愈系动漫", "二次元", "配音整活", "燃向AMV", "新番推荐"],
    "科技":["数码测评", "手机推荐", "装机教程", "人工智能", "自动驾驶", "黑科技", "程序员", "极客", "物理实验", "航天工程"],
    "知识":["科普", "历史冷知识", "地理百科", "心理学", "硬核知识", "大国重器", "纪录片", "法律案例", "生物多样性"],
    "生活":["生活Vlog", "日常整活", "手工DIY", "极简生活", "收纳整理", "萌宠日常", "猫片", "修驴蹄", "赶海", "农村生活"],
    "美食":["美食教程", "探店", "街头小吃", "深夜食堂", "大口吃肉", "懒人食谱", "神仙饮品", "黑暗料理", "复刻美食"],
    "音乐":["周杰伦", "翻唱", "钢琴演奏", "吉他教学", "电音", "纯音乐", "古风音乐", "演唱会现场", "车载音乐"],
    "舞蹈":["宅舞", "韩舞", "街舞", "中国舞", "卡点舞", "变装", "广场舞", "芭蕾"],
    "影视":["影视解说", "电影推荐", "高分神剧", "经典对白", "预告片", "港片回忆", "好莱坞大片"],
    "运动":["NBA", "欧冠", "健身房", "极限运动", "滑板", "羽毛球", "垂钓", "马拉松", "武术"],
    "汽车":["豪车测评", "越野视频", "五菱宏光", "摩托车", "自动泊车", "赛车高光", "房车旅行"],
    "时尚":["穿搭技巧", "美妆教程", "沉浸式护肤", "模特走秀", "开箱视频", "汉服"],
    "娱乐":["明星八卦", "综艺名场面", "脱口秀", "相声", "德云社", "情景剧"],
    "鬼畜":["全明星", "人力VOCALOID", "鬼畜调教", "经典素材", "哲学", "魔性洗脑"]
}

KEYWORDS = [item for sublist in KEYWORDS_POOL.values() for item in sublist]
# ============================================


class StopCollection(Exception):
    """检测到明显风控信号后，主动停止本次任务。"""
    pass


class GlobalRateLimiter:
    """
    所有网络请求都先经过这里，避免“搜索 + 深挖”叠加造成 burst。
    """
    def __init__(self, interval_range=(2.0, 5.0)):
        self.interval_range = interval_range
        self._lock = asyncio.Lock()
        self._next_ok_time = 0.0

    async def wait(self):
        async with self._lock:
            now = time.monotonic()
            if now < self._next_ok_time:
                await asyncio.sleep(self._next_ok_time - now)
            self._next_ok_time = time.monotonic() + random.uniform(*self.interval_range)


rate_limiter = GlobalRateLimiter(GLOBAL_REQUEST_INTERVAL)


def parse_duration(duration_str):
    parts = str(duration_str).split(':')
    try:
        if len(parts) == 2:
            return int(parts[0]) * 60 + int(parts[1])
        if len(parts) == 3:
            return int(parts[0]) * 3600 + int(parts[1]) * 60 + int(parts[2])
    except Exception:
        pass
    return 0


def clean_html(text):
    return re.sub(r'<[^>]+>', '', str(text))


def load_existing_bvids(output_file):
    """
    断点续跑：如果文件已存在，读取已有 bvid。
    """
    if not os.path.exists(output_file):
        return set(), 0

    try:
        df = pd.read_csv(output_file, usecols=["bvid"], dtype=str)
        seen = set(df["bvid"].dropna().astype(str).tolist())
        return seen, len(seen)
    except Exception:
        # 如果文件损坏或列不完整，保守返回空，让你人工检查文件
        return set(), 0


def append_to_csv(rows, output_file):
    """
    增量落盘，避免全量数据一直堆在内存里。
    """
    if not rows:
        return

    df = pd.DataFrame(rows)
    file_exists = os.path.exists(output_file)
    df.to_csv(
        output_file,
        mode="a",
        header=not file_exists,
        index=False,
        encoding="utf-8"
    )


async def guarded_api_call(coro_factory, label="api"):
    """
    统一节流 + 退避。
    一旦检测到 412 / Precondition Failed，直接停止本轮，避免继续撞墙。
    """
    for attempt in range(1, MAX_RETRY + 1):
        try:
            await rate_limiter.wait()
            return await coro_factory()
        except Exception as e:
            msg = str(e)

            # 明显风控信号：直接停
            if "412" in msg or "Precondition Failed" in msg:
                print(f"⛔ 检测到风控信号（{label}: {msg}），本次任务停止，请稍后再续跑。")
                raise StopCollection(msg)

            # 其他异常：指数退避
            if attempt == MAX_RETRY:
                print(f"❌ {label} 连续失败 {MAX_RETRY} 次，放弃本次请求。错误: {msg}")
                return None

            backoff = min(60, (2 ** attempt) * random.uniform(2.0, 4.0))
            print(f"⚠️ {label} 失败，第 {attempt} 次重试前等待 {backoff:.1f}s。错误: {msg}")
            await asyncio.sleep(backoff)

    return None


async def fetch_deep_info(bvid, sem):
    async with sem:
        # 深挖前再补一个轻微随机停顿
        await asyncio.sleep(random.uniform(1.0, 3.0))

        async def _call():
            v = video.Video(bvid=bvid)
            return await v.get_info()

        info = await guarded_api_call(_call, label=f"get_info:{bvid}")
        return bvid, info


async def collect_100k_safe():
    seen_bvids, effective_count = load_existing_bvids(OUTPUT_FILE)
    buffer_rows = []

    sem = asyncio.Semaphore(CONCURRENCY_LIMIT)

    print(f"🚀 启动保守采集模式，目标 {TARGET_COUNT} 条")
    print(f"📂 已有数据 {effective_count} 条，将从断点继续")

    random.shuffle(KEYWORDS)

    try:
        for kw in KEYWORDS:
            if effective_count >= TARGET_COUNT:
                break

            print(f"\n📡 正在扫描关键词: ")
            page = 1

            while page <= 30 and effective_count < TARGET_COUNT:
                async def _search_call():
                    return await search.search_by_type(
                        keyword=kw,
                        search_type=search.SearchObjectType.VIDEO,
                        page=page
                    )

                res = await guarded_api_call(_search_call, label=f"search:{kw}:page{page}")
                if res is None:
                    # 本页失败，跳出当前关键词，换下一个
                    break

                items = res.get("result", [])
                if not items:
                    print(f"   第 {page} 页无结果，结束当前关键词。")
                    break

                valid_items = []
                for item in items:
                    bvid = item.get("bvid")
                    if not bvid or bvid in seen_bvids:
                        continue

                    seconds = parse_duration(item.get("duration", "0:0"))
                    if MIN_DURATION <= seconds <= MAX_DURATION:
                        valid_items.append((bvid, item, seconds))

                if not valid_items:
                    print(f"   第 {page} 页没有符合时长条件的新视频。")
                    page += 1
                    await asyncio.sleep(random.uniform(*PAGE_SLEEP_RANGE))
                    continue

                print(f"   第 {page} 页发现 {len(valid_items)} 个候选视频，开始保守深挖...")

                page_found = 0

                # 改成顺序/极低并发深挖，避免同一页突然 burst
                for bvid, item, seconds in valid_items:
                    if effective_count >= TARGET_COUNT:
                        break
                    if bvid in seen_bvids:
                        continue

                    _, deep_info = await fetch_deep_info(bvid, sem)
                    if not deep_info:
                        continue

                    stat = deep_info.get("stat", {})

                    video_data = {
                        "aid": stat.get("aid", 0),
                        "bvid": bvid,
                        "title": clean_html(item.get("title", "")),
                        "category": item.get("typename", "未知"),
                        "author": clean_html(item.get("author", "未知")),
                        "duration": seconds,
                        "pubdate": deep_info.get("pubdate", 0),
                        "view_count": stat.get("view", item.get("play", 0)),
                        "danmaku": stat.get("danmaku", item.get("danmaku", 0)),
                        "reply": stat.get("reply", 0),
                        "favorite": stat.get("favorite", 0),
                        "coin": stat.get("coin", 0),
                        "share": stat.get("share", 0),
                        "like": stat.get("like", 0),
                        "now_rank": stat.get("now_rank", 0),
                        "his_rank": stat.get("his_rank", 0),
                        "tag": kw
                    }

                    buffer_rows.append(video_data)
                    seen_bvids.add(bvid)
                    effective_count += 1
                    page_found += 1

                    if len(buffer_rows) >= SAVE_EVERY:
                        append_to_csv(buffer_rows, OUTPUT_FILE)
                        print(f"💾 已增量保存 {len(buffer_rows)} 条，当前累计 {effective_count} / {TARGET_COUNT}")
                        buffer_rows.clear()

                print(f"   第 {page} 页完成，新增 {page_found} 条，累计 {effective_count} / {TARGET_COUNT}")

                page += 1
                await asyncio.sleep(random.uniform(*PAGE_SLEEP_RANGE))

            # 一个关键词结束后再停一会儿
            await asyncio.sleep(random.uniform(*KEYWORD_SLEEP_RANGE))

    except StopCollection:
        print("🛑 已主动停止本次任务。你可以稍后再次运行，脚本会从已有 CSV 继续。")

    finally:
        if buffer_rows:
            append_to_csv(buffer_rows, OUTPUT_FILE)
            print(f"💾 最后补写 {len(buffer_rows)} 条到 {OUTPUT_FILE}")

        print(f"✅ 任务结束，当前文件累计目标进度约为：{effective_count} / {TARGET_COUNT}")


if __name__ == "__main__":
    asyncio.run(collect_100k_safe())