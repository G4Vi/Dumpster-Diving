#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <libgen.h>
#include <utime.h>
#include <errno.h>
using namespace std;

struct globalArgs_t
{
    int usage;              //-h
    int all;                //-a
    char **inputFiles;      // input files
    int numInputFiles;      // # of input files
} args;

void usage()
{
    cout << "Usage: dv [OPTION]... FILE..." << endl;
    cout << "Move FILE(s) from DUMPSTER to current directory" << endl;
        cout << "DUMPSTER environmental variable must be set!" << endl << endl;
    cout << "  -a       copy dumpster directory to current directory and clear dumpster" << endl;
    cout << "  -h        display this help and exit" << endl << endl;
    cout << "Program by Gavin Hayes <gahayes@wpi.edu> for CS4513 C16" << endl;
    exit( EXIT_FAILURE );
}

void copyFile(const char* source, const char* dest, int bytes)
{
    FILE* openSource, *openDest;
    if((openSource = fopen(source, "rb"))== NULL)
    {
        cerr << "dv: could not open source file when copying." << endl;
        perror("fopen");
    }
    else
    {

        char* fileBuf = (char *) malloc(bytes);
	if(fileBuf == NULL)
	{
	  cerr << "dv: malloc failed. All is dead." << endl;
	  perror("malloc");
	  exit( EXIT_FAILURE );
	}
        fread(fileBuf, bytes, 1, openSource);
        fclose(openSource);
        if((openDest = fopen(dest, "wb")) != NULL){
            fwrite(fileBuf, bytes, 1, openDest);
            fclose(openDest);
        }
        else
        {
            cerr << "dv: could not create dest file when copying." << endl;
            perror("fopen");
        }
        free(fileBuf);
    }
}

int unLinkFile(string fileName)
{
    int i = unlink(fileName.c_str());
    return i;
}

void touchFile(string newName, struct stat* fileInfo)
{
    //time
    struct utimbuf puttime;
    puttime.actime = fileInfo->st_atime;
    puttime.modtime = fileInfo->st_mtime;

    struct stat test;
    stat(newName.c_str(), &test);
    if (utime(newName.c_str(), &puttime))
    {
        cerr << "dv: error trying to set file times. More permission likely required." << endl;
        perror("utime");
    }

    //permissions
    if(chmod(newName.c_str(), fileInfo->st_mode)== -1)
    {
        cerr << "dv: error trying to set file mode bits. More permission likely required." << endl;
        perror("chmod");
    }

    if(chown(newName.c_str(), fileInfo->st_uid, fileInfo->st_gid) == -1)
    {
        cerr << "dv: error trying to set file owners and group. More permission likely required." << endl;
        perror("chown");
    }
}


void cutFile(string fullName, string newName, struct stat* fileInfo)
{
    if(!newName.empty())
    {
        copyFile(fullName.c_str(), newName.c_str(), fileInfo->st_size);
        unLinkFile(fullName);
        touchFile(newName, fileInfo);
    }
    else
    {
        cout << "dv: file versions exceeded in dumpster, file " << fullName << " not moved.\n";
    }
}

void handlermdir(string fullName)
{
    if((errno != ENOTEMPTY) && (errno != ENOENT))
    {
        cerr << "dv: cannot remove '" << fullName << "'\n";
        perror("rmdir");
    }
}

void cutRecursive(string fullName, string newName, const string dumpster, struct stat* fileInfo)
{
    if(S_ISDIR(fileInfo->st_mode))
    {
        DIR *dp;
        struct dirent *d;
        struct stat fileOther;
        if(!newName.empty())
        {
            if(mkdir(newName.c_str(), 0700) == -1)
            {
                cerr << "dv: cannot create directory '" << newName << "'";
                perror("mkdir");
                dp = NULL;
            }
            else
            {
                touchFile(newName, fileInfo);
                dp = opendir(fullName.c_str());
                if (dp == NULL) {
                    cerr << "dv: cannot remove '" << fullName << "' no such file or directory\n";
                    perror("opendir");
                }
                else
                {
                    if(fullName != dumpster)
                    {
                        if(rmdir(fullName.c_str())== -1) //remove if empty directory
                            handlermdir(fullName);
                    }
                    d = readdir(dp);
                    string toClear;
                    string alt;

                    //loop through files in directory attempting to clear directory on each iteration
                    while (d)
                    {
                        toClear = d->d_name;
                        //ignore special . and .. and call recursive to proceed
                        if((strcmp(d->d_name, ".")!= 0 && strcmp(d->d_name, "..") != 0 ))
                        {
                            //determine whether newName has a /
                            int q = newName.length()-1;
                            if(newName.c_str()[q] == '/')
                                alt = newName + toClear;
                            else
                                alt = newName + "/"+ toClear;

                            //determine if fullName ends with a slash and append accordingly
                            q = fullName.length()-1;
                            if(fullName.c_str()[q] == '/')
                                toClear = fullName + toClear;
                            else
                                toClear = fullName + "/"+ toClear;

                            int ret = stat(toClear.c_str(), &fileOther);
                            if(ret == -1)
                            {
                                cerr << "rm: cannot stat '" << toClear << "' skipping" << endl;
                                perror("stat");
                            }
                            else
                                cutRecursive(toClear, alt, dumpster, &fileOther);
                        }
                        if(fullName != dumpster)
                        {
                            if(rmdir(fullName.c_str())== -1) //remove if empty directory
                                handlermdir(fullName);
                        }
                        d = readdir(dp);
                    }
                    closedir(dp);
                }
            }
        }
        else
        {
             cout << "Folder '" << fullName << "' exists in current directory, folder not moved out of dumpster\n";
        }
    }
    else
    {
        cutFile(fullName, newName, fileInfo);
    }
}

