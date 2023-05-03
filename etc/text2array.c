#include <stdio.h>
#include <stdbool.h>

int main(int argc, char** argv) {
    char* array_name = "array";
    if (argc > 1) {
        array_name = argv[1];
    }

    int count = 0;
    printf("const unsigned char %s[] = {", array_name);

    while (true) {
        int c = getchar();
        if (c == EOF) {
            break;
        }

        if (count % 12 == 0) {
            printf("\n    ");
        }

        printf("0x%02X, ", c);
        count++;
    }

    printf("\n};\n\n");
    printf("const int %s_count = %d;\n", array_name, count);
    return 0;
}
