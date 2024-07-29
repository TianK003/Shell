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
#include <sys/utsname.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_INPUT_LEN 512
#define MAX_TOKENS 64
#define NO_INTERNAL_COMMANDS 37
// ta tabela ima vhodno vrstico
char line[MAX_INPUT_LEN];
char zaPrint[MAX_INPUT_LEN];
char proc_path[MAX_INPUT_LEN];
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
// da ves kako printat
int jePrimerenDebug = 1;
char *interniUkazi[] = {"debug", "exit", "help", "prompt", "status", "print", "echo", "len", "sum", "calc", "basename", "dirname", "dirch", "dirwd", "dirmk", "dirrm", "dirls", "rename", "unlink", "remove", "linkhard", "linksoft", "linkread", "linklist", "cpcat", "pid", "ppid", "uid", "euid", "gid", "egid", "sysinfo", "proc", "pids", "pinfo", "waitone", "waitall"};

int previousDebugLevel = 0;
// da ves ce interaktivno izvajas
int native = 0;
// posebej za debuganje, da se izpise vse kot mora
int spremeniNazajNaNic = 0;
char *inputRedirect = NULL;
char *outputRedirect = NULL;
int stdinOriginal;
int stdoutOriginal;
// izpise vhod, tokene in redirecte
void printToken() {
    int j = 0;
    for (int i = 0; i < tokenCount; i++) {
        printf("Token %d: '%s'\n", j, line + tokens[i]);
        j++;
        if (i == 0 && debugLevel > 0 && strcmp(line + tokens[i], "debug") == 0 && jePrimerenDebug == 1) i++;
    }
}

void redirect() {
    int count = tokenCount;
    //printf("tokencount je %d\n", tokenCount);
    for (int i = 0; i < count; i++) {
        char* beseda = line + tokens[i];
        if (beseda[0] == '>') {
            tokenCount--;
            outputRedirect = beseda + 1;
            if (debugLevel > 0) printf("Output redirect: '%s'\n", beseda + 1);
        }
        else if (beseda[0] == '<') {
            tokenCount--;
            inputRedirect = beseda + 1;
            if (debugLevel > 0) printf("Input redirect: '%s'\n", beseda + 1);
        }
        else if (strcmp(beseda, "&") == 0) {
            tokenCount--;
            background = 1;
            if (debugLevel > 0) printf("Background: '%d'\n", background);
        }
    }
}

