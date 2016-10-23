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

#include <bcm2835.h>
#include <lo/lo.h>

#define PLAY        'P'
#define DELAY       'D'

#define MAX_SEQ_LINES   1024

#define MASTER_DETECT_PIN   37      /* Master/Slave detect */
#define SIGNAL_IN_PIN       30      /* signal pulse input*/
#define SIGNAL_OUT_PIN      31      /* signal pulse output */
#define RS485_DE_PIN        39      /* RS485 Data Enable */

#define RS485_TTY           "/dev/ttyS0"
#define RS485_BAUDRATE      B9600

#define IS_UNKNOWN -1
#define IS_MASTER   0
#define IS_SLAVE    1

#define READVAL()  bcm2835_gpio_lev( SIGNAL_IN_PIN )

#define USE_MULTICAST   1

#include <stdio.h>

static const char *program = NULL;
static int done = 0;
static int master = IS_UNKNOWN;
static char *sequence_lines[MAX_SEQ_LINES];
static unsigned int sequences_loaded = 0;
static int current_line = -1;

static void execute_line(unsigned int n);
static void execute_next_line(void);
static void execute_prev_line(void);
static void do_reboot(void);

void usage(void)
{
    fprintf(stderr, "Usage: %s -p <OSC port> -a <ip address> -d <delay seconds) <sequence file>\n", program);
    fprintf(stderr, "    -p - set osc listen / send port\n");
    fprintf(stderr, "    -a - set send address\n");
    fprintf(stderr, "    -d - set delay in seconds between movies if master\n");

    exit(1);
}

