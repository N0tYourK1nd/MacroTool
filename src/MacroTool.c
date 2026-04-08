#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <X11/XKBlib.h>
#include <ctype.h>
#include "cJSON.h"
#include <sys/stat.h>

// --- Hotkey configuration ---
#define HOTKEY_KEY       "m"                    // X keysym name: "m", "F9", "Return", etc.
#define HOTKEY_MODIFIERS (ControlMask)          // Modifiers: ControlMask, Mod1Mask (Alt), ShiftMask
// ----------------------------

typedef struct { char *path; char *cmd; } MacroEntry;

MacroEntry *entries = NULL;
int entry_count = 0, entry_capacity = 0;

typedef struct { char ch; KeySym ks; int shift; } KeyMap;

KeyMap special_keys[] = {
    {' ', XK_space, 0},
    {',', XK_comma, 0},
    {'.', XK_period, 0},
    {'/', XK_slash, 0},
    {'\\', XK_backslash, 0},
    {';', XK_semicolon, 0},
    {':', XK_colon, 1},
    {'-', XK_minus, 0},
    {'_', XK_underscore, 1},
    {'+', XK_plus, 1},
    {'=', XK_equal, 0},
    {'*', XK_asterisk, 1},
    {'&', XK_ampersand, 1},
    {'|', XK_bar, 1},
    {'!', XK_exclam, 1},
    {'?', XK_question, 1},
    {'$', XK_dollar, 1},
    {'#', XK_numbersign, 1},
    {'%', XK_percent, 1},
    {'^', XK_asciicircum, 1},
    {'~', XK_asciitilde, 1},
    {'`', XK_grave, 0},
    {'\'', XK_apostrophe, 0},
    {'"', XK_quotedbl, 1},
    {'<', XK_less, 1},
    {'>', XK_greater, 1},
    {'(', XK_parenleft, 0},
    {')', XK_parenright, 0},
    {'[', XK_bracketleft, 0},
    {']', XK_bracketright, 0},
    {'{', XK_braceleft, 1},
    {'}', XK_braceright, 1},
    {0, 0, 0}
};

typedef struct { char ch; KeySym ks; int shift; int altgr; } DEKeyMap;

DEKeyMap de_keys[] = {
    {'@', XK_q, 0, 1},
    {'|', XK_less, 0, 1},
    {'[', XK_8, 0, 1},
    {']', XK_9, 0, 1},
    {'{', XK_7, 1, 1},
    {'}', XK_0, 1, 1},
    {'\\', XK_ssharp, 0, 1},
    {'~', XK_plus, 0, 1},
    {'#', XK_numbersign, 0, 0},
    {'\'', XK_numbersign, 1, 0},
    {'`', XK_asciicircum, 0, 1},
    {'/', XK_7, 1, 0},
    {'-', XK_minus, 0, 0},
    {'_', XK_minus, 1, 0},
    {'<', XK_less, 0, 0},
    {'>', XK_less, 1, 0},
    {':', XK_period, 1, 0},
    {';', XK_comma, 1, 0},
    {'&', XK_6, 1, 0},
    {'*', XK_plus, 1, 0},
    {'"', XK_2, 1, 0},
    {'\'', XK_numbersign, 0, 0},
    {'=', XK_0, 0, 1},
    {'+', XK_plus, 0, 0},
    {'.', XK_period, 0, 0},
    {',', XK_comma, 0, 0},
    {'$', XK_4, 1, 0},
    {'%', XK_5, 0, 1},
    {0, 0, 0, 0}
};

DEKeyMap *lookup_de_char(char c) {
    for (int i = 0; de_keys[i].ch; i++)
        if (de_keys[i].ch == c) return &de_keys[i];
    return NULL;
}

KeyMap *lookup_char(char c) {
    for (int i = 0; special_keys[i].ch; i++)
        if (special_keys[i].ch == c) return &special_keys[i];
    return NULL;
}

