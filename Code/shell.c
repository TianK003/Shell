#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#define MAX_INPUT_LEN 512
#define MAX_TOKENS 64
#define NO_INTERNAL_COMMANDS 25
// ta tabela ima vhodno vrstico
char line[MAX_INPUT_LEN];
char zaPrint[MAX_INPUT_LEN];
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
char *interniUkazi[] = {"debug", "exit", "help", "prompt", "status", "print", "echo", "len", "sum", "calc", "basename", "dirname", "dirch", "dirwd", "dirmk", "dirrm", "dirls", "rename", "unlink", "remove", "linkhard", "linksoft", "linkread", "linklist", "cpcat"};
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
    for (int i = 0; i < MAX_INPUT_LEN; i++) {
        if (line[i] == '\n') line[i] = '\0';
        if (line[i] == '\r') line[i] = '\0';
        zaPrint[i] = line[i];
    }
}

// najde indekse tokenov v vhodu
void tokenize() {
    int isciPrvega = 1;
    int jeVNarekovajih = 0;
    for (int i = 0; i < MAX_INPUT_LEN; i++) {
        // naleteli smo na komentar, lahko zakljucimo s to vrstico
        if (line[i] == '#' && jeVNarekovajih == 0 && isciPrvega == 1) {
            for (int j = i; j < MAX_INPUT_LEN; j++) {
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

void izhod_builtin() {
    if (tokenCount == 2) {
        zadnjiStatus = atoi(line + tokens[1]);
    }
    exit(zadnjiStatus);
}

void help_builtin() {
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
    printf("- \033[0;32mdirch [imenik]\033[0m spremeni trenutni aktivni imenik v imenik [imenik]. V kolikor argument ni podan, skocimo v korenski imenik.\n");
    printf("- \033[0;32mdirwd [mode]\033[0m izpise trenutni aktivni imenik. Ce je [mode] 'full', se izpise celotna pot, ce je 'base' pa samo osnova imena (basename), privzeto (ce ni podano) je 'base'.\n");
    printf("- \033[0;32mdirmk [imenik]\033[0m ustvari nov imenik [imenik]. V kolikor ze obstaja, se izpise napaka.\n");
    printf("- \033[0;32mdirrm [imenik]\033[0m izbrise imenik [imenik]. V kolikor ne obstaja, se izpise napaka.\n");
    printf("- \033[0;32mdirls [imenik]\033[0m izpise vsebino imenika [imenik]. V kolikor ni podan argument, se izpise trenutni imenik. Izpisejo se le imena datotek, locena z DVEMA presledkoma.\n");
    printf("- \033[0;32mrename [old] [new]\033[0m preimenuje datoteko [old] v [new].\n");
    printf("- \033[0;32munlink [file]\033[0m izbrise datoteko [file] (gre samo za odstranitev imeniskega vnosa - sistemski klic unlink).\n");
    printf("- \033[0;32mremove [file]\033[0m izbrise datoteko [file] (sistemski klic remove).\n");
    printf("- \033[0;32mlinkhard [old] [new]\033[0m ustvari trdo povezavo [new] na datoteko [old].\n");
    printf("- \033[0;32mlinksoft [old] [new]\033[0m ustvari simbolno povezavo [new] na datoteko [old].\n");
    printf("- \033[0;32mlinkread [link]\033[0m izpise cilj podane simbolicne povezave.\n");
    printf("- \033[0;32mlinklist [name]\033[0m v trenutnem imeniku poisce vse trde povezave na datoteko z imenom [name]. Povezave se izpisejo loceno z dvema presledkoma.\n");
    printf("- \033[0;32mcpcat [file1] [file2]\033[0m prekopira vsebino datoteke [file1] v datoteko [file2].\n");
    printf("\n\033[0;33mOpozorilo\033[0m Ostali ukazi so zunanji in se niso implementirani. Pocakati boste morali na naslednjo nadgradnjo te naloge.\n");
    zadnjiStatus = 0;
}

void prompt_builtin() {
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

void echo_builtin() {
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

void len_builtin() {
    int dolzina = 0;
    for (int i = 1; i < tokenCount; i++) {
        dolzina += strlen(line + tokens[i]);
    }
    printf("%d\n", dolzina);
    zadnjiStatus = 0;
}

void calc_builtin() {
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

void sum_builtin() {
    int vsota = 0;
    for (int i = 1; i < tokenCount; i++) {
        vsota += atoi(line + tokens[i]);
    }
    printf("%d\n", vsota);
    zadnjiStatus = 0;
}

void basename_builtin() {
    if (tokenCount == 1) {
        zadnjiStatus = 1;
        return;
    }
    char *s = line + tokens[1];
    char *print = strrchr(s, '/') + 1; 
    printf("%s\n", print);
    zadnjiStatus = 0;
}

void dirname_builtin() {
    if (tokenCount == 1) {
        zadnjiStatus = 1;
        return;
    }
    char *print = line + tokens[1];
    *strrchr(print, '/') = '\0';
    printf("%s\n", print);
    zadnjiStatus = 0;
}

void status_builtin() {
    printf("%d\n", zadnjiStatus);
}

void dirch_builtin() {
    if (tokenCount == 1) {
        chdir("/");
    }
    else {
        if (chdir(line + tokens[1]) == -1) {
            fflush(stdout);
            fprintf(stderr, "dirch: %s\n", strerror(errno));
            zadnjiStatus = errno;\
            return;
        }
    }
    zadnjiStatus = 0;
}

void dirwd_builtin() {
    char cwd[MAX_INPUT_LEN];
    getcwd(cwd, MAX_INPUT_LEN);
    if (tokenCount == 1 || strcmp(line + tokens[1], "base") == 0) {
        char *tmp = strrchr(cwd, '/') + 1; 
        if (tmp[0] == '\0') {
            tmp--;
        }
        strcpy(cwd, tmp);
    }
    printf("%s\n", cwd);
    zadnjiStatus = 0;
}

void dirmk_builtin() {
    if (mkdir(line + tokens[1], 0777) == -1) {
        fflush(stdout);
        fprintf(stderr, "dirmk: %s\n", strerror(errno));
        zadnjiStatus = errno;
        return;
    }
    zadnjiStatus = 0;
}

void dirrm_builtin() {
    if (rmdir(line + tokens[1]) == -1) {
        fflush(stdout);
        zadnjiStatus = errno;
        fprintf(stderr, "dirrm: %s\n", strerror(errno));
        return;
    }
    zadnjiStatus = 0;
}

void dirls_builtin() {
    DIR *dir;
    struct dirent *entry;
    if (tokenCount == 1) {
        dir = opendir(".");
    }
    else {
        dir = opendir(line + tokens[1]);
    }
    if (dir == NULL) {
        fflush(stdout);
        zadnjiStatus = errno;
        fprintf(stderr, "dirrm: %s\n", strerror(errno));
        zadnjiStatus = errno;
        return;
    }
    while ((entry = readdir(dir)) != NULL) {
        printf("%s  ", entry->d_name);
        fflush(stdout);
    }
    printf("\n");
    closedir(dir);
    zadnjiStatus = 0;
}

void rename_builtin() {
    if (rename(line + tokens[1], line + tokens[2]) == -1) {
        fflush(stdout);
        zadnjiStatus = errno;
        fprintf(stderr, "rename: %s\n", strerror(errno));
        return;
    }
    zadnjiStatus = 0;
}

void unlink_builtin() {
    if (unlink(line + tokens[1]) == -1) {
        fflush(stdout);
        zadnjiStatus = errno;
        fprintf(stderr, "unlink: %s\n", strerror(errno));
        return;
    }
    zadnjiStatus = 0;
}

void remove_builtin() {
    if (remove(line + tokens[1]) == -1) {
        fflush(stdout);
        zadnjiStatus = errno;
        fprintf(stderr, "remove: %s\n", strerror(errno));
        return;
    }
    zadnjiStatus = 0;
}

void linkhard_builtin() {
    if (link(line + tokens[1], line + tokens[2]) == -1) {
        fflush(stdout);
        zadnjiStatus = errno;
        fprintf(stderr, "linkhard: %s\n", strerror(errno));
        return;
    }
    zadnjiStatus = 0;
}

void linksoft_builtin() {
    if (symlink(line + tokens[1], line + tokens[2]) == -1) {
        fflush(stdout);
        zadnjiStatus = errno;
        fprintf(stderr, "linksoft: %s\n", strerror(errno));
        return;
    }
    zadnjiStatus = 0;
}

void linkread_builtin() {
    char buf[1024];
    ssize_t len;
    char *ime = line + tokens[1];
    len = readlink(ime, buf, sizeof(buf)-1);
    if (len != -1) {
        buf[len] = '\0';
        printf("%s\n", buf);
    } else {
        fflush(stdout);
        zadnjiStatus = errno;
        fprintf(stderr, "linkread: %s\n", strerror(errno));
        return;
    }
    zadnjiStatus = 0;
}

void linklist_builtin() {
    char *ime = line + tokens[1];
    struct dirent *dp;
    DIR *dir = opendir(".");
    if (!dir) {
        fflush(stdout);
        zadnjiStatus = errno;
        fprintf(stderr, "opendir: %s\n", strerror(errno));
        return;
    }

    struct stat openedFileHardLinkValue, argHardLinkValue;
    if (lstat(ime, &argHardLinkValue) == -1) {
        fflush(stdout);
        zadnjiStatus = errno;
        fprintf(stderr, "lstat: %s\n", strerror(errno));
        return;
    }

    while ((dp = readdir(dir)) != NULL) {
        if (lstat(dp->d_name, &openedFileHardLinkValue) == -1) {
            fflush(stdout);
            zadnjiStatus = errno;
            fprintf(stderr, "lstat: %s\n", strerror(errno));
            return;
        }
        if (openedFileHardLinkValue.st_ino == argHardLinkValue.st_ino) {
            printf("%s  ", dp->d_name);
        }
    }
    printf("\n");

    closedir(dir);
}

void kopiraj(int input, int output) {
    unsigned char buf = 0;
    while (read(input, &buf, 1) != 0) write(output, &buf, 1);
    if (input != STDIN_FILENO) close(input);
    if (output != STDOUT_FILENO) close(output);
}

void cpcat_builtin() {
    fflush(stdout);
    if (tokenCount == 1) kopiraj(STDIN_FILENO, STDOUT_FILENO);
    else if (tokenCount == 2)
    {
        int vhod = open(line + tokens[1], O_CREAT | O_RDONLY, 0644);
        if (vhod == -1) {
            fflush(stdout);
            zadnjiStatus = errno;
            fprintf(stderr, "cpcat: %s\n", strerror(errno));
            return;
        }
        kopiraj(vhod, STDOUT_FILENO);
    }
    else if (tokenCount == 3)
    {
        int vhod = strcmp(line + tokens[1], "-") ? open(line + tokens[1], O_RDONLY, 0644) : STDIN_FILENO;
        int izhod = open(line + tokens[2], O_CREAT | O_WRONLY, 0644);
        if (vhod == -1 || izhod == -1) {
            fflush(stdout);
            zadnjiStatus = errno;
            fprintf(stderr, "cpcat: %s\n", strerror(errno));
            return;
        }
        kopiraj(vhod, izhod);
    }
    zadnjiStatus = 0;
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
            izhod_builtin();
            printInputLine();
            redirect();
            break;
        case 2:
            help_builtin();
            printInputLine();
            redirect();
            break;
        case 3:
            prompt_builtin();
            printInputLine();
            redirect();
            break;
        case 4:
            status_builtin();
            printInputLine();
            redirect();
            break;
        case 5:
            print_builtin();
            printInputLine();
            redirect();
            break;
        case 6:
            echo_builtin();
            printInputLine();
            redirect();
            break;
        case 7:
            len_builtin();
            printInputLine();
            redirect();
            break;
        case 8:
            sum_builtin();
            printInputLine();
            redirect();
            break;
        case 9:
            calc_builtin();
            printInputLine();
            redirect();
            break;
        case 10:
            basename_builtin();
            printInputLine();
            redirect();
            break;
        case 11:
            dirname_builtin();
            printInputLine();
            redirect();
            break;
        case 12:
            dirch_builtin();
            printInputLine();
            redirect();
            break;
        case 13:
            dirwd_builtin();
            printInputLine();
            redirect();
            break;
        case 14:
            dirmk_builtin();
            printInputLine();
            redirect();
            break;
        case 15:
            dirrm_builtin();
            printInputLine();
            redirect();
            break;
        case 16:
            dirls_builtin();
            printInputLine();
            redirect();
            break;
        case 17:
            rename_builtin();
            printInputLine();
            redirect();
            break;
        case 18:
            unlink_builtin();
            printInputLine();
            redirect();
            break;
        case 19:
            remove_builtin();
            printInputLine();
            redirect();
            break;
        case 20:
            linkhard_builtin();
            printInputLine();
            redirect();
            break;
        case 21:
            linksoft_builtin();
            printInputLine();
            redirect();
            break;
        case 22:
            linkread_builtin();
            printInputLine();
            redirect();
            break;
        case 23:
            linklist_builtin();
            printInputLine();
            redirect();
            break;
        case 24:
            cpcat_builtin();
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
        for (int i = 0; i < MAX_INPUT_LEN; i++) {
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