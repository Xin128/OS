/* ---------------------------------
 * Lara Goxhaj		* 260574574
 * COMP 310/ECSE 427	* assn 1
 * --------------------------------- */

#define _POSIX_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <sys/types.h>

#define MAX_ARGS 20
#define MAX_LINE 100
#define MAX_HISTORY 10

// struct for holding command history
typedef struct history {
    char lineptr[MAX_LINE+1];
    int count;
} history;

// struct for holding background procs
typedef struct proc {
    pid_t pid;
    char *command;
    int count;
    struct proc *next;
} proc;

int getcmd(char *prompt, char *args[], int *background);
void freecmd(char *args[]);
void cleanUp();
int isBuiltIn(char *args[], int cnt);
void histHandler(int signal);
//void procHandler(int signal);
void push(proc *headP, pid_t cpid, char *line);
pid_t find(proc **headP, pid_t bgPid);
//pid_t findByPID(proc **headP, pid_t bgPid);
pid_t removeProc(proc *current);

history hist[MAX_HISTORY];
proc *headP;		// background proc
int pos, jobs;		// keep track of position in history and proc list


int main() {
    char *args[MAX_ARGS];
    int bg, status;
    pid_t pid, ppid = getpid();
    pos = 0;
    jobs = 0;

    headP = malloc(sizeof(proc));
    if (headP == NULL)
	return -1;

    char *parLine = "shell";
    push(headP, 0, parLine);	// set head proc to parent shell for convenience

    while (1) {
	signal(SIGUSR1, histHandler);
//	signal(SIGCHLD, procHandler);

	int cnt = getcmd("\n>>  ", args, &bg);

	if (cnt < 0)
	    continue;

	status = isBuiltIn(args, cnt);
	if (status == 0)
	    continue;
	else if (status == 1)
	    if (!isBuiltIn(args, cnt))
		continue;

	pid = fork();

	if (pid == -1)
	    perror("fork");

	// child
	else if (pid == 0) {
	    if (execvp(args[0], args) < 0) {
	        printf("exec failure: %s\n", strerror(errno));
	        kill(ppid, SIGUSR1);
	        exit(EXIT_FAILURE);
	    } else
		exit(EXIT_SUCCESS);
	}

	// parent
	// note: background processes are implemented such that calling 'r x' on a process implemented as a background process
	// requires an additional '&' to run that process as a background process as well
	else {
	    if (bg == 1) {
		bg = 0;
		push(headP, pid, hist[(pos-1)%(MAX_HISTORY+1)].lineptr);
		freecmd(args);
		kill(ppid, SIGCONT);
	    } else {
		status=0;
                waitpid(pid, &status, WUNTRACED);
                if (status == -1)
	            printf("An error occured in waiting for child proccess %d\n", pid);
	    }
        }
    }
    return 0;
}




int getcmd(char *prompt, char *args[], int *background) {
    int length, j, i=0, len=MAX_LINE;
    char *token, *loc, *line = NULL;
    size_t linecap = 0;

    freecmd(args);

    printf("%s", prompt);
    length = getline(&line, &linecap, stdin);	// will realloc if linecap too small

    if (length <= 1)
	return -1;

    // Check if background is specified..
    if ((loc = index(line, '&')) != NULL) {
        *background = 1;
        *loc = ' ';
    } else
        *background = 0;

    if (MAX_LINE > strlen(line)+1)
	len = strlen(line)+1;
    memcpy(hist[pos%(MAX_HISTORY+1)].lineptr, line, len);	// line segmented by '\0', must copy for later use

    if ((loc = index(hist[pos%(MAX_HISTORY+1)].lineptr, '\n')) != NULL)
	*loc = '\0';
    
    // if no delimeter found, token taken to be entire &line, and &line made NULL
    while ((token = strsep(&line, " \t\n")) != NULL) {
        for (j=0; j < strlen(token); j++)
            if (token[j] <= 32)  token[j] = '\0';
        if (strlen(token) > 0) {
            args[i] = token;
	    i++;
	}
    }

    hist[pos%(MAX_HISTORY+1)].count = i;
    pos++;
    return i;
}


void getcmdFromHist(char *args[], char *line) {
    int j, i=0, len=MAX_LINE;
    char *token;

    freecmd(args);

    if (MAX_LINE > strlen(line)+1)
	len = strlen(line)+1;
    memcpy(hist[(pos-1)%(MAX_HISTORY+1)].lineptr, line, len);

    // if no delimiter found, token taken to be entire &line, and &line made NULL
    while ((token = strsep(&line, " \t\n")) != NULL) {
        for (j=0; j < strlen(token); j++)
            if (token[j] <= 32)  token[j] = '\0';
        if (strlen(token) > 0) {
            args[i] = token;
	    i++;
	}
    }

    hist[(pos-1)%(MAX_HISTORY+1)].count = i;
    return;
}


void cleanUp() {
    while (jobs > 1)
	find(&headP, 1);
    exit(0);
}


void freecmd(char *args[]) {
    int i=0;
    for (i; i<MAX_ARGS; i++)
	args[i] = '\0';
}


