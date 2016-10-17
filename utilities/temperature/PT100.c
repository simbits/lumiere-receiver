#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define R1  1210
#define C1  -244.83
#define C2  2.3419
#define C3  0.0010664

int main(int argc, char *argv[])
{
    float Vd = atof(argv[1]); /* PT100 ad in */
    float Vr = atof(argv[2]); /* referencde voltage */
    float Vs = atof(argv[3]); /* supply voltage */

    float Rt = (R1 / (1.0f - ((Vr + Vd) / Vs))) - R1;
    float T = C1 + (Rt * (C2 + (C3 * Rt)));

    printf("%.2f %.2f\n", Rt, T);
}