void renameDumpster(string fullName, string newName, struct stat* fileInfo)
{
    if(!newName.empty())
    {
        rename(fullName.c_str(), newName.c_str());
        touchFile(newName, fileInfo);
        //-a
        if(args.all == 1)
        {
            if(mkdir(fullName.c_str(), 0700) == -1)
            {
                cerr << "dv: cannot create directory '" << newName << "'";
                perror("mkdir");
            }
            else
                touchFile(fullName, fileInfo);
        }



    }
    else
    {
        cout << "File '" << fullName << "' exists in current directory, file not moved out of dumpster\n";
    }
}

string getFullName(char* fileName, char* dumpsterPath)
{
    string fullName;

    if(args.all == 0)
    {       
        if(fileName[0] != '/')
        {
            fullName = dumpsterPath;
            fullName += "/";
            fullName  += fileName;
        }
        else
        {

            cerr << "dv: absolute paths not accepted" << endl;
            fullName == "";
        }
    }
    else
    {
        fullName = dumpsterPath;
    }

    return fullName;
}

string getNewName(char* fileName)
{
    //cstring as basename modifies input
    char* abasename = (char *) malloc(strlen(fileName)+1);
    strcpy(abasename, fileName);
    string thebasename = basename(abasename);

    //free
    free(abasename);

    return thebasename;
}

string checkExists(string name)
{
    if(access(name.c_str(), F_OK) == 0)
    {
        return "";
    }
    else
        return name;
}


void setupArgs(int argc, char** argv)
{
    //getopt
    int opt = 0;
    args.usage = 0;
    args.all = 0;
    args.inputFiles = NULL;
    args.numInputFiles = 0;

    while((opt = getopt(argc, argv, "ha")) != -1)
    {
        switch(opt){
        case 'h':
            args.usage = 1;
            usage();
            break;
        case 'a':
            args.all = 1;
            break;

        default:
            //won't go here
            break;
        }
    }
    args.inputFiles = argv + optind;
    args.numInputFiles = argc - optind;
}

int main(int argc, char** argv)
{
    //Check if DUMPSTER exists and getinfo
    struct stat currentInfo;
    char* dumpsterPath;
    dumpsterPath = getenv("DUMPSTER");
    if(dumpsterPath == NULL)
        usage();
	
	 if(argc == 1)
    {
        cerr << "dv: missing operand" << endl;
        cerr << "Try 'dv -h' for more information." << endl;
        exit( EXIT_FAILURE );
    }

    //chdir("/media/g4vi/4ABA5026BA501135/"); for testing across drives

    //dot is working directory
    int q = stat(".", &currentInfo);

    if(q == -1)
    {
        cerr << "dv: cannot stat current directory, exiting." << endl;
        perror("stat");
        exit( EXIT_FAILURE );
    }

    //setup args to effect below flow
    setupArgs(argc, argv);

    //Loop through files and perform ncesssary actions
    struct stat fileInfo;
    int i = 0;
    int ret = 0;
    string dumpster = dumpsterPath;

    //if all, else run loop
    if(args.all == 1)
    {
        string newName = ".DUMPSTER";
        newName = checkExists(newName);
        string fullName = dumpsterPath;
        ret = stat(fullName.c_str(), &fileInfo);
        if(ret == -1)
        {
            cerr << "dv: cannot restore DUMPSTER, DUMPSTER stat error.\n";
            perror("stat");
            exit( EXIT_FAILURE );
        }
        else
        {

            if(currentInfo.st_dev == fileInfo.st_dev) //if on same drive just rename the file
            {
                renameDumpster(fullName, newName, &fileInfo);
            }
            else //moving to dumpster on different drive
            {
                cutRecursive(fullName, newName, dumpster, &fileInfo);
            }
        }
    }
    else
    {

        while(args.numInputFiles > i)
        {
            string newName = getNewName(args.inputFiles[i]);
            newName = checkExists(newName);
            string fullName = getFullName(args.inputFiles[i], dumpsterPath);

            ret = stat(fullName.c_str(), &fileInfo);
            if(ret == -1)
            {
                cerr << "dv: cannot restore '" << args.inputFiles[i] << "' not found in DUMPSTER\n";
                perror("stat");
            }
            else
            {
                if(currentInfo.st_dev == fileInfo.st_dev) //if on same drive just rename the file
                {
                    renameDumpster(fullName, newName, &fileInfo);
                }
                else //moving to dumpster on different drive
                {
                    cutRecursive(fullName, newName, dumpster, &fileInfo);
                }
            }

            i++;
        }
    }
    return 0;
}


