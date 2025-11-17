#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <ctype.h>
#include <signal.h>
#include "Elevator.h"

#define BUFFER_SIZE      4096
#define FRAME_HEADER_LEN 16
#define MENU_LEN         16
#define KEEP_BUF_SIZE    (BUFFER_SIZE * 2)
#define FRAME_MIN_LEN    18
#define DASHBUF_SIZE     4096

typedef struct { const char *code; const char *desc; } LookupEntry;
typedef struct { const char *key; const char *menu; } MenuEntry;

static const MenuEntry menu_map[] = {
    { "SYSTEM",
      "+---------------------+\n"
      "|   SYSTEM - MENU    |\n"
      "+---------------------+\n"
      "| Status: Press 1    |\n"
      "| Tools:  Press 2    |\n"
      "+---------------------+" },
    { "TCBC",
      "+---------------------+\n"
      "|   TCBC - MENU      |\n"
      "+---------------------+\n"
      "| System: Press 1    |\n"
      "| Tools:  Press 2    |\n"
      "+---------------------+" },
    { "STATUS",
      "+---------------------+\n"
      "|   STATUS - MENU    |\n"
      "+---------------------+\n"
      "| Calls:  Press 1    |\n"
      "| Input:  Press 2    |\n"
      "| Output: Press 3    |\n"
      "| Group:  Press 4    |\n"
      "+---------------------+" },
    { NULL, NULL }
};

static const char *principal_menu =
    "+---------------------+\n"
    "|   PRINCIPAL - MENU  |\n"
    "+---------------------+\n"
    "| TCBC:   Press 1     |\n"
    "| DRIVE:  Press 2     |\n"
    "| SPBC:   Press 3     |\n"
    "| RMH:    Press 4     |\n"
    "+---------------------+";

static const LookupEntry direction_map[] = {
    { "u", "Up" }, { "d", "Down" }, { "-", "Stopped" }, { "$", "$" }, { NULL, NULL }
};
static const LookupEntry ocss_map[] = {
    { "NOR", "Normal" }, { "PRK", "Parking" }, { "IDL", "Idle" }, { "$", "$" }, { NULL, NULL }
};
static const LookupEntry mcss_map[] = {
    { "CR", "Correction Run" }, { "EF", "Emergency Fast Run" }, { "ES", "Emergency Stop" },
      { "EW", "Emergency during Wait" }, { "FR", "Fast Run" }, { "ID", "Idle" }, { "IN", "Inspection Run" },
      { "NR", "Not Ready" }, { "RL", "Relevel" }, { "RS", "Rescue Run" }, { "SR", "Slow Run" }, { "ST", "Stop" },
      { "$", "$" }, { NULL, NULL }
};
static const LookupEntry door_map[] = {
    { "][", "Closed" }, { "[]", "Open" }, { "<>", "Opening" }, { "><", "Closing" }, { "**", "Undefined" }, { "$", "$" }, { NULL, NULL }
};
static const LookupEntry pos_map[] = {
    { "00", "Level 0" }, { "01", "Level 1" }, { "02", "Level 2" }, { "03", "Level 3" }, { "04", "Level 4" }, { "05", "Level 5" },
    { "**", "Level Unknown" }, { "$", "$" }, { NULL, NULL }
};

static size_t total_frames_parsed = 0;
static char last_dashboard[DASHBUF_SIZE] = "";

static const char* lookup(const LookupEntry *map, const char *code) {
    for (size_t i = 0; map[i].code != NULL; ++i)
        if (strcmp(map[i].code, code) == 0)
            return map[i].desc;
    return "$";
}

static void explain_doors(const char* doors, char* out, size_t outsize) {
    if (!doors || strlen(doors) < 4) {
        snprintf(out, outsize, "Front: $, Rear: $");
        return;
    }
    char fdoor[3] = { doors[0], doors[1], '\0' };
    char rdoor[3] = { doors[2], doors[3], '\0' };
    snprintf(out, outsize, "Front: %s, Rear: %s",
             lookup(door_map, fdoor), lookup(door_map, rdoor));
    out[outsize-1] = '\0';
}

static void handle_sigint(int sig) {
    (void)sig;
    printf("\n==============================\n");
    printf("Total frames parsed: %zu\n", total_frames_parsed);
    printf("==============================\n");
    exit(0);
}

