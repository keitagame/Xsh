/*
 * XSH - The Xtreme Shell
 * A beautiful, feature-rich Unix shell with ASCII art and stunning visuals
 *
 * Author: XSH Project
 * License: MIT
 *
 * Portability: Linux, macOS, FreeBSD, OpenBSD, NetBSD, and other POSIX systems.
 * Build (static, Linux):  gcc -static -O2 -o xsh xsh.c
 * Build (dynamic, any):   cc -O2 -o xsh xsh.c
 */

/* ---- Portability feature-test macros (must come before any #include) ---- */
#if defined(__linux__)
#  define _GNU_SOURCE          /* enables strdup, setenv, unsetenv on older glibc */
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#  define _BSD_SOURCE
#  define _DEFAULT_SOURCE
#else
#  define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <pwd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <fcntl.h>
#include <glob.h>
#include <limits.h>
#include <ctype.h>

/* environ is in unistd.h on most systems, but declare it explicitly for safety */
#ifndef _GNU_SOURCE
extern char **environ;
#endif

/* GLOB_TILDE is not in POSIX; guard against systems that lack it */
#ifndef GLOB_TILDE
#  define GLOB_TILDE 0
#endif

/* ===== ANSI Color Codes ===== */
#define RESET       "\033[0m"
#define BOLD        "\033[1m"
#define DIM         "\033[2m"
#define ITALIC      "\033[3m"
#define UNDERLINE   "\033[4m"
#define BLINK       "\033[5m"

/* Foreground Colors */
#define FG_BLACK    "\033[30m"
#define FG_RED      "\033[31m"
#define FG_GREEN    "\033[32m"
#define FG_YELLOW   "\033[33m"
#define FG_BLUE     "\033[34m"
#define FG_MAGENTA  "\033[35m"
#define FG_CYAN     "\033[36m"
#define FG_WHITE    "\033[37m"

/* Bright Foreground */
#define FG_BBLACK   "\033[90m"
#define FG_BRED     "\033[91m"
#define FG_BGREEN   "\033[92m"
#define FG_BYELLOW  "\033[93m"
#define FG_BBLUE    "\033[94m"
#define FG_BMAGENTA "\033[95m"
#define FG_BCYAN    "\033[96m"
#define FG_BWHITE   "\033[97m"

/* Background Colors */
#define BG_BLACK    "\033[40m"
#define BG_RED      "\033[41m"
#define BG_BLUE     "\033[44m"
#define BG_MAGENTA  "\033[45m"
#define BG_CYAN     "\033[46m"

/* 256-color support */
#define FG256(n)    "\033[38;5;" #n "m"
#define BG256(n)    "\033[48;5;" #n "m"

/* True color */
#define FGRGB(r,g,b) "\033[38;2;" #r ";" #g ";" #b "m"
#define BGRGB(r,g,b) "\033[48;2;" #r ";" #g ";" #b "m"

/* ===== Constants ===== */
#define XSH_VERSION     "1.0.0"
#define XSH_NAME        "XSH"
#define MAX_CMD_LEN     4096
#define MAX_ARGS        256
#define MAX_PATH        PATH_MAX
#define MAX_HISTORY     1000
#define XSH_HISTORY_FILE ".xsh_history"
#define XSH_RC_FILE     ".xshrc"

/* ===== Global State ===== */
static int last_exit_code = 0;
static int running = 1;
static char cwd[MAX_PATH];
static char hostname[256];
static struct passwd *user_info;

/* ===== ASCII Art Banner ===== */
void print_banner(void) {
    /* Clear screen */
    printf("\033[2J\033[H");

    /* Gradient banner using 256 colors */
    printf("\n");

    /* Top decorative line */
    printf(FGRGB(0,255,200) "  ╔══════════════════════════════════════════════════════════════╗\n" RESET);
    printf(FGRGB(0,220,255) "  ║" RESET);

    /* XSH Logo in ASCII Art */
    printf(FGRGB(0,255,200) BOLD "                                                              " RESET);
    printf(FGRGB(0,220,255) "║\n" RESET);

    printf(FGRGB(0,220,255) "  ║" RESET);
    printf(FGRGB(0,255,180) BOLD "   ██╗  ██╗███████╗██╗  ██╗                                   " RESET);
    printf(FGRGB(0,220,255) "║\n" RESET);

    printf(FGRGB(0,220,255) "  ║" RESET);
    printf(FGRGB(0,240,160) BOLD "   ╚██╗██╔╝██╔════╝██║  ██║                                   " RESET);
    printf(FGRGB(0,220,255) "║\n" RESET);

    printf(FGRGB(0,220,255) "  ║" RESET);
    printf(FGRGB(0,220,140) BOLD "    ╚███╔╝ ███████╗███████║    The Xtreme Unix Shell          " RESET);
    printf(FGRGB(0,220,255) "║\n" RESET);

    printf(FGRGB(0,220,255) "  ║" RESET);
    printf(FGRGB(0,200,120) BOLD "    ██╔██╗ ╚════██║██╔══██║                                   " RESET);
    printf(FGRGB(0,220,255) "║\n" RESET);

    printf(FGRGB(0,220,255) "  ║" RESET);
    printf(FGRGB(0,180,100) BOLD "   ██╔╝ ██╗███████║██║  ██║    v%-6s                        " RESET, XSH_VERSION);
    printf(FGRGB(0,220,255) "║\n" RESET);

    printf(FGRGB(0,220,255) "  ║" RESET);
    printf(FGRGB(0,160,80) BOLD "   ╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝                                   " RESET);
    printf(FGRGB(0,220,255) "║\n" RESET);

    printf(FGRGB(0,220,255) "  ║" RESET);
    printf(FGRGB(0,255,200) BOLD "                                                              " RESET);
    printf(FGRGB(0,220,255) "║\n" RESET);

    /* Separator with decorations */
    printf(FGRGB(0,220,255) "  ╠══════════════════════════════════════════════════════════════╣\n" RESET);

    printf(FGRGB(0,220,255) "  ║" RESET);
    printf(FGRGB(80,200,255) "  ◈  " RESET);
    printf(FGRGB(180,180,180) "Type " RESET);
    printf(FGRGB(0,255,180) BOLD "help" RESET);
    printf(FGRGB(180,180,180) " for commands  " RESET);
    printf(FGRGB(80,200,255) "◈  " RESET);
    printf(FGRGB(180,180,180) "Tab to complete  " RESET);
    printf(FGRGB(80,200,255) "◈  " RESET);
    //printf(FGRGB(180,180,180) "↑/↓ for history" RESET);
    printf(FGRGB(0,220,255) "          ║\n" RESET);

    printf(FGRGB(0,220,255) "  ║" RESET);
    printf(FGRGB(80,200,255) "  ◈  " RESET);

    /* Show current time */
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    printf(FGRGB(180,180,180) "Session started: " RESET);
    printf(FGRGB(0,255,180) BOLD "%04d-%02d-%02d %02d:%02d:%02d" RESET,
           t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
           t->tm_hour, t->tm_min, t->tm_sec);
    printf(FGRGB(0,220,255) "                     ║\n" RESET);

    printf(FGRGB(0,220,255) "  ╚══════════════════════════════════════════════════════════════╝\n" RESET);
    printf("\n");

    /* Welcome message with username */
    if (user_info) {
        printf("  " FGRGB(0,255,180) "▶" RESET " Welcome back, " FGRGB(0,255,200) BOLD "%s" RESET "!\n\n", user_info->pw_name);
    }
}

