#include <bcm2835.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MODE_READ 	0
#define MODE_WRITE 	1
#define CLKDIV 		2500	// for 100KHz

#define PCA1	0x20
#define PCA2	0x21

#define PW_DELAY	 50
#define PW_START	550
#define PW_STOP		250
#define PW_BIT		 50
#define PW_BANG		 50

#define NUM_BULBS	28
#define NUM_BULBS_PCA1	16
#define NUM_BULBS_PCA2  12

typedef struct bulb_gpio {
	uint8_t bulb;
	uint8_t addr;
	uint8_t bank;
	uint8_t pin;
} bulb_gpio_t;

uint8_t write_bulb[32];

bulb_gpio_t bulb[32] = {
	/* bulb, address, bank, gpio */
	{  1,    0x20,    0,    0},
	{  2,    0x20,    0,    1},
	{  3,    0x20,    0,    2},
	{  4,    0x20,    0,    3},
	{  5,    0x20,    0,    4},
	{  6,    0x20,    0,    5},
	{  7,    0x20,    0,    6},
	{  8,    0x20,    0,    7},

	{  9,    0x20,    1,    7},
	{ 10,    0x20,    1,    6},
	{ 11,    0x20,    1,    5},
	{ 12,    0x20,    1,    4},
	{ 13,    0x20,    1,    3},
	{ 14,    0x20,    1,    2},
	{ 15,    0x20,    1,    1},
	{ 16,    0x20,    1,    0},

	{ 17,    0x21,    0,    1},
	{ 18,    0x21,    0,    0},
	{ 19,    0x21,    0,    3},
	{ 20,    0x21,    0,    2},
	{ 21,    0x21,    0,    5},
	{ 22,    0x21,    0,    4},
	{ 23,    0x21,    0,    7},
	{ 24,    0x21,    0,    6},

	{ 25,    0x21,    1,    0},
	{ 26,    0x21,    1,    1},
	{ 27,    0x21,    1,    2},
	{ 28,    0x21,    1,    3},
	{  0,    0x21,    1,    4},
	{  0,    0x21,    1,    5},
	{  0,    0x21,    1,    6},
	{  0,    0x21,    1,    7}};

static inline void write_bulbs(uint8_t bulbs[], uint8_t value)
{
	char bfr[2] = {0x02, 0x00};
	int i;

	bcm2835_i2c_setSlaveAddress(PCA1);
	bfr[0] = 0x02;
	bfr[1] = 0xff;
	for (i=0;i<8;i++) {
		if (bulbs[i]) {
			if (value) {
				bfr[1] |= (bulbs[i]) ? (1 << bulb[i].pin) : 0;
			} else {
				bfr[1] &= ~((bulbs[i]) ? (1 << bulb[i].pin) : 0);
			}
		}
	}
	//printf("PCA1 b1: 0x%.2x\n", bfr[1]);
	bcm2835_i2c_write(bfr, 2);

	bfr[0] = 0x03;
	bfr[1] = 0xff;
	for (i=8;i<16;i++) {
		if (bulbs[i]) {
			if (value) {
				bfr[1] |= (bulbs[i]) ? (1 << bulb[i].pin) : 0;
			} else {
				bfr[1] &= ~((bulbs[i]) ? (1 << bulb[i].pin) : 0);
			}
		}
	}
	//printf("PCA1 b2: 0x%.2x\n", bfr[1]);
	bcm2835_i2c_write(bfr, 2);

	bcm2835_i2c_setSlaveAddress(PCA2);
	bfr[0] = 0x02;
	bfr[1] = 0xff;
	for (i=16;i<24;i++) {
		if (bulbs[i]) {
			if (value) {
				bfr[1] |= (bulbs[i]) ? (1 << bulb[i].pin) : 0;
			} else {
				bfr[1] &= ~((bulbs[i]) ? (1 << bulb[i].pin) : 0);
			}
		}
	}
	//printf("PCA2 b1: 0x%.2x\n", bfr[1]);
	bcm2835_i2c_write(bfr, 2);

	bfr[0] = 0x03;
	bfr[1] = 0xff;
	for (i=24;i<32;i++) {
		if (bulbs[i]) {
			if (value) {
				bfr[1] |= (bulbs[i]) ? (1 << bulb[i].pin) : 0;
			} else {
				bfr[1] &= ~((bulbs[i]) ? (1 << bulb[i].pin) : 0);
			}
		}
	}
	//printf("PCA2 b2: 0x%.2x\n", bfr[1]);
	bcm2835_i2c_write(bfr, 2);
}

static inline void write_cmd(uint8_t bulbs[], int cmd)
{
	write_bulbs(bulbs, 0);
	bcm2835_delay(cmd);
	write_bulbs(bulbs, 1);
	bcm2835_delay(PW_DELAY);
}

static inline void write_stop(uint8_t bulbs[])
{
	write_bulbs(bulbs, 0);
	bcm2835_delay(PW_STOP);
	write_bulbs(bulbs, 1);
	bcm2835_delay(PW_DELAY);
}

static inline void write_bit(uint8_t bulbs[])
{
	write_bulbs(bulbs, 0);
	bcm2835_delay(PW_BIT);
	write_bulbs(bulbs, 1);
	bcm2835_delay(PW_DELAY);
}

static inline void write_bang(uint8_t addr)
{
	char bfr[2] = {0x02, 0x00};
	bcm2835_i2c_setSlaveAddress(addr);
	bcm2835_i2c_write(bfr, 2);
	bcm2835_delay(PW_BANG);
	bfr[1] = 0xff;
	bcm2835_i2c_write(bfr, 2);
	bcm2835_delay(PW_DELAY);
}

void init_pca9555(uint8_t addr) {
	char bfr[2];
	bcm2835_i2c_setSlaveAddress(addr);

	bfr[0] = 0x06;
	bfr[1] = 0x00;
	bcm2835_i2c_write(bfr, 2);
	bfr[0] = 0x07;
	bcm2835_i2c_write(bfr, 2);
	bfr[0] = 0x02;
	bfr[1] = 0xff;
	bcm2835_i2c_write(bfr, 2);
	bfr[0] = 0x03;
	bcm2835_i2c_write(bfr, 2);
}

int main(int argc, char *argv[]) {
	int i;
	uint8_t bfr[2];
	uint8_t count = 0;

	if ((argc < 2) || (argc > 30)) {
		printf("Usage: %s <sequence> [num] [num] ...\n", argv[0]);
		exit(1);
	}

	printf("argc %d\n", argc);
	count = atoi(argv[1]);
	if (argc == 2) {
		for (i=0; i<28; i++) {
			write_bulb[i] = 1;
		}
	} else {
		for (i=2; i<argc; i++) {
			int b = atoi(argv[i]);
			if (b > 0 && b <= 28) {
				printf("%d ", b);
				write_bulb[b-1] = 1;
			}
		}
	}
	printf("\n");

	if (!bcm2835_init()) return 1;

	bcm2835_i2c_begin();
	bcm2835_i2c_setClockDivider(CLKDIV);

	init_pca9555(PCA1);
	init_pca9555(PCA2);

	printf("seq: %d\n", count);
	for (i=0;i<28;i++) {
		printf("%d ", write_bulb[i]);
	}
	printf("\n");

	write_cmd(write_bulb, PW_START);
	for (i=0; i<count; i++) {
		write_cmd(write_bulb, PW_BIT);
	}
	write_cmd(write_bulb, PW_STOP);
	//bcm2835_delay(100);
	write_cmd(write_bulb, PW_BANG);

	bcm2835_close();
}

