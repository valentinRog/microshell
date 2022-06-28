#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define N 100
#define PIPE_READ 0
#define PIPE_WRITE 1

size_t str_len( const char *s ) {
    if ( s && *s ) { return 1 + str_len( s + 1 ); }
    return 0;
}

void d_putstr( int fd, const char *s ) {
    if ( s ) { write( fd, s, str_len( s ) ); }
}

void set_cmds( char *cmds[] ) {
    for ( int i = 0; i < N; i++ ) { cmds[i] = NULL; }
}

void add_cmd( char *cmds[], char *cmd ) {
    for ( int i = 0; i < N; i++ ) {
        if ( !cmds[i] ) {
            cmds[i] = cmd;
            break;
        }
    }
}

int cmds_len( char **cmds ) {
    for ( int i = 0; i < N; i++ ) {
        if ( !cmds[i] ) { return i; }
    }
    return N;
}

void fatal() {
    d_putstr( STDERR_FILENO, "error: fatal\n" );
    exit( EXIT_FAILURE );
}

void close_pipe( int fds[] ) {
    if ( fds ) {
        close( fds[0] );
        close( fds[1] );
    }
}

void bi_cd( char **cmds ) {
    if ( cmds_len( cmds ) != 2 ) {
        d_putstr( STDERR_FILENO, "error: cd: bad arguments\n" );
        exit( EXIT_FAILURE );
    }
    if ( chdir( cmds[1] ) ) {
        d_putstr( STDERR_FILENO, "error: cd: cannot change directory to " );
        d_putstr( STDERR_FILENO, cmds[1] );
        d_putstr( STDERR_FILENO, "\n" );
        exit( EXIT_FAILURE );
    }
}

void executor( char *cmd, char **cmds, int *i_pipe, int *o_pipe, char **ep ) {
    if ( !strcmp( cmd, "cd" ) ) { return bi_cd( cmds ); }
    int pid = fork();
    if ( pid < 0 ) { fatal(); }
    if ( !pid ) {
        if ( i_pipe && dup2( i_pipe[PIPE_READ], STDIN_FILENO ) < 0 ) {
            fatal();
        }
        close_pipe( i_pipe );
        if ( o_pipe && dup2( o_pipe[PIPE_WRITE], STDOUT_FILENO ) < 0 ) {
            fatal();
        }
        close_pipe( o_pipe );
        execve( cmd, cmds, ep );
        d_putstr( STDERR_FILENO, "error: cannot execute " );
        d_putstr( STDERR_FILENO, cmd );
        d_putstr( STDERR_FILENO, "\n" );
        exit( EXIT_FAILURE );
    }
    close_pipe( i_pipe );
    waitpid( pid, NULL, 0 );
}

int main( int ac, char **av, char **ep ) {
    if ( ac < 2 ) { return 1; }
    char *cmd = av[1];
    char *cmds[N];
    int * i_pipe = NULL;
    int   i_fds[2];
    set_cmds( cmds );
    for ( int i = 1; i < ac; i++ ) {
        if ( !( strcmp( av[i], ";" ) ) ) {
            executor( cmd, cmds, i_pipe, NULL, ep );
            set_cmds( cmds );
            cmd    = av[i + 1];
            i_pipe = NULL;
        } else if ( !( strcmp( av[i], "|" ) ) ) {
            int o_fds[2];
            pipe( o_fds );
            executor( cmd, cmds, i_pipe, o_fds, ep );
            set_cmds( cmds );
            cmd      = av[i + 1];
            i_fds[0] = o_fds[0];
            i_fds[1] = o_fds[1];
            i_pipe   = i_fds;
        } else {
            add_cmd( cmds, av[i] );
        }
    }
    executor( cmd, cmds, i_pipe, NULL, ep );
}
