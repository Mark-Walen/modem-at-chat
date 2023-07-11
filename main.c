#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <sys/stat.h>
#include <syslog.h>
#include <stdarg.h>
#include <signal.h>

#ifndef TERMIO
#undef	TERMIOS
#define TERMIOS
#endif

#ifdef TERMIO
#include <termio.h>
#endif
#ifdef TERMIOS
#include <termios.h>
#endif

#define	STR_LEN	1024

#ifndef SIGTYPE
#define SIGTYPE void
#endif

#ifndef O_NONBLOCK
#define O_NONBLOCK	O_NDELAY
#endif

#define MAX_PORT_LENGTH 20
#define MAX_FILE_LENGTH 64
#define	DEFAULT_CHAT_TIMEOUT	45

#ifdef TERMIO
#define term_parms struct termio
#define get_term_param(param) ioctl(0, TCGETA, param)
#define set_term_param(param) ioctl(0, TCSETA, param)
struct termio saved_tty_parameters;
#endif

#ifdef TERMIOS
#define term_parms struct termios
#define get_term_param(param) tcgetattr(0, param)
#define set_term_param(param) tcsetattr(0, TCSANOW, param)
struct termios saved_tty_parameters;
#endif

int verbose         = 0;
int fd              = -1;
int timeout         = DEFAULT_CHAT_TIMEOUT;
int to_log          = 1;
int to_stderr       = 0;
FILE *chat_fp       = NULL;
FILE *report_fp     = NULL;
int n_reports = 0, report_next = 0, report_gathering = 0 ; 
char *program_name;
char report_buffer[4096];
char report_file[MAX_FILE_LENGTH] = "";

int have_tty_parameters = 0;
void init(void);
void set_tty_parameters(void);
int vfmtmsg(char *buf, int buflen, const char *fmt, va_list args);
// void terminate(int status);

/*
 *	We got an error parsing the command line.
 */
void usage(FILE *__restrict __stream)
{
    fprintf(__stream, "\
Usage: %s [-e] [-E] [-v] [-V] [-t timeout] [-r report-file]\n\
     [-p serial port] [-U phone-number2] {-f chat-file | chat-script}\n", program_name);
    exit(1);
}

char line[1024];

/*
 * Send a message to syslog and/or stderr.
 */
void msgf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);

    vfmtmsg(line, sizeof(line), fmt, args);
    if (to_log)
	syslog(LOG_INFO, "%s", line);
    if (to_stderr)
	fprintf(stderr, "%s\n", line);
    va_end(args);
}

/*
 *	Print an error message and terminate.
 */

void fatal(int code, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);

    vfmtmsg(line, sizeof(line), fmt, args);
    if (to_log)
	syslog(LOG_ERR, "%s", line);
    if (to_stderr)
	fprintf(stderr, "%s\n", line);
    va_end(args);
    // terminate(code);
}

int alarmed = 0;
int alarmsig = 0;

SIGTYPE sigalrm(int signo)
{
    int flags;

    alarm(1);
    alarmed = 1;
    alarmsig = 1;
}

const char *fatalsig = NULL;

SIGTYPE sigint(int signo)
{
    fatalsig = "SIGINT";
}

SIGTYPE sigterm(int signo)
{
    fatalsig = "SIGTERM";
}

SIGTYPE sighup(int signo)
{
    fatalsig = "SIGHUP";
}

void checksigs(void)
{
    int err;
    const char *signame;

    if (fatalsig)
    {
        signame = fatalsig;
        fatalsig = NULL;
        alarmsig = 0;
        fatal(2, signame);
    }
    if (alarmsig && verbose)
    {
        err = errno;
        msgf("alarm");
        errno = err;
        alarmsig = 0;
    }
}

void handleCtrlC(int signum)
{
    if (fd >= 0) close(fd);
    if (chat_fp != NULL) fclose(chat_fp);
    if (report_fp != NULL) fclose(report_fp);
    printf("Exiting...\n");
    exit(signum);
}

void init(void)
{
    set_tty_parameters();
}

void set_tty_parameters(void)
{
#if defined(get_term_param)
    term_parms t;

    if (get_term_param (&t) < 0)
	fatal(2, "Can't get terminal parameters: %m");

    saved_tty_parameters = t;
    have_tty_parameters  = 1;

    t.c_iflag     |= IGNBRK | ISTRIP | IGNPAR;
    t.c_oflag      = 0;
    t.c_lflag      = 0;
    t.c_cc[VERASE] =
    t.c_cc[VKILL]  = 0;
    t.c_cc[VMIN]   = 1;
    t.c_cc[VTIME]  = 0;

    if (set_term_param (&t) < 0)
	fatal(2, "Can't set terminal parameters: %m");
#endif
}

