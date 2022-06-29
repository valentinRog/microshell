#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define N 1000
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

void close_pipe( int fds[] ) {
    if ( fds ) {
        close( fds[0] );
        close( fds[1] );
    }
}

void fatal( int *i_pipe, int *o_pipe ) {
    close_pipe( i_pipe );
    close_pipe( o_pipe );
    d_putstr( STDERR_FILENO, "error: fatal\n" );
    exit( EXIT_FAILURE );
}

void bi_cd( char **cmds ) {
    if ( !cmds[1] || cmds[2] ) {
        d_putstr( STDERR_FILENO, "error: cd: bad arguments\n" );
    } else if ( chdir( cmds[1] ) ) {
        d_putstr( STDERR_FILENO, "error: cd: cannot change directory to " );
        d_putstr( STDERR_FILENO, cmds[1] );
        d_putstr( STDERR_FILENO, "\n" );
    }
}

void executor( char *cmd, char **cmds, int *i_pipe, int *o_pipe, char **ep ) {
    if ( !strcmp( cmd, "cd" ) ) {
        close_pipe( i_pipe );
        return bi_cd( cmds );
    }
    int pid = fork();
    if ( pid < 0 ) { fatal( i_pipe, o_pipe ); }
    if ( !pid ) {
        if ( i_pipe && dup2( i_pipe[PIPE_READ], STDIN_FILENO ) < 0 ) {
            fatal( i_pipe, o_pipe );
        }
        close_pipe( i_pipe );
        if ( o_pipe && dup2( o_pipe[PIPE_WRITE], STDOUT_FILENO ) < 0 ) {
            fatal( i_pipe, o_pipe );
        }
        close_pipe( o_pipe );
        execve( cmd, cmds, ep );
        d_putstr( STDERR_FILENO, "error: cannot execute " );
        d_putstr( STDERR_FILENO, cmd );
        d_putstr( STDERR_FILENO, "\n" );
        exit( EXIT_FAILURE );
    }
    close_pipe( i_pipe );
    int exit_code;
    waitpid( pid, &exit_code, 0 );
    if ( exit_code ) {
        close_pipe( o_pipe );
        exit( EXIT_FAILURE );
    }
}

int main( int ac, char **av, char **ep ) {
    if ( ac < 2 ) { return 0; }
    char *cmd = av[1];
    char *cmds[N];
    int  *i_pipe = NULL;
    int   fds[2];
    set_cmds( cmds );
    for ( int i = 1; i < ac; i++ ) {
        if ( !( strcmp( av[i], ";" ) ) ) {
            executor( cmd, cmds, i_pipe, NULL, ep );
            set_cmds( cmds );
            cmd    = i + 1 < ac ? av[i + 1] : NULL;
            i_pipe = NULL;
        } else if ( !( strcmp( av[i], "|" ) ) ) {
            int o_pipe[2];
            if ( pipe( o_pipe ) < 0 ) { fatal( i_pipe, NULL ); }
            executor( cmd, cmds, i_pipe, o_pipe, ep );
            set_cmds( cmds );
            cmd    = i + 1 < ac ? av[i + 1] : NULL;
            fds[0] = o_pipe[0];
            fds[1] = o_pipe[1];
            i_pipe = fds;
        } else {
            add_cmd( cmds, av[i] );
        }
    }
    if ( cmd ) { executor( cmd, cmds, i_pipe, NULL, ep ); }
}