// Returns active layout; sets *all_out to all comma-separated layouts if non-NULL.
char *get_active_keyboard_layout(Display *dpy, char **all_out) {
    static char layout[16] = "unknown";
    static char all[64] = "unknown";

    FILE *fp = popen("setxkbmap -query | grep layout | awk '{print $2}'", "r");
    if (fp) {
        if (fgets(all, sizeof(all), fp))
            all[strcspn(all, "\n")] = 0;
        pclose(fp);
    }
    if (all_out) *all_out = all;

    XkbStateRec state;
    if (XkbGetState(dpy, XkbUseCoreKbd, &state) != Success)
        return layout;

    char tmp[64];
    strncpy(tmp, all, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = 0;
    int idx = 0;
    char *tok = strtok(tmp, ",");
    while (tok) {
        if (idx == state.group) {
            strncpy(layout, tok, sizeof(layout) - 1);
            layout[sizeof(layout) - 1] = 0;
            break;
        }
        tok = strtok(NULL, ",");
        idx++;
    }
    return layout;
}

void type_char(Display *dpy, char c, const char *layout) {
    if (layout && strcmp(layout, "de") == 0) {
        DEKeyMap *map = lookup_de_char(c);
        if (map) {
            if (map->shift) XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_Shift_L), True, 0);
            if (map->altgr) XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_ISO_Level3_Shift), True, 0);
            KeyCode kc = XKeysymToKeycode(dpy, map->ks);
            XTestFakeKeyEvent(dpy, kc, True, 0);
            XTestFakeKeyEvent(dpy, kc, False, 0);
            if (map->altgr) XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_ISO_Level3_Shift), False, 0);
            if (map->shift) XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_Shift_L), False, 0);
            XFlush(dpy);
            usleep(5000);
            return;
        }
    }

    KeyMap *map = lookup_char(c);
    KeySym ks; int needsShift = 0;
    if (map) { ks = map->ks; needsShift = map->shift; }
    else {
        ks = XStringToKeysym((char[]){(char)toupper(c), 0});
        if (!ks) return;
        if (isupper(c)) needsShift = 1;
    }

    KeyCode kc = XKeysymToKeycode(dpy, ks);
    if (!kc) return;
    if (needsShift) XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_Shift_L), True, 0);
    XTestFakeKeyEvent(dpy, kc, True, 0);
    XTestFakeKeyEvent(dpy, kc, False, 0);
    if (needsShift) XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_Shift_L), False, 0);
    XFlush(dpy);
    usleep(5000);
}

void type_string(Display *dpy, const char *str, const char *layout) {
    for (const char *p = str; *p; p++) type_char(dpy, *p, layout);
}

void add_entry(const char *path, const char *cmd) {
    if (entry_count >= entry_capacity) {
        entry_capacity = entry_capacity ? entry_capacity * 2 : 128;
        entries = realloc(entries, entry_capacity * sizeof(MacroEntry));
        if (!entries) { perror("realloc"); exit(1); }
    }
    entries[entry_count].path = strdup(path);
    entries[entry_count].cmd  = strdup(cmd);
    if (!entries[entry_count].path || !entries[entry_count].cmd) { perror("strdup"); exit(1); }
    entry_count++;
}

void collect_entries_with_file(cJSON *node, const char *prefix, const char *file_prefix) {
    cJSON *item;
    cJSON_ArrayForEach(item, node) {
        char new_prefix[512];
        snprintf(new_prefix, sizeof(new_prefix), "%s%s%s", prefix, *prefix ? "/" : "", item->string);

        if (cJSON_IsString(item)) {
            char full_path[768];
            snprintf(full_path, sizeof(full_path), "%s : %s", file_prefix, new_prefix);
            add_entry(full_path, item->valuestring);
        } else if (cJSON_IsObject(item)) {
            collect_entries_with_file(item, new_prefix, file_prefix);
        }
    }
}

static int cmp_entry(const void *a, const void *b) {
    return strcmp(((MacroEntry *)a)->path, ((MacroEntry *)b)->path);
}

char *get_json_directory() {
    char exe_path[4096];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) return NULL;
    exe_path[len] = '\0';

    char *last_slash = strrchr(exe_path, '/');
    if (!last_slash) return NULL;
    *last_slash = '\0';

    static char json_dir[4096];
    snprintf(json_dir, sizeof(json_dir), "%s/json", exe_path);
    return json_dir;
}

int ensure_user_defined_json(const char *json_dir) {
    char filepath[4096];
    snprintf(filepath, sizeof(filepath), "%s/user-defined.json", json_dir);
    if (access(filepath, F_OK) == 0) return 1;

    mkdir(json_dir, 0755);
    FILE *fp = fopen(filepath, "w");
    if (!fp) return 0;
    fprintf(fp, "{\n}\n");
    fclose(fp);
    return 1;
}

