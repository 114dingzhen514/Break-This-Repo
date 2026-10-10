/* ============================================================================
 *  bubble_sort_anim.c  --  冒泡排序动画演示 (Windows 控制台)
 *
 *  把数组画成一根根竖条, 逐帧显示:
 *      - 正在比较的两个元素      (黄色)
 *      - 发生交换                (红色 + 两个条子上方出现 <-> 箭头)
 *      - 本轮结束后归位的元素    (绿色, 之后不再参与比较)
 *      - 还没有排好的元素        (灰蓝色)
 *
 *  【原始数据完全由用户自定义】四种来源:
 *      1) 命令行:    bubble_sort_anim.exe 5 3 8 1 9 2
 *      2) 输入向导:  不带数字运行 -> 选录入方式 -> 收集数据 -> 回显确认
 *      3) 交互编辑:  每轮动画结束后进入菜单, 可改/增/删/插/粘贴/打乱, 再重跑
 *      4) 随机生成:  --random N  (可配 --seed 复现同一组数据)
 *
 *  可选参数:
 *      --delay-ms N   每帧间隔毫秒数 (默认 40, 越小越快)
 *      --scale N      每个数字占几列 (默认 5)
 *      --random N     随机生成 N 个数, 省得手打
 *      --seed N       随机种子, 同一个种子产生同一组数据 (便于复现/对比)
 *      --no-color     关掉颜色 (重定向到文件/老旧终端时用)
 *      --help         显示帮助
 *
 *  编译 (MinGW-w64 / gcc):
 *      gcc -O2 -Wall -Wextra -std=c99 -o bubble_sort_anim.exe bubble_sort_anim.c
 *  编译 (MSVC):
 *      cl bubble_sort_anim.c
 *
 *  注意: MinGW 的汇编器无法写入含中文的绝对路径, 所以要在源文件目录里用
 *        相对文件名做输出 (见 README 第一节)。
 * ==========================================================================*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <conio.h>         /* _kbhit / _getch */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_N 128          /* 最多支持多少个数字 */
#define MAX_VAL 999999     /* 单个数字上限, 用于限制条子宽度 */
#define BUF_LINE 4096      /* 一行输入的缓冲大小 */

/* ------------------------------------------------------------------ 颜色 */
typedef enum {
    COL_RESET = 0,
    COL_PENDING,      /* 还没排到的元素 */
    COL_COMPARE,      /* 正在比较 */
    COL_SWAP,         /* 正在交换 */
    COL_DONE,         /* 已归位 */
    COL_LABEL,        /* 底部数值标签 */
    COL_TITLE,        /* 顶部标题 */
    COL_NOTE,         /* 状态说明 */
    COL_COUNT
} ColorId;

static const char *kAnsi[COL_COUNT] = {
    "\x1b[0m",                  /* RESET     */
    "\x1b[38;5;110m",           /* PENDING   灰蓝 */
    "\x1b[38;5;226m",           /* COMPARE   亮黄 */
    "\x1b[38;5;203m",           /* SWAP      红   */
    "\x1b[38;5;120m",           /* DONE      绿   */
    "\x1b[38;5;250m",           /* LABEL     浅灰 */
    "\x1b[1;38;5;231m",         /* TITLE     白粗 */
    "\x1b[38;5;180m"            /* NOTE      米黄 */
};

/* 非 ANSI 模式下的退化颜色 (SetConsoleTextAttribute) */
static const WORD kWin[COL_COUNT] = {
    0,
    FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    FOREGROUND_RED | FOREGROUND_INTENSITY,
    FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY
};

static int g_use_ansi = 0;     /* 1 = 用 ANSI 转义序列, 0 = 用 Win32 API */
static int g_use_color = 1;
static int g_is_tty = 0;       /* stdin 是不是真控制台 (管道输入时为 0) */
static int g_forced_tty = 0;   /* 1 = BUBBLE_ANIM_FORCE_TTY 强行打开的交互 */
static int g_out_is_console = 0;  /* stdout 是不是真控制台: 决定能不能"就地重画" */
static HANDLE g_out;

