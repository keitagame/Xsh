/*
 * XSH - The eXtremely humorous Shell
 * 
 * "Why be boring when you can be Xsh?"
 * 
 *  ██╗  ██╗███████╗██╗  ██╗
 *  ╚██╗██╔╝██╔════╝██║  ██║
 *   ╚███╔╝ ███████╗███████║
 *   ██╔██╗ ╚════██║██╔══██║
 *  ██╔╝ ██╗███████║██║  ██║
 *  ╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <pwd.h>
#include <limits.h>
#include <signal.h>
#include <ctype.h>

#define XSH_VERSION "1.0.0-lol"
#define XSH_MAX_INPUT 4096
#define MAX_ARGS     256
#define MAX_HISTORY  100
#define PROMPT_MAX   512

/* ANSI color codes */
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define BLINK   "\033[5m"

/* ============================
 *  STARTUP ASCII ART
 * ============================ */

void print_banner(void) {
    printf(CYAN BOLD);
    printf("\n");
    printf("  ██╗  ██╗███████╗██╗  ██╗\n");
    printf("  ╚██╗██╔╝██╔════╝██║  ██║\n");
    printf("   ╚███╔╝ ███████╗███████║\n");
    printf("   ██╔██╗ ╚════██║██╔══██║\n");
    printf("  ██╔╝ ██╗███████║██║  ██║\n");
    printf("  ╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝\n");
    printf(RESET);

    printf(YELLOW BOLD);
    printf("  ┌─────────────────────────────────────────────────┐\n");
    printf("  │   eXtremely Humorous Shell  v%-10s          │\n", XSH_VERSION);
    printf("  │   \"bash? zsh? Pfft. We have MEMES here.\"       │\n");
    printf("  └─────────────────────────────────────────────────┘\n");
    printf(RESET);

    printf(MAGENTA);
    printf("\n");
    printf("       (\\(\\     <( こんにちは、シェルへようこそ！ )\n");
    printf("       ( -.-)  /\n");
    printf("       o_(\")(\")  ← このうさぎがあなたのコマンドを実行します\n");
    printf("\n");
    printf(RESET);

    /* Random startup messages */
    const char *msgs[] = {
        "  " GREEN "✓" RESET " カーネルに賄賂を渡しました\n",
        "  " GREEN "✓" RESET " シェルスクリプトの呪いを解除しました\n",
        "  " GREEN "✓" RESET " バグをフィーチャーに改名しました\n",
        "  " GREEN "✓" RESET " コーヒーをRAMに注入しました\n",
        "  " GREEN "✓" RESET " Windowsとの戦争に勝利しました\n",
        "  " GREEN "✓" RESET " sudo rm -rf /* の実行を阻止しました (今回は)\n",
    };
    srand((unsigned)time(NULL));
    int n = sizeof(msgs) / sizeof(msgs[0]);
    for (int i = 0; i < 3; i++) {
        printf("%s", msgs[rand() % n]);
    }
    printf("\n");
    printf(CYAN "  ヒント: 'xhelp' でコマンド一覧  'xjoke' でジョークを聞く  'xgod' で神になる\n" RESET);
    printf("\n");
}

/* ============================
 *  PROMPT GENERATION
 * ============================ */

static int cmd_count = 0;

const char *get_mood_emoji(void) {
    const char *moods[] = {
        "(っ◕‿◕)っ",
        "(ﾉ◕ヮ◕)ﾉ*:･ﾟ✧",
        "ヽ(•‿•)ノ",
        "( ͡° ͜ʖ ͡°)",
        "(╯°□°）╯",
        "(ง'̀-'́)ง",
        "¯\\_(ツ)_/¯",
        "(>_<)",
        "(^_^;)",
        "٩(◕‿◕｡)۶",
    };
    return moods[cmd_count % (sizeof(moods) / sizeof(moods[0]))];
}

