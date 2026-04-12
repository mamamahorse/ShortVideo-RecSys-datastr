import asyncio
from bilibili_api import video, request_settings

# 配置伪装
request_settings.set("impersonate", "chrome131")

async def test_api():
    # 找一个具体的视频，比如你之前截取到的《黑神话》BV号
    v = video.Video(bvid="BV1AE4m1d7XT")
    
    # 调用 get_info() 方法，把那个“盲盒”字典拿回来
    info = await v.get_info()
    
    # 提取其中的 "stat" (统计) 字段打印出来
    print("\n========== 视频的核心交互统计数据 ==========")
    for key, value in info['stat'].items():
        print(f"{key}: {value}")

if __name__ == "__main__":
    asyncio.run(test_api())