/* ------------------------------------------------------------------ 控制台 */
static void con_init(void)
{
    DWORD mode = 0;

    g_out = GetStdHandle(STD_OUTPUT_HANDLE);

    /* --- 输出端: 能不能"就地重画"? ---
       只有 stdout 真的挂在控制台上时, gotoxy/cls/ANSI 才有意义。
       VS Code 的调试控制台、管道、重定向文件都不行: 定位会失效, 画面会一行行
       堆在一起, 而且 \x1b[2J 会被当成换页符印出乱码。 */
    if (GetConsoleMode(g_out, &mode)) {
        g_out_is_console = 1;
        if (SetConsoleMode(g_out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
            g_use_ansi = 1;
    }
    SetConsoleOutputCP(CP_UTF8);
    if (g_out_is_console && !g_use_ansi) {   /* 老系统: 退回 Win32 颜色 API */
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(g_out, &csbi))
            SetConsoleTextAttribute(g_out, csbi.wAttributes);
    }

    /* --- 输入端: 能不能问用户? ---
       管道/重定向时不进入交互菜单, 免得挂死。
       设了 BUBBLE_ANIM_FORCE_TTY=1 就整个假装成真控制台 (自动化测试/录演示时用):
       交互流程可以从管道喂进去, 不会被跳过; 但既然是假装出来的, 画面仍然只走
       线性文本输出, 也不插入"按回车继续"这种等人操作。
       1 = 全部假装, 2 = 只假装输出端 (想单独试 ANSI 重画时用) */
    if (GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode))
        g_is_tty = 1;
    {
        const char *f = getenv("BUBBLE_ANIM_FORCE_TTY");
        if (f != NULL && (f[0] == '1' || f[0] == '2')) {
            if (f[0] == '1') {
                g_is_tty = 1;
                g_forced_tty = 1;
            }
            g_out_is_console = 1;
        }
    }
}

static void con_color(ColorId c)
{
    if (!g_use_color) return;
    if (g_use_ansi) {
        fputs(kAnsi[c], stdout);
    } else {
        SetConsoleTextAttribute(g_out, c == COL_RESET ? 7 : kWin[c]);
    }
}

static void con_reset(void) { con_color(COL_RESET); }

static void gotoxy(int x, int y)
{
    COORD p;
    if (!g_out_is_console) return;
    p.X = (SHORT)x;
    p.Y = (SHORT)y;
    SetConsoleCursorPosition(g_out, p);
}

static void hide_cursor(int hide)
{
    CONSOLE_CURSOR_INFO ci;
    if (!g_out_is_console) return;
    if (GetConsoleCursorInfo(g_out, &ci)) {
        ci.bVisible = hide ? FALSE : TRUE;
        SetConsoleCursorInfo(g_out, &ci);
    }
}

static int term_width(void)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(g_out, &csbi))
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    return 80;
}

static int term_height(void)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(g_out, &csbi))
        return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    return 25;
}

/* 睡眠。Sleep 默认精度约 15ms, 想要更准可以在启动时 timeBeginPeriod(1) */
static void sleep_ms(int ms)
{
    if (ms <= 0) return;
    Sleep((DWORD)ms);
}

/* 清屏。输出被重定向(管道/文件/调试控制台)时 system("cls") 会吐出乱码, 所以分开处理 */
static void con_clear(void)
{
    if (!g_out_is_console) return;       /* 不是控制台就没"屏"可清 */
    if (g_use_ansi)
        fputs("\x1b[2J\x1b[H", stdout);
    else
        system("cls");
    fflush(stdout);
}

/* ------------------------------------------------------------------ 输入基础 */

/* 把当前行里还没读走的字符丢掉, 防止它污染下一次读取 */
static void drain_stdin(void)
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { /* 丢掉 */ }
}

/* 读一行。返回 0 = 读到 EOF(输入没了), 1 = 正常读到一行 */
static int read_line(char *buf, int n)
{
    if (fgets(buf, n, stdin) == NULL) return 0;
    if (strchr(buf, '\n') == NULL) drain_stdin();   /* 这一行太长被截断了 */
    return 1;
}

/* 把 "123" 解析成 0~MAX_VAL 的整数; 成功返回 1 */
static int parse_int_token(const char *s, int *out)
{
    char *end;
    long v;
    if (s == NULL || *s == '\0') return 0;
    v = strtol(s, &end, 10);
    if (*end != '\0') return 0;                    /* 后面还有非数字字符 */
    if (v < 0 || v > MAX_VAL) return 0;
    *out = (int)v;
    return 1;
}

/* 读一个整数并校验范围。返回 0 = 用户放弃(q/Q/EOF), 1 = 拿到合法值 */
static int ask_int(const char *prompt, int lo, int hi, int *out)
{
    char buf[256];

    for (;;) {
        char *end;
        long v;

        fputs(prompt, stdout);
        fflush(stdout);
        if (!read_line(buf, sizeof(buf))) return 0;      /* EOF: 当成放弃 */

        if (buf[0] == 'q' || buf[0] == 'Q') return 0;
        if (buf[0] == '\n' || buf[0] == '\r' || buf[0] == '\0') {
            printf("  请按提示输入数字, 或输入 q 取消。\n");
            continue;
        }

        v = strtol(buf, &end, 10);
        while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') end++;
        if (*end != '\0') { printf("  '%s' 不是整数。\n", buf); continue; }
        if (v < lo || v > hi) {
            printf("  超出范围, 需要 %d ~ %d。\n", lo, hi);
            continue;
        }
        *out = (int)v;
        return 1;
    }
}