void print_prompt(void) {
    char cwd[PATH_MAX];
    char hostname[256];
    struct passwd *pw = getpwuid(getuid());
    const char *username = pw ? pw->pw_name : "???";

    if (getcwd(cwd, sizeof(cwd)) == NULL)
        strcpy(cwd, "???");
    if (gethostname(hostname, sizeof(hostname)) != 0)
        strcpy(hostname, "???");

    /* Shorten home dir to ~ */
    const char *home = pw ? pw->pw_dir : NULL;
    char *display_cwd = cwd;
    char short_cwd[PATH_MAX];
    if (home && strncmp(cwd, home, strlen(home)) == 0) {
        snprintf(short_cwd, sizeof(short_cwd), "~%s", cwd + strlen(home));
        display_cwd = short_cwd;
    }

    /* Color changes based on command count (fun!) */
    const char *colors[] = { CYAN, GREEN, MAGENTA, YELLOW, RED };
    const char *col = colors[cmd_count % 5];

    printf(BOLD "%s" RESET, get_mood_emoji());
    printf(BOLD " %s%s@%s" RESET, col, username, hostname);
    printf(WHITE " [" RESET);
    printf(YELLOW "%s" RESET, display_cwd);
    printf(WHITE "]" RESET);

    if (getuid() == 0) {
        printf(RED BOLD "\n神 # " RESET);
    } else {
        printf(GREEN BOLD "\n➜ " RESET);
    }
    fflush(stdout);
}

/* ============================
 *  HISTORY
 * ============================ */

static char history[MAX_HISTORY][XSH_MAX_INPUT];
static int  history_count = 0;

void add_history(const char *cmd) {
    if (history_count < MAX_HISTORY) {
        strncpy(history[history_count++], cmd, XSH_MAX_INPUT - 1);
    } else {
        /* Shift */
        for (int i = 0; i < MAX_HISTORY - 1; i++)
            memcpy(history[i], history[i+1], XSH_MAX_INPUT);
        strncpy(history[MAX_HISTORY - 1], cmd, XSH_MAX_INPUT - 1);
    }
}

/* ============================
 *  BUILTIN COMMANDS
 * ============================ */

/* xhelp */
int builtin_xhelp(void) {
    printf(CYAN BOLD);
    printf("\n  ╔══════════════════════════════════════════════════════╗\n");
    printf("  ║           Xsh ビルトインコマンド一覧               ║\n");
    printf("  ╚══════════════════════════════════════════════════════╝\n");
    printf(RESET);

    struct { const char *cmd; const char *desc; } cmds[] = {
        { "cd [dir]",      "ディレクトリを移動 (迷子にならないように)" },
        { "exit / bye",    "シェルを終了 (寂しくなります)" },
        { "history",       "コマンド履歴 (恥ずかしい過去も含む)" },
        { "xhelp",         "このヘルプを表示 (お前が見てるやつ)" },
        { "xjoke",         "プログラマジョークを表示" },
        { "xgod",          "神モード (rootでもないのに)" },
        { "xfish",         "釣りゲーム (暇つぶし)" },
        { "xmotd",         "今日のメッセージ" },
        { "xdance",        "シェルが踊ります" },
        { "xmatrix",       "マトリックス風アニメーション" },
        { "echo [text]",   "テキストを表示 (おうむ返しシェル)" },
        { "pwd",           "現在地を報告 (迷子確認)" },
        { "ls [dir]",      "ファイル一覧 (何が潜んでるか見てみよう)" },
        { "clear",         "画面をきれいに (なかったことにする)" },
    };

    for (size_t i = 0; i < sizeof(cmds)/sizeof(cmds[0]); i++) {
        printf("  " YELLOW "%-18s" RESET " → %s\n", cmds[i].cmd, cmds[i].desc);
    }
    printf("\n");
    return 0;
}