/* ===== Prompt Generation ===== */
char *build_prompt(void) {
    static char prompt[1024];
    char short_cwd[256];
    char *home = getenv("HOME");

    /* Get CWD */
    getcwd(cwd, sizeof(cwd));

    /* Shorten home to ~ */
    if (home && strncmp(cwd, home, strlen(home)) == 0) {
        snprintf(short_cwd, sizeof(short_cwd), "~%s", cwd + strlen(home));
    } else {
        strncpy(short_cwd, cwd, sizeof(short_cwd) - 1);
        short_cwd[sizeof(short_cwd) - 1] = '\0';
    }

    /* Shorten path if too long */
    if (strlen(short_cwd) > 40) {
        char *last_slash = strrchr(short_cwd, '/');
        if (last_slash) {
            char tmp[256];
            snprintf(tmp, sizeof(tmp), "...%s", last_slash);
            strncpy(short_cwd, tmp, sizeof(short_cwd) - 1);
        }
    }

    /* Get git branch if available */
    char git_branch[64] = "";
    FILE *fp = popen("git rev-parse --abbrev-ref HEAD 2>/dev/null", "r");
    if (fp) {
        if (fgets(git_branch, sizeof(git_branch), fp)) {
            git_branch[strcspn(git_branch, "\n")] = '\0';
        }
        pclose(fp);
    }

    /* Build prompt segments */
    char git_seg[128] = "";
    if (strlen(git_branch) > 0) {
        snprintf(git_seg, sizeof(git_seg),
                 "\001\033[38;2;255;200;0m\002 \ue0a0 %s\001\033[0m\002",
                 git_branch);
        /* fallback without nerd font */
        snprintf(git_seg, sizeof(git_seg),
                 "\001\033[38;2;255;200;0m\002 ⎇ %s\001\033[0m\002",
                 git_branch);
    }

    /* Status indicator */
    const char *status_color;
    const char *status_char;
    if (last_exit_code == 0) {
        status_color = "\033[38;2;0;255;180m";
        status_char = "✓";
    } else {
        status_color = "\033[38;2;255;80;80m";
        status_char = "✗";
    }

    /* Root or user indicator */
    const char *is_root = (geteuid() == 0) ? "⚡" : "▶";
    const char *prompt_end_color = (geteuid() == 0) ? "\033[38;2;255;80;80m" : "\033[38;2;0;255;180m";

    /* Format:
     * ╭─[ user@host ]─[ ~/path ]─[ git_branch ]─[ exit_code ]
     * ╰─▶ 
     */
     const char *status_bg;
     (void)status_bg; /* reserved for future use */

if (last_exit_code == 0) {
    status_bg = "\033[48;2;0;200;150m";   // success (green)
} else {
    status_bg = "\033[48;2;255;80;80m";   // error (red)
}
snprintf(prompt, sizeof(prompt),

    /* user@host */
    "\001\033[38;2;120;200;255m\002%s@%s\001\033[0m\002 "

    /* path */
    "\001\033[38;2;180;180;255m\002%s\001\033[0m\002 "

    /* git */
    "%s"

    /* exit status */
    "\001%s\002%s\001\033[0m\002 "

    /* prompt symbol */
    "\001%s\002%s\001\033[0m\002 ",

    user_info ? user_info->pw_name : "user",
    hostname,
    short_cwd,
    git_seg,
    status_color,
    status_char,
    prompt_end_color,
    is_root
);
return prompt;
}
/* ===== Signal Handlers ===== */
void sigint_handler(int sig) {
    (void)sig;
    /* signal handled in readline loop */
}