/* 先问数量: 回车用默认值, 输入 q 取消 */
static int ask_count(const char *prompt, int def, int lo, int hi, int *out)
{
    char buf[256];
    char full[320];

    sprintf(full, "%s (回车 = %d, 输入 q 取消): ", prompt, def);
    for (;;) {
        char *end;
        long v;

        fputs(full, stdout);
        fflush(stdout);
        if (!read_line(buf, sizeof(buf))) return 0;

        if (buf[0] == 'q' || buf[0] == 'Q') return 0;
        if (buf[0] == '\n' || buf[0] == '\r' || buf[0] == '\0') {
            *out = def;
            return 1;
        }

        v = strtol(buf, &end, 10);
        while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') end++;
        if (*end != '\0') { printf("  请输入整数。\n"); continue; }
        if (v < lo || v > hi) {
            printf("  超出范围, 需要 %d ~ %d。\n", lo, hi);
            continue;
        }
        *out = (int)v;
        return 1;
    }
}

/* 回车/EOF 都当作"是", 只有明确的 n/N 才是"否" */
static int ask_yes(const char *q)
{
    char buf[64];
    printf("%s? [Y/n] ", q);
    fflush(stdout);
    if (!read_line(buf, sizeof(buf))) return 1;
    if (buf[0] == 'n' || buf[0] == 'N') return 0;
    return 1;
}

/* 读一行数字, 空格/逗号/制表符分隔。非法词跳过并提示, 返回读到的个数 */
static int parse_numbers(const char *line, int *out, int maxn, int quiet)
{
    char tmp[BUF_LINE];
    char *tok;
    int n = 0;

    strncpy(tmp, line, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    tok = strtok(tmp, " ,\t\r\n");
    while (tok != NULL) {
        if (n >= maxn) {
            if (!quiet) printf("  (最多 %d 个, 后面的忽略)\n", maxn);
            break;
        }
        if (!parse_int_token(tok, &out[n])) {
            if (!quiet) printf("  '%s' 不是 0~%d 的数字, 已跳过\n", tok, MAX_VAL);
        } else {
            n++;
        }
        tok = strtok(NULL, " ,\t\r\n");
    }
    return n;
}

/* 打印数组: "5 3 8 1", 每 16 个换一行免得刷屏 */
static void show_numbers(const int *a, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        if (i > 0 && i % 16 == 0) printf("\n  ");
        printf("%d ", a[i]);
    }
    printf("\n");
}

/* ------------------------------------------------------------------ 动画状态 */
typedef struct {
    int  a[MAX_N];        /* 当前数组 */
    int  n;               /* 元素个数 */
    int  i, j;            /* 当前比较的一对下标 */
    int  swapping;        /* 本帧是否发生了交换 */
    int  sorted_from;     /* 从哪个下标开始已经有序 (尾部已归位) */
    int  pass_no;         /* 第几轮 (从 1 开始) */
    int  total_compare;   /* 累计比较次数 */
    int  total_swap;      /* 累计交换次数 */
    const char *note;     /* 底部说明文字 */
} State;

/* 每个元素的显示宽度 (列数) */
static int  g_scale = 5;
static int  g_delay_ms = 40;
static char g_bar[MAX_N][16];   /* 预生成的条子字符串 */

static void build_bars(const State *st)
{
    int maxv = 1, i, k;
    char unit[16];

    for (i = 0; i < st->n; i++)
        if (st->a[i] > maxv) maxv = st->a[i];

    for (i = 0; i < st->n; i++) {
        /* 用 '#' 的数量表示大小, 保证最小也有 1 格, 看得见 */
        int len = (int)((double)st->a[i] / (double)maxv * (g_scale - 2) + 0.5);
        if (len < 1) len = 1;
        if (len > g_scale - 1) len = g_scale - 1;
        for (k = 0; k < len; k++) unit[k] = '#';
        unit[len] = '\0';
        /* 右对齐补空格, 这样条子底部对齐 */
        for (k = 0; k < g_scale - len; k++) g_bar[i][k] = ' ';
        strcpy(g_bar[i] + k, unit);
    }
}

/* 输出不是控制台时的降级画面: 老老实实一行一行往下印, 不用光标定位、
   不用清屏。VS Code 调试控制台 / 管道 / 重定向文件里只有这种画法是对的
   (定位会失效, \x1b[2J 还会被当成换页符变成乱码)。 */
static void render_plain_line(const State *st)
{
    int i;

    printf("%2d |", st->pass_no < 10 ? st->pass_no : 9);
    for (i = 0; i < st->n; i++) {
        char cell[4];
        int big = (st->a[i] > 9);
        char tag = (char)('0' + (st->a[i] % 10));

        if (st->j >= 0 && i == st->j) {
            cell[0] = st->swapping ? '<' : '[';
            cell[1] = big ? '#' : tag;
            cell[2] = ' ';
        } else if (st->j >= 0 && i == st->j + 1) {
            cell[0] = big ? '#' : tag;
            cell[1] = st->swapping ? '>' : ']';
            cell[2] = ' ';
        } else if (st->sorted_from >= 0 && i >= st->sorted_from) {
            cell[0] = big ? '#' : tag;
            cell[1] = '.';
            cell[2] = ' ';
        } else {
            cell[0] = big ? '#' : tag;
            cell[1] = ' ';
            cell[2] = ' ';
        }
        cell[3] = '\0';
        fputs(cell, stdout);
    }
    printf("  比较%d 交换%d", st->total_compare, st->total_swap);
    if (st->j >= 0)
        printf("  a[%d]=%d vs a[%d]=%d%s", st->j, st->a[st->j], st->j + 1,
               st->a[st->j + 1], st->swapping ? " 交换" : "");
    printf("\n");

    fflush(stdout);
    sleep_ms(g_delay_ms);
}

