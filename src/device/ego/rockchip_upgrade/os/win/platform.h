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

#include <Windows.h>
#include <time.h>
#include <wchar.h>
#include <shellapi.h>
#include <setupapi.h>
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "shell32.lib")

#define CURSOR_MOVEUP_LINE(n) cursor_moveup_line(n);
#define CURSOR_DEL_LINE cursor_del_line();
#define CURSOR_MOVE_HOME cursor_move_home();
#define CURSOR_CLEAR_SCREEN cursor_clear_screen();
#define ERROR_COLOR_ATTR  error_color_attr();
#define NORMAL_COLOR_ATTR  normal_color_attr();


#define PATH_DELIMITER '\\'
#define PATH_DELIMITER_CS '/'
#define fseek _fseeki64
#define ftell _ftelli64
#define __func__ __FUNCTION__

void create_dir(string dir);
bool exist_dir(string dir);
bool exist_file(string file);
void copy_file(string src, string dst);
struct tm get_now_time();
void sleep_s(int n);
void sleep_ms(int n);
string get_program_dir();
string get_user_dir();

void cursor_moveup_line(int n);
void cursor_del_line();
void cursor_move_home();
void cursor_clear_screen();
void error_color_attr();
void normal_color_attr();
int context_enter();
void context_exit();
void split_token(string strText, string strDelim, vector<string> &vecToken);
char **split_commandline(const char *cmdline, int *argc);
void free_commandline(char **argv, int argc);
#endif
