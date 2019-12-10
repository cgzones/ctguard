#include "daemon_helper.hpp"

#include "logger.hpp"
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

void daemonize(const std::string & pidfile, const std::string & user, const std::string & group, sighandler_t signal_handler, bool foreground)
{
    if (!foreground) {
        {
            const pid_t i = ::fork();
            if (i < 0) {
                FILE_LOG(ctguard::libs::log_level::ERROR) << "Can not fork: " << ::strerror(errno);
                exit(1);
            }
            if (i > 0) {
                exit(0);
            }
        }

        if (::setsid() < 0) {
            FILE_LOG(ctguard::libs::log_level::ERROR) << "Can not setsid: " << ::strerror(errno);
            exit(1);
        }
    }

    ::signal(SIGCHLD, SIG_IGN);
    ::signal(SIGTSTP, SIG_IGN);
    ::signal(SIGTTOU, SIG_IGN);
    ::signal(SIGTTIN, SIG_IGN);
    ::signal(SIGHUP, signal_handler);
    ::signal(SIGINT, signal_handler);
    ::signal(SIGTERM, signal_handler);

    if (!foreground) {
        {
            const pid_t i = ::fork();
            if (i < 0) {
                FILE_LOG(ctguard::libs::log_level::ERROR) << "Can not fork: " << ::strerror(errno);
                exit(1);
            }
            if (i > 0) {
                exit(0);
            }
        }

        {
            const int fd = ::open("/dev/null", O_RDWR);
            if (fd < 0) {
                FILE_LOG(ctguard::libs::log_level::ERROR) << "Can not open /dev/null: " << ::strerror(errno);
                exit(1);
            }

            ::dup2(fd, 0);
            ::dup2(fd, 1);
            ::dup2(fd, 2);
            ::close(fd);
        }
    }

    ::umask(027);

    /*if (::chdir("/") == -1) {
        FILE_LOG(ctguard::libs::log_level::ERROR) << "Can not chdir to /: " <<
    ::strerror(errno); exit (1);
    }*/

    {
        const int pid_fd = open(pidfile.c_str(), O_WRONLY | O_CREAT, 0640);
        if (pid_fd < 0) {
            FILE_LOG(ctguard::libs::log_level::ERROR) << "Can not open '" << pidfile << "': " << ::strerror(errno);
            exit(1);
        }

        struct flock fl;
        fl.l_type = F_WRLCK;
        fl.l_whence = SEEK_SET;
        fl.l_start = 0;
        fl.l_len = 0;
        fl.l_pid = getpid();
        if (::fcntl(pid_fd, F_SETLK, &fl) < 0) {
            FILE_LOG(ctguard::libs::log_level::ERROR) << "Can not lock '" << pidfile << "': " << ::strerror(errno);
            FILE_LOG(ctguard::libs::log_level::ERROR) << "Is another instance already running?";
            exit(1);
        }

        constexpr size_t pid_size = 8;
        char pid[pid_size];
        ::snprintf(pid, pid_size, "%u\n", getpid());
        if (::write(pid_fd, pid, ::strlen(pid)) == -1) {
            FILE_LOG(ctguard::libs::log_level::ERROR) << "Can not write pid to '" << pidfile << "': " << ::strerror(errno);
            exit(1);
        }

        //::close(pid_fd); do not close to hold lock
    }

    if (!group.empty()) {
        // cppcheck-suppress getgrnamCalled
        const struct group * grp = ::getgrnam(group.c_str());
        if (!grp) {
            FILE_LOG(ctguard::libs::log_level::ERROR) << "Can not find group '" << group << "': " << ::strerror(errno);
            exit(1);
        }

        if (::setgid(grp->gr_gid) < 0) {
            FILE_LOG(ctguard::libs::log_level::ERROR) << "Can not change to group '" << group << "': " << ::strerror(errno);
            exit(1);
        }
    }

    if (!user.empty()) {
        // cppcheck-suppress getpwnamCalled
        const struct passwd * pw = ::getpwnam(user.c_str());
        if (!pw) {
            FILE_LOG(ctguard::libs::log_level::ERROR) << "Can not find user '" << user << "': " << ::strerror(errno);
            exit(1);
        }

        if (::setuid(pw->pw_uid) < 0) {
            FILE_LOG(ctguard::libs::log_level::ERROR) << "Can not change to user '" << user << "': " << ::strerror(errno);
            exit(1);
        }
    }
}
