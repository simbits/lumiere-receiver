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

#include <bcm2835.h>

//#define PIN 21    /* for B+ PI */
#define PIN 30      /* for Projector */
#define LED_PIN 19      /* LED pin */

#define READVAL()  bcm2835_gpio_lev( PIN )

#define STATE_UNKNOWN           0
#define STATE_WAIT_FOR_START    1
#define STATE_SEEN_START        2
#define STATE_WAIT_FOR_STOP     3
#define STATE_WAIT_FOR_BANG     4
#define STATE_MESSAGE_VALID     5

#define SEQ_LED_ON              0
#define SEQ_LED_OFF             1
#define SEQ_FADE_ATOB           2
#define SEQ_FADE_BTOA           3
#define SEQ_FADE_ATOW           4
#define SEQ_FADE_WTOA           5
#define SEQ_START_STORM         6
#define SEQ_LIGHTNING_BOLT      7
#define SEQ_CLEAR_SKY           8
#define SEQ_CLOUD_1             9
#define SEQ_CLOUD_2             10
#define SEQ_CLOUD_1_NORAND      11
#define SEQ_CLOUD_2_NORAND      12
#define SEQ_CLEAR_SKY_FTOB      13
#define SEQ_STILLS              14

#define FADE_FIFO_IN            "/tmp/fade_in.fifo"
#define FADE_FIFO_OUT           "/tmp/fade_out.fifo"

static inline uint64_t wait_for_rising()
{
    uint64_t start = bcm2835_st_read();
    while ( ! READVAL() )
        bcm2835_delayMicroseconds(1000);
    return bcm2835_st_read() - start;
}

static inline uint64_t wait_for_falling()
{
    uint64_t start = bcm2835_st_read();
    while ( READVAL() )
        bcm2835_delayMicroseconds(1000);
    return bcm2835_st_read() - start;
}

static inline uint64_t wait_for_pulse()
{
    wait_for_falling();
    return wait_for_rising();
}

void do_sequence(int n, int port)
{

    printf("starting sequence: %d\n", n);

    switch (n) {
        case SEQ_LED_ON:
            printf("turning on projector\n");
            bcm2835_gpio_write(LED_PIN, HIGH);
            break;

        case SEQ_LED_OFF:
            printf("turning off projector\n");
            bcm2835_gpio_write(LED_PIN, LOW);
            break;

        case SEQ_FADE_ATOB:
        case SEQ_FADE_BTOA:
        case SEQ_FADE_ATOW:
        case SEQ_FADE_WTOA:
            {
                char bfr[128];
                sprintf(bfr, "echo %d | netcat localhost %d", n-1, port);
                printf("executing: %s\n", bfr);
                system(bfr);
                bzero(bfr, 128);
                printf("done\n");
            }
            break;

        case SEQ_START_STORM:
            printf("starting seq start_storm\n");
            system("/opt/lumiere/bin/sequences/start_storm.sh");
            break;

        case SEQ_LIGHTNING_BOLT:
            printf("starting seq random_lightning\n");
            bcm2835_delay(rand() % 2000);
            system("/opt/lumiere/bin/sequences/random_lightning.sh");
            break;

        case SEQ_CLEAR_SKY:
            printf("starting seq clear_sky\n");
            system("/opt/lumiere/bin/sequences/clear_sky.sh");
            break;

        case SEQ_CLOUD_1:
            printf("starting seq clouds_1\n");
            system("/opt/lumiere/bin/sequences/clouds_1.sh s");
            break;

        case SEQ_CLOUD_2:
            printf("starting seq clouds_2\n");
            system("/opt/lumiere/bin/sequences/clouds_2.sh s");
            break;

        case SEQ_CLOUD_1_NORAND:
            printf("starting seq clouds_1\n");
            system("/opt/lumiere/bin/sequences/clouds_1.sh");
            break;

        case SEQ_CLOUD_2_NORAND:
            printf("starting seq clouds_2\n");
            system("/opt/lumiere/bin/sequences/clouds_2.sh");
            break;

        case SEQ_CLEAR_SKY_FTOB:
            printf("starting seq clear_sky\n");
            system("/opt/lumiere/bin/sequences/clear_sky.sh b");
            break;

        case SEQ_STILLS:
            printf("starting seq stills\n");
            system("/opt/lumiere/bin/sequences/stills.sh");
            break;

        default:
            printf("0 nothing read\n");
    }


}

int main(int argc, char **argv)
{
    uint8_t prev_value = 0;
    int pulse_cntr = 0;
    int state = STATE_WAIT_FOR_START;
    uint64_t timediff = 0;
    uint64_t timer = 0;
    int fifo_in;
    int fader_port = 9999;

    if (argc == 2) {
       fader_port = atoi(argv[1]);
    }

    printf("%s using fader port %d\n", argv[0], fader_port);

    if (!bcm2835_init())
        return 1;

    bcm2835_gpio_fsel(PIN, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_set_pud(PIN, BCM2835_GPIO_PUD_UP);

    srand(time(NULL));

    while (1) {
        uint8_t value;
        value = bcm2835_gpio_lev(PIN);

        switch (state) {
            case STATE_WAIT_FOR_START:
                {
                    uint64_t duration = 0;
                    printf("waiting for start condition\n");
                    duration = wait_for_pulse();

                    if (duration >= 500000) {
                        state = STATE_SEEN_START;
                        printf("start seen\n");
                    } else {
                        break;
                    }
                }
            case STATE_SEEN_START:
                {
                    pulse_cntr = 0;
                    state = STATE_WAIT_FOR_STOP;
                }
            case STATE_WAIT_FOR_STOP:
                {
                    uint64_t duration = 0;

                    duration = wait_for_pulse();

                    if (duration >= 25000 && duration <= 100000) {
                        pulse_cntr++;
                        break;
                    } else if (duration >= 100000 && duration < 500000) {
                        state = STATE_WAIT_FOR_BANG;
                    }
                }
            case STATE_WAIT_FOR_BANG:
                {
                    uint64_t duration = 0;
                    duration = wait_for_pulse();
                    if (duration >= 1000 && duration <= 100000) {
                        printf("BANG\n");
                        state = STATE_MESSAGE_VALID;
                    } else if (duration >= 500000) {
                        printf("Missing BANG, got START\n");
                        state = STATE_SEEN_START;
                        break;
                    } else {
                        printf("Missing BANG, restart\n");
                        state = STATE_WAIT_FOR_START;
                        break;
                    }
                }
            case STATE_MESSAGE_VALID:
                {
                    do_sequence(pulse_cntr, fader_port);
                    state = STATE_WAIT_FOR_START;
                }
                break;
            default:
                break;

        }
        bcm2835_delayMicroseconds(100);
    }

    bcm2835_close();
    return 0;
}
