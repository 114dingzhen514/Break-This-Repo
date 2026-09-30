# 🍔 LOLCODE 接管计划 · LOLCODE TAKEOVER

```lolcode
HAI 1.2
    VISIBLE "I HAS A REPO."
    VISIBLE "REPO IS NOW A LOLCODE PROGRAM."
    VISIBLE "KTHXBAI."
KTHXBAI
```

> **一句话：** 这个目录里放着一份 **1.42 MB、26,104 行、真的能跑** 的 LOLCODE 程序。
> 跑一下，它会把 `translations/` 里 **52 种人类语言的 README** 一字不差地吐出来。

---

## 1. 为什么要做这件事

因为 **这个仓库的 GitHub 语言统计条已经死了。**

这不是比喻，是实锤。你可以自己验：

```bash
curl -s https://api.github.com/repos/KrisTHL181/Break-This-Repo/languages
# => {}
```

| 检查项 | 结果 |
| --- | --- |
| `GET /repos/KrisTHL181/Break-This-Repo/languages` | `{}` —— 空的 |
| 仓库主页侧栏 JSON `sidebarAbout.sections.languages` | `false` |
| 仓库体积（GitHub API `size` 字段） | `16,210,352` KB ≈ **16.2 GB** |
| 对照：`awwwsl/Break-This-Repo`（2.5 GB，2026-09-13 建） | `"languages": true`，**有**完整语言条 |

结论很清楚：GitHub 的 Linguist 在仓库超过约 2.5–3 GB 之后就不再出统计了。
这个仓库早被 OpenJDK 全量源码一类的上传撑过了那条线，语言条从此不再渲染。
**连新 fork 也一样** —— 现在 fork 一份，语言条照样是空的。

主仓库 `.gitattributes` 里那句注释，是这个仓库还健康时的遗言：

```gitattributes
# Rebalance GitHub language stats drowned by mega Rust trees.
*.rs linguist-vendored
**/target/** linguist-vendored
```

所以，**往这个仓库里灌几百 MB 的 `.bf` / `.lol` 是没用的** —— 语言条不会动，
你只是又往一个 16 GB 的仓库里加了几百 MB。那是破坏，不是乐子。

**那怎么办？**

> 语言**条**刷不动，那就把**语言本身**刷掉。

这个仓库最引以为傲的东西是 `translations/` 的「🧲 语言磁力入口」—— 52 种人类语言、
184 种目标语种、带结构校验的译文体系。它是这个仓库的招牌。

那我们就把它整个编译掉：**52 种人类语言 → 1 种编程语言。**

---

## 2. 目录里有什么

| 文件 | 大小 | 说明 |
| --- | --- | --- |
| `lolrun.py` | ~24 KB | **零依赖的 LOLCODE 1.2 子集解释器。** 只需要 Python 3.8+，不装任何东西 |
| `CHEEZBURGER.lol` | **1.42 MB** | 人类历史上最大的 LOLCODE 程序。26,104 行，全部是 `VISIBLE` |
| `BREAK_THIS_REPO.lol` | ~3 KB | 手写的演示程序：有函数、有循环、有 `O RLY?`、有 `WTF?`、有返回值 |
| `HAI_WORLD.lol` | <1 KB | 最小可运行样本 |
| `gen_cheezburger.py` | ~4 KB | 编译器本体。把 `translations/` 编译成 `CHEEZBURGER.lol`，可复现 |
| `CHEEZBURGER.lol.expected.txt` | 1.16 MB | 校验基准。`CHEEZBURGER.lol` 的 stdout 应当与它**逐字节相同** |

---

## 3. 怎么跑（30 秒）

```bash
# 1. 最小样本
python lolrun.py HAI_WORLD.lol

# 2. 手写演示（有 ASCII art）
python lolrun.py BREAK_THIS_REPO.lol

# 3. 拉满的那发炮弹
python lolrun.py CHEEZBURGER.lol > out.txt
wc -c out.txt          # 1216343
```

