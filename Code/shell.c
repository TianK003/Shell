#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

#define MAX_INPUT 512
#define MAX_TOKENS 64
#define NO_INTERNAL_COMMANDS 12
// ta tabela ima vhodno vrstico
char line[MAX_INPUT];
char zaPrint[MAX_INPUT];
// v to tabelo si hranim samo indekse, kjer se zacnejo besede
int tokens[MAX_TOKENS];
// si belezim koliko tokenov sem nasel
int tokenCount = 0;
// spremlja debug nivo
int debugLevel = 0;
int izpisiDebugLevel = 0;

// ce se izvaja v ospredju je 0, ce v ozadju je 1
int background = 0;
char shellName[9];
int zadnjiStatus = 0;
// da ves ce je ukaz zunanji
int isExternal = 0;
// da ves kako printat
int jePrimerenDebug = 1;
char *interniUkazi[] = {"debug", "exit", "help", "prompt", "status", "print", "echo", "len", "sum", "calc", "basename", "dirname"};
int previousDebugLevel = 0;
// da ves ce interaktivno izvajas
int native = 0;
// posebej za debuganje, da se izpise vse kot mora
int spremeniNazajNaNic = 0;

// izpise vhod, tokene in redirecte
void printToken() {
    int j = 0;
    for (int i = 0; i < tokenCount; i++) {
            printf("Token %d: '%s'\n", j, line + tokens[i]);
            j++;
            if (i == 0 && debugLevel > 0 && strcmp(line + tokens[i], "debug") == 0 && jePrimerenDebug == 1) i++;
    }
}

void printInputLine() {
    if (debugLevel > 0) {
        printf("Input line: '%s'\n", zaPrint);
        printToken();
    }    
}

void redirect() {
    int count = tokenCount;
    //printf("tokencount je %d\n", tokenCount);
    for (int i = 0; i < count; i++) {
        char beseda = line[tokens[i]];
        if (beseda == '>') {
            tokenCount--;
            if (debugLevel > 0) printf("Output redirect: '%s'\n", line + tokens[i] + 1);
        }
        else if (beseda == '<') {
            tokenCount--;
            if (debugLevel > 0) printf("Input redirect: '%s'\n", line + tokens[i] + 1);
        }
        else if (beseda == '&') {
            tokenCount--;
            background = 1;
            if (debugLevel > 0) printf("Background: '%d'\n", background);
        }
    }
    //printf("tokencount je %d\n", tokenCount);
    
    // posebej obravnavam ce je zunanji ukaz
    if (isExternal) {
        printf("External command '");
        for (int i = 0; i < tokenCount; i++) {
            if (i != tokenCount-1) {
                printf("%s ", line + tokens[i]);
            }
            else {
                printf("%s", line + tokens[i]);
            }
        }
        printf("'\n");
        return;
    }
    else return;
}

void pripraviInput() {
    for (int i = 0; i < MAX_INPUT; i++) {
        if (line[i] == '\n') line[i] = '\0';
        if (line[i] == '\r') line[i] = '\0';
        zaPrint[i] = line[i];
    }
}

// najde indekse tokenov v vhodu
void tokenize() {
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
}

void debug() {
    previousDebugLevel = debugLevel;
    // samo izpises trenutni debug level
    if (tokenCount == 1) {
        printInputLine();
        redirect();
        izpisiDebugLevel = 1;
    } 
    // spreminjas ali pa tudi ne
    else {
        debugLevel = atoi(line + tokens[1]);
        if (debugLevel == 0 && line[tokens[1]] != '0') {
            jePrimerenDebug = 0;
            // zacasno spremeni debugLevel na 1, da izpise debugLevel
            debugLevel = 1;
            printInputLine();
            redirect();
            spremeniNazajNaNic = 1;
        }
        else {
            jePrimerenDebug = 1;
        }
        izpisiDebugLevel = 0;
    }
    zadnjiStatus = 0;
}

