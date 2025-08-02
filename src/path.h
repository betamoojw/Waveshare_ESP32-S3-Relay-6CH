#ifndef PATH_H
#define PATH_H

#include <Arduino.h>
#include <string>

#define SYS_CONFIG_FIL_PATH    ("/system_config.json")
#define SYS_CONFIG_FIL_SIZE    (1024 * 2)   // 2KB

#define SYS_LOG_FIL_PATH       ("/system_log.txt")
#define SYS_LOG_FIL_SIZE       (1024 * 2)  // 10KB

#define SYS_DATA_FIL_PATH      ("/system_data.json")
#define SYS_DATA_FIL_SIZE      (1024 * 2)  // 10KB

#endif // PATH_H