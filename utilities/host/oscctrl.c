#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <lo/lo.h>

#define USE_MULTICAST   1

#include <stdio.h>

static const char *program = NULL;

void usage(void)
{
    fprintf(stderr, "Usage: %s -p <OSC port> -a <ip address> -d <delay seconds) <sequence file> -c <osc path> -r <osc argument>\n", program);
    fprintf(stderr, "    -p - set osc listen / send port\n");
    fprintf(stderr, "    -a - set send address\n");
    fprintf(stderr, "    -c - osc path\n");
    fprintf(stderr, "    -r - osc arguments\n");
    exit(1);
}

void osc_error(int num, const char *msg, const char *path)
{
        fprintf(stderr, "liblo server error %d in path %s: %s\n", num, path, msg);
}

/* catch any incoming messages and display them. returning 1 means that the
 *  * message has not been fully handled and the server should try other methods */
int generic_handler(const char *path, const char *types, lo_arg ** argv,
                            int argc, void *data, void *user_data)
{
        int i;

    printf("path: <%s>\n", path);
    for (i = 0; i < argc; i++) {
        printf("arg %d '%c' ", i, types[i]);
        lo_arg_pp((lo_type)types[i], argv[i]);
        printf("\n");
    }
    printf("\n");
    fflush(stdout);

    return 1;
}

#if 0
int led_high_handler(const char *path, const char *types, lo_arg ** argv,
                        int argc, void *data, void *user_data)
{
    set_high_power();
    return 0;
}

int led_low_handler(const char *path, const char *types, lo_arg ** argv,
                        int argc, void *data, void *user_data)
{
    set_low_power();
    return 0;
}

int led_off_handler(const char *path, const char *types, lo_arg ** argv,
                        int argc, void *data, void *user_data)
{
    led_off();
    return 0;
}

int led_on_handler(const char *path, const char *types, lo_arg ** argv,
                        int argc, void *data, void *user_data)
{
    led_on();
    return 0;
}

int projector_test_handler(const char *path, const char *types, lo_arg ** argv,
                        int argc, void *data, void *user_data)
{
    projector_test_cycle();
    return 0;
}

int play_handler(const char *path, const char *types, lo_arg ** argv,
                        int argc, void *data, void *user_data)
{
        /* example showing pulling the argument values out of the argv array */
        printf("%s <- i:%d\n\n", path, argv[0]->i);
        fflush(stdout);

        execute_line(argv[0]->i);

        return 0;
}

int next_handler(const char *path, const char *types, lo_arg ** argv,
                         int argc, void *data, void *user_data)
{
        execute_next_line();
        return 0;
}

int prev_handler(const char *path, const char *types, lo_arg ** argv,
                         int argc, void *data, void *user_data)
{
        execute_prev_line();
        return 0;
}

int reboot_handler(const char *path, const char *types, lo_arg ** argv,
                         int argc, void *data, void *user_data)
{
        do_reboot();
        return 0;
}

int quit_handler(const char *path, const char *types, lo_arg ** argv,
                         int argc, void *data, void *user_data)
{
        done = 1;
        printf("quiting\n\n");

        return 0;
}

#endif

int main(int argc, char **argv)
{
    char osc_port[8];
    char ip[16] = "0.0.0.0";
    char cmd[32];
    char arg[16];
    int opt = 0;
    lo_server s;
    int lo_fd = -1;
    fd_set rfds;
    struct timeval tv, *tvp;
    int retval;
    int have_cmd = 0;
    int have_arg = 0;

    program = basename(argv[0]);
    srand(time(NULL));

    while ((opt = getopt(argc, argv, "p:a:c:r:")) != -1)
    {
        switch (opt)
        {
            case 'p':
                strncpy(osc_port, optarg, 8);
                break;
            case 'a':
                strncpy(ip, optarg, 16);
                break;
            case 'c':
                strncpy(cmd, optarg, 32);
                have_cmd = 1;
                break;
            case 'r':
                strncpy(arg, optarg, 16);
                have_arg = 1;
                break;
            default:
                usage();
                break;
        }
    }
/*
    if (optind >= argc) {
        usage();
    }
*/

#ifdef USE_MULTICAST
    fprintf(stdout, "starting multicast OSC\n");
    fprintf(stdout, "  port: %s\n", osc_port);
    fprintf(stdout, "  multicast group: %s\n", ip);
    s = lo_server_new_multicast_iface(ip, osc_port, "eth0", NULL, osc_error);
#else
    fprintf(stdout, "starting unicast OSC\n");
    fprintf(stdout, "  port: %s\n", osc_port);
    fprintf(stdout, "  ip: %s\n", ip);
    s = lo_server_new(osc_port, osc_error);
#endif

    lo_server_add_method(s, NULL, NULL, generic_handler, NULL);
    lo_fd = lo_server_get_socket_fd(s);

    if (lo_fd <= 0) {
        fprintf(stderr, "Could not get osc server file descriptor\n");
        lo_fd = -1;;
        exit(1);
    }

    if (have_cmd) {
        lo_address t = lo_address_new(ip, osc_port);
#if USE_MULTICAST
        lo_address_set_ttl(t, 1);
#endif
        if (!strcmp(cmd, "/play")) {
            if (have_arg) {
                if (lo_send(t, cmd, "i", atoi(arg)) == -1) {
                    fprintf(stderr, "OSC error %d: %s\n", lo_address_errno(t), lo_address_errstr(t));
                }
            } else {
                fprintf(stderr, "cmd: '%s' needs an argument\n", cmd);
            }
        } else {
            if (lo_send(t, cmd, "") == -1) {
                fprintf(stderr, "OSC error %d: %s\n", lo_address_errno(t), lo_address_errstr(t));
            }
        }
    }

    do {
        FD_ZERO(&rfds);
        FD_SET(lo_fd, &rfds);

        tvp = NULL;

        retval = select(lo_fd + 1, &rfds, NULL, NULL, tvp);

        if (retval == -1) {
            printf("select() error\n");
            exit(1);
        } else if (retval > 0) {
            if (FD_ISSET(lo_fd, &rfds)) {
                lo_server_recv_noblock(s, 0);
            }
        }
    } while (1);

    return 0;
}

/* vi:set ts=8 sts=4 sw=4: */