static void render(const State *st)
{
    int i, row;
    int maxv = 1;
    int rows;
    int tw;
    int legend;                        /* 右侧说明文字要留的列数 */
    char line[256];

    /* 输出不是真控制台就没法就地重画, 直接走线性文本画面 */
    if (!g_out_is_console) {
        render_plain_line(st);
        return;
    }

    tw = term_width();                 /* 终端宽度, 超出会折行, 必须裁剪 */
    for (i = 0; i < st->n; i++)
        if (st->a[i] > maxv) maxv = st->a[i];

    rows = g_scale;                    /* 条子最大高度 = 每个元素的列宽 */
    if (rows > term_height() - 8) rows = term_height() - 8;
    if (rows < 2) rows = 2;

    build_bars(st);

    gotoxy(0, 0);

    /* ---------- 标题行 ---------- */
    con_color(COL_TITLE);
    sprintf(line, "冒泡排序动画演示   元素 %d 个   第 %d 轮   比较 %d 次   交换 %d 次",
            st->n, st->pass_no, st->total_compare, st->total_swap);
    fputs(line, stdout);
    con_reset();
    fputs("\n", stdout);

    con_color(COL_NOTE);
    if (st->j >= 0)
        sprintf(line, "当前比较: a[%d]=%d  与  a[%d]=%d %s",
                st->j, st->a[st->j], st->j + 1, st->a[st->j + 1],
                st->swapping ? "-> 需要交换!" : "-> 顺序正确");
    else
        sprintf(line, "本轮结束");
    fputs(line, stdout);
    con_reset();
    fputs("\n\n", stdout);

    /* 右侧说明: 只在屏幕放得下时才写, 否则不写, 免得折行把画面搞乱 */
    legend = (st->n * g_scale + 24 <= tw) ? 1 : 0;

    /* ---------- 条形图: 从上往下画 ---------- */
    for (row = rows - 1; row >= 0; row--) {
        for (i = 0; i < st->n; i++) {
            int height = (int)((double)st->a[i] / (double)maxv * rows + 0.999);
            int is_compare = (i == st->j || i == st->j + 1);
            int is_done = (st->sorted_from >= 0 && i >= st->sorted_from);
            char cell[32];

            if (row < height) {
                if (is_compare)
                    con_color(st->swapping ? COL_SWAP : COL_COMPARE);
                else if (is_done)
                    con_color(COL_DONE);
                else
                    con_color(COL_PENDING);
                strcpy(cell, g_bar[i]);
            } else {
                con_reset();
                memset(cell, ' ', (size_t)g_scale);
                cell[g_scale] = '\0';
            }
            fputs(cell, stdout);
        }
        con_reset();
        /* 行尾的说明文字 */
        if (legend) {
            if (row == rows - 1) {
                con_color(COL_NOTE);  fputs("  <- 条子高度 = 数字大小", stdout);
            } else if (row == rows - 2) {
                con_color(COL_DONE);  fputs("  <- 绿色 = 已归位", stdout);
            } else if (row == rows - 3) {
                con_color(COL_COMPARE); fputs("  <- 黄色 = 正在比较", stdout);
            } else if (row == rows - 4) {
                con_color(COL_SWAP);  fputs("  <- 红色 = 正在交换", stdout);
            }
            con_reset();
        }
        fputs("\n", stdout);
    }

    /* ---------- 交换箭头 ---------- */
    con_reset();
    for (i = 0; i < st->n; i++) {
        if (st->swapping && i == st->j) {
            con_color(COL_SWAP);
            sprintf(line, "%-*s", g_scale, "<->");
            fputs(line, stdout);
            i++;                                  /* 箭头占两格 */
            con_reset();
        } else {
            sprintf(line, "%-*s", g_scale, "");
            fputs(line, stdout);
        }
    }
    fputs("\n", stdout);

    /* ---------- 分隔线 + 数值标签 ---------- */
    con_color(COL_LABEL);
    for (i = 0; i < st->n * g_scale && i < 200; i++) fputc('-', stdout);
    con_reset();
    fputs("\n", stdout);

    for (i = 0; i < st->n; i++) {
        int is_compare = (i == st->j || i == st->j + 1);
        int is_done = (st->sorted_from >= 0 && i >= st->sorted_from);

        if (is_compare)      con_color(st->swapping ? COL_SWAP : COL_COMPARE);
        else if (is_done)    con_color(COL_DONE);
        else                 con_color(COL_LABEL);

        sprintf(line, "%-*d", g_scale, st->a[i]);
        fputs(line, stdout);
    }
    con_reset();
    fputs("\n", stdout);

    /* ---------- 底部提示 ---------- */
    con_color(COL_NOTE);
    sprintf(line, "%-70s", st->note ? st->note : "");
    fputs(line, stdout);
    con_reset();
    fputs("\n", stdout);

    fflush(stdout);
    sleep_ms(g_delay_ms);
}

