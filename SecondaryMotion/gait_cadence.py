# gait_cadence.py - 步频记录小工具（笨但能用）
# 盯日志里的 [CLIP] walk/run/sprint 循环动画时长，追加写到 data\gait_log.txt
# 切人自动加分隔行；Manager 窗口关闭时自动删 txt 并退出。
# 用法: 双击 gait_cadence.bat（保持窗口开着即可）
import json
import os
import re
import subprocess
import time

LOG = r"E:\GAMU\Hypergryph Launcher\games\Endfield Game\plugin\breast_probe_log.txt"
STATUS = r"E:\GAMU\Hypergryph Launcher\games\Endfield Game\SecondaryMotion\runtime\runtime_status.json"
OUT = r"D:\Project\EndfieldBreastMotion\SecondaryMotion\data\gait_log.txt"
RX = re.compile(r"\[CLIP\] (\S+?_(walk|run|sprint)_loop) dur=[0-9.]+s spd=[0-9.]+ eff=[0-9.]+s cyc=([0-9.]+)/s")


def game_running():
    try:
        # 无参数 tasklist：避免 /FI 被路径转换吞掉（最笨最稳）
        r = subprocess.run(["tasklist"], capture_output=True, text=True,
                           errors="ignore", timeout=10)
        return "Endfield.exe" in r.stdout
    except Exception:
        return True  # 查不到就当还开着，别乱删


def current_char():
    try:
        st = json.load(open(STATUS, encoding="utf-8"))
        return st.get("character", "") or ""
    except Exception:
        return ""


# 启动：从头全量扫一遍（先跑动后开脚本也能立刻出结果），之后增量
pos = 0

last_char = ""
seen = set()  # (char, gait, dur) 已写过的不再重复写

print("gait cadence watcher started. 跑动后看 " + OUT)
print("游戏关闭后本工具自动退出并删除 txt。")

while True:
    if not game_running():
        try:
            if os.path.exists(OUT):
                os.remove(OUT)
                print("game closed - gait_log.txt removed")
        except Exception:
            pass
        break

    char = current_char()
    if char and char != last_char:
        if last_char:
            try:
                with open(OUT, "a", encoding="utf-8") as f:
                    f.write("\n--- switch to " + char + " ---\n")
                print("--- switch to " + char + " ---")
            except Exception:
                pass
        last_char = char

    try:
        size = os.path.getsize(LOG)
        if size < pos:
            pos = 0  # 日志被清空重来
        if size > pos:
            with open(LOG, "r", encoding="utf-8", errors="replace") as f:
                f.seek(pos)
                data = f.read()
                pos = f.tell()
            for m in RX.finditer(data):
                gait = m.group(2)
                cyc = float(m.group(3))  # 每秒实际循环数（已含播放速度）
                key = (char, gait, round(cyc, 3))
                if key in seen:
                    continue
                seen.add(key)
                step2 = 2.0 * cyc  # 2步/循环假设 -> 步频
                entry = "%s | %s | cyc=%.2f/s | step2=%.2f/s | stepT=%.3fs" % (
                    char, gait, cyc, step2, 1.0 / step2 if step2 > 0 else 0)
                with open(OUT, "a", encoding="utf-8") as f:
                    f.write(entry + "\n")
                print(entry)
    except Exception:
        pass

    time.sleep(2)
