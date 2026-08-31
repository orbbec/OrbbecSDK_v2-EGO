#include "platform.h"

namespace {
bool existsWithCodePage(const string &file, UINT codePage) {
    const DWORD flags  = codePage == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0;
    const int   length = MultiByteToWideChar(codePage, flags, file.c_str(), -1, NULL, 0);
    if(length == 0) {
        return false;
    }
    vector<wchar_t> widePath(length);
    if(MultiByteToWideChar(codePage, flags, file.c_str(), -1, widePath.data(), length) == 0) {
        return false;
    }
    WIN32_FIND_DATAW findFileData;
    HANDLE           handle = FindFirstFileW(widePath.data(), &findFileData);
    if(handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    FindClose(handle);
    return true;
}
}  // namespace

bool exist_file(string file)
{
    // Public SDK paths are UTF-8, but existing Windows applications may pass
    // the local ANSI code page from their native file picker. Try UTF-8 first
    // and fall back to ACP only if that path does not exist.
    return existsWithCodePage(file, CP_UTF8) || (CP_ACP != CP_UTF8 && existsWithCodePage(file, CP_ACP));
}
bool exist_dir(string dir)
{
    uint_32 ret;
    ret = GetFileAttributesA(dir.c_str());
    if (ret == INVALID_FILE_ATTRIBUTES)
        return false;
    if (ret & FILE_ATTRIBUTE_DIRECTORY)
        return true;
    else
        return false;
}
void copy_file(string src, string dst)
{
    CopyFileA(src.c_str(), dst.c_str(),FALSE);
}
void create_dir(string dir)
{
    CreateDirectoryA(dir.c_str(),NULL);
}
struct tm get_now_time()
{
    time_t	now;
	struct tm timeNow;
	time(&now);
	return (*localtime(&now));
}
void sleep_s(int n)
{
	Sleep(n*1000);
}
void sleep_ms(int n)
{
	Sleep(n);
}
void cursor_moveup_line(int n)
{
	COORD crDest;
	CONSOLE_SCREEN_BUFFER_INFO bInfo;
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	GetConsoleScreenBufferInfo( hOut, &bInfo );
	crDest.X = bInfo.dwCursorPosition.X;
	crDest.Y = bInfo.dwCursorPosition.Y-n;
	SetConsoleCursorPosition(hOut,crDest);
}
void cursor_del_line()
{
	SMALL_RECT rcScroll, rcClip;
	COORD crDest;
	CHAR_INFO chFill;
	CONSOLE_SCREEN_BUFFER_INFO bInfo;
	SHORT row;
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	GetConsoleScreenBufferInfo( hOut, &bInfo );
	row = bInfo.dwCursorPosition.Y;
	rcScroll.Left = 0;
	rcScroll.Top = row;
	rcScroll.Right = bInfo.dwSize.X - 1;
	rcScroll.Bottom = bInfo.dwSize.Y - 1;
	rcClip = rcScroll;
	chFill.Attributes = bInfo.wAttributes;
	chFill.Char.UnicodeChar = ' ';
	crDest.X = 0;
	crDest.Y = row-1;
	ScrollConsoleScreenBufferA(hOut, &rcScroll, &rcClip, crDest, &chFill);
}
void cursor_move_home()
{
	HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD coord = {0, 0};
	SetConsoleCursorPosition(hStdOut, coord);
}
void cursor_clear_screen()
{
	CONSOLE_SCREEN_BUFFER_INFO bInfo;
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	GetConsoleScreenBufferInfo( hOut, &bInfo );
	COORD home = {0, 0};
	WORD att = bInfo.wAttributes;
	DWORD dwCount=0;
	unsigned long size = bInfo.dwSize.X * bInfo.dwSize.Y;
	FillConsoleOutputAttribute(hOut, att, size, home, &dwCount);
	FillConsoleOutputCharacterA(hOut, ' ', size, home, &dwCount);
}
void error_color_attr()
{
	WORD wColor = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE ;
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), wColor);
	wColor = BACKGROUND_RED|BACKGROUND_INTENSITY;
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), wColor);
}
void normal_color_attr()
{
	WORD wColor = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE ;
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), wColor);
}
string get_program_dir()
{
	char szProgramDir[MAX_PATH];
	memset(szProgramDir,0,MAX_PATH);
	GetModuleFileNameA(NULL, szProgramDir, MAX_PATH);
	char *pos=NULL;
	pos = strrchr(szProgramDir,PATH_DELIMITER);
	if (pos)
	{
		*pos = '\0';
	}
	return szProgramDir;
}
string get_user_dir()
{
	char szUserDir[MAX_PATH];
	char *pBuf=NULL;
	memset(szUserDir,0,MAX_PATH);
	pBuf = getenv("USERPROFILE");
	if (pBuf)
		strcpy(szUserDir,pBuf);
	return szUserDir;
}
int context_enter()
{
	return 0;
}
void context_exit(){}
char **split_commandline(const char *cmdline, int *argc)
{
	uint_16 **wargs = NULL;
	int needed = 0,i;
	uint_16 *cmdlinew = NULL;
	char **argv = NULL;
	size_t len;


    if (!cmdline)
    {
        return NULL;
    }
	len = strlen(cmdline) + 1;

	if (!(cmdlinew = (uint_16 *)calloc(len, sizeof(uint_16))))
		goto fail;

	if (!MultiByteToWideChar(CP_ACP, 0, cmdline, -1, (LPWSTR)cmdlinew, len))
		goto fail;

	if (!(wargs = (uint_16 **)CommandLineToArgvW((LPCWSTR)cmdlinew, argc)))
		goto fail;

	if (!(argv = (char **)calloc(*argc, sizeof(char *))))
		goto fail;

	// Convert from wchar_t * to ANSI char *
	for (i = 0; i < *argc; i++)
	{
		// Get the size needed for the target buffer.
		// CP_ACP = Ansi Codepage.
		needed = WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)wargs[i], -1,
									NULL, 0, NULL, NULL);

		if (!(argv[i] = (char *)malloc(needed)))
			goto fail;

		// Do the conversion.
		needed = WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)wargs[i], -1,
									argv[i], needed, NULL, NULL);
	}

fail:
	if (wargs) LocalFree(wargs);
	if (cmdlinew) free(cmdlinew);
	return argv;
}
void free_commandline(char **argv, int argc)
{
	int i;
	if (argc > 0)
    {
        for (i = 0; i < argc; i++)
        {
            if (argv[i])
            {
                free(argv[i]);
            }
        }
        free(argv);
    }
}
void split_token(string strText, string strDelim, vector<string> &vecToken)
{
    size_t start;
    size_t end = 0;
 
    while ((start = strText.find_first_not_of(strDelim, end)) != std::string::npos)
    {
        end = strText.find(strDelim, start);
        vecToken.push_back(strText.substr(start, end - start));
    }
}