/* ------------------------------------------------------------------ 排序 */
static void bubble_sort_anim(State *st)
{
    int i, j, tmp, swapped;

    st->sorted_from = st->n;          /* 一开始没有任何元素归位 */
    st->total_compare = st->total_swap = 0;

    for (i = 0; i < st->n - 1; i++) {
        st->pass_no = i + 1;
        swapped = 0;

        st->note = "从左到右两两比较, 前面比后面大就交换";
        for (j = 0; j < st->n - 1 - i; j++) {
            st->j = j;
            st->swapping = 0;

            st->total_compare++;
            render(st);                            /* 第 1 帧: 亮出正在比较的一对 */

            if (st->a[j] > st->a[j + 1]) {
                st->swapping = 1;
                render(st);                        /* 第 2 帧: 亮出发生交换 */
                tmp = st->a[j];
                st->a[j] = st->a[j + 1];
                st->a[j + 1] = tmp;
                st->total_swap++;
                swapped = 1;
                render(st);                        /* 第 3 帧: 展示交换后的结果 */
            }
        }

        /* 本轮最大的元素已经冒到最后, 标记为已归位 */
        st->sorted_from = st->n - 1 - i;
        st->j = -1;
        st->swapping = 0;
        st->note = "本轮结束: 最大的元素已经冒到右端, 锁定它";
        render(st);

        if (!swapped) {                            /* 一趟下来没交换 -> 已经有序 */
            st->sorted_from = 0;
            st->note = "本轮没有发生任何交换, 说明数组已经有序, 提前结束";
            render(st);
            break;
        }
    }

    /* 收尾全绿 */
    st->sorted_from = 0;
    st->j = -1;
    st->swapping = 0;
    st->note = "排序完成!";
    render(st);
}

/* 跑完整段动画(含清屏/隐藏光标/最终结果); 返回时 st->a 已经有序 */
static void run_sort(State *st)
{
    int i;

    st->j = -1;
    st->swapping = 0;
    st->pass_no = 0;
    st->sorted_from = st->n;
    st->total_compare = st->total_swap = 0;
    st->note = "准备开始";

    con_reset();
    con_clear();
    hide_cursor(1);

    bubble_sort_anim(st);

    /* 动画最后一帧停在屏幕中段, 清屏后再打印总结, 保证结果干净 */
    con_reset();
    con_clear();
    hide_cursor(0);

    con_color(COL_DONE);
    printf("最终结果: ");
    for (i = 0; i < st->n; i++) printf("%d ", st->a[i]);
    printf("\n");
    con_reset();
    printf("共比较 %d 次, 交换 %d 次。\n", st->total_compare, st->total_swap);
    printf("(n=%d 的冒泡排序, 最坏情况需要 n(n-1)/2 = %d 次比较)\n",
           st->n, st->n * (st->n - 1) / 2);
}

/* ------------------------------------------------------------------ 数据录入 */

/* 方式一: 一次输入一整行。返回元素个数, -1 = 取消
   行内数字不够/全是非法词时就在原地重问, 不用退回主菜单重新选 */
static int input_line_mode(int *out, int maxn)
{
    char buf[BUF_LINE];

    for (;;) {
        int n;

        printf("\n请输入数字, 空格或逗号分隔, 回车结束 (最多 %d 个, 输入 q 取消):\n> ", maxn);
        fflush(stdout);
        if (!read_line(buf, sizeof(buf))) return -1;
        if (buf[0] == 'q' || buf[0] == 'Q') return -1;

        n = parse_numbers(buf, out, maxn, 0);
        if (n >= 2) return n;

        printf("至少需要 2 个数字 (这一行只读到 %d 个), 请重新输入。\n", n);
    }
}

/* 方式二: 先问数量, 再逐个录入, 每输一个立刻校验并回显。返回个数, -1 = 取消 */
static int input_step_mode(int *out, int maxn)
{
    for (;;) {
        int n, i, cnt;

        printf("\n先告诉我这一组要排几个数, 然后一个一个输入。\n");
        if (!ask_count("元素个数", 5, 2, maxn, &cnt)) return -1;

        for (i = 0; i < cnt; i++) {
            char p[96];
            sprintf(p, "  第 %d/%d 个 (q 取消整组): ", i + 1, cnt);
            if (!ask_int(p, 0, MAX_VAL, &out[i])) {
                printf("\n  这一组作废 (刚输了 %d 个), 重新来。\n", i);
                break;
            }
            printf("      已记下 %d, 当前: ", out[i]);
            for (n = 0; n <= i; n++) printf("%d ", out[n]);
            printf("\n");
        }
        if (i == cnt) return cnt;      /* 整组输完了 */
    }
}

