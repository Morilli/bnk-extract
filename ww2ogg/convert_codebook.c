#include <stdio.h>
#include <assert.h>
#include <stdint.h>

int main(int argc, char* argv[])
{
    if (argc != 3) {
        fprintf(stderr, "%s input codebook output codebook\n", argv[0]);
        exit(1);
    }

    FILE* input = fopen(argv[1], "rb");
    assert(input);
    FILE* output = fopen(argv[2], "wb");
    assert(output);

    int c;

    int i;
    while ( (c = fgetc(input)) != EOF) {
        char hex[4];
        snprintf(hex, 4, "\\x%02X", c);
        fwrite(hex, 4, 1, output);
        i++;
    }

    printf("waht is i: %d\n", i);
}
