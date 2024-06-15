#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>

#define MAX_INPUT 512
#define MAX_TOKENS 64
// ta tabela ima vhodno vrstico
char line[MAX_INPUT];
// v to tabelo si hranim samo indekse, kjer se zacnejo besede
int tokens[MAX_TOKENS];
// si belezim koliko tokenov sem nasel
int tokenCount = 0;
char shellName[9];
int native = 0;

// funkcija pripravi vrstico za izhodno obliko
void printInputLine() {
    int r, n = 0;
    for (int i = 0; i < MAX_INPUT; i++) {
        // carriage return smo ze popravili
        if (r && n) break;
        // popravi po potrebi
        if (line[i] == '\n') {
            line[i] = '\0';
            n = 1;
        }
        if (line[i] == '\r') {
            line[i] = '\0';
            r = 1;
        }
    }
    // izpisi input line
    printf("Input line: '%s'\n", line);
}

// najde indekse tokenov v vhodu
void tokenize() {
    // najdi vse tokene v input line
    int isciPrvega = 1;
    int jeVNarekovajih = 0;

    for (int i = 0; i < MAX_INPUT; i++) {
        // naleteli smo na komentar, lahko zakljucimo s to vrstico
        if (line[i] == '#' && jeVNarekovajih == 0 && isciPrvega == 1) {
            for (int j = i; j < MAX_INPUT; j++) {
                line[j] = '\0';
            }
            break;
        }

        // ce je konec vrstice, prekini
        if (line[i] == '\0') break;
        // ce je presledek, ga zamenjaj z '\0' in povecaj stevilo tokenov
        if (line[i] == ' ' && jeVNarekovajih == 0) {
            line[i] = '\0';
            isciPrvega = 1;
            continue;
        }
        else if (isspace(line[i]) && jeVNarekovajih == 0) {
            line[i] = '\0';
            isciPrvega = 1;
            continue;
        }

        // to pomeni da smo drugic srecali narekovaje
        if (line[i] == '"' && jeVNarekovajih == 1) {
            jeVNarekovajih = 0;
            line[i] = '\0';
            continue;
        }

        // prvi preverja presledke
        if (isciPrvega && !isspace(line[i])){
            tokens[tokenCount] = i;
            isciPrvega = 0;
            // prvic smo se srecali z narekovaji
            if (line[i] == '"' && jeVNarekovajih == 0) {
                jeVNarekovajih = 1;
                tokens[tokenCount]++;
            } else jeVNarekovajih = 0;
            tokenCount++;
        }
    }

    // izpisi tokene
    for (int i = 0; i < tokenCount; i++) {
        printf("Token %d: '%s'\n", i, line + tokens[i]);
    }
}

int main(int argc, char *argv[]) {
    native = isatty(STDIN_FILENO);
    strcpy(shellName, "mysh");
    if (native) printf("%s> ", shellName);

    while (fgets(line, sizeof(line), stdin) != NULL) {
        printInputLine();
        tokenize();
        // sprazni input line, da ni problemov pri naslednjem branju
        for (int i = 0; i < MAX_INPUT; i++) {
            line[i] = '\0';
        }
        // sprazni se tokens tabelo
        for (int i = 0; i < MAX_TOKENS; i++) {
            tokens[i] = 0;
        }
        tokenCount = 0;
        if (native) {
            printf("%s> ", shellName);
        }
    }
    return 0;
}