/* 方式三: 随机生成。返回个数, -1 = 取消 */
static int input_random_mode(int *out, int maxn)
{
    static unsigned int extra = 1;      /* 菜单里连续随机时换一组, 不重复 */
    int cnt, i;

    if (!ask_count("随机生成几个", 10, 2, maxn, &cnt)) return -1;

    srand((unsigned)time(NULL) + extra++ * 7919u);
    for (i = 0; i < cnt; i++) out[i] = rand() % 99 + 1;
    printf("\n已生成 %d 个随机数 (1~99)。\n", cnt);
    return cnt;
}

/* 输入向导: 三种录入方式任选, 回显确认后才采用 */
static void input_wizard(State *st)
{
    for (;;) {
        int n = 0, choice;

        printf("\n=========== 自定义原始数据 ===========\n");
        printf("  1    一行输入全部数字   例: 5 3,8 1,9 2\n");
        printf("  2    先定数量, 再逐个输入 (带即时校验)\n");
        printf("  3    随机生成一组\n");
        printf("  0    退出程序\n");
        printf("=====================================\n");
        if (!ask_int("请选择 [0-3]: ", 0, 3, &choice)) return;   /* EOF/q: 退出 */
        if (choice == 0) return;

        if (choice == 1)      n = input_line_mode(st->a, MAX_N);
        else if (choice == 2) n = input_step_mode(st->a, MAX_N);
        else                  n = input_random_mode(st->a, MAX_N);

        if (n < 2) {                    /* 取消或者数量不够 -> 回向导开头 */
            if (n == 0) printf("(没有读到有效数字, 请重新来)\n");
            continue;
        }

        printf("\n这一组原始数据 (%d 个):\n  ", n);
        show_numbers(st->a, n);
        if (ask_yes("就用这一组开始排序")) {
            st->n = n;
            return;
        }
        printf("好, 重新来。\n");
    }
}

/* ------------------------------------------------------------------ 交互编辑菜单 */

static void shuffle_array(int *a, int n)
{
    int i;
    for (i = n - 1; i > 0; i--) {
        int k = rand() % (i + 1);
        int t = a[i]; a[i] = a[k]; a[k] = t;
    }
}

/* 数据太长时只显示前 24 个, 免得刷屏 */
static void show_brief(const int *a, int n)
{
    int i, lim = n < 24 ? n : 24;
    printf("  当前数据 (%d 个): ", n);
    for (i = 0; i < lim; i++) printf("%d ", a[i]);
    if (lim < n) printf("...");
    printf("\n");
}

/* 返回 1 = 退出程序, 0 = 用当前数据重新跑动画
   clear = 1 时先把屏幕擦干净再画 (否则接着上面往下印, 保留上一步的回显) */
