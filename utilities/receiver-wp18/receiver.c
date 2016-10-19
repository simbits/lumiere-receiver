#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <errno.h>

#include <bcm2835.h>

#define PLAY        'P'
#define DELAY       'D'

#define MAX_SEQ_LINES   1024

#define INPIN 30      /* for Projector */
#define OUTPIN 31
#define LED_PIN 19      /* LED pin */

#define READVAL()  bcm2835_gpio_lev( INPIN )

#include <stdio.h>

char *sequence_lines[MAX_SEQ_LINES];

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
    bmc2835_gpio_write(OUTPIN, HIGH);
    return bcm2835_st_read() - start;
}

static inline uint64_t wait_for_falling()
{
    uint64_t start = bcm2835_st_read();
    while ( READVAL() )
        bcm2835_delayMicroseconds(200);
    bmc2835_gpio_write(OUTPIN, LOW);
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

    if (n >= MAX_SEQ_LINES) {
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
}

int main(int argc, char **argv)
{
    char *path;
    unsigned int pulse_counter = 0;
    unsigned sequences = 0;


    if (!bcm2835_init())
        return 1;

    bcm2835_gpio_fsel(OUTPIN, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(INPIN, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_set_pud(INPIN, BCM2835_GPIO_PUD_OFF);
    bcm2825_gpio_write(OUTPIN, bcm2835_gpio_lev(INPIN));

    srand(time(NULL));

    if (argc < 2)
        path = "/opt/lumiere/bin/sequences/lumiere_default_sequence.txt";
    else {
        path = argv[1];
        if (argc >= 3) {
            int level = 0;

            if ( READVAL() ) level++;
            bcm2825_gpio_write(OUTPIN, bcm2835_gpio_lev(INPIN));
            bcm2835_delay(200);
            if ( READVAL() ) level++;
            bcm2825_gpio_write(OUTPIN, bcm2835_gpio_lev(INPIN));
            bcm2835_delay(200);
            if ( READVAL() ) level++;
            bcm2825_gpio_write(OUTPIN, bcm2835_gpio_lev(INPIN));

            if (level >= 2) {
                path = argv[2];
            } else {
                path = argv[1];
            }
        }
    }
    sequences = open_sequence_file(path);

    if (sequences <= 0) {
        printf("Warning no sequences read\n");
        exit(1);
    }

    while (1) {
        execute_line(pulse_counter);

        if (++pulse_counter >= sequences)
            pulse_counter = 0;

        bcm2835_delayMicroseconds(100);

        wait_for_high_pulse();
    }

    bcm2835_close();

    return 0;
}
