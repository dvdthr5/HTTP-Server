#ifndef LOG_H
#define LOG_H

#include "http.h"

/**
 * Initialize the logging subsystem.
 * Returns 0 on success, -1 on failure (errno set).
 */
int log_init(void);

/**
 * Tear down the logging subsystem.
 */
void log_close(void);

/**
 * Emit an audit log entry for the provided request/status combination.
 * Safe to call from multiple threads.
 */
void log_request(const struct http_request *req, int status_code);

#endif
