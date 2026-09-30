#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
lolrun.py —— 一个零依赖的 LOLCODE 1.2 子集解释器。

    python lolrun.py BREAK_THIS_REPO.lol

为什么存在这个文件：
    在 GitHub 上，用 LOLCODE 写成的东西 99% 是静态展品 —— 没人能跑，因为社区里
    没有一个「下载下来就能用」的解释器，大家还得先装 .NET 或者 Perl 的 LOLCODE 实现。
    这个文件把「LOLCODE 能跑」这件事的门槛降到了一个 python 命令。

支持的子集（本目录下的程序只用得到这些）：
    HAI <ver> / KTHXBAI
    VISIBLE <expr> [<expr> ...] [!]
    I HAS A <var> [ITZ <expr>]
    <var> R <expr>
    <var> IS NOW A <TYPE>
    SUM OF / DIFF OF / PRODUKT OF / QUOSHUNT OF / MOD OF / BIGGR OF / SMALLR OF
    BOTH OF / EITHER OF / WON OF / NOT / ALL OF ... MKAY / ANY OF ... MKAY
    BOTH SAEM ... AN ... / DIFFRINT ... AN ...
    SMOOSH ... MKAY / MAEK <expr> A <TYPE>
    O RLY? YA RLY / MEBBE / NO WAI / OIC
    WTF? OMG <lit> / OMGWTF / OIC
    IM IN YR <label> [<op> YR <var> [AN <expr>]] [TIL|WILE <expr>] ... IM OUTTA YR <label>
    HOW IZ I <name> [YR <a> [AN YR <b> ...]] ... IF U SAY SO
    I IZ <name> [YR <e> [AN YR <e> ...]] MKAY
    FOUND YR <expr> / GTFO
    BTW 行注释 / OBTW 块注释 TLDR

类型：NOOB(None) / TROOF(bool) / NUMBR(int) / NUMBAR(float) / YARN(str)
字符串转义：:) 换行  :> 制表  :" 引号  :: 冒号

