#include "platform.h"
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
	system(strCopyCmd.c_str());
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
		getcwd(szProgramDir,256);
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

    for (i = 0; i < p.we_wordc; i++)
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