/* xjoke */
int builtin_xjoke(void) {
    const char *jokes[] = {
        "Q: プログラマーはなぜ眼鏡をかけるの？\nA: C# が見えないから！",
        "Q: なんでJavaプログラマーは眼鏡が必要？\nA: C を見失ったから！",
        "バグのないコードを書く方法は？\nコードを書かないこと。",
        "Q: 再帰とは何ですか？\nA: 再帰とは何ですかを参照。",
        "なぜプログラマーはダークモードを使う？\nライトがバグを引き寄せるから！",
        "Q: Gitコミットメッセージに書く最もよくある文字列は？\nA: 'fix'",
        "プログラマーが「もうすぐ終わる」と言ったら...\n99%の確率でまだ始まってもいない。",
        "// TODO: このジョークをあとで直す",
        "人生はコードみたいなもの。\nバグだらけで、ドキュメントもなく、\n本番環境で動いている。",
        "NULL == 愛 // これはコンパイルエラーではない",
    };
    srand((unsigned)time(NULL) + cmd_count);
    int idx = rand() % (sizeof(jokes)/sizeof(jokes[0]));
    printf(YELLOW "\n  🤣 %s\n\n" RESET, jokes[idx]);
    return 0;
}

/* xgod */
int builtin_xgod(void) {
    printf(MAGENTA BOLD);
    printf("\n");
    printf("      ✨ 神モード起動中... ✨\n\n");
    printf("       /\\_____/\\\n");
    printf("      /  o   o  \\\n");
    printf("     ( ==  ^  == )\n");
    printf("      )         (\n");
    printf("     (           )\n");
    printf("    ( (  )   (  ) )\n");
    printf("   (__(__)___(__)__)\n");
    printf("\n");
    printf(RESET YELLOW);
    printf("  あなたはrootではありません。でも気持ちは神です。\n");
    printf("  現実はあなたの解釈次第。sudo? 必要ない。\n\n");
    printf(RESET);
    return 0;
}

/* xdance */
int builtin_xdance(void) {
    const char *frames[] = {
        "\r  ヽ(^o^)ノ  ♪",
        "\r  ノ(^_^)ヽ  ♫",
        "\r  ヽ(^▽^)ノ  ♪",
        "\r  ノ(^o^)ヽ  ♫",
    };
    printf(CYAN "\n  シェルがダンス中... (Ctrl+Cで停止)\n\n" RESET);
    for (int i = 0; i < 40; i++) {
        printf(YELLOW BOLD "%s" RESET, frames[i % 4]);
        fflush(stdout);
        usleep(200000);
    }
    printf("\n\n");
    return 0;
}

/* xfish - 簡単な釣りゲーム */
int builtin_xfish(void) {
    const char *fish[] = { "🐟", "🐠", "🐡", "🦈", "🐙", "🦑", "💀", "👟", "🥫" };
    const char *names[] = { "マグロ", "クマノミ", "フグ", "サメ！！", "タコ", "イカ", "骸骨", "古い靴", "缶詰" };
    printf(CYAN "\n  🎣 釣りを開始します...\n\n");
    printf("  ~~~~ 🌊 ~~~~~ 🌊 ~~~~~ 🌊 ~~~~~\n\n");
    for (int i = 0; i < 5; i++) {
        printf("  " YELLOW "." RESET);
        fflush(stdout);
        usleep(400000);
    }
    srand((unsigned)time(NULL));
    int idx = rand() % 9;
    printf(RESET "\n\n  " GREEN BOLD "釣れた！ %s (%s) をゲット！\n\n" RESET,
           fish[idx], names[idx]);
    if (idx >= 6) {
        printf(RED "  ハズレ！ゴミを拾っただけです。\n\n" RESET);
    }
    return 0;
}

/* xmotd */
int builtin_xmotd(void) {
    const char *msgs[] = {
        "今日もバグを生産するあなたを応援しています。",
        "コーヒーは飲みましたか？飲んでください。",
        "Stack Overflowを5回開いた？それは普通です。",
        "今日中に全部終わります（終わりません）。",
        "コードレビューを怖がらないで。みんな同じです。",
        "変数名に 'tmp2' はやめましょう。'tmp3' になるから。",
        "今日のあなたのコードは未来の自分への手紙です。優しくしてあげてください。",
        "デバッグは探偵仕事。あなたが探偵で、あなたが犯人。",
    };
    srand((unsigned)time(NULL));
    printf(CYAN BOLD "\n  📣 今日のメッセージ:\n" RESET);
    printf(YELLOW "  \"%s\"\n\n" RESET, msgs[rand() % 8]);
    return 0;
}