// --- Dashboard Printer: prints only on change ---
static void print_if_new(const char *screen) {
    if (strcmp(last_dashboard, screen)) {
        printf("\033[2J\033[H"); // Clear screen
        puts(screen);
        strncpy(last_dashboard, screen, DASHBUF_SIZE);
        last_dashboard[DASHBUF_SIZE-1] = '\0';
    }
}

// --- Menu Frame Parser ---
static void parse_menu_frame(const char* frame) {
    const char *p = frame + 2; // Move past [H
    int ws_count = 0;
    while (*p && ws_count < 2 && isspace((unsigned char)*p)) { ++p; ++ws_count; }
    // Get next token as keyword
    char keyword[32] = {0};
    size_t k = 0;
    while (*p && !isspace((unsigned char)*p) && k < sizeof(keyword)-1) {
        keyword[k++] = *p++;
    }
    keyword[k] = '\0';
    int found = 0;
    for (int i = 0; menu_map[i].key != NULL; ++i) {
        if (strcmp(menu_map[i].key, keyword) == 0) {
            print_if_new(menu_map[i].menu);
            found = 1;
            break;
        }
    }
    if (!found) {
        char fallback[256];
        printf("\033[2J\033[H"); // Clear screen
        print_if_new(fallback);
    }
}

// --- Elevator Frame Parser ---
// Use POSIX pipe/dup to redirect printf temporarily to buffer
static void parse_elevator_frame(const char* buf, int frame_index, size_t offset) {
    char header[FRAME_HEADER_LEN+1];
    strncpy(header, buf+2, FRAME_HEADER_LEN);
    header[FRAME_HEADER_LEN] = '\0';
    const char *nl = strchr(buf+2+FRAME_HEADER_LEN, '\n');
    if (!nl) return;
    nl++;
    size_t menu_max = 0;
    const char* next_h = strstr(nl, "[H");
    menu_max = next_h ? (size_t)(next_h - nl) : strlen(nl);
    char menu[MENU_LEN+1] = {0};
    strncpy(menu, nl, (menu_max < MENU_LEN ? menu_max : MENU_LEN));
    menu[MENU_LEN] = '\0';
    char carid[2]   = { header[0], '\0' };
    char dir[2]     = { header[1], '\0' };
    char level[3]   = { header[2], header[3], '\0' };
    char ocss[4]    = { header[5], header[6], header[7], '\0' };
    char mcss[3]    = { header[9], header[10], '\0' };
    char doors[5]   = { header[12], header[13], header[14], header[15], '\0' };
    char pos_desc[32], ocss_desc[32], mcss_desc[32], doors_desc[64], dir_desc[32];
    strncpy(pos_desc, lookup(pos_map, level), sizeof(pos_desc));
    pos_desc[sizeof(pos_desc)-1] = '\0';
    strncpy(ocss_desc, lookup(ocss_map, ocss), sizeof(ocss_desc));
    ocss_desc[sizeof(ocss_desc)-1] = '\0';
    strncpy(mcss_desc, lookup(mcss_map, mcss), sizeof(mcss_desc));
    mcss_desc[sizeof(mcss_desc)-1] = '\0';
    strncpy(dir_desc, lookup(direction_map, dir), sizeof(dir_desc));
    dir_desc[sizeof(dir_desc)-1] = '\0';
    explain_doors(doors, doors_desc, sizeof(doors_desc));
    Elevator* e = Elevator_create();
    Elevator_setCarID(e, carid);
    Elevator_setLevel(e, pos_desc);
    Elevator_setDir(e, dir_desc);
    Elevator_setOCSS(e, ocss_desc);
    Elevator_setMCSS(e, mcss_desc);
    Elevator_setDoors(e, doors_desc);
    Elevator_setMenuScreen(e, menu);
    // --- Dashboard update logic ---
    // Temporarily redirect stdout to buffer and call Elevator_printScreen
    int pipefd[2];
    char buffer[DASHBUF_SIZE] = "";
    fflush(stdout);
    if (pipe(pipefd) == 0) {
        int old_stdout = dup(fileno(stdout));
        dup2(pipefd[1], fileno(stdout));
        Elevator_printScreen(e);
        fflush(stdout);
        close(pipefd[1]);
        ssize_t len = read(pipefd[0], buffer, DASHBUF_SIZE-1);
        if (len > 0) buffer[len] = '\0'; else buffer[0] = '\0';
        dup2(old_stdout, fileno(stdout));
        close(pipefd[0]);
        close(old_stdout);
        print_if_new(buffer);
    } else {
        // Fallback: just print
        print_if_new("Elevator status unavailable.");
    }
    Elevator_destroy(e);
}

