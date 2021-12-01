/******************************************************
 * minishell.cpp
 * Funktion: implementierung einer Shell die minimale funktionen hat
 * Autor: Maximilian Jaesch
 *
 * Historie:
 *      2021.11.11: getUserName ausprobiert
 *      2021.11.18: getUserName und getWD funktioniert
 *                  find out what environment variables do
 *
 * To Do:
 *      compare with env when there is a $ in front of the string -> whitespace
 *      in prompt, be able to see that its custom shell
 ******************************************************/
// Quelle:
// https://brennan.io/2015/01/16/write-a-shell-in-c/
// https://stackoverflow.com/questions/13801175/classic-c-using-pipes-in-execvp-function-stdin-and-stdout-redirection


//includes
#include <iostream>
#include <iomanip>
#include <limits.h>
#include <algorithm>
#include <unistd.h>
#include <sstream>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <pwd.h>
#include <cstring>

//defines
#define BUFFERSIZE 64
#define DELIM " =\t\r\n\a"

//Methode um den Username zu bekommen mit @id...
std::string getUserName() {
    uid_t uid = geteuid();
    struct passwd *pw = getpwuid(uid);
    if (pw) {
        return std::string(pw->pw_name);
    }
    return {};
}

//Quelle: https://www.geeksforgeeks.org/making-linux-shell-c/
//mit jedem Aufruf wird der Username und die aktuelle Directory ausgegeben
void printUserDir() {
    char cwd[1024];
    char *userName = getenv("LOGNAME");
    std::string CoolUserName = getUserName();
    std::stringstream ss;
    std::string userNameString;
    ss << userName;
    ss >> userNameString;
    getcwd(cwd, sizeof(cwd));
    //printf("%s@%s:$ ", userNameString.c_str(), cwd);
    printf("%s:%s$ ", CoolUserName.c_str(), cwd);
}

//Die Eingabe wird gelesen
char *miniShellEingabe(void) {
    char *line = NULL;
    ssize_t bufsize = 0; // have getline allocate a buffer for us

    if (getline(&line, reinterpret_cast<size_t *>(&bufsize), stdin) == -1) {
        if (feof(stdin)) {
            exit(EXIT_SUCCESS);  // We recieved an EOF
        } else {
            perror("readline");
            exit(EXIT_FAILURE);
        }
    }

    return line;
}

//Die enviroment Variablen werden Verarbeitet
char* envVerarbeiten(char* token);

//Methode fürs Piping
int miniShellPipe(char** args1, char** args2);


char** parsePipe(char* str);

//Anzahl der Pipe-Symbole bekommen
int countNumberOfPipes(char* line);


int miniShellStarten(char **args);

int miniShellAusfuehren(char **args, char** line);

int miniShellPipe(char** args1, char** args2);

int boi(char** args1, char** args2);


/*
  Function Declarations for builtin shell commands:
 */
int miniShellCD(char **args);

int miniShellHELP(char **args);

int miniShellEXIT(char **args);

int miniShellExport(char **args);

int miniShellShowEnv(char **args);



/*
  List of builtin commands, followed by their corresponding functions.
 */
char *eingebauteStrings[] = {
        "cd",
        "help",
        "exit",
        "export",
        "showenv",
};

int (*eingebauteBefehle[])(char **) = {
        &miniShellCD,
        &miniShellHELP,
        &miniShellEXIT,
        &miniShellExport,
        &miniShellShowEnv,
};

int anzahlEingebauterBefehle() {
    return sizeof(eingebauteStrings) / sizeof(char *);
}

/*
  Builtin function implementations.
*/
int miniShellCD(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "MiniShell: expected argument to \"cd\"\n");
    }else {
        if (chdir(args[1]) != 0) {
            perror("MiniShell");
        }
    }
    return 1;
}
//https://www.proggen.org/doku.php?id=c:lib:string:strtok
//strtok hilfe benutzt
//man in bash eingeben
int miniShellExport(char **args) {

        printf("hallo %s",args[1]);

        if (args[1] == NULL) {
            fprintf(stderr, "MiniShell: expected argument to \"export\"\n");
        } else {
            setenv(args[1],args[2],0);
        }

    return 1;
}

int miniShellShowEnv(char **args) {
    char* env = envVerarbeiten(args[1]);
    printf("%s\n",env);
}


int miniShellHELP(char **args) {
    int i;
    printf("Maxi und abdus minishell\n");
    printf("befehle und argumente eingaben, dann enter\n");
    printf("diese befehle sind implementiert\n");

    for (i = 0; i < anzahlEingebauterBefehle(); i++) {
        printf("  %s\n", eingebauteStrings[i]);
    }

    printf("Use the man command for information on other programs.\n");
    return 1;
}

int miniShellEXIT(char **args) {
    return 0;
}