// checks if command is built in; if so, executes it
int isBuiltIn(char *args[], int cnt) {
    int i, id;
    int stat=0, exists=0, pStatus=0, len=0;
    char cwd[pathconf(".", _PC_PATH_MAX)];

    // exit
    if (strcmp(args[0], "exit") == 0)
        cleanUp();

    // cd
    else if (strcmp(args[0], "cd") == 0) {
        if (cnt == 1)
            args[1] = getenv("HOME");
        pStatus = chdir(args[1]);
        if (pStatus == -1) {
            printf("cd: \'%s\': No such file or directory\n", args[1]);
	    pos--;
	}
    }

    // pwd
    else if (strcmp(args[0], "pwd") == 0) {
	if (cnt != 1)
	    printf("pwd: Too many args");
	else {
	    if (getcwd(cwd, pathconf(".", _PC_PATH_MAX)) == cwd)
		printf("%s\n", cwd);
	    else {
		printf("pwd: Error getting pwd");
		pos--;
	    }
	}
    }

    // print history
    else if (strcmp(args[0], "history") == 0) {
	for (i=1; i<MAX_HISTORY+1; i++) {
	    if ((pos-i) < 1)
		break;
	    printf("command %d: %s\n", (pos-i), hist[(pos-i-1)%(MAX_HISTORY+1)].lineptr);
	}
    }

    // access command in history
    else if (strcmp(args[0], "r") == 0) {

        if (cnt > 2) {
	    printf("r: Cannot have more than 1 argument\n");
	    pos--;
	} else if ((cnt == 2) && (strlen(args[1]) != 1)) {
	    printf("r: Faulty argument\n");
	    pos--;
	} else if (pos < 2) {
	    printf("r: No recent commands\n");
	    pos--;
	} else {
	    if (cnt == 1)
		exists = pos-2;
	    else {
		exists = -1;
		for (i=2; i<=MAX_HISTORY+1; i++) {
		    if (strncmp(hist[(pos-i)%(MAX_HISTORY+1)].lineptr, args[1], 1) == 0) {
			exists = pos-i;
			break;
		    }
		}
		if (exists < 0) {
		    printf("r: No recent command starting with \'%s\'\n", args[1]);
		    pos--;
		    return stat;
		}
	    }

	    printf("running command %d: %s\n", (exists+1), hist[exists%(MAX_HISTORY+1)].lineptr);
	    getcmdFromHist(args, hist[exists%(MAX_HISTORY+1)].lineptr);
	    stat = 1;	// to run thru again
	}
    }

    // fg
    else if (strcmp(args[0], "fg") == 0) {

        if (cnt != 2) {
	    printf("fg: Faulty argument\n");
	    pos--;
	} else if (jobs == 1) {
	    printf("fg: No current background jobs\n");
	    pos--;
	} else {
	    if (sscanf(args[1], "%d", &id) != 1) {
		printf("fg: Faulty argument\n");
		pos--;
	    } else {
		pid_t cpid = find(&headP, id);
		if (!(cpid == -1)) {
		    kill(cpid, SIGCONT);
		    waitpid(cpid, &pStatus, WUNTRACED);
		    if (pStatus == -1)
			printf("An error occured in waiting for child proccess %d\n", id);
		}
	    }
	}

    }

    // jobs
    else if (strcmp(args[0], "jobs") == 0) {
	proc *current = headP->next;

	if (jobs != 1) {
	    int idx = 0;
	    while (current != NULL) {
		if (current->pid != 0) {
		    pid_t result = waitpid(current->pid, &pStatus, WNOHANG);
		    if (result == 0)
		    	printf("[%d]\t%d\t%s\n", (idx), current->pid, current->command);
		    else
			removeProc(current);
		}
		current = current->next;
		idx++;
	    }
	}
    }

    else
	stat = -1;	// not built-in

    return stat;   
}


// allows overwriting of history in case of failed command or 'r [x]' command
void histHandler(int signal) {
    if (signal == SIGUSR1)
	pos--;
}

// parent has issues, removing processes during print instead
// invokes fn to remove bg process from list when finished executing
/*
void procHandler (int signal)
{
    if (signal == SIGCHLD) {
	pid_t cpid = wait(NULL);
	findByPID(&headP, cpid);

    }
}
*/


// push proc onto linked list of procedures
void push(proc *headP, pid_t cpid, char *line) {
    proc *new_proc;
    new_proc = malloc(sizeof(proc));
    new_proc->pid = cpid;
    new_proc->command = line;
    new_proc->count = hist[pos%(MAX_HISTORY+1)].count;
    new_proc->next = NULL;

    proc *current = headP;
    while (current->next != NULL)
        current = current->next;

    current->next = new_proc;
    jobs++;
}


// remove proc from linked list of procedures based on generic id
pid_t find(proc **headP, int id) {
    if (jobs == 1) {
	printf("No current background jobs\n");
	return (pid_t)(-1);
    }

    int p = 0;
    proc *current = *headP;

    while (current != NULL) {
	if (p == id)
	    return removeProc(current);
	else {
	    current = current->next;
	    p++;
	}
    }

    printf("No process with id %d exists\n", id);
    return (pid_t)(-1);
}

// not using, only procHandler
// remove procedure by PID (for automatic removal when process ends)
/*
pid_t findByPID(proc **headP, pid_t cpid)
{
    proc *current = *headP;

    while (current != NULL) {
	if (current->pid == cpid)
	    return removeProc(current);
	else
	    current = current->next;
    }

    return (pid_t)(-1);
}
*/


// remove procedure from list
pid_t removeProc(proc *current) {
    proc *temp_proc = NULL;
    int lineLen = MAX_LINE;
    if (MAX_LINE > strlen(current->command)+1)
	lineLen = strlen(current->command)+1;

    memcpy(hist[(pos-1)%(MAX_HISTORY+1)].lineptr, (current->command), lineLen);
    hist[(pos-1)%(MAX_HISTORY+1)].count = current->count;
    pid_t cpid = current->pid;

    if (!(current->next == NULL)) {
	temp_proc = current->next;
	current->next = temp_proc->next;
	free(temp_proc);
    }

    jobs--;
    return cpid;
}
