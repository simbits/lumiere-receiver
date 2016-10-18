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

//#define PIN 21    /* for B+ PI */
#define PIN 30      /* for Projector */
#define PINOUT 31
#define LED_PIN 19      /* LED pin */

#define READVAL()  bcm2835_gpio_lev( PIN )

#include <stdio.h>

int main(int argc, char **argv)
{
    char *path;
    unsigned int pulse_counter = 0;
    unsigned sequences = 0;


    if (!bcm2835_init())
        return 1;

    bcm2835_gpio_fsel(PINOUT, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_write(PINOUT, HIGH);
    bcm2835_gpio_fsel(PIN, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_set_pud(PIN, BCM2835_GPIO_PUD_UP);
    bcm2835_gpio_fen(PIN);

    srand(time(NULL));

    while (1) {
        if (bcm2835_gpio_eds(PIN)) {
            bcm2835_gpio_write(PINOUT, LOW);
            bcm2835_delayMicroseconds(2000);
            bcm2835_gpio_write(PINOUT, HIGH);
            bcm2835_gpio_set_eds(PIN);
        } else {
            bcm2835_delayMicroseconds(100);
        }
    }

    bcm2835_close();

    return 0;
}