void sigchld_handler(int sig) {
    (void)sig;
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

/* ===== Tokenizer ===== */
typedef struct {
    char **tokens;
    int count;
    int pipe_positions[MAX_ARGS]; /* indices where pipes occur */
    int pipe_count;
    char *input_file;
    char *output_file;
    int append_output;
} ParseResult;

void free_parse_result(ParseResult *pr) {
    if (!pr) return;
    for (int i = 0; i < pr->count; i++) {
        free(pr->tokens[i]);
    }
    free(pr->tokens);
    free(pr->input_file);
    free(pr->output_file);
}

/* Expand environment variables in a token */
char *expand_token(const char *tok) {
    if (!tok) return NULL;
    char buf[MAX_CMD_LEN];
    int bi = 0;
    int ti = 0;
    int len = strlen(tok);

    while (ti < len && bi < MAX_CMD_LEN - 1) {
        if (tok[ti] == '$') {
            ti++;
            if (tok[ti] == '{') {
                ti++;
                char varname[256];
                int vi = 0;
                while (tok[ti] && tok[ti] != '}' && vi < 255) {
                    varname[vi++] = tok[ti++];
                }
                varname[vi] = '\0';
                if (tok[ti] == '}') ti++;
                char *val = getenv(varname);
                if (val) {
                    int vl = strlen(val);
                    if (bi + vl < MAX_CMD_LEN - 1) {
                        memcpy(buf + bi, val, vl);
                        bi += vl;
                    }
                }
            } else if (tok[ti] == '(') {
                /* Command substitution $(...) */
                ti++;
                char cmd[MAX_CMD_LEN];
                int ci2 = 0;
                int depth = 1;
                while (tok[ti] && depth > 0 && ci2 < MAX_CMD_LEN - 1) {
                    if (tok[ti] == '(') depth++;
                    else if (tok[ti] == ')') { depth--; if (depth == 0) { ti++; break; } }
                    cmd[ci2++] = tok[ti++];
                }
                cmd[ci2] = '\0';
                /* Execute command and capture output */
                FILE *fp = popen(cmd, "r");
                if (fp) {
                    char out[MAX_CMD_LEN];
                    int oi = 0;
                    int c;
                    while ((c = fgetc(fp)) != EOF && oi < MAX_CMD_LEN - 1) {
                        out[oi++] = c;
                    }
                    pclose(fp);
                    /* Strip trailing newlines */
                    while (oi > 0 && out[oi-1] == '\n') oi--;
                    out[oi] = '\0';
                    if (bi + oi < MAX_CMD_LEN - 1) {
                        memcpy(buf + bi, out, oi);
                        bi += oi;
                    }
                }
            } else if (tok[ti] == '?') {
                ti++;
                char num[16];
                snprintf(num, sizeof(num), "%d", last_exit_code);
                int nl = strlen(num);
                memcpy(buf + bi, num, nl);
                bi += nl;
            } else if (tok[ti] == '$') {
                ti++;
                char num[16];
                snprintf(num, sizeof(num), "%d", getpid());
                int nl = strlen(num);
                memcpy(buf + bi, num, nl);
                bi += nl;
            } else {
                char varname[256];
                int vi = 0;
                while (tok[ti] && (isalnum(tok[ti]) || tok[ti] == '_') && vi < 255) {
                    varname[vi++] = tok[ti++];
                }
                varname[vi] = '\0';
                char *val = getenv(varname);
                if (val) {
                    int vl = strlen(val);
                    if (bi + vl < MAX_CMD_LEN - 1) {
                        memcpy(buf + bi, val, vl);
                        bi += vl;
                    }
                }
            }
        } else if (tok[ti] == '~' && bi == 0) {
            ti++;
            char *home = getenv("HOME");
            if (home) {
                int hl = strlen(home);
                memcpy(buf + bi, home, hl);
                bi += hl;
            } else {
                buf[bi++] = '~';
            }
        } else {
            buf[bi++] = tok[ti++];
        }
    }
    buf[bi] = '\0';
    return strdup(buf);
}

ParseResult *parse_line(const char *line) {
    ParseResult *pr = calloc(1, sizeof(ParseResult));
    pr->tokens = malloc(MAX_ARGS * sizeof(char*));
    pr->count = 0;
    pr->pipe_count = 0;
    pr->input_file = NULL;
    pr->output_file = NULL;
    pr->append_output = 0;

    char buf[MAX_CMD_LEN];
    int bi = 0;
    int in_single = 0, in_double = 0;
    int i = 0;
    int len = strlen(line);

    while (i <= len) {
        char c = line[i];

        if (c == '\'' && !in_double) {
            in_single = !in_single;
            i++;
            continue;
        }
        if (c == '"' && !in_single) {
            in_double = !in_double;
            i++;
            continue;
        }

        /* Handle pipes (but not ||) */
        if (c == '|' && !in_single && !in_double && line[i+1] != '|') {
            if (bi > 0) {
                buf[bi] = '\0';
                pr->tokens[pr->count++] = expand_token(buf);
                bi = 0;
            }
            pr->pipe_positions[pr->pipe_count++] = pr->count;
            i++;
            continue;
        }

        /* Handle redirections */
        if (c == '<' && !in_single && !in_double) {
            if (bi > 0) {
                buf[bi] = '\0';
                pr->tokens[pr->count++] = expand_token(buf);
                bi = 0;
            }
            i++;
            while (line[i] == ' ') i++;
            int ri = 0;
            char rbuf[MAX_PATH];
            while (line[i] && line[i] != ' ' && line[i] != '>' && line[i] != '<') {
                rbuf[ri++] = line[i++];
            }
            rbuf[ri] = '\0';
            pr->input_file = strdup(rbuf);
            continue;
        }

        if (c == '>' && !in_single && !in_double) {
            if (bi > 0) {
                buf[bi] = '\0';
                pr->tokens[pr->count++] = expand_token(buf);
                bi = 0;
            }
            i++;
            if (line[i] == '>') {
                pr->append_output = 1;
                i++;
            }
            while (line[i] == ' ') i++;
            int ri = 0;
            char rbuf[MAX_PATH];
            while (line[i] && line[i] != ' ' && line[i] != '>' && line[i] != '<') {
                rbuf[ri++] = line[i++];
            }
            rbuf[ri] = '\0';
            pr->output_file = strdup(rbuf);
            continue;
        }

        if ((c == ' ' || c == '\t' || c == '\0') && !in_single && !in_double) {
            if (bi > 0) {
                buf[bi] = '\0';
                pr->tokens[pr->count++] = expand_token(buf);
                bi = 0;
            }
            if (c == '\0') break;
            i++;
            continue;
        }

        buf[bi++] = c;
        i++;
    }

    return pr;
}

/* ===== Built-in Commands ===== */

/* cd */
int builtin_cd(char **args, int argc) {
    const char *target;
    if (argc < 2 || args[1] == NULL) {
        target = getenv("HOME");
        if (!target) target = "/";
    } else if (strcmp(args[1], "-") == 0) {
        target = getenv("OLDPWD");
        if (!target) {
            fprintf(stderr, "xsh: cd: OLDPWD not set\n");
            return 1;
        }
        printf("%s\n", target);
    } else {
        target = args[1];
    }

    char old[MAX_PATH];
    getcwd(old, sizeof(old));

    if (chdir(target) != 0) {
        fprintf(stderr, FGRGB(255,80,80) "✗" RESET " xsh: cd: %s: %s\n", target, strerror(errno));
        return 1;
    }

    setenv("OLDPWD", old, 1);
    getcwd(cwd, sizeof(cwd));
    setenv("PWD", cwd, 1);
    return 0;
}

/* pwd */
int builtin_pwd(void) {
    getcwd(cwd, sizeof(cwd));
    printf(FGRGB(0,255,180) "%s\n" RESET, cwd);
    return 0;
}

/* echo */
int builtin_echo(char **args, int argc) {
    int newline = 1;
    int start = 1;

    if (argc > 1 && strcmp(args[1], "-n") == 0) {
        newline = 0;
        start = 2;
    }

    for (int i = start; i < argc; i++) {
        if (i > start) printf(" ");
        /* Handle escape sequences */
        const char *s = args[i];
        while (*s) {
            if (*s == '\\' && *(s+1)) {
                s++;
                switch (*s) {
                    case 'n': printf("\n"); break;
                    case 't': printf("\t"); break;
                    case 'r': printf("\r"); break;
                    case 'e': printf("\033"); break;
                    case '\\': printf("\\"); break;
                    default: printf("\\%c", *s); break;
                }
            } else {
                putchar(*s);
            }
            s++;
        }
    }
    if (newline) printf("\n");
    return 0;
}

/* export */
int builtin_export(char **args, int argc) {
    if (argc < 2) {
        /* Print all env variables */
        for (int i = 0; environ[i]; i++) {
            printf(FGRGB(0,200,255) "export " RESET "%s\n", environ[i]);
        }
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        char *eq = strchr(args[i], '=');
        if (eq) {
            char varname[256];
            int nlen = eq - args[i];
            strncpy(varname, args[i], nlen);
            varname[nlen] = '\0';
            setenv(varname, eq + 1, 1);
        } else {
            /* Just mark for export (no-op for env vars already set) */
        }
    }
    return 0;
}

/* unset */
int builtin_unset(char **args, int argc) {
    for (int i = 1; i < argc; i++) {
        unsetenv(args[i]);
    }
    return 0;
}

/* history - forward declaration, implemented after custom readline */
int builtin_history(char **args, int argc);

/* help */
int builtin_help(void) {
    printf("\n");
    printf(FGRGB(0,255,200) BOLD "  ╔══════════════════════════════════════════════╗\n" RESET);
    printf(FGRGB(0,255,200) BOLD "  ║       XSH - Built-in Commands                ║\n" RESET);
    printf(FGRGB(0,255,200) BOLD "  ╚══════════════════════════════════════════════╝\n" RESET);
    printf("\n");

    struct { const char *cmd; const char *desc; } cmds[] = {
        {"cd [dir]",       "Change directory (- for previous)"},
        {"pwd",            "Print working directory"},
        {"echo [args]",    "Print text (-n to suppress newline)"},
        {"export [k=v]",   "Set/show environment variables"},
        {"unset [var]",    "Unset environment variable"},
        {"history [n]",    "Show command history"},
        {"jobs",           "List background jobs"},
        {"fg [job]",       "Bring job to foreground"},
        {"bg [job]",       "Resume job in background"},
        {"source [file]",  "Execute commands from file"},
        {"alias [k=v]",    "Create or list aliases"},
        {"unalias [name]", "Remove an alias"},
        {"which [cmd]",    "Show command path"},
        {"type [cmd]",     "Describe a command"},
        {"true",           "Return exit code 0"},
        {"false",          "Return exit code 1"},
        {"exit [n]",       "Exit shell with code n"},
        {"help",           "Show this help"},
        {NULL, NULL}
    };

    for (int i = 0; cmds[i].cmd; i++) {
        printf("  " FGRGB(0,220,255) BOLD "%-18s" RESET "  " FGRGB(180,180,200) "%s\n" RESET,
               cmds[i].cmd, cmds[i].desc);
    }

    printf("\n");
    printf("  " FGRGB(100,100,150) "Features: pipes (|), redirection (< > >>), background (&),\n" RESET);
    printf("  " FGRGB(100,100,150) "          glob expansion, env variables ($VAR), ~expansion\n" RESET);
    printf("\n");
    return 0;
}

/* ===== Alias System ===== */
#define MAX_ALIASES 256
typedef struct {
    char *name;
    char *value;
} Alias;

static Alias aliases[MAX_ALIASES];
static int alias_count = 0;

void alias_set(const char *name, const char *value) {
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i].name, name) == 0) {
            free(aliases[i].value);
            aliases[i].value = strdup(value);
            return;
        }
    }
    if (alias_count < MAX_ALIASES) {
        aliases[alias_count].name = strdup(name);
        aliases[alias_count].value = strdup(value);
        alias_count++;
    }
}

