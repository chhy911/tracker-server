#ifndef SQL_UTIL_HPP
#define SQL_UTIL_HPP

#include <string>

namespace sql_util {

std::string escape(const std::string& value);

/** 40 位十六进制 info_hash（20 字节 SHA1 的 hex 表示） */
bool is_valid_info_hash(const std::string& hash);

/** 40 位十六进制 peer_id */
bool is_valid_peer_id(const std::string& peer_id);

}  // namespace sql_util

#endif