// Extract a parameter value from input. Stops at '/' if stop_slash is set.
static void extract_param(const char *in, const char *lf, const char *sf,
                           char *out, int max, int stop_slash) {
    const char *p = strstr(in, lf);
    int skip = (int)strlen(lf);
    if (!p) {
        if (!(p = strstr(in, sf))) return;
        skip = (int)strlen(sf);
    }
    p += skip;
    while (*p == ' ') p++;
    const char *e = p;
    while (*e && *e != '\n' && !(stop_slash && *e == '/')) e++;
    while (e > p && e[-1] == ' ') e--;
    int len = (int)(e - p) < max ? (int)(e - p) : max;
    strncpy(out, p, len);
    out[len] = '\0';
}

void parse_add_command(const char *input, char *category, char *name, char *command) {
    category[0] = name[0] = command[0] = '\0';
    extract_param(input, "/category:", "/c:",    category, 255,  1);
    extract_param(input, "/name:",     "/n:",    name,     255,  1);
    extract_param(input, "/command:",  "/cmd:",  command,  1023, 0);
}

int add_macro_to_json(const char *category, const char *name, const char *command) {
    char *json_dir = get_json_directory();
    if (!json_dir) return 0;
    if (!ensure_user_defined_json(json_dir)) return 0;

    char filepath[4096];
    snprintf(filepath, sizeof(filepath), "%s/user-defined.json", json_dir);

    FILE *fp = fopen(filepath, "r");
    if (!fp) return 0;
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    rewind(fp);
    char *buffer = malloc(len + 1);
    fread(buffer, 1, len, fp);
    buffer[len] = '\0';
    fclose(fp);

    cJSON *root = cJSON_Parse(buffer);
    free(buffer);
    if (!root) return 0;

    cJSON *current = root;
    if (strlen(category) > 0) {
        char *cat_copy = strdup(category);
        char *token = strtok(cat_copy, "/");
        while (token) {
            cJSON *next = cJSON_GetObjectItem(current, token);
            if (!next) {
                next = cJSON_CreateObject();
                cJSON_AddItemToObject(current, token, next);
            }
            current = next;
            token = strtok(NULL, "/");
        }
        free(cat_copy);
    }

    cJSON_AddStringToObject(current, name, command);

    fp = fopen(filepath, "w");
    if (!fp) { cJSON_Delete(root); return 0; }
    char *json_string = cJSON_Print(root);
    fprintf(fp, "%s", json_string);
    fclose(fp);
    free(json_string);
    cJSON_Delete(root);
    return 1;
}

void show_notification(const char *title, const char *message, int success) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "notify-send -t 1500 -u %s '%s' '%s' 2>/dev/null",
             success ? "low" : "critical", title, message);
    if (fork() == 0) { system(cmd); exit(0); }
}

// Split a macro path ("file.json : cat/subcat/name") into file, cat, title parts.
static void parse_path(const char *path, char *file, char *cat, char *title) {
    file[0] = cat[0] = title[0] = '\0';
    const char *sep = strstr(path, " : ");
    if (!sep) { strncpy(title, path, 255); return; }
    int flen = (int)(sep - path);
    strncpy(file, path, flen); file[flen] = '\0';
    const char *rest = sep + 3;
    const char *last_slash = strrchr(rest, '/');
    if (last_slash) {
        int clen = (int)(last_slash - rest);
        strncpy(cat, rest, clen); cat[clen] = '\0';
        strncpy(title, last_slash + 1, 255);
    } else {
        strncpy(title, rest, 255);
    }
}