void pripraviInput() {
    for (int i = 0; i < MAX_INPUT_LEN; i++) {
        if (line[i] == '\n') line[i] = '\0';
        if (line[i] == '\r') line[i] = '\0';
        zaPrint[i] = line[i];
    }

    if (debugLevel > 0) {
        printf("Input: '%s'\n", zaPrint);
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
    if (strcmp(line + tokens[tokenCount - 1], "&") == 0) {
        background = 1;
    }
    else background = 0;

    if (debugLevel > 0) {
        printToken();
    }
}   

void debug_builtin() {
    // samo izpises trenutni debug level
    if (tokenCount == 1) {
        izpisiDebugLevel = 1;
    } 
    // spreminjas ali pa tudi ne
    else {
        debugLevel = atoi(line + tokens[1]);
        if (debugLevel == 0 && line[tokens[1]] != '0') {
            jePrimerenDebug = 0;
            // zacasno spremeni debugLevel na 1, da izpise debugLevel
            debugLevel = 1;
            spremeniNazajNaNic = 1;
        }
        else {
            jePrimerenDebug = 1;
        }
        izpisiDebugLevel = 0;
    }
    zadnjiStatus = 0;
}

void exit_builtin() {
    if (tokenCount == 2) {
        zadnjiStatus = atoi(line + tokens[1]);
    }
    fflush(stdout);
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
    printf("- \033[0;32mpid\033[0m izpise PID procesa lupine.\n");
    printf("- \033[0;32mppid\033[0m izpise PID starsevsekga procesa lupine.\n");
    printf("- \033[0;32muid\033[0m izpise UID uporabnika, ki je lastnik procesa lupine.\n");
    printf("- \033[0;32meuid\033[0m izpise EUID uporabnika, ki je AKTUALNI lastnik procesa lupine.\n");
    printf("- \033[0;32mgid\033[0m izpise GID skupine, kateri pripada procesa lupine.\n");
    printf("- \033[0;32megid\033[0m izpise EGID skupine, kateri AKTUALNO pripada proces lupine.\n");
    printf("- \033[0;32msysinfo\033[0m izpise osnovne informacije o sistemu (sysname, nodename, release, version, machine).\n");
    printf("- \033[0;32mproc [pot]\033[0m nastavitev poti do procfs datotecnega sistema. Ce argument ni podan, se izpise trenutna nastavljena pot.\n");
    printf("- \033[0;32mpids\033[0m izpise PID-e vseh procesov v procfs.\n");
    printf("- \033[0;32mpinfo \033[0m izpise informacije o trenutnih procesih (PID, PPID, STANJE, IME), ki jih pridobi iz datoteke stat v procfs.\n");
    printf("- \033[0;32mwaitone [pid]\033[0m pocaka na zakljucitev procesa s PID [pid]. V kolikor pid ni podan, se pocaka na prvega.\n");
    printf("- \033[0;32mwaitall\033[0m pocaka na zakljucitev vseh procesov, ki se izvajajo v ozadju.\n");
    printf("\n\033[0;33mOpozorilo\033[0m Ostali ukazi so zunanji in njihovo obnasanje ni poznano je odvisno od posameznih ukazov. S previdnostjo izvajajte zunanje ukaze!\n");
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
    fflush(stdout);
    printf("%d\n", zadnjiStatus);
}

void dirch_builtin() {
    if (tokenCount == 1) {
        chdir("/");
    }
    else {
        if (chdir(line + tokens[1]) == -1) {
            zadnjiStatus = errno;
            perror("dirch");
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
        zadnjiStatus = errno;
        perror("dirmk");
        return;
    }
    zadnjiStatus = 0;
}

void dirrm_builtin() {
    if (rmdir(line + tokens[1]) == -1) {
        zadnjiStatus = errno;
        perror("dirrm");
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
        zadnjiStatus = errno;
        perror("dirls");
        return;
    }
    while ((entry = readdir(dir)) != NULL) {
        printf("%s  ", entry->d_name);
    }
    printf("\n");
    closedir(dir);
    zadnjiStatus = 0;
}

void rename_builtin() {
    if (rename(line + tokens[1], line + tokens[2]) == -1) {
        zadnjiStatus = errno;
        perror("rename");
        return;
    }
    zadnjiStatus = 0;
}

void unlink_builtin() {
    if (unlink(line + tokens[1]) == -1) {
        zadnjiStatus = errno;
        perror("unlink");
        return;
    }
    zadnjiStatus = 0;
}

void remove_builtin() {
    if (remove(line + tokens[1]) == -1) {
        zadnjiStatus = errno;
        perror("remove");
        return;
    }
    zadnjiStatus = 0;
}

void linkhard_builtin() {
    if (link(line + tokens[1], line + tokens[2]) == -1) {
        zadnjiStatus = errno;
        perror("linkhard");
        return;
    }
    zadnjiStatus = 0;
}

void linksoft_builtin() {
    if (symlink(line + tokens[1], line + tokens[2]) == -1) {
        zadnjiStatus = errno;
        perror("linksoft");
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
        zadnjiStatus = errno;
        perror("linkread");
        return;
    }
    zadnjiStatus = 0;
}

void linklist_builtin() {
    char *ime = line + tokens[1];
    struct dirent *dp;
    DIR *dir = opendir(".");
    if (!dir) {
        zadnjiStatus = errno;
        perror("opendir");
        return;
    }

    struct stat openedFileHardLinkValue, argHardLinkValue;
    if (lstat(ime, &argHardLinkValue) == -1) {
        zadnjiStatus = errno;
        perror("lstat");
        return;
    }

    while ((dp = readdir(dir)) != NULL) {
        if (lstat(dp->d_name, &openedFileHardLinkValue) == -1) {
            zadnjiStatus = errno;
            perror("lstat");
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
    if (tokenCount == 1) kopiraj(STDIN_FILENO, STDOUT_FILENO);
    else if (tokenCount == 2)
    {
        int vhod = open(line + tokens[1], O_CREAT | O_RDONLY, 0644);
        if (vhod == -1) {
            zadnjiStatus = errno;
            perror("cpcat");
            return;
        }
        kopiraj(vhod, STDOUT_FILENO);
    }
    else if (tokenCount == 3)
    {
        int vhod = strcmp(line + tokens[1], "-") ? open(line + tokens[1], O_RDONLY, 0644) : STDIN_FILENO;
        int izhod = open(line + tokens[2], O_CREAT | O_WRONLY, 0644);
        if (vhod == -1 || izhod == -1) {
            zadnjiStatus = errno;
            perror("cpcat");
            return;
        }
        kopiraj(vhod, izhod);
    }
    zadnjiStatus = 0;
}

void pid_builtin() {
    printf("%d\n", getpid());
    zadnjiStatus = 0;
}

void ppid_builtin() {
    printf("%d\n", getppid());
    zadnjiStatus = 0;
}

void uid_builtin() {
    printf("%d\n", getuid());
    zadnjiStatus = 0;
}

void euid_builtin() {
    printf("%d\n", geteuid());
    zadnjiStatus = 0;
}

void gid_builtin() {
    printf("%d\n", getgid());
    zadnjiStatus = 0;
}

void egid_builtin() {
    printf("%d\n", getegid());
    zadnjiStatus = 0;
}

void sysinfo_builtin() {
    struct utsname uts;
    if (uname(&uts) == -1) {
        zadnjiStatus = errno;
        perror("sysinfo");
        return;
    }
    printf("Sysname: %s\nNodename: %s\nRelease: %s\nVersion: %s\nMachine: %s\n", uts.sysname, uts.nodename, uts.release, uts.version, uts.machine);
    zadnjiStatus = 0;
}

void proc_builtin() {
    if (tokenCount == 1) {
        printf("%s\n", proc_path);
    }
    else {
        char *pot = line + tokens[1];
        if (access(pot, F_OK | R_OK) == -1) {
            zadnjiStatus = 1;
            return;
        }
        strcpy(proc_path, pot);
    }
    zadnjiStatus = 0;
}

void sortirajProcese(int *pids, int stProcesov, int pinfo) {
    int i, j;
    for (i = 0; i < stProcesov - 1; i++) {
        for (j = 0; j < stProcesov - i - 1; j++) {
            if (pids[j] > pids[j + 1]) {
                // swap
                int temp = pids[j];
                pids[j] = pids[j + 1];
                pids[j + 1] = temp;
            }
        }
    }
    if (pinfo == 0) {
        for (int i = 0; i < stProcesov; i++) {
            printf("%d\n", pids[i]);
        }
    }
    else {
        printf("%5s %5s %6s %s\n", "PID", "PPID", "STANJE", "IME");
        for (int i = 0; i < stProcesov; i++) {
            char pot[1024];
            int pid, ppid;
            char stanje;
            char ime[128];
            snprintf(pot, sizeof(pot), "%s/%d/stat", proc_path, pids[i]);
            FILE *stat = fopen(pot, "r");
            if (stat == NULL) {
                zadnjiStatus = errno;
                perror("fopen");
                return;
            }
            // te zacetni podakti ne bi smeli bit daljsi od tega - ze to je ful prevec
            char vsebina[512];
            if (fgets(vsebina, sizeof(vsebina), stat) != NULL) {
                sscanf(vsebina, "%d %s %c %d", &pid, ime, &stanje, &ppid);
            } else {
                zadnjiStatus = errno;
                perror("fgets");
                return;
            }
            // zadnji oklepaj odstrani, sprednjega pa ne izpisi
            ime[strlen(ime) - 1] = '\0';
            printf("%5d %5d %6c %s\n", pid, ppid, stanje, ime + 1);
        }
    }
}

void pids_builtin() {
    DIR *proc_dir;
    struct dirent *proces;

    int stProcesov = 0;
    proc_dir = opendir(proc_path);
    if (proc_dir == NULL) {
        zadnjiStatus = errno;
        perror("pids");
        return;
    } 
    while ((proces = readdir(proc_dir)) != NULL) {
            if (isdigit(proces->d_name[0])) {
                stProcesov++;
            }
    }
    int shraniStProcesov = stProcesov;
    closedir(proc_dir);
    int *pids = (int *) malloc(stProcesov * sizeof(int));
    proc_dir = opendir(proc_path);
    while ((proces = readdir(proc_dir)) != NULL) {
            if (isdigit(proces->d_name[0])) {
                pids[--stProcesov] = atoi(proces->d_name);
            }
    }
    sortirajProcese(pids, shraniStProcesov, 0);
    zadnjiStatus = 0;
}

void pinfo_builtin() {
    DIR *proc_dir;
    struct dirent *proces;

    int stProcesov = 0;
    proc_dir = opendir(proc_path);
    if (proc_dir == NULL) {
        zadnjiStatus = errno;
        perror("pinfo");
        return;
    } 
    while ((proces = readdir(proc_dir)) != NULL) {
            if (isdigit(proces->d_name[0])) {
                stProcesov++;
            }
    }
    int shraniStProcesov = stProcesov;
    closedir(proc_dir);
    int *pids = (int *)malloc(stProcesov * sizeof(int));
    proc_dir = opendir(proc_path);
    while ((proces = readdir(proc_dir)) != NULL) {
            if (isdigit(proces->d_name[0])) {
                pids[--stProcesov] = atoi(proces->d_name);
            }
    }
    sortirajProcese(pids, shraniStProcesov, 1);
    zadnjiStatus = 0;
}

void waitone_builtin() {
    int status, pid;
    if (tokenCount == 1) {
        pid = wait(&status);
        if (pid == -1) {
            zadnjiStatus = 0;
            return;
        }
        zadnjiStatus = WEXITSTATUS(status);
        
    } else {
        pid = atoi(line + tokens[1]);
        if (kill(pid, 0) == -1) {
            zadnjiStatus = 0;
            return;
        }
        waitpid(pid, &status, 0);
        zadnjiStatus = WEXITSTATUS(status);
    }
}

void waitall_builtin() {
    int status, pid;
    while (1) {
        pid = wait(&status);
        if (pid == -1) {
            break;
        }
    }
    zadnjiStatus = 0;
}

void (*pFunkcije[]) () = {debug_builtin, exit_builtin, help_builtin, prompt_builtin, status_builtin, print_builtin, echo_builtin, len_builtin, sum_builtin, calc_builtin, basename_builtin, dirname_builtin, dirch_builtin, dirwd_builtin, dirmk_builtin, dirrm_builtin, dirls_builtin, rename_builtin, unlink_builtin, remove_builtin, linkhard_builtin, linksoft_builtin, linkread_builtin, linklist_builtin, cpcat_builtin, pid_builtin, ppid_builtin, uid_builtin, euid_builtin, gid_builtin, egid_builtin, sysinfo_builtin, proc_builtin, pids_builtin, pinfo_builtin, waitone_builtin, waitall_builtin};

void execute_builtin(int ukaz) {
    if (!background) {
        stdinOriginal = dup(STDIN_FILENO);
        stdoutOriginal = dup(STDOUT_FILENO);
        fflush(stdout);
        // naredi preusmeritve
        if (inputRedirect != NULL) {
            int input = open(inputRedirect, O_RDONLY, 0644);
            if (input == -1) {
                zadnjiStatus = errno;
                perror("open");
                return;
            }
            dup2(input, STDIN_FILENO);
            close(input);
        }
        
        if (outputRedirect != NULL) {
            int output = open(outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (output == -1) {
                zadnjiStatus = errno;
                perror("open");
                return;
            }
            dup2(output, STDOUT_FILENO);
            close(output);
        }
        pFunkcije[ukaz]();

        fflush(stdout);
        if (inputRedirect != NULL) {
            dup2(stdinOriginal, STDIN_FILENO);
            close(stdinOriginal);
        }
        if (outputRedirect != NULL) {
            dup2(stdoutOriginal, STDOUT_FILENO);
            close(stdoutOriginal);
        }
    }
    else {
        fflush(stdin);
        int pid = fork();
        if (pid == -1) {
            zadnjiStatus = errno;
            perror("fork");
            return;
        } else if (pid == 0) {
            // naredi preusmeritve
            if (inputRedirect != NULL) {
                int input = open(inputRedirect, O_RDONLY, 0644);
                if (input == -1) {
                    zadnjiStatus = errno;
                    perror("open");
                    return;
                }
                dup2(input, STDIN_FILENO);
                close(input);
            }
            
            if (outputRedirect != NULL) {
                int output = open(outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (output == -1) {
                    zadnjiStatus = errno;
                    perror("open");
                    return;
                }
                dup2(output, STDOUT_FILENO);
                close(output);
            }
            pFunkcije[ukaz]();
            exit(zadnjiStatus);
        }
    }
}

void execute_external() {
    if (debugLevel > 0) printf("Executing external '%s' in %s\n", line + tokens[0], background ? "background" : "foreground" );
    char *zetoni[tokenCount + 1];
    for (int i = 0; i < tokenCount; i++) {
        zetoni[i] = line + tokens[i];
    }
    zetoni[tokenCount] = NULL;

    fflush(stdin);
    int pid = fork();
    if (pid == -1) {
        zadnjiStatus = errno;
        perror("fork");
        return;    
    } else if (pid == 0) {
        // naredi preusmeritve
        if (inputRedirect != NULL) {
            int input = open(inputRedirect, O_RDONLY, 0644);
            if (input == -1) {
                zadnjiStatus = errno;
                perror("open");
                return;
            }
            dup2(input, STDIN_FILENO);
            close(input);
        }
        
        if (outputRedirect != NULL) {
            int output = open(outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (output == -1) {
                zadnjiStatus = errno;
                perror("open");
                return;
            }
            dup2(output, STDOUT_FILENO);
            close(output);
        }
        execvp(zetoni[0], zetoni);
        // ce se vrne je napaka
        perror("exec"); 
        zadnjiStatus = 127;
        exit(zadnjiStatus);
    } else {
        if (!background) {
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status)) {
                zadnjiStatus = WEXITSTATUS(status);
            }
        }
    }
}

void find_builtin() {
    for (int i = 0; i < NO_INTERNAL_COMMANDS; i++) {
        if (strcmp(line + tokens[0], interniUkazi[i]) == 0) {
            // mal grdo hendlanje debugga, ampak deluje
            if (debugLevel > 0 && previousDebugLevel > 0) {
                printf("Executing builtin '%s' in %s\n", interniUkazi[i], background ? "background" : "foreground" );
                if (spremeniNazajNaNic) {
                    debugLevel = 0;
                    spremeniNazajNaNic = 0;
                }
            }
            previousDebugLevel = debugLevel;
            // izpisuje debug level
            if (i == 0 && izpisiDebugLevel) {
                printf("%d\n", debugLevel);
            }
            execute_builtin(i);
            return;
        }
    }
    execute_external();
}

// prepisano iz vaj
void handle_sigchld (int signum)
{
    int pid, status, serrno;
    serrno = errno;
    while (1) {
        pid = waitpid(WAIT_ANY, &status, WNOHANG);
        if (pid < 0) {
                break;
        }
        if (pid == 0) {
            break;
        }
    }
    errno = serrno;
}

int main(int argc, char *argv[]) {
    native = isatty(STDIN_FILENO);
    strcpy(shellName, "mysh");
    if (native) printf("%s> ", shellName);
    strcpy(proc_path, "/proc");
    signal(SIGCHLD, handle_sigchld);

    while (fgets(line, sizeof(line), stdin) != NULL) {
        fflush(stdout);
        fflush(stderr);
        pripraviInput();
        tokenize();
        if (tokenCount == 0) {
            if (native) { printf("%s> ", shellName); }
            continue;
        }
        redirect();
        find_builtin();
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
        inputRedirect = NULL;
        outputRedirect = NULL;
    }
    return zadnjiStatus;
}