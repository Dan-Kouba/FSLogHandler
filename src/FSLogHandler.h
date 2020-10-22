// FSLogHandler: A log handler that writes logs to the onboard filesystem on Gen 3 and Tracker
// Author:  Dan Kouba <dan.kouba@particle.io>
// Date:    October 2020
// Company: Particle

#ifndef __FSLOGHANDLER_H
#define __FSLOGHANDLER_H

#include "Particle.h"

// Set up some debug macros:
// - You cannot log from inside a logger
// - Loggers and Serial.printf may interfere with each other

#define FS_LOG_HANDLER_DEBUG_LEVEL 0    // 0, 1, or 2

#if FS_LOG_HANDLER_DEBUG_LEVEL > 0
# define DEBUG_PRINTF(fmt, ...) Serial.printf("DEBUG: " fmt, __VA_ARGS__)
# define DEBUG_PRINTLNF(fmt, ...) Serial.printlnf("DEBUG: " fmt, __VA_ARGS__)
#else
# define DEBUG_PRINTF(fmt, ...) do {} while (0)
# define DEBUG_PRINTLNF(fmt, ...) do {} while (0)
#endif

#if FS_LOG_HANDLER_DEBUG_LEVEL > 1
# define TRACE_PRINTF(fmt, ...) Serial.printf("TRACE: " fmt, __VA_ARGS__)
# define TRACE_PRINTLNF(fmt, ...) Serial.printlnf("TRACE: " fmt, __VA_ARGS__)
#else
# define TRACE_PRINTF(fmt, ...) do {} while (0)
# define TRACE_PRINTLNF(fmt, ...) do {} while (0)
#endif

/**
 * @brief Class for logging to the Particle Filesystem, as introduced in 1.5.4/2.0.0
 * 
 * The class will log to a a file in /log/<supplied filename>.log
 * Syncing logs from the buffer is handled through a loop() function that needs to be called from the main file's loop()
 * 
 * Optionally you can configure the buffer size before fsyncing, as well as a timeout.
 */
class FSLogHandler : public LogHandler {
public:
	/**
	 * @brief Constructor. The object is normally instantiated as a global object.
	 *
	 * @param filename Filename to log to, including full path
     * @param enable_immediately Flag to start logging immediately, vs waiting for start() (optional, default is True)
	 * @param level  (optional, default is LOG_LEVEL_INFO)
	 * @param filters (optional, default is none)
	 */
	explicit FSLogHandler(String filename, bool enable_now = true, LogLevel level = LOG_LEVEL_INFO, LogCategoryFilters filters = {});
    virtual ~FSLogHandler();

    /**
	 * @brief Required housekeeping function required to manage filesystem syncs.  Should be called periodically from main loop() function.  
     * 
     * See configureFsync() for configuration options
	 */
    void loop();

	/**
	 * @brief Public function to dump the target logfile to a supplied stream
	 *
	 * @param stream Stream object to dump data to
	 * @param read_from_beginning Flag to indicate whether to continue reading, or read from beginning (optional, default is true)
	 */
    void dump(Print &stream, bool read_from_beginning = true);

    /**
	 * @brief Clear/delete the current logfile
	 */
    void clearLogs();

    /**
	 * @brief Get current logfile size in bytes
	 */
    long getLogSize();

    /* Setters and getters for private vars */

    /**
	 * @brief Public function to get the full path to the target logfile
     * 
     * @return String of the full path
	 */
    String getPath() { return _path; };

    /**
	 * @brief Configure the filesystem synchronization routine. Set a number of bytes to buffer before writing, and a timeout
     * 
     * @param max_bytes Number of bytes to allow to be buffered before triggering a fsync()
     * @param timeout_s Timeout before next fsync(), regardless of the number of bytes in the buffer
	 */
    inline FSLogHandler &configureFsync(unsigned int max_bytes, unsigned int timeout_s) {
        _max_bytes_queued = max_bytes;
        _fsync_timeout_s  = timeout_s;
        return *this;   // Allow for chaining with other setters
    };

    /**
	 * @brief Start or stop logging to file.  Logs are dropped if not enabled.
	 */
    inline FSLogHandler &enable(bool enable = true) { 
        _enabled = enable;
        return *this;   // Allow for chaining with other setters
    };   

    /**
	 * @brief Check if we are enabled
     * @return True if we are enabled or False if not
	 */
    bool enabled() { return _enabled; };

private:
    const char* extractFileName(const char *s);
    const char* extractFuncName(const char *s, size_t *size);

    bool _enabled;                  // Whether or not we are logging
    int _fd;                        // File descriptor
    bool _open;                     // File open flag
    String _path;                   // Full logfile path
    unsigned int _bytes_queued;     // Num of bytes queued for fs write
    unsigned int _max_bytes_queued; // Max number of bytes to be queued before forcing a fsync()
    unsigned int _fsync_timeout_s;  // Max number of seconds elapsed before manually triggering a fsync(), given bytes available.

    void writeToFile(String message);
    bool fileInit();
    void syncAndClose();
    static bool createDirIfNecessary(const char *path);

protected:
    /*!
        @brief Performs processing of a log message.
        @param msg Text message.
        @param level Logging level.
        @param category Category name (can be null).
        @param attr Message attributes.
        This method should be implemented by all subclasses.
    */
    virtual void logMessage(const char *msg, LogLevel level, const char *category, const LogAttributes &attr) override;
};

#endif  //__FSLOGHANDLER_H