static int edit_menu(State *st, int clear)
{
    static const char *kOpt =
        "[e]改一个 [a]追加 [d]删除 [i]插入 [p]粘贴 [s]打乱 [g]随机\n"
        "  [t]调速 [l]列出 [r]重跑(回车) [q]退出";

    if (clear) con_clear();

    for (;;) {
        int c;

        printf("\n------------------ 编辑原始数据 ------------------\n");
        show_brief(st->a, st->n);
        printf("  每帧 %d 毫秒\n", g_delay_ms);
        printf("  %s\n", kOpt);
        printf("请选择: ");
        fflush(stdout);

        c = getchar();
        if (c == EOF) return 1;              /* 输入没了, 直接退出 */
        drain_stdin();

        if (c == 'e' || c == 'E') {
            int idx, val;
            if (!ask_int("  要改第几个 (从 0 开始) [q 取消]: ", 0, st->n - 1, &idx)) {
                printf("  已取消。\n");
            } else if (!ask_int("  新的数值: ", 0, MAX_VAL, &val)) {
                printf("  已取消。\n");
            } else {
                printf("  a[%d]: %d -> %d\n", idx, st->a[idx], val);
                st->a[idx] = val;
            }

        } else if (c == 'a' || c == 'A') {
            int k, i, val;
            if (!ask_int("  要追加几个: ", 1, MAX_N - st->n, &k)) {
                printf("  已取消。\n");
                continue;
            }
            for (i = 0; i < k; i++) {
                char p[160];   /* 中文提示按 UTF-8 算字节, 留够 */
                sprintf(p, "  第 %d/%d 个新元素 [q 放弃本次追加]: ", i + 1, k);
                if (!ask_int(p, 0, MAX_VAL, &val)) {
                    printf("  追加中断, 前面加的保留。\n");
                    break;
                }
                st->a[st->n++] = val;
            }

        } else if (c == 'd' || c == 'D') {
            int idx, j;
            if (st->n <= 2) {
                printf("  至少得留 2 个元素才能演示排序。\n");
                continue;
            }
            if (!ask_int("  要删第几个 (从 0 开始) [q 取消]: ", 0, st->n - 1, &idx)) {
                printf("  已取消。\n");
                continue;
            }
            printf("  已删除 a[%d] = %d\n", idx, st->a[idx]);
            for (j = idx; j < st->n - 1; j++) st->a[j] = st->a[j + 1];
            st->n--;

        } else if (c == 'i' || c == 'I') {
            int pos, val, j;
            if (st->n >= MAX_N) {
                printf("  元素已经 %d 个, 到上限了。\n", MAX_N);
                continue;
            }
            if (!ask_int("  插到第几个 (从 0 开始) [q 取消]: ", 0, st->n, &pos)) {
                printf("  已取消。\n");
                continue;
            }
            if (!ask_int("  数值: ", 0, MAX_VAL, &val)) {
                printf("  已取消。\n");
                continue;
            }
            for (j = st->n; j > pos; j--) st->a[j] = st->a[j - 1];
            st->a[pos] = val;
            st->n++;
            printf("  已在 a[%d] 插入 %d\n", pos, val);

        } else if (c == 'p' || c == 'P') {
            int n = input_line_mode(st->a, MAX_N);
            if (n >= 2) st->n = n;

        } else if (c == 's' || c == 'S') {
            shuffle_array(st->a, st->n);
            printf("  已打乱成随机顺序。\n");

        } else if (c == 'g' || c == 'G') {
            int n = input_random_mode(st->a, MAX_N);
            if (n >= 2) st->n = n;

        } else if (c == 't' || c == 'T') {
            int v, was = g_delay_ms;
            if (!ask_int("  每帧多少毫秒 (0~5000, 0 = 不停顿) [q 取消]: ", 0, 5000, &v)) {
                printf("  已取消。\n");
            } else if (v == was) {
                printf("  还是 %d 毫秒, 没变。\n", v);
            } else {
                g_delay_ms = v;
                printf("  每帧 %d -> %d 毫秒%s\n", was, g_delay_ms,
                       g_delay_ms == 0 ? " (飞快)" : "");
            }

        } else if (c == 'l' || c == 'L') {
            printf("  全部数据 (%d 个):\n  ", st->n);
            show_numbers(st->a, st->n);

        } else if (c == 'r' || c == 'R' || c == '\n' || c == '\r') {
            return 0;                        /* 空回车也直接重跑, 最顺手 */

        } else if (c == 'q' || c == 'Q') {
            return 1;

        } else {
            printf("  '%c' 不认识, 再看一眼上面的菜单。\n", c);
        }
    }
}

/* 窗口太窄放不下时, 让用户自己减少数量; 返回 0 = 已调整好, 1 = 直接退出 */
static int fit_scale_to_window(State *st)
{
    for (;;) {
        int tw = term_width();
        int new_scale;

        if (tw < 10) return 0;                       /* 拿不到窗口宽度, 不折腾 */
        if (st->n * g_scale + 2 <= tw) return 0;      /* 放得下 */

        new_scale = (tw - 2) / st->n;
        if (new_scale >= 3) {                         /* 缩小列宽就够了 */
            g_scale = new_scale;
            printf("(数字较多, 列宽自动调整为 %d)\n", g_scale);
            return 0;
        }

        printf("数字太多 (%d 个), 屏幕宽度 %d 列放不下。\n", st->n, tw);
        printf("请减少数量, 或先把窗口拉宽 / 调整字号。\n");
        if (!g_is_tty) return 1;                      /* 管道模式没法让用户改 */
        if (edit_menu(st, 0)) return 1;
    }
}

/* ------------------------------------------------------------------ 用法/主函数 */
static void die(const char *msg)
{
    fprintf(stderr, "错误: %s\n", msg);
    exit(1);
}

static void usage(const char *prog)
{
    printf("用法:\n");
    printf("  %s [选项] 数字1 数字2 数字3 ...\n", prog);
    printf("  %s [选项]              (不带数字则进入输入向导)\n", prog);
    printf("\n选项:\n");
    printf("  --delay-ms N   每帧间隔毫秒数, 默认 %d (越小越快)\n", g_delay_ms);
    printf("  --scale N      每个数字占几列, 默认 %d\n", g_scale);
    printf("  --random N     随机生成 N 个 1~99 的数字 (2 <= N <= %d)\n", MAX_N);
    printf("  --seed N       随机种子, 同一种子出同一组数据 (配合 --random)\n");
    printf("  --no-color     关闭颜色\n");
    printf("  --help         显示本帮助\n");
    printf("\n交互 (在真控制台里运行时):\n");
    printf("  排序结束后进入编辑菜单, 可以改单个元素/增/删/插/粘贴/打乱,\n");
    printf("  按 r 或回车就用新数据重新演示 —— 不必退出程序重开。\n");
    printf("  向导和菜单里输入 q 都能退出, 管道/重定向输入时自动跳过菜单。\n");
    printf("\n例:\n");
    printf("  %s 5 3 8 1 9 2\n", prog);
    printf("  %s --random 12 --delay-ms 25\n", prog);
    printf("  %s --random 10 --seed 42      (每次都是同一组数据)\n", prog);
}

