#pragma once
#include "minilog.hh"
#include <cstddef>
#include <cstdint>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <string>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <sys/stat.h>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <string_view>
#include <iostream>
#include <httplib.h>

using namespace std::string_literals;
using namespace std::string_view_literals;
inline MiniLog lg;

constexpr uint16_t default_port{static_cast<uint16_t>(8888)};
constexpr int backlog{32};
constexpr size_t bufsz{4096*16};
constexpr std::string webroot = "../webroot"s;
inline std::string errorpath = "../webroot/errpage.html"s;
constexpr size_t threadnum{10};
enum Err : uint8_t
{
	OK,
	LISTEN_ERR,
	SOCKET_ERR,
	BIND_ERR,
	RECV_ERR,
};

