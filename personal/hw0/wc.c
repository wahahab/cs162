#include <stdio.h>


void wordcount(FILE *file, char *filename);

int main(int argc, char *argv[]) {
   
    char *filename = NULL;
    FILE *file;

    filename = argv[1];
    if (argc == 1) {
        file = stdin;
    }
    else 
        file = fopen(filename, "r");
    // Read file content
    if (file) {
        wordcount(file, filename);
        fclose(file);
    }
    else {
        fprintf(stderr, "Fail to open file: %s.\n", filename);
        return 1;
    }
    return 0;
}

void wordcount(FILE *file, char* filename) {

    char c;
    int charcount = 0;
    int wcount = 0;
    int linecount = 0;
    int inword = 0;

    while ((c = fgetc(file)) != EOF) {
        charcount++;
        switch(c) {
            case ' ':
                if (inword) {
                    wcount++;
                    inword = 0;
                }
                break;
            case '\n':
                if (inword) {
                    wcount++;
                    inword = 0;
                }
                linecount++;
                break;
            default:
                inword = 1;
                break;
        }
    }
    printf(" %d %d %d %s\n", linecount, wcount, charcount, (filename == NULL ? "": filename));
}
