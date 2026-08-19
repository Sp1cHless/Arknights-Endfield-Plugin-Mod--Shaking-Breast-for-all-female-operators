# zh_names.py - replace display_name values in characters.default.json
# with Chinese names (ZH package only).  Dumb and explicit: only touches
# the display_name field of known character ids, skips anything unknown.
# Usage: python zh_names.py <path-to-characters.default.json>
import os
import re
import shutil
import sys

MAP = {
    "chr_0014_aurora": "\u663c\u96ea",        # Snowshine 昼雪
    "chr_0017_yvonne": "\u4f0a\u51af",        # Yvonne 伊冯
    "chr_0031_mifu": "\u5f25\u5f17",          # Mifu 弥弗
    "chr_0003_endminf": "\u5973\u7ba1\u7406\u5458",  # Endministrator(F) 女管理员
    "chr_0030_zhuangfy": "\u5e84\u65b9\u5b9c", # Zhuang Fangyi 庄方宜
    "chr_0016_laevat": "\u83b1\u4e07\u6c40",   # Laevatain 莱万汀
    "chr_0004_pelica": "\u4f69\u5229\u5361",   # Perlica 佩利卡
    "chr_0005_chen": "\u9648\u5343\u8bed",     # Chen Qianyu 陈千语
    "chr_0032_lizhiyan": "\u8bc0",             # Arcane 诀
    "chr_0035_liino": "\u68a8\u8bfa",          # Liino 梨诺
    "chr_0013_aglina": "\u6d01\u5c14\u4f69\u5854", # Gilberta 洁尔佩塔
    "chr_0026_lastrite": "\u522b\u793c",       # Lastrite 别礼
    "chr_0009_azrila": "\u4f59\u70ec",         # Ember 余烬
    "chr_0012_avywen": "\u827e\u95fb\u7ef4\u5a1c", # Avywenna 艾闻维娜
    "chr_0011_seraph": "\u8d5b\u5e0c",         # Xaihi 赛希
    "chr_0007_ikut": "\u5f27\u5149",           # Arclight 弧光
    "chr_0021_whiten": "\u57c3\u7279\u62c9",   # Estella 埃特拉
    "chr_0019_karin": "\u79cb\u6817",          # Akekuri 秋栗
    "chr_0022_bounda": "\u8424\u77f3",         # Fluorite 萤石
}

path = sys.argv[1]
src = open(path, encoding="utf-8").read()
changed = 0
for cid, zh in MAP.items():
    pat = '"%s": \\{\\s*"display_name": "[^"]*"' % cid
    repl = '"%s": { "display_name": "%s"' % (cid, zh)
    new, n = re.subn(pat, repl, src)
    if n:
        src = new
        changed += n
open(path, "w", encoding="utf-8").write(src)
print("zh_names: replaced %d display_name entries" % changed)

# ZH packages also carry the Chinese user guide (使用说明.txt) at the
# package root.  Stage dir is argv[2] when the assemble script passes it.
if len(sys.argv) > 2:
    stage = sys.argv[2]
    here = os.path.dirname(os.path.abspath(__file__))
    guide = os.path.join(here, "使用说明.txt")
    if os.path.exists(guide):
        shutil.copyfile(guide, os.path.join(stage, "使用说明.txt"))
        print("zh_names: copied 使用说明.txt")
    else:
        print("zh_names: WARNING 使用说明.txt not found")