void chat_input()
{
    // Read and discard any initial data from the modem
    char discardBuffer[256];
    ssize_t bytesRead = read(fd, discardBuffer, sizeof(discardBuffer));

    // Main loop for AT command chatter
    char command[256];
    char response[256];
    while (1) {
        printf("Enter AT command: ");
        fgets(command, sizeof(command), stdin);
        strtok(command, "\n"); // Remove trailing newline
        printf("%s\r\n", command);

        // Send the command to the modem
        write(fd, command, strlen(command));
        write(fd, "\r\n", 2); // Append carriage return and newline
        sleep(1);

        // Read response from modem
        bytesRead = read(fd, response, sizeof(response) - 1);
        if (bytesRead >= 0) {
            response[bytesRead] = '\0';
            printf("Response: %s\n", response);
        } else {
            perror("Error reading from modem device");
        }
    }
}

void do_file(FILE *__restrict __stream)
{
    char buffer[256];
    char command[256];
    // Read and discard any initial data from the modem
    char discardBuffer[256];
    ssize_t bytesRead = read(fd, discardBuffer, sizeof(discardBuffer));

    while (fgets(buffer, sizeof(buffer), __stream) != NULL)
    {
        // handle buffer;
    }
    
}

/*
 * chat [ -v ] [ -E ] [ -T number ] [ -U number ] [ -t timeout ] [ -f chat-file ] \
 * [ -r report-file ] \
 *		[...[[expect[-say[-expect...]] say expect[-say[-expect]] ...]]]
 *
 *	Perform a UUCP-dialer-like chat script on stdin and stdout.
 */
int main(int argc, char* argv[]) {
    // signal(SIGINT, handleCtrlC);
    struct termios options;
    char port[MAX_PORT_LENGTH] = "/dev/ttyUSB1";
    char chat_file[MAX_FILE_LENGTH] = "";
    char timeout_s[128];

    // Parse command-line arguments
    int option;
    while ((option = getopt(argc, argv, ":f:p:t:r:v:S:s:")) != -1) {
        switch (option)
        {
        case 'f':
            strncpy(chat_file, optarg, sizeof(chat_file) - 1);
            break;
        case 'p':
            strncpy(port, optarg, sizeof(port) - 1);
            break;
        case 't':
            strncpy(timeout_s, optarg, sizeof(port) - 1);
            timeout = atoi(timeout_s);
        case 'r':
            strncpy(report_file, optarg, sizeof(report_file) - 1);

            if (report_fp != NULL)
                close(report_fp);
            report_fp = fopen(report_file, "a");
            if (report_fp != NULL)
            {
                if (verbose) fprintf(report_fp, "Opening \"%s\"...\n", report_file);
            }
        case 'S':
            to_log = 0;
            break;
        case 'V':
            printf("chat version 0.1.0");
            return 0;
        case 'v':
            verbose++;
            break;
        default:
            usage(stderr);
            return 1;
        }
    }

    if (report_fp == NULL) report_fp = stderr;
    if (to_log) 
    {
        openlog("chat", LOG_PID | LOG_NDELAY, LOG_LOCAL2);
        if (verbose)
            setlogmask(LOG_UPTO(LOG_INFO));
        else
            setlogmask(LOG_UPTO(LOG_WARNING));
    }

    // init signal


    // Open the modem device (e.g., /dev/ttyUSB2)
    printf("Opening port: %s\r\n", port);
    fd = open(port, O_RDWR | O_NONBLOCK | O_NOCTTY);
    if (fd < 0) {
        printf("Error: %s\n", strerror(errno));
        return -1;
    }
    printf("comm port.\n\r");

    // Get the current terminal settings
    if (tcgetattr(fd, &options) != 0)
    {
        perror("error from tcgetattr");
        return 1;
    }

    options.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;

    // Set the new terminal settings
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &options);

    // Read and discard any initial data from the modem
    char discardBuffer[256];
    ssize_t bytesRead = read(fd, discardBuffer, sizeof(discardBuffer));

    // Main loop for AT command chatter
    char command[256];
    char response[256];

    if (chat_file != NULL)
    {
        chat_fp = open(chat_file, O_RDONLY);
    }

    if (chat_fp < 0)
    {
        chat_input();
    }
    else
    {
        do_file(chat_fp);
    }

    // Close the modem device
    close(fd);

    return 0;
}

/*
 * vfmtmsg - format a message into a buffer.  Like vsprintf except we
 * also specify the length of the output buffer, and we handle the
 * %m (error message) format.
 * Doesn't do floating-point formats.
 * Returns the number of chars put into buf.
 */
#define OUTCHAR(c)	(buflen > 0? (--buflen, *buf++ = (c)): 0)