/* xmatrix */
int builtin_xmatrix(void) {
    const char *chars[] = {"ﾊ","ﾋ","ｼ","ﾂ","ｳ","ｦ","ｲ","ｸ","ｺ","ｿ","ﾁ","ﾄ","ﾉ","ﾌ","0","1","2","3","4","5"};
    int n = sizeof(chars)/sizeof(chars[0]);
    printf(GREEN "\n  (Ctrl+C で停止)\n\n");
    srand((unsigned)time(NULL));
    for (int row = 0; row < 20; row++) {
        printf("  ");
        for (int col = 0; col < 60; col++) {
            if (rand() % 3 == 0)
                printf(BOLD "%s" RESET GREEN, chars[rand() % n]);
            else
                printf("%s", chars[rand() % n]);
        }
        printf("\n");
        fflush(stdout);
        usleep(80000);
    }
    printf(RESET "\n");
    return 0;
}

/* ls builtin */
int builtin_ls(const char *path) {
    if (!path) path = ".";
    DIR *d = opendir(path);
    if (!d) {
        printf(RED "  ls: '%s' を開けません: %s\n" RESET, path, strerror(errno));
        return 1;
    }
    struct dirent *entry;
    int count = 0;
    printf("\n");
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (entry->d_type == DT_DIR)
            printf("  " CYAN BOLD "📁 %s/" RESET "\n", entry->d_name);
        else
            printf("  " WHITE "📄 %s" RESET "\n", entry->d_name);
        count++;
    }
    closedir(d);
    if (count == 0)
        printf(YELLOW "  （空っぽ。まるで私の心のように）\n" RESET);
    printf("\n");
    return 0;
}

/* ============================
 *  TOKENIZER
 * ============================ */

int tokenize(char *input, char **args, int max_args) {
    int count = 0;
    char *p = input;
    while (*p && count < max_args - 1) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (*p == '"') {
            p++;
            args[count++] = p;
            while (*p && *p != '"') p++;
            if (*p) *p++ = '\0';
        } else {
            args[count++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) *p++ = '\0';
        }
    }
    args[count] = NULL;
    return count;
}

/* ============================
 *  EXECUTE EXTERNAL
 * ============================ */

int execute_external(char **args) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {
        execvp(args[0], args);
        /* Command not found */
        printf(RED "\n  Xsh: '%s' ？そんなコマンドは知らない！\n" RESET, args[0]);
        printf(YELLOW "       タイポじゃないですか？または 'xhelp' を試してください。\n\n" RESET);
        exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
}

/* ============================
 *  SIGNAL HANDLER
 * ============================ */

void sigint_handler(int sig) {
    (void)sig;
    printf(YELLOW "\n  (Ctrl+C を押しましたね。逃げても無駄です)\n" RESET);
    print_prompt();
}

/* ============================
 *  MAIN LOOP
 * ============================ */