void izhod() {
    if (tokenCount == 2) {
        zadnjiStatus = atoi(line + tokens[1]);
    }
    exit(zadnjiStatus);
}

void help() {
    printf("Izpis vseh moznih ukazov in njihova uporaba:\n");
    printf("- \033[0;32mdebug [level]\033[0m nastavi debug level na [level] ali izpise trenutno vrednost ce argument ni podan.\n");
    printf("- \033[0;32mexit [status]\033[0m zapre shell z statusom [status] ali z zadnjim zabelezenim statusom, ce argument ni podan.\n");
    printf("- \033[0;32mhelp\033[0m izpise vse moznosti ukazov in njihovo uporabo.\n");
    printf("- \033[0;32mprompt [name]\033[0m nastavi ime shell-a na [name] ali izpise trenutno ime ce argument ni podan.\n");
    printf("- \033[0;32mstatus\033[0m izpise se izhodni status zadnjega izvedenega ukaza. Status pusti nespremenjen.\n");
    printf("- \033[0;32mprint [args]\033[0m podane argumente na standardni izhod (brez koncnega skoka v novo vrstico).\n");
    printf("- \033[0;32mecho [args]\033[0m izpise podane argumente na standardni izhod in naredi skok v novo vrstico (enako kot print + doda skok v novo vrstico).\n");
    printf("- \033[0;32mlen [args]\033[0m izpise skupno dolzino argumentov (kot nizi).\n");
    printf("- \033[0;32msum [args]\033[0m izpise vsoto argumentov (kot cela stevila).\n");
    printf("- \033[0;32mcalc[arg1 op arg2]\033[0m izpise rezultat racunske operacije podane v argumentih (podprte operacije: +, -, *, /, mod).\n");
    printf("- \033[0;32mbasename [path]\033[0m izpise zadnji del poti (ime datoteke) podane v argumentu. V kolikor argument ni podan, je izhodni status 1.\n");
    printf("- \033[0;32mdirname [path]\033[0m izpise imenik podane poti [path]. V kolikor argument ni podan, je izhodni status 1.\n");
    printf("\n\033[0;33mOpozorilo\033[0m Ostali ukazi so zunanji in se niso implementirani. Pocakati boste morali na naslednjo nadgradnjo te naloge.\n");
    zadnjiStatus = 0;
}

void prompt() {
    if (tokenCount == 1) printf("%s\n", shellName);
    else {
        if (strlen(line + tokens[1]) > 8) {
            zadnjiStatus = 1;
            return;
        }
        else {
            strcpy(shellName, line + tokens[1]);
            // printf("%s\n", shellName);
        }
    }
    zadnjiStatus = 0;
}

void echo() {
    for (int i = 1; i < tokenCount; i++) {
        printf("%s ", line + tokens[i]);
    }
    printf("\n");
    zadnjiStatus = 0;
}

void print_builtin() {
    for (int i = 1; i < tokenCount; i++) {
        
        if (i != tokenCount -1) printf("%s ", line + tokens[i]);
        else printf("%s", line + tokens[i]);
    }
    zadnjiStatus = 0;
}

void len() {
    int dolzina = 0;
    for (int i = 1; i < tokenCount; i++) {
        dolzina += strlen(line + tokens[i]);
    }
    printf("%d\n", dolzina);
    zadnjiStatus = 0;
}

void calc() {
    int rezultat = 0;
    if (tokenCount == 4) {
        int prvoStevilo = atoi(line + tokens[1]);
        int drugoStevilo = atoi(line + tokens[3]);
        char operacija = line[tokens[2]];
        switch (operacija) {
            case '+':
                rezultat = prvoStevilo + drugoStevilo;
                break;
            case '-':
                rezultat = prvoStevilo - drugoStevilo;
                break;
            case '*':
                rezultat = prvoStevilo * drugoStevilo;
                break;
            case '/':
                rezultat = prvoStevilo / drugoStevilo;
                break;
            case '%':
                rezultat = prvoStevilo % drugoStevilo;
                break;
            default:
                printf("Napaka pri racunanju! Vnesel si nepodprto operacijo.\n");
                zadnjiStatus = 1;
                break;
        }
        printf("%d\n", rezultat);
    }
    else {
        printf("Napaka pri racunanju! Stevilo podanih argumentov ni pravilno.\n");
        zadnjiStatus = 1;
        return;
    }
    zadnjiStatus = 0;
}