int main(int argc, char **argv)
{
    State st;
    int i, n = 0;
    int random_n = 0;
    int has_seed = 0;
    unsigned int seed = 0;
    int expect_value = 0;      /* 上一个是 --delay-ms / --scale / --random / --seed */

    con_init();

    printf("========================================\n");
    printf("      冒泡排序动画演示  (Bubble Sort)\n");
    printf("========================================\n\n");

    /* ---- 解析参数 ---- */
    for (i = 1; i < argc; i++) {
        const char *s = argv[i];
        int v;

        if (expect_value == 1) {                       /* --delay-ms */
            if (!parse_int_token(s, &v) || v < 0 || v > 5000) die("--delay-ms 需要 0~5000 的整数");
            g_delay_ms = v;
            expect_value = 0;
            continue;
        }
        if (expect_value == 2) {                       /* --scale */
            if (!parse_int_token(s, &v) || v < 3 || v > 12) die("--scale 需要 3~12 的整数");
            g_scale = v;
            expect_value = 0;
            continue;
        }
        if (expect_value == 3) {                       /* --random */
            if (!parse_int_token(s, &v) || v < 2 || v > MAX_N)
                die("--random 需要 2~128 的整数");
            random_n = v;
            expect_value = 0;
            continue;
        }
        if (expect_value == 4) {                       /* --seed */
            if (!parse_int_token(s, &v) || v < 0) die("--seed 需要 0 或正整数");
            seed = (unsigned int)v;
            has_seed = 1;
            expect_value = 0;
            continue;
        }

        if (strcmp(s, "--help") == 0 || strcmp(s, "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else if (strcmp(s, "--no-color") == 0) {
            g_use_color = 0;
        } else if (strcmp(s, "--delay-ms") == 0) {
            expect_value = 1;
        } else if (strcmp(s, "--scale") == 0) {
            expect_value = 2;
        } else if (strcmp(s, "--random") == 0) {
            expect_value = 3;
        } else if (strcmp(s, "--seed") == 0) {
            expect_value = 4;
        } else if (s[0] == '-' && s[1] != '\0') {
            printf("未知选项: %s\n\n", s);
            usage(argv[0]);
            return 1;
        } else {
            if (n >= MAX_N) { printf("最多 %d 个数字\n", MAX_N); return 1; }
            if (!parse_int_token(s, &v)) {
                printf("'%s' 不是 0~%d 的整数\n", s, MAX_VAL);
                return 1;
            }
            st.a[n++] = v;
        }
    }
    if (expect_value != 0) die("选项后面缺少数值");

    /* ---- 准备数据 ---- */
    if (random_n > 0) {
        srand(has_seed ? seed : (unsigned)time(NULL));
        st.n = random_n;
        for (i = 0; i < random_n; i++)
            st.a[i] = rand() % 99 + 1;                 /* 1~99, 动画更好看 */
    } else if (n > 0) {
        if (n < 2) die("至少需要 2 个数字才能演示排序");
        st.n = n;
    } else {
        if (!g_is_tty || !g_out_is_console)
            die("没有给数字, 而当前不是能问答的控制台, 没法进入输入向导");   /* 免得挂死 */
        input_wizard(&st);
        if (st.n < 2) return 1;                        /* 向导里选了 0: 退出 */
    }

    /* ---- 确认原始数据 ---- */
    printf("原始数据 (%d 个):\n  ", st.n);
    show_numbers(st.a, st.n);
    if (g_forced_tty) {
        printf("\n");                     /* 输入是喂进来的, 不能停下来等人 */
    } else if (g_is_tty && g_out_is_console) {
        char buf[64];
        printf("\n按回车开始动画...");
        fflush(stdout);
        if (!read_line(buf, sizeof(buf))) return 0;    /* Ctrl+Z / 输入没了 */
    } else {
        printf("\n");
        if (!g_out_is_console)
            printf("(标准输出不是控制台: 用逐行文本方式演示, 不做原地重画)\n\n");
    }

    /* ---- 跑动画; 交互模式下跑完进编辑菜单, 可以改数据重跑 ---- */
    for (;;) {
        if (fit_scale_to_window(&st)) return 0;

        run_sort(&st);

        if (!g_is_tty || !g_out_is_console) {
            /* 管道/重定向/调试控制台: 跑完就结束, 不等人也不进菜单 */
            if (g_out_is_console)
                printf("\n(标准输入不是控制台, 已跳过交互编辑菜单)\n");
            else
                printf("\n(标准输出不是控制台, 已跳过交互编辑菜单)\n");
            return 0;
        }

        if (edit_menu(&st, 1)) {
            printf("\n再见。\n");
            return 0;
        }
    }
}