int main(void) {
    signal(SIGINT, sigint_handler);
    print_banner();
    builtin_xmotd();

    char input[XSH_MAX_INPUT];
    char *args[MAX_ARGS];

    while (1) {
        print_prompt();

        if (!fgets(input, sizeof(input), stdin)) {
            /* EOF */
            printf(CYAN "\n  さようなら！また会いましょう (^_^)/~\n\n" RESET);
            break;
        }

        /* Strip newline */
        size_t len = strlen(input);
        if (len > 0 && input[len-1] == '\n') input[len-1] = '\0';

        /* Skip empty */
        if (input[0] == '\0') continue;

        /* Add to history */
        add_history(input);
        cmd_count++;

        /* Tokenize */
        char input_copy[XSH_MAX_INPUT];
        strncpy(input_copy, input, XSH_MAX_INPUT - 1);
        int argc = tokenize(input_copy, args, MAX_ARGS);
        if (argc == 0) continue;

        /* ---- BUILTINS ---- */

        /* exit / bye / sayonara */
        if (strcmp(args[0], "exit") == 0 ||
            strcmp(args[0], "bye") == 0 ||
            strcmp(args[0], "さようなら") == 0) {
            printf(CYAN "\n");
            printf("   (\\(\\      バイバイ！\n");
            printf("   ( T.T)  /\n");
            printf("   o_(\")(\") またね...\n\n");
            printf(RESET);
            exit(0);
        }

        /* cd */
        if (strcmp(args[0], "cd") == 0) {
            const char *target = args[1];
            if (!target) {
                struct passwd *pw = getpwuid(getuid());
                target = pw ? pw->pw_dir : "/";
            }
            if (chdir(target) != 0)
                printf(RED "  cd: '%s' に行けません: %s\n\n" RESET, target, strerror(errno));
            continue;
        }

        /* pwd */
        if (strcmp(args[0], "pwd") == 0) {
            char cwd[PATH_MAX];
            getcwd(cwd, sizeof(cwd));
            printf(CYAN "\n  今ここ: " YELLOW "%s\n\n" RESET, cwd);
            continue;
        }

        /* echo */
        if (strcmp(args[0], "echo") == 0) {
            printf("\n  ");
            for (int i = 1; args[i]; i++) {
                printf("%s", args[i]);
                if (args[i+1]) printf(" ");
            }
            printf("\n\n");
            continue;
        }

        /* clear */
        if (strcmp(args[0], "clear") == 0) {
            printf("\033[2J\033[H");
            print_banner();
            continue;
        }

        /* history */
        if (strcmp(args[0], "history") == 0) {
            printf(CYAN "\n  📜 コマンド履歴 (恥ずかしいやつも全部):\n\n" RESET);
            for (int i = 0; i < history_count; i++)
                printf("  " YELLOW "%3d" RESET "  %s\n", i+1, history[i]);
            printf("\n");
            continue;
        }

        /* ls */
        if (strcmp(args[0], "ls") == 0) {
            builtin_ls(args[1]);
            continue;
        }

        /* xhelp */
        if (strcmp(args[0], "xhelp") == 0) { builtin_xhelp(); continue; }

        /* xjoke */
        if (strcmp(args[0], "xjoke") == 0) { builtin_xjoke(); continue; }

        /* xgod */
        if (strcmp(args[0], "xgod") == 0) { builtin_xgod(); continue; }

        /* xdance */
        if (strcmp(args[0], "xdance") == 0) { builtin_xdance(); continue; }

        /* xfish */
        if (strcmp(args[0], "xfish") == 0) { builtin_xfish(); continue; }

        /* xmotd */
        if (strcmp(args[0], "xmotd") == 0) { builtin_xmotd(); continue; }

        /* xmatrix */
        if (strcmp(args[0], "xmatrix") == 0) { builtin_xmatrix(); continue; }

        /* xversion */
        if (strcmp(args[0], "xversion") == 0) {
            printf(CYAN "\n  Xsh version " BOLD "%s" RESET CYAN
                   " - 世界一ユーモアのあるシェル\n\n" RESET, XSH_VERSION);
            continue;
        }

        /* Easter egg: rm -rf / */
        if (argc >= 3 &&
            strcmp(args[0], "rm") == 0 &&
            strcmp(args[1], "-rf") == 0 &&
            (strcmp(args[2], "/") == 0 || strcmp(args[2], "/*") == 0)) {
            printf(RED BOLD "\n  ⚠️  待って！！！\n\n" RESET);
            printf(YELLOW);
            printf("       ┌──────────────────────────────┐\n");
            printf("       │  Xsh がこの宇宙を救いました   │\n");
            printf("       │  （実行をブロックしました）    │\n");
            printf("       └──────────────────────────────┘\n");
            printf("\n  次回からは気をつけてください。うさぎが震えています。\n\n");
            printf(RESET);
            continue;
        }

        /* External command */
        execute_external(args);
    }

    return 0;
}