int miniShellStarten(char **args) {
    pid_t pid, wpid;
    int status;


    pid = fork();
    if (pid == 0) {
        // Child process
        //hier wird der befehl ausgeführt!!
        if (execvp(args[0], args) == -1) {
            perror("MiniShell");
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // Error forking
        perror("MiniShell");
    } else {
        // Parent process
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return 1;
}

int miniShellAusfuehren(char **args, char** line) {
    int i;

    if (args[0] == NULL) {
        // An empty command was entered.
        return 1;
    }

    //loop durch das array von eingebautenbefehlen, und gucken ob es das ist.
    for (i = 0; i < anzahlEingebauterBefehle(); i++) {
        if (strcmp(args[0], eingebauteStrings[i]) == 0) {
            return (*eingebauteBefehle[i])(args);
        }
    }
    //wenn nicht, dann minishellStarten
    return miniShellStarten(args);
}

int miniShellPipe(char** args1, char** args2){
    //filedescriptor ->
    //fd[0] ist read
    //fd[1] ist write
    int fd[2];
    pid_t p1, p2, wpid;
    int status;

    if(pipe(fd)<0){
        perror("pipe hat ein fehler");
        return 1;
    }
    //erster process
    p1 = fork();
    if(p1<0){
        perror("fork1 hat ein fehler");
        return 1;
    }
    if(p1 == 0){
        //printf("erster cmd ausführen");

        close(STDOUT_FILENO);
        dup(fd[1]);
        close(fd[0]);
        close(fd[1]);


        if (execvp(args1[0], args1) < 0) {
            printf("\ncmd 1 konnte nicht ausgeführt werden");
            exit(0);

        }
    } else{
        //noch ein fork machen!
        //printf("2ter cmd ausführen");
        p2 = fork();
        if(p2<0){
            perror("fork2 hat ein fehler");
            return 1;
        }
        if (p2 == 0) {
            close(STDIN_FILENO);
            dup(fd[0]);
            close(fd[0]);
            close(fd[1]);

            if (execvp(args2[0], args2) < 0) {
                printf("\ncmd 2 konnte nicht ausgeführt werden");
                exit(0);
            }
        }
    }
    close(fd[0]);
    close(fd[1]);
    wait(0);
    wait(0);
    return 0;

}

char** parsePipe(char* str){

    char** strpiped = static_cast<char **>(malloc(BUFFERSIZE * sizeof(char *)));;
    int i;
    for (i = 0; i < 2; i++) {
        strpiped[i] = strsep(&str, "|");
        if (strpiped[i] == NULL)
            break;
    }


    return strpiped;
}

int countNumberOfPipes(char* line){
    int count = 0;
    std::string lein(line);
    std::string target = "|";
    std::string::size_type pos = 0;

    while ((pos = lein.find(target, pos )) != std::string::npos) {
        ++ count;
        pos += target.length();
    }

    return count;
}

char **miniShellSplitter(char *line) {
    int bufsize = BUFFERSIZE, position = 0;
    char **tokens = static_cast<char **>(malloc(bufsize * sizeof(char *)));
    char *token;

    if (!tokens) {
        fprintf(stderr, "MiniShell: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, DELIM);
    while (token != NULL) {
        tokens[position] = token;
        position++;

        if (position >= bufsize) {
            bufsize += BUFFERSIZE;
            tokens = static_cast<char **>(realloc(tokens, bufsize * sizeof(char *)));

            if (!tokens) {
                fprintf(stderr, "MiniShell: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, DELIM);
        token = envVerarbeiten(token);
    }
    tokens[position] = NULL;
    return tokens;
}

char* envVerarbeiten(char * token){
    if(token!=NULL){
        if(token[0]=='$'){
            char* help =getenv(token+1);
            if (help!=NULL){
                token=help;
            }
        }
    }
    return token;
}

void miniShellLoop(void) {
    char *line;
    char **args;
    int status;

    char** halvedLine;
    char* line1;
    char* line2;
    char** args1;
    char** args2;

    do {
        printUserDir();
        printf(" ");
        line = miniShellEingabe();
        //printf(line);
        //if das is son piping, dann args1 und 2
        if(countNumberOfPipes(line)==0){
            args = miniShellSplitter(line);
            status = miniShellAusfuehren(args, (char**)line);
            free(args);
        }else if(countNumberOfPipes(line)==1){
            halvedLine = parsePipe(line);
            line1 = *halvedLine;
            line2 = *(halvedLine + 1);

            args1 = miniShellSplitter(line1);
            args2 = miniShellSplitter(line2);
            miniShellPipe(args1,args2);

            free(halvedLine);
            free(args1);
            free(args2);
        }

        free(line);
    } while (status);
}

int main() {

    miniShellLoop();

    return EXIT_SUCCESS;
}


/*code graveyard!!!!
 *
int boi(char** args1, char** args2){
    int des_p[2];
    if(pipe(des_p) == -1) {
        perror("Pipe failed");
        exit(1);
    }

    if(fork() == 0)            //first fork
    {
        close(STDOUT_FILENO);  //closing stdout
        dup(des_p[1]);         //replacing stdout with pipe write
        close(des_p[0]);       //closing pipe read
        close(des_p[1]);

         char* prog1[] = { "ls", "-l", nullptr};
        execvp(args1[0], args1);
        perror("execvp of ls failed");
        exit(1);
    }

    if(fork() == 0)            //creating 2nd child
    {
        close(STDIN_FILENO);   //closing stdin
        dup(des_p[0]);         //replacing stdin with pipe read
        close(des_p[1]);       //closing pipe write
        close(des_p[0]);

        char* prog2[] = { "wc", "-l", nullptr};
        execvp(args2[0], args2);
        perror("execvp of wc failed");
        exit(1);
    }
//            if (execvp(prog2[0], prog2) < 0) {

    close(des_p[0]);
    close(des_p[1]);
    wait(0);
    wait(0);
    return 0;
}

 //boi();
    //
    /*
    char testinput[14] = {"ls -al | wc"};
    printf("%i\n", countNumberOfPipes(testinput));
    char** tokens = parsePipe(testinput);

    char* argseins = *tokens;
    char* argszwei = *(tokens+1);

    printf("%s\n",argseins);
    printf(argszwei);
     */

/*
while (*tokens != NULL) {
    printf("%s lmao", *tokens);
    tokens++;
}*/


// Perform any shutdown/cleanup.
// Quelle: https://brennan.io/2015/01/16/write-a-shell-in-c/