char *select_macro(Display *dpy, const char *layout) {
    char *json_dir = get_json_directory();
    if (!json_dir) return NULL;
    ensure_user_defined_json(json_dir);

    DIR *dir = opendir(json_dir);
    if (!dir) { perror("opendir"); return NULL; }

    char old_cwd[4096];
    if (!getcwd(old_cwd, sizeof(old_cwd))) { perror("getcwd"); closedir(dir); return NULL; }
    if (chdir(json_dir) != 0) { perror("chdir"); closedir(dir); return NULL; }

    entry_count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (!strstr(ent->d_name, ".json")) continue;
        FILE *fp = fopen(ent->d_name, "r");
        if (!fp) continue;
        fseek(fp, 0, SEEK_END); long len = ftell(fp); rewind(fp);
        char *buf = malloc(len + 1); fread(buf, 1, len, fp); buf[len] = 0; fclose(fp);
        cJSON *root = cJSON_Parse(buf); free(buf);
        if (!root) { fprintf(stderr, "JSON parse error in %s\n", ent->d_name); continue; }
        collect_entries_with_file(root, "", ent->d_name);
        cJSON_Delete(root);
    }
    closedir(dir);
    chdir(old_cwd);

    qsort(entries, entry_count, sizeof(MacroEntry), cmp_entry);

    // Calculate column widths and build display lines in one pass
    int max_file = 0, max_cat = 0, max_title = 0;
    char file[256], cat[256], title[256];
    for (int i = 0; i < entry_count; i++) {
        parse_path(entries[i].path, file, cat, title);
        if ((int)strlen(file)  > max_file)  max_file  = strlen(file);
        if ((int)strlen(cat)   > max_cat)   max_cat   = strlen(cat);
        if ((int)strlen(title) > max_title) max_title = strlen(title);
    }

    char **display_lines = malloc(entry_count * sizeof(char *));
    for (int i = 0; i < entry_count; i++) {
        parse_path(entries[i].path, file, cat, title);
        int len = snprintf(NULL, 0, "%-*s | %-*s | %-*s | %s",
                           max_file, file, max_cat, cat, max_title, title, entries[i].cmd);
        display_lines[i] = malloc(len + 2);
        snprintf(display_lines[i], len + 2, "%-*s | %-*s | %-*s | %s",
                 max_file, file, max_cat, cat, max_title, title, entries[i].cmd);
    }

    FILE *tmp = fopen("/tmp/macro_list.txt", "w");
    for (int i = 0; i < entry_count; i++) fprintf(tmp, "%s\n", display_lines[i]);
    fclose(tmp);

    FILE *pipe = popen("dmenu -i -l 15 -nb '#23272e' -nf '#ffffff' -sb '#901bbe' -sf '#ffffff' < /tmp/macro_list.txt", "r");
    char macro_choice[4096];
    if (!fgets(macro_choice, sizeof(macro_choice), pipe)) {
        pclose(pipe);
        for (int i = 0; i < entry_count; i++) free(display_lines[i]);
        free(display_lines);
        return NULL;
    }
    pclose(pipe);
    macro_choice[strcspn(macro_choice, "\n")] = 0;

    if (strncmp(macro_choice, "!add", 4) == 0) {
        char category[256], name[256], command[1024];
        parse_add_command(macro_choice, category, name, command);
        if (strlen(name) == 0 || strlen(command) == 0) {
            show_notification("MacroTool", "Error: Name and command are required", 0);
        } else if (add_macro_to_json(category, name, command)) {
            char success_msg[256];
            snprintf(success_msg, sizeof(success_msg), "Added '%s' to %s",
                     name, strlen(category) > 0 ? category : "root");
            show_notification("MacroTool", success_msg, 1);
        } else {
            show_notification("MacroTool", "Failed to add macro", 0);
        }
        for (int i = 0; i < entry_count; i++) free(display_lines[i]);
        free(display_lines);
        return NULL;
    }

    char *cmd = NULL;
    for (int i = 0; i < entry_count; i++) {
        if (strcmp(macro_choice, display_lines[i]) == 0) {
            cmd = entries[i].cmd;
            break;
        }
    }

    for (int i = 0; i < entry_count; i++) free(display_lines[i]);
    free(display_lines);
    return cmd;
}

void grab_hotkey(Display *dpy, Window root) {
    KeyCode keycode = XKeysymToKeycode(dpy, XStringToKeysym(HOTKEY_KEY));
    unsigned int modifiers[] = {
        HOTKEY_MODIFIERS,
        HOTKEY_MODIFIERS|Mod2Mask,
        HOTKEY_MODIFIERS|LockMask,
        HOTKEY_MODIFIERS|Mod2Mask|LockMask
    };
    for (int i = 0; i < 4; i++)
        XGrabKey(dpy, keycode, modifiers[i], root, True, GrabModeAsync, GrabModeAsync);
    XSelectInput(dpy, root, KeyPressMask | KeyReleaseMask);
    XFlush(dpy);
}

int main() {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "Cannot open display\n"); return 1; }

    Window root = DefaultRootWindow(dpy);
    char *all_layouts;
    char *active_layout = get_active_keyboard_layout(dpy, &all_layouts);

    printf("Detected keyboard layouts: %s\n", all_layouts    ? all_layouts    : "unknown");
    printf("Active keyboard layout: %s\n",    active_layout  ? active_layout  : "unknown");

    grab_hotkey(dpy, root);

    XEvent ev;
    while (1) {
        XNextEvent(dpy, &ev);
        if (ev.type == KeyPress) {
            char *cmd = select_macro(dpy, active_layout);
            if (!cmd) continue;
            usleep(500000);
            type_string(dpy, cmd, active_layout);
        }
    }

    for (int i = 0; i < entry_count; i++) {
        free(entries[i].path);
        free(entries[i].cmd);
    }
    free(entries);
    XCloseDisplay(dpy);
    return 0;
}