const char *alias_get(const char *name) {
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i].name, name) == 0)
            return aliases[i].value;
    }
    return NULL;
}

void alias_remove(const char *name) {
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i].name, name) == 0) {
            free(aliases[i].name);
            free(aliases[i].value);
            aliases[i] = aliases[--alias_count];
            return;
        }
    }
}

int builtin_alias(char **args, int argc) {
    if (argc < 2) {
        for (int i = 0; i < alias_count; i++) {
            printf(FGRGB(0,200,255) "alias " RESET "%s=" FGRGB(0,255,150) "'%s'\n" RESET,
                   aliases[i].name, aliases[i].value);
        }
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        char *eq = strchr(args[i], '=');
        if (eq) {
            char name[256];
            int nlen = eq - args[i];
            strncpy(name, args[i], nlen);
            name[nlen] = '\0';
            /* Strip surrounding quotes from value */
            char *val = eq + 1;
            int vlen = strlen(val);
            if (vlen >= 2 && ((val[0] == '\'' && val[vlen-1] == '\'') ||
                               (val[0] == '"' && val[vlen-1] == '"'))) {
                char tmp[MAX_CMD_LEN];
                strncpy(tmp, val + 1, vlen - 2);
                tmp[vlen - 2] = '\0';
                alias_set(name, tmp);
            } else {
                alias_set(name, val);
            }
        } else {
            const char *v = alias_get(args[i]);
            if (v) {
                printf(FGRGB(0,200,255) "alias " RESET "%s=" FGRGB(0,255,150) "'%s'\n" RESET, args[i], v);
            } else {
                fprintf(stderr, "xsh: alias: %s: not found\n", args[i]);
                return 1;
            }
        }
    }
    return 0;
}

int builtin_unalias(char **args, int argc) {
    for (int i = 1; i < argc; i++) {
        alias_remove(args[i]);
    }
    return 0;
}

/* ===== which / type ===== */
int builtin_which(char **args, int argc) {
    for (int i = 1; i < argc; i++) {
        char *path_env = getenv("PATH");
        if (!path_env) continue;
        char path_copy[MAX_CMD_LEN];
        strncpy(path_copy, path_env, sizeof(path_copy) - 1);
        char *dir = strtok(path_copy, ":");
        int found = 0;
        while (dir) {
            char full[MAX_PATH];
            snprintf(full, sizeof(full), "%s/%s", dir, args[i]);
            if (access(full, X_OK) == 0) {
                printf(FGRGB(0,255,150) "%s\n" RESET, full);
                found = 1;
                break;
            }
            dir = strtok(NULL, ":");
        }
        if (!found) {
            fprintf(stderr, "xsh: which: %s: not found\n", args[i]);
            return 1;
        }
    }
    return 0;
}

/* ===== Job Control ===== */
#define MAX_JOBS 64
typedef struct {
    pid_t pid;
    int job_id;
    char cmd[256];
    int running;
} Job;

static Job jobs[MAX_JOBS];
static int job_count = 0;
static int next_job_id = 1;

Job *job_add(pid_t pid, const char *cmd) {
    if (job_count >= MAX_JOBS) return NULL;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pid == 0) {
            jobs[i].pid = pid;
            jobs[i].job_id = next_job_id++;
            strncpy(jobs[i].cmd, cmd, 255);
            jobs[i].running = 1;
            job_count++;
            return &jobs[i];
        }
    }
    return NULL;
}

void job_remove(pid_t pid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pid == pid) {
            jobs[i].pid = 0;
            jobs[i].running = 0;
            job_count--;
            return;
        }
    }
}

int builtin_jobs(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pid != 0) {
            int status;
            int r = waitpid(jobs[i].pid, &status, WNOHANG);
            if (r == jobs[i].pid) {
                /* Job finished */
                printf("[%d] Done     %s\n", jobs[i].job_id, jobs[i].cmd);
                job_remove(jobs[i].pid);
            } else {
                printf(FGRGB(0,200,255) "[%d]" RESET " Running   " FGRGB(200,200,200) "%s\n" RESET,
                       jobs[i].job_id, jobs[i].cmd);
            }
        }
    }
    return 0;
}

int builtin_fg(char **args, int argc) {
    if (job_count == 0) {
        fprintf(stderr, "xsh: fg: no current job\n");
        return 1;
    }
    int jid = (argc > 1) ? atoi(args[1]) : -1;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pid != 0 && (jid == -1 || jobs[i].job_id == jid)) {
            kill(jobs[i].pid, SIGCONT);
            int status;
            waitpid(jobs[i].pid, &status, 0);
            job_remove(jobs[i].pid);
            return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
        }
    }
    fprintf(stderr, "xsh: fg: job not found\n");
    return 1;
}

int builtin_bg(char **args, int argc) {
    int jid = (argc > 1) ? atoi(args[1]) : -1;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pid != 0 && (jid == -1 || jobs[i].job_id == jid)) {
            kill(jobs[i].pid, SIGCONT);
            jobs[i].running = 1;
            printf("[%d] %d %s\n", jobs[i].job_id, jobs[i].pid, jobs[i].cmd);
            return 0;
        }
    }
    return 1;
}

/* ===== Source file ===== */
/* Forward declaration */
int execute_line(const char *line);

int builtin_source(char **args, int argc) {
    if (argc < 2) {
        fprintf(stderr, "xsh: source: filename required\n");
        return 1;
    }
    FILE *f = fopen(args[1], "r");
    if (!f) {
        fprintf(stderr, "xsh: source: %s: %s\n", args[1], strerror(errno));
        return 1;
    }
    char line[MAX_CMD_LEN];
    int ret = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '#' || line[0] == '\0') continue;
        ret = execute_line(line);
    }
    fclose(f);
    return ret;
}

/* ===== Glob Expansion ===== */
char **expand_globs(char **tokens, int count, int *new_count) {
    char **result = malloc(MAX_ARGS * sizeof(char*));
    *new_count = 0;

    for (int i = 0; i < count && *new_count < MAX_ARGS; i++) {
        if (strpbrk(tokens[i], "*?[")) {
            glob_t g;
            int r = glob(tokens[i], GLOB_NOCHECK | GLOB_TILDE, NULL, &g);
            if (r == 0) {
                for (size_t j = 0; j < g.gl_pathc && *new_count < MAX_ARGS; j++) {
                    result[(*new_count)++] = strdup(g.gl_pathv[j]);
                }
                globfree(&g);
            } else {
                result[(*new_count)++] = strdup(tokens[i]);
            }
        } else {
            result[(*new_count)++] = strdup(tokens[i]);
        }
    }
    result[*new_count] = NULL;
    return result;
}