// --- Main Frame Scanner ---
static size_t parse_and_consume_frames(char* buf, size_t buflen, int* frame_counter) {
    size_t i = 0, frame_end = 0;
    while (i < buflen) {
        const char* frame_start = strstr(buf + i, "[H");
        if (!frame_start) break;
        size_t start_idx = frame_start - buf;
        if (start_idx + 2 >= buflen) break;
        const char *p = buf + start_idx + 2;
        while (*p && isspace((unsigned char)*p)) ++p;
        if (*p == '1') {
            print_if_new(principal_menu);
        } else if (isspace((unsigned char)buf[start_idx+2])) {
            parse_menu_frame(buf + start_idx);
        } else if (buf[start_idx+2] == 'A' || buf[start_idx+2] == 'B' || buf[start_idx+2] == 'C') {
            parse_elevator_frame(buf + start_idx, (*frame_counter), start_idx);
        } else {
            char fallback[128];
            printf("\033[2J\033[H"); // Clear screen
            print_if_new(fallback);
        }
        const char* next = strstr(frame_start + 2, "[H");
        size_t end_idx = next ? (size_t)(next - buf) : buflen;
        size_t frame_len = end_idx - start_idx;
        if (frame_len < FRAME_MIN_LEN) { i = end_idx; continue; }
        (*frame_counter)++;
        total_frames_parsed++;
        frame_end = end_idx;
        i = end_idx;
    }
    return frame_end;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint);
    int fd = -1, use_stdin = 0;
    if (argc < 2) {
        printf("Usage: %s <short_serial_device>   (or use '-' for stdin test)\n", argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "-") == 0) {
        use_stdin = 1;
        printf("Reading frame data from STDIN for test/demo...\n");
    } else {
        char devpath[64];
        snprintf(devpath, sizeof(devpath), "/dev/%s", argv[1]);
        fd = open(devpath, O_RDWR | O_NOCTTY | O_SYNC);
        if (fd < 0) { printf("Error opening %s: %s\n", devpath, strerror(errno)); return 1; }
        struct termios tty;
        if (tcgetattr(fd, &tty) < 0) { perror("tcgetattr"); return 1; }
        cfsetospeed(&tty, (speed_t)B9600);
        cfsetispeed(&tty, (speed_t)B9600);
        tty.c_cflag |= (CLOCAL | CREAD);
        tty.c_cflag &= ~CSIZE; tty.c_cflag |= CS8;
        tty.c_cflag &= ~PARENB; tty.c_cflag &= ~CSTOPB;
        tty.c_iflag = 0; tty.c_oflag = 0; tty.c_lflag = 0;
        tty.c_cc[VMIN] = 1; tty.c_cc[VTIME] = 5;
        if (tcsetattr(fd, TCSANOW, &tty) != 0) { perror("tcsetattr"); return 1; }
        printf("Listening for data blocks on %s...\n", devpath);
    }
    char buf[BUFFER_SIZE];
    char keep[KEEP_BUF_SIZE];
    size_t keep_len = 0;
    ssize_t n = 0;
    int block_counter = 1;
    int frame_counter = 1;
    while (1) {
        size_t buflen = 0;
        if (use_stdin) {
            if (!fgets(buf, sizeof(buf), stdin)) break;
            buflen = strlen(buf);
        } else {
            n = read(fd, buf, sizeof(buf) - 1);
            if (n > 0) {
                buflen = n;
                buf[n] = '\0';
                block_counter++;
            } else if (n < 0) { printf("Read error: %s\n", strerror(errno)); break; }
            else if (n == 0) { break; }
        }
        if (keep_len + buflen > KEEP_BUF_SIZE) keep_len = 0;
        memmove(keep + keep_len, buf, buflen);
        keep_len += buflen;
        keep[keep_len] = '\0';
        size_t frame_end = parse_and_consume_frames(keep, keep_len, &frame_counter);
        size_t remain = keep_len - frame_end;
        if (remain > 0)
            memmove(keep, keep + frame_end, remain);
        keep_len = remain;
        usleep(350 * 1000); // tweak as needed, e.g. 350ms
    }
    if (keep_len > 0)
        parse_and_consume_frames(keep, keep_len, &frame_counter);
    if (!use_stdin && fd >= 0) close(fd);
    printf("\n==============================\n");
    printf("Total frames parsed: %zu\n", total_frames_parsed);
    printf("==============================\n");
    return 0;
}
