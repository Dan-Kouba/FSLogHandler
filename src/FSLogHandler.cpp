// FSLogHandler: A log handler that writes logs to the onboard filesystem on Gen 3 and Tracker
// Author:  Dan Kouba <dan.kouba@particle.io>
// Date:    October 2020
// Company: Particle

#include "FSLogHandler.h"
#include <fcntl.h>
#include <sys/stat.h>

FSLogHandler::FSLogHandler(String filename, bool enable_now, LogLevel level, LogCategoryFilters filters) : 
        LogHandler(level, filters) {
    
    // Private var init
    _enabled = enable_now;
    _open = false;
    _fd = -1;
    _path = "/log/" + filename + ".log";
    _bytes_queued = 0;

    // Sane defaults for fsync triggers
    configureFsync(4096, 10);

    // Add this log handler to the system log manager
    LogManager::instance()->addHandler(this);
}

FSLogHandler::~FSLogHandler() {
    LogManager::instance()->removeHandler(this);
    syncAndClose();
}

void FSLogHandler::syncAndClose() {
    if (_open) {
        fsync(_fd);
        close(_fd);
        _bytes_queued = 0;
        _open = false;
        TRACE_PRINTLNF("FSLogHandler()::syncAndClose() File %s closed", _path.c_str());
    }
}

long FSLogHandler::getLogSize() {
    struct stat statbuf;
    if (_open) {
        fstat(_fd, &statbuf);
    } else {
        stat(_path, &statbuf);
    }
    return statbuf.st_size;
}

void FSLogHandler::clearLogs() {
    syncAndClose();
    unlink(getPath().c_str());
    TRACE_PRINTLNF("FSLogHandler()::clearLogs() Close and delete logfile %s", _path.c_str());
}

void FSLogHandler::writeToFile(String message) {
    int result = ::write(_fd, message.c_str(), message.length());
    if (result == -1) {
        DEBUG_PRINTLNF("FSLogHandler::write() FAILED! Errno=%i", errno);
        return;
    }
    _bytes_queued += message.length();
    // fsync(_fd);
    TRACE_PRINTF("FSLogHandler::write() msg=%s", message.c_str());
}

void FSLogHandler::loop() {
    static unsigned int last_ran = System.uptime();
    if (_open) {
        if ( (System.uptime() - last_ran > 10 && _bytes_queued > 0) || (_bytes_queued > 4096) ) {
            DEBUG_PRINTLNF("FSLogHandler::loop() fsync() %u bytes", _bytes_queued);
            fsync(_fd);
            _bytes_queued = 0;
            last_ran = System.uptime();
        }
    }
}

// Open our file if not opened
bool FSLogHandler::fileInit() {
    if (!_open || _fd == -1) {
        createDirIfNecessary("/log");
        _fd = open(_path, O_WRONLY | O_CREAT | O_TRUNC);
        if (_fd == -1) {
            DEBUG_PRINTLNF("FSLogHandler::fileInit() Logfile \"%s\" open FAILED! errno=%i", _path.c_str(), errno);
            _open = false;
            return false;
        }
        TRACE_PRINTLNF("FSLogHandler::fileInit() Logfile \"%s\" opened successfully!", _path.c_str());
        _open = true;
    }
    return true;
}

void FSLogHandler::dump(Print &stream, bool read_from_beginning) {
    int dump_fd = -1;
    static _off_t f_cursor = 0;

    dump_fd = open(_path, O_RDONLY);
    if (!dump_fd) {
        DEBUG_PRINTLNF("Logfile for dump \"%s\" open FAILED! errno=%i", _path.c_str(), errno);
        return;
    }

    if (read_from_beginning) {
        f_cursor = 0;
    } else {
        lseek(dump_fd, f_cursor, SEEK_SET);
    }

    char buf[1024];
    int bytes = 0;
    do {
        bytes = read(dump_fd, buf, sizeof(buf)-1);
        f_cursor += (_off_t)bytes;  // Still increment this in case the next call isn't read from beginning
        buf[bytes] = 0;   // term char
        stream.printf("%s", buf);
        Particle.process();
    } while (bytes);

    close(dump_fd);
}

const char* FSLogHandler::extractFileName(const char *s) {
    const char *s1 = strrchr(s, '/');
    if (s1) {
        return s1 + 1;
    }
    return s;
}

const char* FSLogHandler::extractFuncName(const char *s, size_t *size) {
    const char *s1 = s;
    for (; *s; ++s) {
        if (*s == ' ') {
            s1 = s + 1; // Skip return type
        } else if (*s == '(') {
            break; // Skip argument types
        }
    }
    *size = s - s1;
    return s1;
}

// Copied from StreamLogHandler
void FSLogHandler::logMessage(const char *msg, LogLevel level, const char *category, const LogAttributes &attr) {
    if (!_enabled) {
        return;
    }
    
    if (!fileInit()) {
        TRACE_PRINTLNF("FSLogHandler::logMessage() fileInit() for file %s returned FALSE", _path.c_str());
        return;
    }

    String s;

    // Timestamp
    if (attr.has_time) {
        s.concat(String::format("%010u ", (unsigned)attr.time));
    }

    // Category
    if (category) {
        s.concat("[");
        s.concat(category);
        s.concat("] ");
    }

    // Source file
    if (attr.has_file) {
        s = extractFileName(attr.file); // Strip directory path
        s.concat(s); // File name
        if (attr.has_line) {
            s.concat(":");
            s.concat(String(attr.line)); // Line number
        }
        if (attr.has_function) {
            s.concat(", ");
        } else {
            s.concat(": ");
        }
    }

    // Function name
    if (attr.has_function) {
        size_t n = 0;
        s = extractFuncName(attr.function, &n); // Strip argument and return types
        s.concat(s);
        s.concat("(): ");
    }

    // Level
    s.concat(levelName(level));
    s.concat(": ");

    // Message
    if (msg) {
        s.concat(msg);
    }

    // Additional attributes
    if (attr.has_code || attr.has_details) {
        s.concat(" [");
        // Code
        if (attr.has_code) {
            s.concat(String::format("code = %p" , (intptr_t)attr.code));
        }
        // Details
        if (attr.has_details) {
            if (attr.has_code) {
                s.concat(", ");
            }
            s.concat("details = ");
            s.concat(attr.details);
        }
        s.concat(']');
    }

    s.concat("\n\r");
    writeToFile(s);
}

bool FSLogHandler::createDirIfNecessary(const char *path) {
    struct stat statbuf;

    int result = stat(path, &statbuf);
    if (result == 0) {
        if ((statbuf.st_mode & S_IFDIR) != 0) {
            DEBUG_PRINTLNF("FSLogHandler::createDirIfNecessary(): %s exists and is a directory", path);
            return true;
        }

        DEBUG_PRINTLNF("FSLogHandler::createDirIfNecessary() ERROR: file in the way, deleting %s", path);
        unlink(path);
    }
    else {
        if (errno != ENOENT) {
            // Error other than file does not exist
            DEBUG_PRINTLNF("FSLogHandler::createDirIfNecessary() ERROR: stat filed errno=%i", errno);
            return false;
        }
    }

    // File does not exist (errno == 2)
    result = mkdir(path, 0777);
    if (result == 0) {
        DEBUG_PRINTLNF("FSLogHandler::createDirIfNecessary() created dir %s", path);
        return true;
    }
    else {
        DEBUG_PRINTLNF("FSLogHandler::createDirIfNecessary() ERROR: mkdir failed errno=%i", errno);
        return false;
    }
}