/* ===== Execute External Command ===== */
int execute_external(char **args, int argc, char *input_file, char *output_file, int append) {
    (void)argc;
    /* Expand globs */
    int new_count;
    char **expanded = expand_globs(args, argc, &new_count);

    pid_t pid = fork();
    if (pid == 0) {
        /* Setup redirections */
        if (input_file) {
            int fd = open(input_file, O_RDONLY);
            if (fd < 0) {
                fprintf(stderr, "xsh: %s: %s\n", input_file, strerror(errno));
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        if (output_file) {
            int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
            int fd = open(output_file, flags, 0644);
            if (fd < 0) {
                fprintf(stderr, "xsh: %s: %s\n", output_file, strerror(errno));
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        /* Reset signal handlers */
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);

        execvp(expanded[0], expanded);
        fprintf(stderr, FGRGB(255,80,80) "✗" RESET " xsh: %s: %s\n", expanded[0], strerror(errno));
        exit(127);
    } else if (pid < 0) {
        perror("xsh: fork");
        for (int i = 0; expanded[i]; i++) free(expanded[i]);
        free(expanded);
        return 1;
    }

    int status;
    waitpid(pid, &status, 0);
    for (int i = 0; expanded[i]; i++) free(expanded[i]);
    free(expanded);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

/* ===== Execute Pipeline ===== */
int execute_pipeline(char **all_tokens, int *pipe_positions, int pipe_count, int total_count,
                     char *input_file, char *output_file, int append) {
    if (pipe_count == 0) {
        /* No pipe, just execute */
        int argc = total_count;
        char **args = all_tokens;
        args[argc] = NULL;

        /* Check for background */
        int background = 0;
        if (argc > 0 && strcmp(args[argc - 1], "&") == 0) {
            background = 1;
            args[--argc] = NULL;
        }

        if (argc == 0) return 0;

        /* Check for variable assignment: KEY=value */
        if (strchr(args[0], '=') && args[0][0] != '=') {
            /* Handle multiple assignments like: A=1 B=2 cmd or just A=1 */
            int last_assign = -1;
            for (int i = 0; i < argc; i++) {
                char *eq = strchr(args[i], '=');
                if (eq && eq != args[i]) {
                    /* Check all chars before = are valid identifier chars */
                    int valid = 1;
                    for (char *p = args[i]; p < eq; p++) {
                        if (!isalnum((unsigned char)*p) && *p != '_') { valid = 0; break; }
                    }
                    if (valid) last_assign = i;
                    else break;
                } else break;
            }
            if (last_assign == argc - 1) {
                /* Pure assignment(s), no command */
                for (int i = 0; i <= last_assign; i++) {
                    char *eq = strchr(args[i], '=');
                    char varname[256];
                    int nlen = eq - args[i];
                    strncpy(varname, args[i], nlen);
                    varname[nlen] = '\0';
                    setenv(varname, eq + 1, 1);
                }
                return 0;
            }
        }

        /* Check alias */
        const char *alias_val = alias_get(args[0]);
        if (alias_val) {
            char expanded[MAX_CMD_LEN];
            /* Build new command: alias_val + rest of args */
            strncpy(expanded, alias_val, sizeof(expanded) - 1);
            for (int i = 1; i < argc; i++) {
                strncat(expanded, " ", sizeof(expanded) - strlen(expanded) - 1);
                strncat(expanded, args[i], sizeof(expanded) - strlen(expanded) - 1);
            }
            return execute_line(expanded);
        }

        /* Builtins - handle redirections by saving/restoring stdio */
        int saved_stdout = -1, saved_stdin = -1;
        if (output_file) {
            int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
            int fd = open(output_file, flags, 0644);
            if (fd < 0) {
                fprintf(stderr, "xsh: %s: %s\n", output_file, strerror(errno));
                return 1;
            }
            saved_stdout = dup(STDOUT_FILENO);
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        if (input_file) {
            int fd = open(input_file, O_RDONLY);
            if (fd < 0) {
                fprintf(stderr, "xsh: %s: %s\n", input_file, strerror(errno));
                if (saved_stdout >= 0) { dup2(saved_stdout, STDOUT_FILENO); close(saved_stdout); }
                return 1;
            }
            saved_stdin = dup(STDIN_FILENO);
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        int ret = 0;
        if (strcmp(args[0], "cd") == 0)      ret = builtin_cd(args, argc);
        else if (strcmp(args[0], "pwd") == 0)     ret = builtin_pwd();
        else if (strcmp(args[0], "echo") == 0)    ret = builtin_echo(args, argc);
        else if (strcmp(args[0], "export") == 0)  ret = builtin_export(args, argc);
        else if (strcmp(args[0], "unset") == 0)   ret = builtin_unset(args, argc);
        else if (strcmp(args[0], "history") == 0) ret = builtin_history(args, argc);
        else if (strcmp(args[0], "help") == 0)    ret = builtin_help();
        else if (strcmp(args[0], "alias") == 0)   ret = builtin_alias(args, argc);
        else if (strcmp(args[0], "unalias") == 0) ret = builtin_unalias(args, argc);
        else if (strcmp(args[0], "which") == 0)   ret = builtin_which(args, argc);
        else if (strcmp(args[0], "jobs") == 0)    ret = builtin_jobs();
        else if (strcmp(args[0], "fg") == 0)      ret = builtin_fg(args, argc);
        else if (strcmp(args[0], "bg") == 0)      ret = builtin_bg(args, argc);
        else if (strcmp(args[0], "source") == 0 || strcmp(args[0], ".") == 0)
            ret = builtin_source(args, argc);
        else if (strcmp(args[0], "true") == 0)    ret = 0;
        else if (strcmp(args[0], "false") == 0)   ret = 1;
        else if (strcmp(args[0], "exit") == 0) {
            if (saved_stdout >= 0) { dup2(saved_stdout, STDOUT_FILENO); close(saved_stdout); }
            if (saved_stdin >= 0) { dup2(saved_stdin, STDIN_FILENO); close(saved_stdin); }
            int code = (argc > 1) ? atoi(args[1]) : last_exit_code;
            running = 0;
            return code;
        } else if (strcmp(args[0], "type") == 0) {
            for (int i = 1; i < argc; i++) {
                if (alias_get(args[i])) {
                    printf("%s is an alias for '%s'\n", args[i], alias_get(args[i]));
                } else {
                    const char *builtins[] = {"cd","pwd","echo","export","unset","history",
                        "help","alias","unalias","which","jobs","fg","bg","source","true","false","exit","type",NULL};
                    int is_builtin = 0;
                    for (int j = 0; builtins[j]; j++) {
                        if (strcmp(args[i], builtins[j]) == 0) {
                            printf("%s is a shell builtin\n", args[i]);
                            is_builtin = 1; break;
                        }
                    }
                    if (!is_builtin) {
                        char *tmp[] = {"which", args[i], NULL};
                        builtin_which(tmp, 2);
                    }
                }
            }
            ret = 0;
        } else {
            /* Restore stdio before external command */
            if (saved_stdout >= 0) { dup2(saved_stdout, STDOUT_FILENO); close(saved_stdout); saved_stdout = -1; }
            if (saved_stdin >= 0) { dup2(saved_stdin, STDIN_FILENO); close(saved_stdin); saved_stdin = -1; }

            /* Background execution */
            if (background) {
                pid_t pid = fork();
                if (pid == 0) {
                    signal(SIGINT, SIG_IGN);
                    /* Re-apply redirections in child */
                    if (output_file) {
                        int flags2 = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
                        int fd = open(output_file, flags2, 0644);
                        if (fd >= 0) { dup2(fd, STDOUT_FILENO); close(fd); }
                    }
                    if (input_file) {
                        int fd = open(input_file, O_RDONLY);
                        if (fd >= 0) { dup2(fd, STDIN_FILENO); close(fd); }
                    }
                    int nc;
                    char **exp = expand_globs(args, argc, &nc);
                    execvp(exp[0], exp);
                    exit(127);
                } else if (pid > 0) {
                    char cmd_str[256];
                    strncpy(cmd_str, args[0], 255);
                    Job *j = job_add(pid, cmd_str);
                    if (j) printf("[%d] %d\n", j->job_id, pid);
                    return 0;
                }
            }
            return execute_external(args, argc, input_file, output_file, append);
        }

        /* Restore stdio for builtins */
        fflush(stdout);
        if (saved_stdout >= 0) { dup2(saved_stdout, STDOUT_FILENO); close(saved_stdout); }
        if (saved_stdin >= 0) { dup2(saved_stdin, STDIN_FILENO); close(saved_stdin); }
        return ret;
    }

    /* Pipeline execution */
    int num_cmds = pipe_count + 1;
    int pipefds[MAX_ARGS][2];
    pid_t pids[MAX_ARGS];


    for (int i = 0; i < pipe_count; i++) {
        if (pipe(pipefds[i]) < 0) {
            perror("xsh: pipe");
            return 1;
        }
    }

    int prev_end = 0;
    for (int ci = 0; ci < num_cmds; ci++) {
        int cmd_start = (ci == 0) ? 0 : pipe_positions[ci - 1];
        int cmd_end   = (ci < pipe_count) ? pipe_positions[ci] : total_count;

        char **cmd_args = all_tokens + cmd_start;
        int cmd_argc = cmd_end - cmd_start;
        char *saved_tok = cmd_args[cmd_argc]; /* save token before overwriting */
        cmd_args[cmd_argc] = NULL;

        pids[ci] = fork();
        if (pids[ci] < 0) { perror("fork"); return 1; }
        if (pids[ci] == 0) {
            signal(SIGINT, SIG_DFL);

            /* Input */
            if (ci == 0 && input_file) {
                int fd = open(input_file, O_RDONLY);
                if (fd >= 0) { dup2(fd, STDIN_FILENO); close(fd); }
            } else if (ci > 0) {
                if (dup2(pipefds[ci - 1][0], STDIN_FILENO) < 0) { perror("dup2 stdin"); exit(1); }
            }

            /* Output */
            if (ci == num_cmds - 1 && output_file) {
                int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
                int fd = open(output_file, flags, 0644);
                if (fd >= 0) { dup2(fd, STDOUT_FILENO); close(fd); }
            } else if (ci < num_cmds - 1) {
                if (dup2(pipefds[ci][1], STDOUT_FILENO) < 0) { perror("dup2 stdout"); exit(1); }
            }

            /* Close all pipe fds */
            for (int j = 0; j < pipe_count; j++) {
                close(pipefds[j][0]);
                close(pipefds[j][1]);
            }

            int new_count;
            char **expanded = expand_globs(cmd_args, cmd_argc, &new_count);
            execvp(expanded[0], expanded);
            fprintf(stderr, "xsh: %s: %s\n", cmd_args[0], strerror(errno));
            exit(127);
        }
        /* Restore the token we overwrote with NULL */
        cmd_args[cmd_argc] = saved_tok;
        (void)prev_end;
        prev_end = pipefds[ci < pipe_count ? ci : 0][0];
    }

    /* Close all pipes in parent */
    for (int i = 0; i < pipe_count; i++) {
        close(pipefds[i][0]);
        close(pipefds[i][1]);
    }

    /* Wait for all children */
    int last_status = 0;
    for (int ci = 0; ci < num_cmds; ci++) {
        int status;
        waitpid(pids[ci], &status, 0);
        if (ci == num_cmds - 1) {
            last_status = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
        }
    }
    return last_status;
}

/* ===== Main Execute Line ===== */
int execute_line(const char *line) {
    /* Skip empty or comments */
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0' || *p == '#') return 0;

    /* Handle semicolon-separated commands */
    /* Simple check for ; outside quotes */
    int in_s = 0, in_d = 0;
    for (int i = 0; line[i]; i++) {
        if (line[i] == '\'' && !in_d) { in_s = !in_s; continue; }
        if (line[i] == '"' && !in_s) { in_d = !in_d; continue; }
        if (line[i] == ';' && !in_s && !in_d) {
            char part1[MAX_CMD_LEN], part2[MAX_CMD_LEN];
            strncpy(part1, line, i);
            part1[i] = '\0';
            strncpy(part2, line + i + 1, sizeof(part2) - 1);
            part2[sizeof(part2) - 1] = '\0';
            execute_line(part1);
            return execute_line(part2);
        }
    }

    /* Handle && */
    in_s = 0; in_d = 0;
    for (int i = 0; line[i]; i++) {
        if (line[i] == '\'' && !in_d) { in_s = !in_s; continue; }
        if (line[i] == '"' && !in_s) { in_d = !in_d; continue; }
        if (line[i] == '&' && line[i+1] == '&' && !in_s && !in_d) {
            char part1[MAX_CMD_LEN], part2[MAX_CMD_LEN];
            strncpy(part1, line, i);
            part1[i] = '\0';
            strncpy(part2, line + i + 2, sizeof(part2) - 1);
            int r = execute_line(part1);
            if (r == 0) return execute_line(part2);
            return r;
        }
    }

    /* Handle || */
    in_s = 0; in_d = 0;
    for (int i = 0; line[i]; i++) {
        if (line[i] == '\'' && !in_d) { in_s = !in_s; continue; }
        if (line[i] == '"' && !in_s) { in_d = !in_d; continue; }
        if (line[i] == '|' && line[i+1] == '|' && !in_s && !in_d) {
            char part1[MAX_CMD_LEN], part2[MAX_CMD_LEN];
            strncpy(part1, line, i);
            part1[i] = '\0';
            strncpy(part2, line + i + 2, sizeof(part2) - 1);
            int r = execute_line(part1);
            if (r != 0) return execute_line(part2);
            return r;
        }
    }

    ParseResult *pr = parse_line(line);
    if (!pr || pr->count == 0) {
        free_parse_result(pr);
        free(pr);
        return 0;
    }

    /* DEBUG */
    for (int di = 0; di < pr->count; di++) {
    }

    int result = execute_pipeline(pr->tokens, pr->pipe_positions, pr->pipe_count, pr->count,
                                   pr->input_file, pr->output_file, pr->append_output);

    free_parse_result(pr);
    free(pr);
    return result;
}

/* ===== Custom Readline Implementation ===== */

static struct termios orig_termios;
static char history_buf[MAX_HISTORY][MAX_CMD_LEN];
static int history_count = 0;

void history_add(const char *line) {
    if (!line || !*line) return;
    /* Avoid duplicates */
    if (history_count > 0 && strcmp(history_buf[(history_count - 1) % MAX_HISTORY], line) == 0)
        return;
    strncpy(history_buf[history_count % MAX_HISTORY], line, MAX_CMD_LEN - 1);
    history_count++;
}

void history_load(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_CMD_LEN];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (*line) history_add(line);
    }
    fclose(f);
}

void history_save(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    int start = (history_count > MAX_HISTORY) ? history_count - MAX_HISTORY : 0;
    for (int i = start; i < history_count; i++) {
        fprintf(f, "%s\n", history_buf[i % MAX_HISTORY]);
    }
    fclose(f);
}

static void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

/* Get terminal width */
static int term_width(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return ws.ws_col;
    return 80;
}

/* Tab completion: return sorted list of matches */
static char **get_completions(const char *buf, int pos, int *count) {
    /* Find current word */
    int word_start = pos;
    while (word_start > 0 && buf[word_start - 1] != ' ') word_start--;
    const char *word = buf + word_start;
    int word_len = pos - word_start;

    char prefix[MAX_CMD_LEN];
    strncpy(prefix, word, word_len);
    prefix[word_len] = '\0';

    char **results = malloc(4096 * sizeof(char*));
    *count = 0;

    int is_first_word = (word_start == 0);

    if (!is_first_word || strchr(prefix, '/') || prefix[0] == '.' || prefix[0] == '~') {
        /* File completion */
        char pattern[MAX_PATH];
        snprintf(pattern, sizeof(pattern), "%s*", prefix);
        glob_t g;
        if (glob(pattern, GLOB_TILDE | GLOB_MARK, NULL, &g) == 0) {
            for (size_t i = 0; i < g.gl_pathc && *count < 4095; i++) {
                results[(*count)++] = strdup(g.gl_pathv[i]);
            }
            globfree(&g);
        }
    } else {
        /* Command completion */
        /* Builtins */
        const char *builtins[] = {"cd","pwd","echo","export","unset","history","help",
            "alias","unalias","which","jobs","fg","bg","source","true","false","exit","type",NULL};
        for (int i = 0; builtins[i] && *count < 4095; i++) {
            if (strncmp(builtins[i], prefix, word_len) == 0)
                results[(*count)++] = strdup(builtins[i]);
        }
        /* Aliases */
        for (int i = 0; i < alias_count && *count < 4095; i++) {
            if (strncmp(aliases[i].name, prefix, word_len) == 0)
                results[(*count)++] = strdup(aliases[i].name);
        }
        /* PATH */
        char *path_env = getenv("PATH");
        if (path_env) {
            char path_copy[MAX_CMD_LEN];
            strncpy(path_copy, path_env, sizeof(path_copy) - 1);
            char *dir = strtok(path_copy, ":");
            while (dir && *count < 4095) {
                DIR *d = opendir(dir);
                if (d) {
                    struct dirent *ent;
                    while ((ent = readdir(d)) && *count < 4095) {
                        if (strncmp(ent->d_name, prefix, word_len) == 0) {
                            char full[MAX_PATH];
                            snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);
                            if (access(full, X_OK) == 0) {
                                results[(*count)++] = strdup(ent->d_name);
                            }
                        }
                    }
                    closedir(d);
                }
                dir = strtok(NULL, ":");
            }
        }
    }
    results[*count] = NULL;
    return results;
}

/* Find common prefix of completions */
static char *common_prefix(char **completions, int count) {
    if (count == 0) return strdup("");
    char *base = completions[0];
    int len = strlen(base);
    for (int i = 1; i < count; i++) {
        int j = 0;
        while (j < len && base[j] == completions[i][j]) j++;
        len = j;
    }
    char *cp = malloc(len + 1);
    strncpy(cp, base, len);
    cp[len] = '\0';
    return cp;
}

/* Our custom readline function */
char *xsh_readline(const char *prompt) {
    if (!isatty(STDIN_FILENO)) {
        /* Non-interactive: just read a line */
        static char buf[MAX_CMD_LEN];
        if (!fgets(buf, sizeof(buf), stdin)) return NULL;
        buf[strcspn(buf, "\n")] = '\0';
        return strdup(buf);
    }

    printf("%s", prompt);
    fflush(stdout);

    enable_raw_mode();

    char buf[MAX_CMD_LEN];
    int len = 0;
    int cursor = 0;
    int hist_idx = history_count; /* start at "current" (empty) */
    char saved[MAX_CMD_LEN] = "";

    buf[0] = '\0';

    while (1) {
        unsigned char c;
        int n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) break;

        if (c == '\r' || c == '\n') {
            /* Enter */
            printf("\r\n");
            break;

        } else if (c == 3) {
            /* Ctrl-C */
            printf("^C\r\n");
            buf[0] = '\0';
            len = 0;
            break;

        } else if (c == 4) {
            /* Ctrl-D */
            if (len == 0) {
                disable_raw_mode();
                return NULL;
            }

        } else if (c == 127 || c == 8) {
            /* Backspace */
            if (cursor > 0) {
                memmove(buf + cursor - 1, buf + cursor, len - cursor);
                len--;
                cursor--;
                buf[len] = '\0';
            }

        } else if (c == 23) {
            /* Ctrl-W: delete word */
            while (cursor > 0 && buf[cursor-1] == ' ') { cursor--; len--; memmove(buf+cursor, buf+cursor+1, len-cursor+1); }
            while (cursor > 0 && buf[cursor-1] != ' ') { cursor--; len--; memmove(buf+cursor, buf+cursor+1, len-cursor+1); }
            buf[len] = '\0';

        } else if (c == 21) {
            /* Ctrl-U: clear line */
            cursor = 0;
            len = 0;
            buf[0] = '\0';

        } else if (c == 1) {
            /* Ctrl-A: home */
            cursor = 0;

        } else if (c == 5) {
            /* Ctrl-E: end */
            cursor = len;

        } else if (c == 12) {
            /* Ctrl-L: clear screen */
            printf("\033[2J\033[H");
            printf("%s%.*s", prompt, len, buf);
            fflush(stdout);

        } else if (c == '\t') {
            /* Tab completion */
            int comp_count;
            char **completions = get_completions(buf, cursor, &comp_count);

            if (comp_count == 1) {
                /* Find word start */
                int ws = cursor;
                while (ws > 0 && buf[ws-1] != ' ') ws--;
                int word_len_cur = cursor - ws;
                char *match = completions[0];
                int match_len = strlen(match);
                int add_len = match_len - word_len_cur;
                if (add_len > 0 && len + add_len < MAX_CMD_LEN - 1) {
                    memmove(buf + cursor + add_len, buf + cursor, len - cursor);
                    memcpy(buf + cursor, match + word_len_cur, add_len);
                    len += add_len;
                    cursor += add_len;
                    buf[len] = '\0';
                    /* Add space if single match */
                    if (buf[len-1] != '/' && len < MAX_CMD_LEN - 1) {
                        memmove(buf + cursor + 1, buf + cursor, len - cursor);
                        buf[cursor] = ' ';
                        len++;
                        cursor++;
                        buf[len] = '\0';
                    }
                }
            } else if (comp_count > 1) {
                /* Show completions */
                char *cp = common_prefix(completions, comp_count);
                int ws = cursor;
                while (ws > 0 && buf[ws-1] != ' ') ws--;
                int word_len_cur = cursor - ws;
                int cp_len = strlen(cp);
                if (cp_len > word_len_cur) {
                    int add_len = cp_len - word_len_cur;
                    memmove(buf + cursor + add_len, buf + cursor, len - cursor);
                    memcpy(buf + cursor, cp + word_len_cur, add_len);
                    len += add_len;
                    cursor += add_len;
                    buf[len] = '\0';
                } else {
                    /* List completions */
                    printf("\r\n");
                    int tw = term_width();
                    int col_w = 20;
                    int cols = tw / col_w;
                    if (cols < 1) cols = 1;
                    for (int i = 0; i < comp_count; i++) {
                        printf(FGRGB(0,220,255) "%-*s" RESET, col_w, completions[i]);
                        if ((i + 1) % cols == 0) printf("\r\n");
                    }
                    if (comp_count % cols != 0) printf("\r\n");
                    printf("%s%.*s", prompt, len, buf);
                    fflush(stdout);
                }
                free(cp);
            }
            for (int i = 0; completions[i]; i++) free(completions[i]);
            free(completions);

        } else if (c == 27) {
            /* Escape sequence */
            unsigned char seq[4] = {0};
            if (read(STDIN_FILENO, &seq[0], 1) <= 0) continue;
            if (seq[0] == '[') {
                if (read(STDIN_FILENO, &seq[1], 1) <= 0) continue;
                if (seq[1] == 'A') {
                    /* Up arrow: history prev */
                    if (hist_idx > 0) {
                        if (hist_idx == history_count) {
                            strncpy(saved, buf, MAX_CMD_LEN - 1);
                        }
                        hist_idx--;
                        int real_idx = hist_idx % MAX_HISTORY;
                        strncpy(buf, history_buf[real_idx], MAX_CMD_LEN - 1);
                        len = strlen(buf);
                        cursor = len;
                    }
                } else if (seq[1] == 'B') {
                    /* Down arrow: history next */
                    if (hist_idx < history_count) {
                        hist_idx++;
                        if (hist_idx == history_count) {
                            strncpy(buf, saved, MAX_CMD_LEN - 1);
                        } else {
                            strncpy(buf, history_buf[hist_idx % MAX_HISTORY], MAX_CMD_LEN - 1);
                        }
                        len = strlen(buf);
                        cursor = len;
                    }
                } else if (seq[1] == 'C') {
                    /* Right arrow */
                    if (cursor < len) cursor++;
                } else if (seq[1] == 'D') {
                    /* Left arrow */
                    if (cursor > 0) cursor--;
                } else if (seq[1] == 'H') {
                    cursor = 0;
                } else if (seq[1] == 'F') {
                    cursor = len;
                } else if (seq[1] == '3') {
                    /* Delete key: ESC[3~ */
                    unsigned char tmp;
                    read(STDIN_FILENO, &tmp, 1);
                    if (cursor < len) {
                        memmove(buf + cursor, buf + cursor + 1, len - cursor);
                        len--;
                        buf[len] = '\0';
                    }
                } else if (seq[1] == '1' || seq[1] == '7') {
                    unsigned char tmp;
                    read(STDIN_FILENO, &tmp, 1);
                    cursor = 0;
                } else if (seq[1] == '4' || seq[1] == '8') {
                    unsigned char tmp;
                    read(STDIN_FILENO, &tmp, 1);
                    cursor = len;
                }
            }
        } else if (c >= 32) {
            /* Regular character */
            if (len < MAX_CMD_LEN - 1) {
                memmove(buf + cursor + 1, buf + cursor, len - cursor);
                buf[cursor] = c;
                len++;
                cursor++;
                buf[len] = '\0';
            }
        }

        /* Redraw line */
        printf("\r\033[K%s%s", prompt, buf);
        /* Position cursor */
        if (cursor < len) {
            /* Move cursor left by (len - cursor) */
            printf("\033[%dD", len - cursor);
        }
        fflush(stdout);
    }

    disable_raw_mode();
    if (buf[0] == '\0' && len == 0) {
        /* Check if Ctrl-C */
        return strdup("");
    }
    return strdup(buf);
}

/* Wrapper for history display */
int builtin_history_display(char **args, int argc) {
    int limit = 0;
    if (argc > 1) limit = atoi(args[1]);
    int start = 0;
    if (limit > 0 && limit < history_count) start = history_count - limit;
    for (int i = start; i < history_count; i++) {
        printf(FGRGB(0,150,255) " %4d " RESET "%s\n", i + 1, history_buf[i % MAX_HISTORY]);
    }
    return 0;
}

int builtin_history(char **args, int argc) {
    return builtin_history_display(args, argc);
}

/* ===== Load RC File ===== */
void load_rc(void) {
    char rc_path[MAX_PATH];
    const char *home = getenv("HOME");
    if (!home) return;
    snprintf(rc_path, sizeof(rc_path), "%s/%s", home, XSH_RC_FILE);

    if (access(rc_path, R_OK) == 0) {
        char *args[] = {"source", rc_path, NULL};
        builtin_source(args, 2);
    }
}

/* ===== Load/Save History ===== */
void load_history_file(void) {
    char hist_path[MAX_PATH];
    const char *home = getenv("HOME");
    if (!home) return;
    snprintf(hist_path, sizeof(hist_path), "%s/%s", home, XSH_HISTORY_FILE);
    history_load(hist_path);
}

void save_history_file(void) {
    char hist_path[MAX_PATH];
    const char *home = getenv("HOME");
    if (!home) return;
    snprintf(hist_path, sizeof(hist_path), "%s/%s", home, XSH_HISTORY_FILE);
    history_save(hist_path);
}

/* ===== Main ===== */
int main(int argc, char *argv[]) {
    /* Initialize */
    gethostname(hostname, sizeof(hostname));
    /* Remove domain from hostname */
    char *dot = strchr(hostname, '.');
    if (dot) *dot = '\0';

    user_info = getpwuid(getuid());

    /* Setup signals */
    signal(SIGINT, sigint_handler);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGCHLD, sigchld_handler);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    /* Set XSH as shell env — use argv[0] if available, otherwise a generic path */
    {
        const char *shell_path = (argc > 0 && argv[0][0] == '/') ? argv[0] : "xsh";
        setenv("SHELL", shell_path, 0);
    }
    setenv("XSH_VERSION", XSH_VERSION, 1);

    /* Initialize readline */
    /* (using custom implementation) */

    /* Check if running as interactive shell */
    int interactive = isatty(STDIN_FILENO);

    /* Handle -c option */
    if (argc > 1 && strcmp(argv[1], "-c") == 0) {
        if (argc > 2) {
            int r = execute_line(argv[2]);
            return r;
        }
        return 1;
    }

    /* Script mode */
    if (argc > 1) {
        FILE *f = fopen(argv[1], "r");
        if (!f) {
            fprintf(stderr, "xsh: %s: %s\n", argv[1], strerror(errno));
            return 1;
        }
        /* Redirect stdin to /dev/null so child processes don't consume script */
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            close(devnull);
        }
        char line[MAX_CMD_LEN];
        int ret = 0;
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\n")] = '\0';
            if (line[0] == '#' || line[0] == '\0') continue;
            ret = execute_line(line);
        }
        fclose(f);
        return ret;
    }

    if (interactive) {
        print_banner();
        load_history_file();
        load_rc();

        /* Default aliases — use platform-appropriate color flags */
#if defined(__linux__)
        alias_set("ls",   "ls --color=auto");
        alias_set("ll",   "ls -la --color=auto");
        alias_set("la",   "ls -A --color=auto");
        alias_set("grep", "grep --color=auto");
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
        alias_set("ls",   "ls -G");
        alias_set("ll",   "ls -laG");
        alias_set("la",   "ls -AG");
        alias_set("grep", "grep --color=auto");
#else
        alias_set("ls",   "ls");
        alias_set("ll",   "ls -la");
        alias_set("la",   "ls -A");
        alias_set("grep", "grep");
#endif
        alias_set("..", "cd ..");
        alias_set("...", "cd ../..");
        alias_set("~", "cd ~");
    }

    /* Main REPL loop */
    while (running) {
        char *prompt = interactive ? build_prompt() : "";
        char *line = xsh_readline(prompt);

        if (!line) {
            /* EOF */
            if (interactive) printf("\n" FGRGB(0,255,180) "  Goodbye! 👋\n\n" RESET);
            break;
        }

        /* Skip empty lines */
        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

        if (*trimmed) {
            history_add(trimmed);
            last_exit_code = execute_line(trimmed);
        }

        free(line);
    }

    if (interactive) {
        save_history_file();
    }

    return last_exit_code;
}
