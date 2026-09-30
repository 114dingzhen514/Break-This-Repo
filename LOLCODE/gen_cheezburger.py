#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_cheezburger.py —— 把 translations/ 里的 52 份人类语言 README 编译成
一份 LOLCODE 程序（LOLCODE/CHEEZBURGER.lol）。

    cd <仓库根目录>
    python LOLCODE/gen_cheezburger.py

同时会写出 LOLCODE/CHEEZBURGER.lol.expected.txt —— 那是校验基准，
CHEEZBURGER.lol 跑出来的 stdout 必须与它逐字节相同：

    python LOLCODE/lolrun.py LOLCODE/CHEEZBURGER.lol | diff - LOLCODE/CHEEZBURGER.lol.expected.txt

程序文本与期望输出由同一个 out_lines 列表派生，所以两者不可能对不上。
"""
import glob
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(HERE, "CHEEZBURGER.lol")

PROLOGUE = [
    'HAI 1.2',
    'BTW ==========================================================================',
    'BTW   CHEEZBURGER.lol  —— 人类历史上最大的 LOLCODE 程序（据我们所知）',
    'BTW',
    'BTW   它是什么：',
    'BTW       Break-This-Repo 的 translations/ 目录里躺着 52 种人类语言的 README。',
    'BTW       这个文件把「52 种人类语言」塌缩成了「1 种编程语言」。',
    'BTW       运行它，它会把这 52 份文档一字不差地 VISIBLE 出来。',
    'BTW',
    'BTW   跑法：',
    'BTW       python LOLCODE/lolrun.py LOLCODE/CHEEZBURGER.lol > out.txt',
    'BTW',
    'BTW   为什么要这样做：',
    'BTW       因为 GitHub 已经挂掉了这个仓库的语言统计条 —— 仓库太大，Linguist 放弃治疗。',
    'BTW       既然语言「条」刷不动，那就把语言本身刷掉。',
    'BTW',
    'BTW   本文件由 LOLCODE/gen_cheezburger.py 生成，不要手改。',
    'BTW   校验：stdout 应与 CHEEZBURGER.lol.expected.txt 逐字节一致。',
    'BTW ==========================================================================',
    '',
]

BANNER = [
    "==========================================================",
    " CHEEZBURGER.lol — 52 种人类语言 -> 1 种编程语言",
    "==========================================================",
    "",
]

FOOTER = [
    "",
    "==========================================================",
    " 完。以上每一个字节都是从本仓库自己的 translations/ 里搬过来的。",
    " KTHXBAI.",
    "==========================================================",
]


def esc(s):
    """LOLCODE 字符串转义：先转冒号，再转引号（顺序不能反）。"""
    return s.replace(":", "::").replace('"', ':"')


def main():
    files = sorted(glob.glob(os.path.join(ROOT, "translations", "README.*.md")))
    named = [(os.path.basename(p), p) for p in files]
    root_md = os.path.join(ROOT, "README.md")
    if os.path.exists(root_md):
        named.append(("README.md (仓库主页源文档)", root_md))

    out_lines = list(BANNER)
    src_bytes = 0
    for name, path in named:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            raw = f.read().lstrip("\ufeff")
        src_bytes += len(raw.encode("utf-8"))
        bar = "-" * 58
        out_lines += ["", bar, ">>> %s" % name, bar]
        out_lines += [ln.rstrip("\r") for ln in raw.split("\n")]

    out_lines += FOOTER

    prog = list(PROLOGUE)
    prog += ['VISIBLE "%s"' % esc(l) if l else 'VISIBLE ""' for l in out_lines]
    prog.append("KTHXBAI")

    text = "\n".join(prog) + "\n"
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    with open(OUT + ".expected.txt", "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out_lines) + "\n")

    print("源文件数        : %d" % len(named))
    print("源文本字节      : %d (%.2f MB)" % (src_bytes, src_bytes / 1048576))
    print("生成 .lol 字节  : %d (%.2f MB)" % (len(text.encode()), len(text.encode()) / 1048576))
    print("生成 .lol 行数  : %d" % len(prog))


if __name__ == "__main__":
    main()