void sum() {
    int vsota = 0;
    for (int i = 1; i < tokenCount; i++) {
        vsota += atoi(line + tokens[i]);
    }
    printf("%d\n", vsota);
    zadnjiStatus = 0;
}

void basename() {
    if (tokenCount == 1) {
        zadnjiStatus = 1;
        return;
    }
    char *s = line + tokens[1];
    char *print = strrchr(s, '/') + 1; 
    printf("%s\n", print);
    zadnjiStatus = 0;
}

void dirname() {
    if (tokenCount == 1) {
        zadnjiStatus = 1;
        return;
    }
    char *print = line + tokens[1];
    *strrchr(print, '/') = '\0';
    printf("%s\n", print);
    zadnjiStatus = 0;
}

void status() {
    printf("%d\n", zadnjiStatus);
}

void execute_builtin(int ukaz) {
    if (line[tokens[tokenCount]] == '&') {
        background = 1;
    }
    else background = 0;

    switch (ukaz) {
        case 0:
            // posebna funkcija
            debug();
            break;
        case 1:
            izhod();
            printInputLine();
            redirect();
            break;
        case 2:
            help();
            printInputLine();
            redirect();
            break;
        case 3:
            prompt();
            printInputLine();
            redirect();
            break;
        case 4:
            status();
            printInputLine();
            redirect();
            break;
        case 5:
            print_builtin();
            printInputLine();
            redirect();
            break;
        case 6:
            echo();
            printInputLine();
            redirect();
            break;
        case 7:
            len();
            printInputLine();
            redirect();
            break;
        case 8:
            sum();
            printInputLine();
            redirect();
            break;
        case 9:
            calc();
            printInputLine();
            redirect();
            break;
        case 10:
            basename();
            printInputLine();
            redirect();
            break;
        case 11:
            dirname();
            printInputLine();
            redirect();
            break;
        default:
            printf("Napaka pri prepoznavanju internega ukaza! Do sem ne bi smel priti.\n");
            break;
    }
}

int find_builtin() {
    for (int i = 0; i < NO_INTERNAL_COMMANDS; i++) {
        if (strcmp(line + tokens[0], interniUkazi[i]) == 0) {
            isExternal = 0;
            execute_builtin(i);
            // mal grdo hendlanje debugga, ampak deluje
            if (debugLevel > 0 && previousDebugLevel > 0) {
                printf("Executing builtin '%s' in %s\n", interniUkazi[i], background ? "background" : "foreground" );
                if (spremeniNazajNaNic) {
                    debugLevel = 0;
                    spremeniNazajNaNic = 0;
                }
            }
            // izpisuje debug level
            if (i == 0 && izpisiDebugLevel) {
                printf("%d\n", debugLevel);
            }
            return 1;
        }
    }
    return 0;
}

void execute_external() {
    isExternal = 1;
    printInputLine();
    redirect();
}

int main(int argc, char *argv[]) {
    native = isatty(STDIN_FILENO);
    strcpy(shellName, "mysh");
    if (native) printf("%s> ", shellName);

    while (fgets(line, sizeof(line), stdin) != NULL) {
        pripraviInput();
        tokenize();
        if (tokenCount == 0) {
            if (native) { printf("%s> ", shellName); }
            continue;
        }
        // poisci ukaz
        if (find_builtin() == 0) {
            execute_external();
        }
        // sprazni input line, da ni problemov pri naslednjem branju
        for (int i = 0; i < MAX_INPUT; i++) {
            line[i] = '\0';
            zaPrint[i] = '\0';
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
    return zadnjiStatus;
}