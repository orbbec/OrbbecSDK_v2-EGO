#include "platform.h"
#include <cctype>
#include <cstdlib>
bool global_context_ok = false;
bool exist_file(string file)
{
    int err;
	err = access(file.c_str(),F_OK);
	return err==0;
}
bool exist_dir(string path)
{
    DIR* dir = opendir(path.c_str());
    if (dir) {
        closedir(dir);
        return true;
    } else if (ENOENT == errno) {
        return false;
    } else {
        return true;
    }
}
void copy_file(string src, string dst)
{
    string strCopyCmd;
    strCopyCmd = "cp -f "+ src + " " + dst ;
	int sysRet = system(strCopyCmd.c_str());
	(void)sysRet;
}
void create_dir(string dir)
{
    mkdir(dir.c_str(),0755);
}
struct tm get_now_time()
{
    time_t	now;
	struct tm timeNow;
	time(&now);
	localtime_r(&now,&timeNow);
    return timeNow;
}
void sleep_s(int n)
{
	sleep(n);
}
void sleep_ms(int n)
{
	usleep(n*1000);
}
string get_program_dir()
{
	char szProgramDir[256];
	char szBuf[100];
	memset(szProgramDir,0,256);
	sprintf(szBuf,"/proc/%d/exe",getpid());
	if (readlink(szBuf,szProgramDir,256)==-1)
	{
		char *pCwd = getcwd(szProgramDir,256);
		(void)pCwd;
	}
	else
	{
		char *pSlash;
		pSlash = strrchr(szProgramDir,PATH_DELIMITER);
		if (pSlash)
			*pSlash = '\0';
	}
	return szProgramDir;
}
string get_user_dir()
{
	char *pszUserHome=NULL;
	pszUserHome = getenv("HOME");
	if (!pszUserHome)
		return "";
	else
		return pszUserHome;
}
int context_enter()
{
    int ret;
	ret = libusb_init(NULL);
	if (ret>=0)
		global_context_ok = true;
	return ret;
}
void context_exit()
{
	if (global_context_ok)
		libusb_exit(NULL);
}
char **split_commandline(const char *cmdline, int *argc)
{
#if defined(__ANDROID__)
    if (!cmdline || !argc) {
        return NULL;
    }

    // Android NDK does not provide POSIX wordexp(). The SDK does not use this
    // CLI helper, but keep it buildable without shell-variable expansion.
    vector<string> arguments;
    const char *cursor = cmdline;
    while (*cursor) {
        while (*cursor && std::isspace(static_cast<unsigned char>(*cursor))) {
            ++cursor;
        }
        if (!*cursor) {
            break;
        }

        const char quote = (*cursor == '\'' || *cursor == '"') ? *cursor++ : '\0';
        string argument;
        while (*cursor && (quote ? *cursor != quote
                                 : !std::isspace(static_cast<unsigned char>(*cursor)))) {
            if (*cursor == '\\' && cursor[1]) {
                ++cursor;
            }
            argument += *cursor++;
        }
        if (quote && *cursor == quote) {
            ++cursor;
        }
        arguments.push_back(argument);
    }

    *argc = static_cast<int>(arguments.size());
    if (arguments.empty()) {
        return NULL;
    }

    char **argv = static_cast<char **>(calloc(arguments.size(), sizeof(char *)));
    if (!argv) {
        *argc = 0;
        return NULL;
    }
    for (int i = 0; i < *argc; ++i) {
        argv[i] = strdup(arguments[static_cast<size_t>(i)].c_str());
        if (!argv[i]) {
            while (--i >= 0) {
                free(argv[i]);
            }
            free(argv);
            *argc = 0;
            return NULL;
        }
    }
    return argv;
#else
    int i;
    char **argv = NULL;

    if (!cmdline)
    {
        return NULL;
    }

    wordexp_t p;

    // Note! This expands shell variables.
    if (wordexp(cmdline, &p, 0))
    {
        return NULL;
    }

    *argc = p.we_wordc;

    if (!(argv = (char **)calloc(*argc, sizeof(char *))))
    {
        goto fail_split;
    }

    for (i = 0; static_cast<size_t>(i) < p.we_wordc; i++)
    {
        if (!(argv[i] = strdup(p.we_wordv[i])))
        {
            goto fail_split;
        }
    }

    wordfree(&p);

    return argv;
fail_split:
    wordfree(&p);
	return NULL;
#endif
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
