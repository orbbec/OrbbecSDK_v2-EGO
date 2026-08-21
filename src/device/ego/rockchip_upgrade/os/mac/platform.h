#ifndef _PLATFORM_H_
#define _PLATFORM_H_
#include <string>
#include <string.h>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <list>
#include <map>
#include <set>
#include <iomanip>
// This header is included throughout the legacy Rockchip sources. Import only
// the symbols they use instead of leaking the complete std namespace.
using std::copy;
using std::cout;
using std::endl;
using std::equal;
using std::fill;
using std::find;
using std::fstream;
using std::ifstream;
using std::ios;
using std::istringstream;
using std::list;
using std::make_pair;
using std::map;
using std::max;
using std::min;
using std::ofstream;
using std::ostringstream;
using std::pair;
using std::remove;
using std::reverse;
using std::set;
using std::sort;
using std::stringstream;
using std::string;
using std::swap;
using std::transform;
using std::vector;

typedef unsigned char uint_8;
typedef unsigned short uint_16;
typedef unsigned int uint_32;
typedef unsigned long long uint_64;
typedef long long int_64;

#include "libusb.h"
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <wordexp.h>


#define CURSOR_MOVEUP_LINE(n) printf("%c[%dA",0x1B,n)
#define CURSOR_DEL_LINE printf("%c[2K",0x1B)
#define CURSOR_MOVE_HOME printf("%c[H",0x1B)
#define CURSOR_CLEAR_SCREEN printf("%c[2J",0x1B)
#define ERROR_COLOR_ATTR  printf("%c[30;41m",0x1B)
#define NORMAL_COLOR_ATTR  printf("%c[37;40m",0x1B)
#define PATH_DELIMITER '/'
#define PATH_DELIMITER_CS '\\'
#define fseek fseeko
#define ftell ftello
#define stricmp strcasecmp

void create_dir(string dir);
bool exist_dir(string dir);
bool exist_file(string file);
void copy_file(string src, string dst);
struct tm get_now_time();
void sleep_s(int n);
void sleep_ms(int n);
string get_program_dir();
string get_user_dir();
int context_enter();
void context_exit();
void split_token(string strText, string strDelim, vector<string> &vecToken);
char **split_commandline(const char *cmdline, int *argc);
void free_commandline(char **argv, int argc);
#endif