void do_reboot(void)
{
    fprintf(stderr, "\n\nrebooting...\n");
    system("/sbin/reboot");
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

void read_tty(int fd)
{
    char buf[256];
    int len = read(fd, buf, 256);
    if (len > 0) {
        fprintf(stdout, "Recv RS485: ");
        fwrite(buf, len, 1, stdout);
        fprintf(stdout, "\n");
        fflush(stdout);
    }

    char c = buf[0];
    if (c == 'n') {
        execute_next_line();
    } else if (c == 'p') {
        execute_prev_line();
    } else if (c == 'r') {
        do_reboot();
    } else {
        errno = 0;
        long int n = strtol(buf, NULL, 10);
        if (errno == 0) {
            execute_line((int)n);
        }
    }
}


int open_sequence_file(const char *path)
{
    FILE* file = NULL;
    int i = 0;
    char line[512];

    if ((file = fopen(path, "r")) == NULL) {
        printf("Error reading sequence: %s\n", strerror(errno));
        return 0;
    }

    while (fgets(line, sizeof(line), file)) {
        printf("%d: %s", i+1, line);
        sequence_lines[i++] = strdup(line);
        if (i >= MAX_SEQ_LINES) {
            printf("Warning: too many sequence lines, truncating sequence\n");
            break;
        }
    }

    fclose(file);

    return i;
}

static inline uint64_t wait_for_rising()
{
    uint64_t start = bcm2835_st_read();
    while ( ! READVAL() )
        bcm2835_delayMicroseconds(200);
    bcm2835_gpio_write(SIGNAL_OUT_PIN, HIGH);
    return bcm2835_st_read() - start;
}

static inline uint64_t wait_for_falling()
{
    uint64_t start = bcm2835_st_read();
    while ( READVAL() )
        bcm2835_delayMicroseconds(200);
    bcm2835_gpio_write(SIGNAL_OUT_PIN, LOW);
    return bcm2835_st_read() - start;
}

static inline uint64_t wait_for_high_pulse()
{
    wait_for_rising();
    return wait_for_falling();
}

static inline uint64_t wait_for_low_pulse()
{
    wait_for_falling();
    return wait_for_rising();
}

void execute_line(unsigned int n)
{
    char cmd;
    char *line;

    if (n < 0 || n >= sequences_loaded || n >= MAX_SEQ_LINES) {
        printf("Error: sequence line %d out of bounds\n", n);
        return;
    }

    line = sequence_lines[n];
    cmd = *line++;
    line++;

    switch (cmd) {
        case PLAY:
            printf("playing line %d: %s\n", n, line);
            system(line);
            break;
        case DELAY:
            {
                unsigned int delay = (unsigned int)atoi(line);
                printf("delaying for %d us\n", delay);
                bcm2835_delay(delay);
                break;
            }
        default:
            break;
    }

    current_line = n;
}

void execute_next_line(void)
{
    int n = current_line + 1;
    if (n >= sequences_loaded)
        n = 0;
    execute_line(n);
}

void execute_prev_line(void)
{
    int n = current_line - 1;
    if (n < 0)
        n = sequences_loaded - 1;
    execute_line(n);
}

int is_master(void)
{
    if  (master == IS_UNKNOWN) {
        master = bcm2835_gpio_lev(MASTER_DETECT_PIN);
    }
    return (master == IS_MASTER) ? IS_MASTER : IS_SLAVE;
}

void initialize_gpio_pins(void)
{
    /* set pin directions */
    bcm2835_gpio_fsel(MASTER_DETECT_PIN, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(SIGNAL_OUT_PIN, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(SIGNAL_IN_PIN, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(RS485_DE_PIN, BCM2835_GPIO_FSEL_OUTP);

    /* SIGNAL_IN_PIN pull up enable. SIGNAL_OUT_PIN mimics in pin level */
    bcm2835_gpio_set_pud(SIGNAL_IN_PIN, BCM2835_GPIO_PUD_UP);
    bcm2835_gpio_write(SIGNAL_OUT_PIN, bcm2835_gpio_lev(SIGNAL_IN_PIN));

    /* MASTER_DETECT_PIN pull up enable */
    bcm2835_gpio_set_pud(MASTER_DETECT_PIN, BCM2835_GPIO_PUD_UP);

    /* RS485_DE_PIN low level */
    bcm2835_gpio_write(RS485_DE_PIN, LOW);
}

int initialize_RS485(const char *tty)
{
    struct termios termConfig, savedTermConfig;
    int    fd = -1;

    if ((fd = open(tty, O_RDWR | O_NOCTTY /*| O_NDELAY*/)) < 0) {
        fprintf(stderr, "Could not open tty '%s': %s", tty, strerror(errno));
        return -1;
    }

    if (!isatty(fd)) {
        fprintf(stderr, "%s is not atty\n", tty);
        return -1;
    }

    if (tcgetattr(fd, &termConfig) < 0) {
        fprintf(stderr, "%s tcgetattr failed\n", tty);
        return -1;
    }

    memcpy(&savedTermConfig, &termConfig, sizeof(struct termios));

    fcntl(fd, F_SETFL, 0);

    /* control options: 8n1 */
    termConfig.c_cflag |=  (CREAD | CLOCAL);
    termConfig.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
    termConfig.c_cflag |=  CS8;
#if defined(CNEW_RTSCTS)
    termConfig.c_cflag &= ~CNEW_RTSCTS;	/* disable hw flow control */
#endif

    /* line options: non-canonical, no echo, no signals */
    termConfig.c_lflag &= ~(ICANON  | ECHO   | ECHOE  | ECHOK | ECHONL |
                            ECHOCTL | ECHOKE | IEXTEN | ISIG  | IGNPAR);

    /* output options */
    termConfig.c_oflag &= ~(OCRNL | ONLCR | ONLRET |
                            ONOCR | OFILL | OLCUC  | OPOST);

/* input options */
    termConfig.c_iflag = ~(IGNBRK | BRKINT | ICRNL | IGNCR   |
                           INLCR  | PARMRK | INPCK | ISTRIP  | IXON |
                           IXOFF  | IUCLC  | IXANY | IMAXBEL | IUTF8);

    /* overall read timer (2 seconds) */
    termConfig.c_cc[VTIME] = 20;

    cfsetispeed(&termConfig, RS485_BAUDRATE);
    cfsetospeed(&termConfig, RS485_BAUDRATE);

    if (tcsetattr(fd, TCSAFLUSH, &termConfig) < 0) {
        fprintf(stderr, "%s tcsetattr failed\n", tty);
        tcsetattr(fd, TCSAFLUSH, &savedTermConfig);
        close(fd);
        return -1;
    }

    return fd;
}

int main(int argc, char **argv)
{
    unsigned int pulse_counter = 0;
    char osc_port[8];
    char ip[16] = "0.0.0.0";
    int opt = 0;
    lo_server s;
    int lo_fd = -1;
    int rs485_fd = -1;
    fd_set rfds;
    struct timeval tv, *tvp;
    int retval;
    float sequence_delay = 0.0;
    int use_osc = 0;

    program = basename(argv[0]);
    srand(time(NULL));

    while ((opt = getopt(argc, argv, "p:a:d:")) != -1)
    {
        switch (opt)
        {
            case 'p':
                strncpy(osc_port, optarg, 8);
                break;
            case 'a':
                strncpy(ip, optarg, 16);
                use_osc = 1;
                break;
            case 'd':
                sequence_delay = atof(optarg);
                break;
            default:
                usage();
                break;
        }
    }

    if (optind >= argc) {
        usage();
    }

    if ((sequences_loaded = open_sequence_file(argv[optind])) <= 0) {
        fprintf(stderr, "unable to load sequence file '%s'\n", argv[optind]);
        exit(2);
    }

    fprintf(stdout, "loaded %d sequence lines\n", sequences_loaded);

    if (!bcm2835_init())
        exit(3);
    initialize_gpio_pins();

    rs485_fd = initialize_RS485(RS485_TTY);

    fprintf(stdout, "We are %s\n", (is_master() == IS_MASTER) ? "MASTER" : "SLAVE");
    if (is_master() == IS_MASTER) {
        fprintf(stdout, "  seqeuence line delay is %f seconds\n", sequence_delay);
    }

    if (use_osc) {
#ifdef USE_MULTICAST
        fprintf(stdout, "starting multicast OSC\n");
        fprintf(stdout, "  port: %s\n", osc_port);
        fprintf(stdout, "  multicast group: %s\n", ip);
        s = lo_server_new_multicast(ip, osc_port, osc_error);
#else
        fprintf(stdout, "starting unicast OSC\n");
        fprintf(stdout, "  port: %s\n", osc_port);
        fprintf(stdout, "  ip: %s\n", ip);
        s = lo_server_new(osc_port, osc_error);
#endif

        lo_server_add_method(s, NULL, NULL, generic_handler, NULL);
        lo_server_add_method(s, "/play", "i", play_handler, NULL);
        lo_server_add_method(s, "/next", "", next_handler, NULL);
        lo_server_add_method(s, "/prev", "", prev_handler, NULL);
        lo_server_add_method(s, "/quit", "", quit_handler, NULL);
        lo_server_add_method(s, "/reboot", "", reboot_handler, NULL);
        lo_fd = lo_server_get_socket_fd(s);

        if (lo_fd <= 0) {
            fprintf(stderr, "Could not get osc server file descriptor\n");
            fprintf(stderr, "  falling back on RS485\n");
            use_osc = 0;
            lo_fd = -1;;
        }
    } else {
        fprintf(stdout, "Using RS485 as command interface\n");
        lo_fd = -1;
    }

    execute_line(0);

    do {

        FD_ZERO(&rfds);
        if (use_osc) FD_SET(lo_fd, &rfds);
        FD_SET(rs485_fd, &rfds);

        if (is_master() == IS_MASTER && sequence_delay > 0.0) {
            tv.tv_sec = floorf(sequence_delay);
            tv.tv_usec = (sequence_delay - tv.tv_sec) * 1000000;
            tvp = &tv;
        } else {
            tvp = NULL;
        }

        retval = select(((lo_fd > rs485_fd) ? lo_fd : rs485_fd) + 1, &rfds,
                    NULL, NULL, tvp);

        if (retval == -1) {
            printf("select() error\n");
            exit(1);
        } else if (retval > 0) {
            if (FD_ISSET(rs485_fd, &rfds)) {
                read_tty(rs485_fd);
            }
            if (use_osc && FD_ISSET(lo_fd, &rfds)) {
                lo_server_recv_noblock(s, 0);
            }
        } else {
            int line = current_line + 1;
            if (line >= sequences_loaded) {
                line = 0;
            }

            if (use_osc) {
                lo_address t = lo_address_new(ip, osc_port);
#if USE_MULTICAST
                lo_address_set_ttl(t, 1);
#endif

                if (lo_send(t, "/play", "i", line) == -1) {
                    printf("OSC error %d: %s\n", lo_address_errno(t),
                                                 lo_address_errstr(t));
                }
            } else {
                fprintf(stdout, "Sending via RS485: %d\n", line);
                if (bcm2835_gpio_lev(RS485_DE_PIN)) {
                    fprintf(stderr, "Send RS485: line busy, waiting for next slot\n");
                } else {
                    char bfr[8];
                    int n = 0;
                    bcm2835_gpio_write(RS485_DE_PIN, HIGH);
                    bcm2835_delay(1);
                    n = snprintf(bfr, 8, "%d", line);
                    printf(stdout, "Send RS485: %s\n", n, bfr);
                    write(rs485_fd, bfr, n);
                    bcm2835_delay(1);
                    bcm2835_gpio_write(RS485_DE_PIN, LOW);
                }
            }
        }
    } while (!done);

    close(rs485_fd);
    bcm2835_close();

    return 0;
}

/* vi:set ts=8 sts=4 sw=4: */