int vfmtmsg(char *buf, int buflen, const char *fmt, va_list args)
{
    int c, i, n;
    int width, prec, fillch;
    int base, len, neg, quoted;
    unsigned long val = 0;
    char *str, *buf0;
    const char *f;
    unsigned char *p;
    char num[32];
    static char hexchars[] = "0123456789abcdef";

    buf0 = buf;
    --buflen;
    while (buflen > 0)
    {
        for (f = fmt; *f != '%' && *f != 0; ++f)
            ;
        if (f > fmt)
        {
            len = f - fmt;
            if (len > buflen)
                len = buflen;
            memcpy(buf, fmt, len);
            buf += len;
            buflen -= len;
            fmt = f;
        }
        if (*fmt == 0)
            break;
        c = *++fmt;
        width = prec = 0;
        fillch = ' ';
        if (c == '0')
        {
            fillch = '0';
            c = *++fmt;
        }
        if (c == '*')
        {
            width = va_arg(args, int);
            c = *++fmt;
        }
        else
        {
            while (isdigit(c))
            {
                width = width * 10 + c - '0';
                c = *++fmt;
            }
        }
        if (c == '.')
        {
            c = *++fmt;
            if (c == '*')
            {
                prec = va_arg(args, int);
                c = *++fmt;
            }
            else
            {
                while (isdigit(c))
                {
                    prec = prec * 10 + c - '0';
                    c = *++fmt;
                }
            }
        }
        str = 0;
        base = 0;
        neg = 0;
        ++fmt;
        switch (c)
        {
        case 'd':
            i = va_arg(args, int);
            if (i < 0)
            {
                neg = 1;
                val = -i;
            }
            else
                val = i;
            base = 10;
            break;
        case 'o':
            val = va_arg(args, unsigned int);
            base = 8;
            break;
        case 'x':
            val = va_arg(args, unsigned int);
            base = 16;
            break;
        case 'p':
            val = (unsigned long)va_arg(args, void *);
            base = 16;
            neg = 2;
            break;
        case 's':
            str = va_arg(args, char *);
            break;
        case 'c':
            num[0] = va_arg(args, int);
            num[1] = 0;
            str = num;
            break;
        case 'm':
            str = strerror(errno);
            break;
        case 'v': /* "visible" string */
        case 'q': /* quoted string */
            quoted = c == 'q';
            p = va_arg(args, unsigned char *);
            if (fillch == '0' && prec > 0)
            {
                n = prec;
            }
            else
            {
                n = strlen((char *)p);
                if (prec > 0 && prec < n)
                    n = prec;
            }
            while (n > 0 && buflen > 0)
            {
                c = *p++;
                --n;
                if (!quoted && c >= 0x80)
                {
                    OUTCHAR('M');
                    OUTCHAR('-');
                    c -= 0x80;
                }
                if (quoted && (c == '"' || c == '\\'))
                    OUTCHAR('\\');
                if (c < 0x20 || (0x7f <= c && c < 0xa0))
                {
                    if (quoted)
                    {
                        OUTCHAR('\\');
                        switch (c)
                        {
                        case '\t':
                            OUTCHAR('t');
                            break;
                        case '\n':
                            OUTCHAR('n');
                            break;
                        case '\b':
                            OUTCHAR('b');
                            break;
                        case '\f':
                            OUTCHAR('f');
                            break;
                        default:
                            OUTCHAR('x');
                            OUTCHAR(hexchars[c >> 4]);
                            OUTCHAR(hexchars[c & 0xf]);
                        }
                    }
                    else
                    {
                        if (c == '\t')
                            OUTCHAR(c);
                        else
                        {
                            OUTCHAR('^');
                            OUTCHAR(c ^ 0x40);
                        }
                    }
                }
                else
                    OUTCHAR(c);
            }
            continue;
        default:
            *buf++ = '%';
            if (c != '%')
                --fmt; /* so %z outputs %z etc. */
            --buflen;
            continue;
        }
        if (base != 0)
        {
            str = num + sizeof(num);
            *--str = 0;
            while (str > num + neg)
            {
                *--str = hexchars[val % base];
                val = val / base;
                if (--prec <= 0 && val == 0)
                    break;
            }
            switch (neg)
            {
            case 1:
                *--str = '-';
                break;
            case 2:
                *--str = 'x';
                *--str = '0';
                break;
            }
            len = num + sizeof(num) - 1 - str;
        }
        else
        {
            len = strlen(str);
            if (prec > 0 && len > prec)
                len = prec;
        }
        if (width > 0)
        {
            if (width > buflen)
                width = buflen;
            if ((n = width - len) > 0)
            {
                buflen -= n;
                for (; n > 0; --n)
                    *buf++ = fillch;
            }
        }
        if (len > buflen)
            len = buflen;
        memcpy(buf, str, len);
        buf += len;
        buflen -= len;
    }
    *buf = 0;
    return buf - buf0;
}