这个解释器不追求 100% 覆盖规范，只追求「本目录的程序逐字节可复现地跑出结果」，
并且跑错的时候大声报错，而不是默默给你一个瞎编的答案。
"""

import sys

if hasattr(sys.stdout, "reconfigure"):
    # newline="\n" 很关键：否则 Windows 上会把每个换行写成 \r\n，
    # 同一份程序在不同平台上就会吐出不同的字节。
    sys.stdout.reconfigure(encoding="utf-8", newline="\n")


class LolError(Exception):
    """LOLCODE 层面的运行时/语法错误。"""


# --------------------------------------------------------------------------
# 1. 预处理：去掉注释，合并 `...` 续行，切成逻辑行
# --------------------------------------------------------------------------

def strip_comments(src):
    """去掉 BTW 行注释与 OBTW...TLDR 块注释，字符串内部不动。"""
    out = []
    i, n = 0, len(src)
    in_str = False
    while i < n:
        c = src[i]
        if in_str:
            if c == '"':
                in_str = False
                out.append(c)
                i += 1
            elif c == ':' and i + 1 < n:
                out.append(src[i:i + 2])
                i += 2
            else:
                out.append(c)
                i += 1
            continue

        if c == '"':
            in_str = True
            out.append(c)
            i += 1
            continue

        # OBTW ... TLDR  块注释
        if src.startswith("OBTW", i) and (i == 0 or not (src[i - 1].isalnum() or src[i - 1] == '_')):
            j = src.find("TLDR", i)
            if j == -1:
                break
            i = j + 4
            continue

        # BTW 行注释
        if src.startswith("BTW", i) and (i == 0 or not (src[i - 1].isalnum() or src[i - 1] == '_')):
            j = src.find("\n", i)
            if j == -1:
                break
            i = j
            continue

        out.append(c)
        i += 1
    return "".join(out)


def logical_lines(src):
    """把源码切成逻辑行；行尾的 `...` 表示下一行是它的延续。"""
    raw = strip_comments(src).split("\n")
    lines, buf = [], ""
    for ln in raw:
        s = ln.rstrip()
        if s.endswith("..."):
            buf += s[:-3] + " "
            continue
        buf += s
        if buf.strip():
            lines.append(buf.strip())
        buf = ""
    if buf.strip():
        lines.append(buf.strip())
    return lines


# --------------------------------------------------------------------------
# 2. 词法：一行 -> token 列表
# --------------------------------------------------------------------------

def tokenize(line):
    toks = []
    i, n = 0, len(line)
    while i < n:
        c = line[i]
        if c.isspace():
            i += 1
            continue

        if c == '"':
            j, buf = i + 1, []
            while j < n and line[j] != '"':
                if line[j] == ':' and j + 1 < n:
                    buf.append(line[j:j + 2])
                    j += 2
                else:
                    buf.append(line[j])
                    j += 1
            if j >= n:
                raise LolError("没关上的字符串引号：%s" % line)
            toks.append(("STR", unescape("".join(buf))))
            i = j + 1
            continue

        if c.isdigit() or (c == '-' and i + 1 < n and line[i + 1].isdigit()):
            j = i + 1
            while j < n and (line[j].isdigit() or line[j] == '.'):
                j += 1
            toks.append(("NUM", line[i:j]))
            i = j
            continue

        j = i
        while j < n and not line[j].isspace() and line[j] != '"':
            j += 1
        toks.append(("W", line[i:j].upper()))
        i = j
    return toks


def unescape(s):
    out, i, n = [], 0, len(s)
    while i < n:
        if s[i] == ':' and i + 1 < n:
            nxt = s[i + 1]
            if nxt == ')':
                out.append("\n")
            elif nxt == '>':
                out.append("\t")
            elif nxt == '"':
                out.append('"')
            elif nxt == ':':
                out.append(':')
            elif nxt == '(':
                out.append("\v")
            else:
                out.append(s[i:i + 2])
            i += 2
            continue
        out.append(s[i])
        i += 1
    return "".join(out)


# --------------------------------------------------------------------------
# 3. 类型系统
# --------------------------------------------------------------------------

def typename(v):
    if v is None:
        return "NOOB"
    if isinstance(v, bool):
        return "TROOF"
    if isinstance(v, int):
        return "NUMBR"
    if isinstance(v, float):
        return "NUMBAR"
    if isinstance(v, str):
        return "YARN"
    return "NOOB"


def to_yarn(v):
    if v is None:
        return ""
    if isinstance(v, bool):
        return "WIN" if v else "FAIL"
    if isinstance(v, float):
        return "%.2f" % v
    return str(v)


def cast(v, t):
    """MAEK 的语义。"""
    if t == "NOOB":
        return None
    if t == "TROOF":
        if v is None:
            return False
        if isinstance(v, bool):
            return v
        if isinstance(v, (int, float)):
            return v != 0
        return len(v) > 0
    if t == "NUMBR":
        if isinstance(v, bool):
            return 1 if v else 0
        if v is None:
            return 0
        if isinstance(v, (int, float)):
            return int(v)
        try:
            return int(float(v.strip() or 0))
        except ValueError:
            raise LolError("没法把 %r MAEK 成 NUMBR" % v)
    if t == "NUMBAR":
        if isinstance(v, bool):
            return 1.0 if v else 0.0
        if v is None:
            return 0.0
        if isinstance(v, (int, float)):
            return float(v)
        try:
            return float(v.strip() or 0)
        except ValueError:
            raise LolError("没法把 %r MAEK 成 NUMBAR" % v)
    if t == "YARN":
        return to_yarn(v)
    raise LolError("我不认识这个类型：%s" % t)


def truthy(v):
    """条件判断：只有 WIN 走 YA RLY。LOLCODE 里非 TROOF 要先 MAEK。"""
    if isinstance(v, bool):
        return v
    if v is None:
        return False
    if isinstance(v, (int, float)):
        return v != 0
    return len(v) > 0


# --------------------------------------------------------------------------
# 4. 语法：表达式（递归下降，基于 token 游标）
# --------------------------------------------------------------------------

TYPES = ("NOOB", "TROOF", "NUMBR", "NUMBAR", "YARN")


class Cursor:
    def __init__(self, toks):
        self.toks = toks
        self.i = 0

    def eof(self):
        return self.i >= len(self.toks)

    def peek(self, k=0):
        j = self.i + k
        return self.toks[j] if j < len(self.toks) else (None, None)

    def word(self, k=0):
        t = self.peek(k)
        return t[1] if t[0] == "W" else None

    def take(self):
        t = self.toks[self.i]
        self.i += 1
        return t

    def expect_word(self, w):
        got = self.word()
        if got != w:
            raise LolError("这里应该出现 %s，实际是 %r" % (w, got))
        self.i += 1


class Interp:
    def __init__(self):
        self.globals = {}
        self.funcs = {}
        self.out = sys.stdout

    # ---- 表达式 ----
    def parse_expr(self, cur):
        t = cur.peek()
        kind, val = t

        if kind == "STR":
            cur.take()
            return ("lit", val)
        if kind == "NUM":
            cur.take()
            return ("lit", float(val) if "." in val else int(val))

        if kind != "W":
            raise LolError("看不懂的表达式片段：%r" % (val,))

        w = val

        if w == "WIN":
            cur.take()
            return ("lit", True)
        if w == "FAIL":
            cur.take()
            return ("lit", False)
        if w == "NOOB":
            cur.take()
            return ("lit", None)

        # 一元
        if w == "NOT":
            cur.take()
            return ("not", self.parse_expr(cur))

        # 二元算术 / 比较 / 布尔
        BIN = {
            "SUM": "sum", "DIFF": "diff", "PRODUKT": "produkt", "QUOSHUNT": "quoshunt",
            "MOD": "mod", "BIGGR": "biggr", "SMALLR": "smallr",
            "BOTH": "both", "EITHER": "either", "WON": "won",
        }
        if w in BIN or w in ("BOTH", "DIFFRINT"):
            # BOTH SAEM / BOTH OF
            if w == "DIFFRINT":
                cur.take()
                a = self.parse_expr(cur)
                cur.expect_word("AN")
                b = self.parse_expr(cur)
                return ("diffrint", a, b)
            nxt = cur.word(1)
            if w == "BOTH" and nxt == "SAEM":
                cur.take(); cur.take()
                a = self.parse_expr(cur)
                cur.expect_word("AN")
                b = self.parse_expr(cur)
                return ("bothsaem", a, b)
            if nxt == "OF":
                cur.take(); cur.take()
                a = self.parse_expr(cur)
                cur.expect_word("AN")
                b = self.parse_expr(cur)
                return (BIN[w], a, b)

        if w in ("ALL", "ANY"):
            cur.take()
            cur.expect_word("OF")
            parts = [self.parse_expr(cur)]
            while cur.word() == "AN":
                cur.take()
                parts.append(self.parse_expr(cur))
            cur.expect_word("MKAY")
            return ("allof" if w == "ALL" else "anyof", parts)

        if w == "SMOOSH":
            cur.take()
            parts = [self.parse_expr(cur)]
            while True:
                if cur.word() == "AN":
                    cur.take()
                    parts.append(self.parse_expr(cur))
                    continue
                if cur.word() == "MKAY" or cur.eof():
                    break
                if cur.word() in ("YA", "NO", "OIC", "MEBBE", "IM", "KTHXBAI"):
                    break
                parts.append(self.parse_expr(cur))
            if cur.word() == "MKAY":
                cur.take()
            return ("smoosh", parts)

        if w == "MAEK":
            cur.take()
            e = self.parse_expr(cur)
            cur.expect_word("A")
            tt = cur.word()
            if tt not in TYPES:
                raise LolError("MAEK 后面只能是 %s，不是 %r" % ("/".join(TYPES), tt))
            cur.take()
            return ("maek", e, tt)

        if w == "I" and cur.word(1) == "IZ":
            cur.take(); cur.take()
            name = cur.take()[1]
            args = []
            while cur.word() == "YR":
                cur.take()
                args.append(self.parse_expr(cur))
                if cur.word() == "AN":
                    cur.take()
            if cur.word() == "MKAY":
                cur.take()
            return ("call", name, args)

        # 变量
        cur.take()
        return ("var", w)

    # ---- 取值 ----
    def eval_(self, node, env):
        k = node[0]
        if k == "lit":
            return node[1]
        if k == "var":
            name = node[1]
            if name in env:
                return env[name]
            if name in self.globals:
                return self.globals[name]
            raise LolError("变量 %s 还没出生就被用了 (NOOB)" % name)
        if k == "sum":
            return self.num(self.eval_(node[1], env)) + self.num(self.eval_(node[2], env))
        if k == "diff":
            return self.num(self.eval_(node[1], env)) - self.num(self.eval_(node[2], env))
        if k == "produkt":
            return self.num(self.eval_(node[1], env)) * self.num(self.eval_(node[2], env))
        if k == "quoshunt":
            a, b = self.num(self.eval_(node[1], env)), self.num(self.eval_(node[2], env))
            if b == 0:
                raise LolError("QUOSHUNT OF ... AN 0 —— 你这是要毁灭宇宙吗")
            r = a / b
            return r
        if k == "mod":
            a, b = self.num(self.eval_(node[1], env)), self.num(self.eval_(node[2], env))
            if b == 0:
                raise LolError("MOD OF ... AN 0 —— INFINITE LOOP 警告")
            return a % b
        if k == "biggr":
            return max(self.num(self.eval_(node[1], env)), self.num(self.eval_(node[2], env)))
        if k == "smallr":
            return min(self.num(self.eval_(node[1], env)), self.num(self.eval_(node[2], env)))
        if k == "both":
            return truthy(self.eval_(node[1], env)) and truthy(self.eval_(node[2], env))
        if k == "either":
            return truthy(self.eval_(node[1], env)) or truthy(self.eval_(node[2], env))
        if k == "won":
            return truthy(self.eval_(node[1], env)) != truthy(self.eval_(node[2], env))
        if k == "not":
            return not truthy(self.eval_(node[1], env))
        if k == "allof":
            return all(truthy(self.eval_(e, env)) for e in node[1])
        if k == "anyof":
            return any(truthy(self.eval_(e, env)) for e in node[1])
        if k == "bothsaem":
            return self.eq(self.eval_(node[1], env), self.eval_(node[2], env))
        if k == "diffrint":
            return not self.eq(self.eval_(node[1], env), self.eval_(node[2], env))
        if k == "smoosh":
            return "".join(to_yarn(self.eval_(e, env)) for e in node[1])
        if k == "maek":
            return cast(self.eval_(node[1], env), node[2])
        if k == "call":
            return self.call(node[1], [self.eval_(e, env) for e in node[2]])
        raise LolError("内部错误：未知节点 %r" % (k,))

    @staticmethod
    def num(v):
        if isinstance(v, bool):
            return 1 if v else 0
        if isinstance(v, (int, float)):
            return v
        if isinstance(v, str):
            try:
                return int(v)
            except ValueError:
                try:
                    return float(v)
                except ValueError:
                    raise LolError("YE GODS! %r 不是个数字" % v)
        raise LolError("NOOB 不能参与算术")

    @staticmethod
    def eq(a, b):
        if isinstance(a, bool) or isinstance(b, bool):
            return truthy(a) == truthy(b) if (isinstance(a, bool) or isinstance(b, bool)) else a == b
        if isinstance(a, (int, float)) and isinstance(b, (int, float)):
            return a == b
        if isinstance(a, str) and isinstance(b, str):
            return a == b
        if a is None or b is None:
            return a is None and b is None
        return a == b

    def call(self, name, args):
        if name not in self.funcs:
            raise LolError("I IZ %s —— 这个函数不存在" % name)
        params, body = self.funcs[name]
        if len(args) != len(params):
            raise LolError("%s 要 %d 个参数，你给了 %d 个" % (name, len(params), len(args)))
        env = dict(zip(params, args))
        try:
            self.exec_block(body, env)
        except ReturnSig as r:
            return r.v
        return None

    # ---- 语句块 ----
    def exec_block(self, stmts, env):
        i = 0
        while i < len(stmts):
            s = stmts[i]
            kind = s[0]

            if kind == "visible":
                cur = Cursor([("W", "X")])
                val = "".join(to_yarn(self.eval_(e, env)) for e in s[1])
                self.out.write(val)
                if not s[2]:
                    self.out.write("\n")
                i += 1
                continue

            if kind == "assign":
                env[s[1]] = self.eval_(s[2], env)
                # 全局可见性：全局作用域里赋值也写回 global
                if env is self.globals:
                    self.globals[s[1]] = env[s[1]]
                i += 1
                continue

            if kind == "declare":
                env[s[1]] = self.eval_(s[2], env) if s[2] is not None else None
                if env is self.globals:
                    self.globals[s[1]] = env[s[1]]
                i += 1
                continue

            if kind == "retype":
                env[s[1]] = cast(env.get(s[1]), s[2])
                i += 1
                continue

            if kind == "exprstmt":
                env["IT"] = self.eval_(s[1], env)
                i += 1
                continue

            if kind == "if":
                it = env.get("IT", self.globals.get("IT"))
                if truthy(it):
                    self.exec_block(s[1][0][1], env)
                else:
                    hit = False
                    for cond, blk in s[1][1:]:
                        if truthy(self.eval_(cond, env)):
                            self.exec_block(blk, env)
                            hit = True
                            break
                    if not hit and s[2] is not None:
                        self.exec_block(s[2], env)
                i += 1
                continue

            if kind == "loop":
                _, label, op, var, step, mode, cond, body = s
                guard = 0
                while True:
                    guard += 1
                    if guard > 100_000_000:
                        raise LolError("IM IN YR %s 跑了 1 亿次还没停，我罢工了" % label)
                    if cond is not None:
                        v = truthy(self.eval_(cond, env))
                        if mode == "TIL" and v:
                            break
                        if mode == "WILE" and not v:
                            break
                    try:
                        self.exec_block(body, env)
                    except BreakSig:
                        break
                    if op is not None:
                        cur = self.num(env.get(var, 0))
                        if step is None:
                            raise LolError("UPPIN/NERFIN 需要一个步子 (AN <expr>)")
                        amt = self.num(self.eval_(step, env))
                        if op == "UPPIN":
                            env[var] = cur + amt
                        elif op == "NERFIN":
                            env[var] = cur - amt
                        else:
                            raise LolError("不支持这个循环算子：%s" % op)
                    else:
                        break
                i += 1
                continue

            if kind == "callstmt":
                # 单独的 I IZ ... MKAY 语句：返回值写进 IT，供后面的 O RLY? / WTF? 用
                env["IT"] = self.eval_(("call", s[1], s[2]), env)
                i += 1
                continue

            if kind == "gtfo":
                raise BreakSig()

            if kind == "found":
                raise ReturnSig(self.eval_(s[1], env))

            if kind == "switch":
                _, val_expr, cases, default = s
                if val_expr is None:
                    v = env.get("IT", self.globals.get("IT"))
                else:
                    v = self.eval_(val_expr, env)
                hit = None
                for lit, blk in cases:
                    if self.eq(v, self.eval_(lit, env)):
                        hit = blk
                        break
                if hit is None:
                    hit = default
                if hit is not None:
                    try:
                        self.exec_block(hit, env)
                    except BreakSig:
                        pass
                i += 1
                continue

            raise LolError("内部错误：未知语句 %r" % (kind,))
        return None


class BreakSig(Exception):
    pass


class ReturnSig(Exception):
    def __init__(self, v):
        self.v = v


# --------------------------------------------------------------------------
# 5. 解析整个程序（块结构）
# --------------------------------------------------------------------------

class Parser:
    def __init__(self, lines):
        self.lines = lines
        self.pos = 0
        self.interp = Interp()

    def cur(self):
        return self.lines[self.pos] if self.pos < len(self.lines) else None

    def parse_program(self):
        if not self.lines or not self.lines[0].upper().startswith("HAI"):
            raise LolError("第一行必须是 HAI（这个文件看起来不是 LOLCODE）")
        self.pos = 1
        body = self.parse_block(stop=("KTHXBAI",))
        return body

    @staticmethod
    def is_stop(line, stop):
        parts = line.split()
        head = parts[0].upper() if parts else ""
        if head == "IM" and len(parts) > 1 and parts[1].upper() == "OUTTA":
            return "IM" in stop
        return head in stop

    def parse_block(self, stop):
        stmts = []
        while self.pos < len(self.lines):
            line = self.lines[self.pos]
            parts = line.split()
            head = parts[0].upper() if parts else ""
            if self.is_stop(line, stop):
                return stmts

            if head == "HAI":
                self.pos += 1
                continue
            if head in ("KTHXBAI",):
                raise LolError("KTHXBAI 出现得莫名其妙")

            stmts.append(self.parse_stmt())
        if stop and stop != ("KTHXBAI",):
            raise LolError("块没有正常收尾，缺 %s" % "/".join(stop))
        return stmts

    def parse_stmt(self):
        line = self.lines[self.pos]
        toks = tokenize(line)
        cur = Cursor(toks)
        w0 = cur.word(0)

        # I HAS A <var> [ITZ <expr>]
        if w0 == "I" and cur.word(1) == "HAS" and cur.word(2) == "A":
            self.pos += 1
            cur.take(); cur.take(); cur.take()
            var = cur.take()[1]
            init = None
            if cur.word() == "ITZ":
                cur.take()
                init = self.interp.parse_expr(cur)
            return ("declare", var, init)

        # VISIBLE ...
        if w0 == "VISIBLE":
            self.pos += 1
            cur.take()
            noret = False
            if cur.toks:
                last = cur.toks[-1]
                if last == ("W", "!"):
                    noret = True
                    cur.toks = cur.toks[:-1]
                elif last[0] == "W" and last[1].endswith("!"):
                    noret = True
                    cur.toks = cur.toks[:-1] + [("W", last[1][:-1])]
            exprs = []
            while not cur.eof():
                exprs.append(self.interp.parse_expr(cur))
            return ("visible", exprs, noret)

        # O RLY?
        if w0 == "O" and cur.word(1) == "RLY?":
            self.pos += 1
            return self.parse_if()

        # WTF?
        if w0 == "WTF?":
            self.pos += 1
            return self.parse_switch()

        # IM IN YR <label> ...
        if w0 == "IM" and cur.word(1) == "IN" and cur.word(2) == "YR":
            return self.parse_loop()

        # HOW IZ I <name> ...
        if w0 == "HOW" and cur.word(1) == "IZ" and cur.word(2) == "I":
            return self.parse_func()

        # GTFO
        if w0 == "GTFO":
            self.pos += 1
            return ("gtfo",)

        # FOUND YR <expr>
        if w0 == "FOUND" and cur.word(1) == "YR":
            self.pos += 1
            cur.take(); cur.take()
            return ("found", self.interp.parse_expr(cur))

        # I IZ <name> ... MKAY   （把函数当语句调用，忽略返回值）
        if w0 == "I" and cur.word(1) == "IZ":
            self.pos += 1
            node = self.interp.parse_expr(cur)
            if node[0] != "call":
                raise LolError("这句话不是函数调用")
            return ("callstmt", node[1], node[2])

        # <var> R <expr>  /  <var> IS NOW A <TYPE>
        if w0 is not None and cur.word(1) == "R":
            self.pos += 1
            var = cur.take()[1]
            cur.take()
            return ("assign", var, self.interp.parse_expr(cur))

        if w0 is not None and cur.word(1) == "IS" and cur.word(2) == "NOW" and cur.word(3) == "A":
            self.pos += 1
            var = cur.take()[1]
            cur.take(); cur.take(); cur.take()
            tt = cur.take()[1]
            return ("retype", var, tt)

        # 兜底：把整行当成一个表达式求值，结果放进 IT（供 O RLY? / WTF? 使用）
        node = self.interp.parse_expr(cur)
        if not cur.eof():
            raise LolError("第 %d 行看不懂（表达式后面还有多余的东西）：%s" % (self.pos + 1, line))
        self.pos += 1
        return ("exprstmt", node)

    def parse_if(self):
        branches, default = [], None
        cur_pos = self.pos
        while self.pos < len(self.lines):
            head = self.lines[self.pos].split()[0].upper()
            if head == "YA" and self.lines[self.pos].split()[1].upper() == "RLY":
                self.pos += 1
                branches.append((None, self.parse_block(stop=("MEBBE", "NO", "OIC"))))
                continue
            if head == "MEBBE":
                toks = tokenize(self.lines[self.pos])
                c = Cursor(toks)
                c.take()
                cond = self.interp.parse_expr(c)
                self.pos += 1
                branches.append((cond, self.parse_block(stop=("MEBBE", "NO", "OIC"))))
                continue
            if head == "NO" and self.lines[self.pos].split()[1].upper() == "WAI":
                self.pos += 1
                default = self.parse_block(stop=("OIC",))
                continue
            if head == "OIC":
                self.pos += 1
                break
            raise LolError("O RLY? 块里出现了怪东西：%s" % self.lines[self.pos])
        else:
            raise LolError("O RLY? 没有 OIC 收尾")
        if not branches:
            raise LolError("O RLY? 后面既没有 YA RLY 也没有别的")
        return ("if", branches, default)

    def parse_switch(self):
        # WTF? 没有操作数，它切换的是上一条表达式的值（IT）。
        # 注意：调用方 parse_stmt 已经吃掉了 `WTF?` 这一行。
        val_expr = None
        cases, default = [], None
        while self.pos < len(self.lines):
            head = self.lines[self.pos].split()[0].upper()
            if head == "OMG":
                toks = tokenize(self.lines[self.pos])
                c = Cursor(toks)
                c.take()
                lit = self.interp.parse_expr(c)
                self.pos += 1
                cases.append((lit, self.parse_block(stop=("OMG", "OMGWTF", "OIC"))))
                continue
            if head == "OMGWTF":
                self.pos += 1
                default = self.parse_block(stop=("OIC",))
                continue
            if head == "OIC":
                self.pos += 1
                break
            raise LolError("WTF? 块里出现了怪东西：%s" % self.lines[self.pos])
        else:
            raise LolError("WTF? 没有 OIC 收尾")
        return ("switch", val_expr, cases, default)

    def parse_loop(self):
        toks = tokenize(self.lines[self.pos])
        c = Cursor(toks)
        c.take(); c.take(); c.take()
        label = c.take()[1]
        op = var = step = mode = cond = None
        if c.word() in ("UPPIN", "NERFIN"):
            op = c.take()[1]
            c.expect_word("YR")
            var = c.take()[1]
            if c.word() == "AN":
                c.take()
                step = self.interp.parse_expr(c)
        if c.word() in ("TIL", "WILE"):
            mode = c.take()[1]
            cond = self.interp.parse_expr(c)
        self.pos += 1
        body = self.parse_block(stop=("IM",))
        # 吃掉 IM OUTTA YR <label>
        head = tokenize(self.lines[self.pos])
        if not (head[0] == ("W", "IM") and head[1] == ("W", "OUTTA")):
            raise LolError("IM IN YR 没有 IM OUTTA YR 收尾")
        self.pos += 1
        return ("loop", label, op, var, step, mode, cond, body)

    def parse_func(self):
        toks = tokenize(self.lines[self.pos])
        c = Cursor(toks)
        c.take(); c.take(); c.take()
        name = c.take()[1]
        params = []
        while c.word() == "YR":
            c.take()
            params.append(c.take()[1])
            if c.word() == "AN":
                c.take()
        self.pos += 1
        body = self.parse_block(stop=("IF",))
        head = tokenize(self.lines[self.pos])
        if not (head[0] == ("W", "IF") and head[1] == ("W", "U") and head[2] == ("W", "SAY")):
            raise LolError("HOW IZ I 没有 IF U SAY SO 收尾")
        self.pos += 1
        outer = self.interp.globals
        self.interp.funcs[name] = (params, body)
        return ("declare", "__func_%s__" % name, ("lit", None))


# --------------------------------------------------------------------------
# 6. 入口
# --------------------------------------------------------------------------

def run_file(path):
    with open(path, "r", encoding="utf-8") as f:
        src = f.read()
    p = Parser(logical_lines(src))
    body = p.parse_program()
    interp = p.interp
    interp.globals = {}
    try:
        interp.exec_block(body, interp.globals)
    except ReturnSig as r:
        return r.v
    except BreakSig:
        raise LolError("GTFO 出现在了函数外面")


def main(argv):
    if len(argv) < 2:
        print("用法: python lolrun.py <file.lol>")
        print("      python lolrun.py --selftest")
        return 2
    if argv[1] == "--selftest":
        return selftest()
    try:
        run_file(argv[1])
    except LolError as e:
        print("\n[LOLERROR] %s" % e, file=sys.stderr)
        return 1
    return 0


def selftest():
    ok = True
    cases = [
        ('HAI 1.2\nVISIBLE "HAI WORLD!"\nKTHXBAI', "HAI WORLD!\n"),
        ('HAI 1.2\nI HAS A X ITZ 3\nX R SUM OF X AN 4\nVISIBLE X\nKTHXBAI', "7\n"),
        ('HAI 1.2\nI HAS A I ITZ 0\nIM IN YR L UPPIN YR I AN 1 TIL BOTH SAEM I AN 3\n'
         'VISIBLE I\nIM OUTTA YR L\nKTHXBAI', "0\n1\n2\n"),
        ('HAI 1.2\nVISIBLE SMOOSH "A" AN "B" AN 7 MKAY\nKTHXBAI', "AB7\n"),
        ('HAI 1.2\nVISIBLE "NO NEWLINE"!\nVISIBLE "!"\nKTHXBAI', "NO NEWLINE!\n"),
        ('HAI 1.2\nI HAS A X ITZ 2\nX IS NOW A YARN\nVISIBLE SMOOSH X AN "3" MKAY\nKTHXBAI', "23\n"),
        ('HAI 1.2\nBOTH SAEM 1 AN 1\nO RLY?\nYA RLY\nVISIBLE "WIN"\nNO WAI\nVISIBLE "FAIL"\nOIC\nKTHXBAI',
         "WIN\n"),
        ('HAI 1.2\nHOW IZ I ADD YR A AN YR B\nFOUND YR SUM OF A AN B\nIF U SAY SO\n'
         'VISIBLE I IZ ADD YR 2 AN YR 40 MKAY\nKTHXBAI', "42\n"),
    ]
    import io
    for i, (src, expected) in enumerate(cases, 1):
        p = Parser(logical_lines(src))
        try:
            body = p.parse_program()
            buf = io.StringIO()
            p.interp.out = buf
            p.interp.globals = {}
            p.interp.exec_block(body, p.interp.globals)
            got = buf.getvalue()
        except Exception as e:  # noqa: BLE001
            got = "<异常: %s>" % e
        mark = "OK " if got == expected else "FAIL"
        if got != expected:
            ok = False
        print("[%s] case %d  期望=%r  实际=%r" % (mark, i, expected, got))
    print("\nselftest:", "全部通过" if ok else "有失败")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
