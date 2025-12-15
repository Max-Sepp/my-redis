#ifndef HANDLER_HANDLERHELPERS_H
#define HANDLER_HANDLERHELPERS_H

#include <memory>
#include <string>

#include "logging/Logger.h"

const std::string INTERNAL_ERROR_RESP = "-ERR internal error\r\n";
const std::string OK_RESP = "+OK\r\n";

void SendResponse(int client_fd, const std::string& message,
                  std::shared_ptr<Logger> logger);

#endif  // HANDLER_HANDLERHELPERS_H