**它不挑平台。** `lolrun.py` 强制 `newline="\n"` 输出，所以在 Windows 上跑出来的字节
和 Linux 上一模一样 —— 这也是下面那条校验能成立的前提。

### 自检

```bash
python lolrun.py --selftest
```

会跑 8 组内置用例（变量、算术、循环、`SMOOSH`、`MAEK`、`O RLY?`、函数与返回值、
无换行 `VISIBLE`）。全绿才算这个解释器是活的。

### 逐字节复现

```bash
python lolrun.py CHEEZBURGER.lol | diff - CHEEZBURGER.lol.expected.txt && echo "一字不差"
```

实测：26,104 行程序，0.37 秒跑完，输出 **1,216,343 字节**，与基准**完全相同**。

---

## 4. 关于「最大」

据我们所知，**`CHEEZBURGER.lol` 是现存最大的 LOLCODE 程序**。

- 经典的 LOLCODE 程序（`HAI WORLD` 那种）通常在 **100 字节**量级。
- 网上能找到的最复杂的 LOLCODE 实现（比如 LOLCODE 写的解释器）也不到 **100 KB**。
- 这个文件是 **1.42 MB**，量级上大了一到两个数量级。

而且它**不是二进制垃圾，不是随机字节**：每一行都是合法的 LOLCODE，
每一个字符都来自本仓库自己的 `translations/` 目录。它打印出来的东西，
就是那 52 份 README 本身。

**这不是灌水，这是编译。**

---

## 5. LOLCODE 速查表

如果你从没读过 LOLCODE，看这张表就够读懂这个目录：

| LOLCODE | 意思 |
| --- | --- |
| `HAI 1.2` | 程序开始（对应别的语言的 `int main()`） |
| `KTHXBAI` | 程序结束（对应 `return 0`） |
| `VISIBLE x` | 打印 x。结尾加 `!` 表示不换行 |
| `I HAS A x ITZ 5` | 声明变量，类型随缘 |
| `x R 5` | 赋值 |
| `SUM OF a AN b` | a + b。同理 `DIFF OF` / `PRODUKT OF` / `QUOSHUNT OF` / `MOD OF` |
| `BOTH SAEM a AN b` | a == b，返回 `WIN` / `FAIL` |
| `O RLY?` | if。配 `YA RLY` / `MEBBE` / `NO WAI` / `OIC` |
| `WTF?` | switch。配 `OMG <值>` / `OMGWTF` / `OIC` |
| `IM IN YR L ... IM OUTTA YR L` | 循环。`UPPIN YR i` 自增，`TIL <cond>` 条件停 |
| `HOW IZ I f ... IF U SAY SO` | 定义函数。`FOUND YR x` 返回 |
| `I IZ f MKAY` | 调用函数 |
| `BTW ...` | 行注释 |
| `CHEEZBURGER` | 没有语法作用。就是好吃 |

---

## 6. 解释器实现的边界

`lolrun.py` 是一个**子集**实现，不是完整规范。它**故意**在遇到不认识的东西时大声报错，
而不是默默给你一个编出来的答案。已实现的部分见文件头注释；已知未实现：

- `GTFO` 跳出多层嵌套的精细语义
- `SRS` / `NOSRS`（数组）
- 用户自定义 `BUKKIT`
- `VISIBLE` 的多参数分隔符 `+`

如果你发现它跑错了，**那是 bug，请开 issue** —— 但请附上能复现的最小 `.lol` 文件。

---

## 7. 声明

- 这个目录**不碰 `.github/`**（那是受保护目录）。
- `CHEEZBURGER.lol` 的每一个字节都来自本仓库自己的 `translations/`，**没有引入任何外部内容**，
  也不涉及任何新文件的版权问题。
- 解释器 `lolrun.py` 为本目录原创，随便用。
- 我们不建议你把这个仓库 clone 下来。它 16 GB。真的。

```lolcode
VISIBLE "I HAS A POINT. ITZ '语言条死了，但乐子还活着。'"
KTHXBAI
```
