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
    int force;              //-f
    int recursive;          //-r
    char **inputFiles;      // input files
    int numInputFiles;      // # of input files
} args;

void usage()
{
    cout << "Usage: rm [OPTION]... FILE..." << endl;
    cout << "Move FILE(s) to DUMPSTER or delete with -f." << endl;
    cout << "DUMPSTER environmental variable must be set!" << endl << endl;
    cout << "  -f        delete permanently" << endl;
    cout << "  -r        remove directories and their contents recursively" << endl;
    cout << "  -h        display this help and exit" << endl << endl;
    cout << "By default, rm does not affect directories.  Use the -r option to move or remove each listed directory, too, along with all of its contents."<<endl;
    cout << endl<< "Program by Gavin Hayes <gahayes@wpi.edu> for CS4513 C16" << endl;
    exit( EXIT_FAILURE );
}

void handlermdir(string fullName)
{
    if((errno != ENOTEMPTY) && (errno != ENOENT))
    {
        cerr << "dv: cannot remove '" << fullName << "'\n";
        perror("rmdir");
    }
}

string getext(string name)
{
    string origString = name;
    int i = 49; //ascii for 1(one)
    //start at no ext then stop at 10
    while((access(name.c_str(), F_OK) == 0)&& i < 59){
        string temp = ".";
        temp += i;

        name = origString + temp;
        i++;
    }
    if(i == 59) //return empty string to signify no filename was availible
        return "";
    else
        return name;
}

void copyFile(const char* source, const char* dest, int bytes)
{
    FILE* openSource, *openDest;
    if((openSource = fopen(source, "rb"))== NULL)
    {
        cerr << "rm: could not open source file when copying." << endl;
        perror("fopen");
    }
    else
    {

        char* fileBuf = (char *) malloc(bytes);
	if(fileBuf == NULL)
	{
	  cerr << "rm: malloc failed. All is dead." << endl;
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
            cerr << "rm: could not create dest file when copying." << endl;
            perror("fopen");
        }
        free(fileBuf);
    }
}

int unLinkFile(string fileName)
{
    int i = unlink(fileName.c_str());
    if(i == -1)
    {
        //when recursive this function is used to test if its not a directory
        if(args.recursive != 1)
        {
            cerr<< "rm: file '" << fileName << "' could not be deleted\n";
            perror("unlink");
        }
    }
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
        cerr << "rm: error trying to set file times. More permission likely required." << endl;
        perror("utime");
    }

    //permissions
    if(chmod(newName.c_str(), fileInfo->st_mode)== -1)
    {
        cerr << "rm: error trying to set file mode bits. More permission likely required." << endl;
        perror("chmod");
    }

    if(chown(newName.c_str(), fileInfo->st_uid, fileInfo->st_gid) == -1)
    {
        cerr << "rm: error trying to set file owners and group. More permission likely required." << endl;
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
        cout << "File versions exceeded in dumpster, file " << fullName << " not moved.\n";
    }
}

void cutRecursive(string fullName, string newName, struct stat* fileInfo)
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
                cerr << "rm: cannot create directory '" << newName << "'";
                perror("mkdir");
                dp = NULL;
            }
            else
            {
                touchFile(newName, fileInfo);
                dp = opendir(fullName.c_str());
                if (dp == NULL) {
                    cerr << "rm: cannot remove '" << fullName << "' no such file or directory\n";
                    perror("opendir");
                }
                else
                {
                    if(rmdir(fullName.c_str())== -1) //remove if empty directory
                        handlermdir(fullName);
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
                                cutRecursive(toClear, alt, &fileOther);
                        }

                        if(rmdir(fullName.c_str())== -1) //remove if empty directory
                            handlermdir(fullName);
                        d = readdir(dp);
                    }
                    closedir(dp);
                }
            }
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
        if(S_ISDIR(fileInfo->st_mode))
	{
	  if(args.recursive == 1)
	  {
	    rename(fullName.c_str(), newName.c_str());
	    touchFile(newName, fileInfo);
	  }
	  else
	  {
	    cout << "rm: cannot remove '" << fullName << "': Is a directory" << endl;
	  }
	}
	else
	{	  
	  rename(fullName.c_str(), newName.c_str());
	  touchFile(newName, fileInfo);
	}
    }
    else
    {
        cout << "File versions exceeded in dumpster, file '" << fullName << "' not moved.\n";
    }
}

string getNewName(char* fileName, char* dumpsterPath)
{
    //cstring as basename modifies input
    char* abasename = (char *) malloc(strlen(fileName)+1);
    strcpy(abasename, fileName);
    string thebasename = basename(abasename);

    string newName = dumpsterPath;
    newName += "/";
    newName  += thebasename;

    //get name with extension
    newName =  getext(newName);

    //free
    free(abasename);

    return newName;
}

void setupArgs(int argc, char** argv)
{
    //getopt
    int opt = 0;
    args.usage = 0;
    args.force = 0;
    args.recursive = 0;
    args.inputFiles = NULL;
    args.numInputFiles = 0;

    while((opt = getopt(argc, argv, "hfr")) != -1)
    {
        switch(opt){
        case 'h':
            args.usage = 1;
            usage();
            break;
        case 'f':
            args.force = 1;
            break;
        case 'r':
            args.recursive = 1;
            break;

        default:
            //won't go here
            break;
        }
    }
    args.inputFiles = argv + optind;
    args.numInputFiles = argc - optind;
}

void recursiveClear(string fullName)
{
    DIR *dp;
    struct dirent *d;

    //delete if just a file
    if(unLinkFile(fullName) == -1)
    {
        //is a directory
        dp = opendir(fullName.c_str());
        if (dp == NULL) {
            cerr << "rm: cannot remove '" << fullName << "' no such file or directory\n";
        }
        else
        {
            if(rmdir(fullName.c_str())== -1) //remove if empty directory
                handlermdir(fullName);
            d = readdir(dp);
            string toClear;

            //loop through files in directory attempting to clear directory on each iteration
            while (d)
            {
                toClear = d->d_name;
                //ignore special . and .. and call recursive to delete otherwise
                if((strcmp(d->d_name, ".")!= 0 && strcmp(d->d_name, "..") != 0 ))
                {

                    //determine if fullName ends with a slash and append accordingly
                    int q = fullName.length()-1;
                    if(fullName.c_str()[q] == '/')
                        toClear = fullName + toClear;
                    else
                        toClear = fullName + "/"+ toClear;

                    recursiveClear(toClear);
                }

                if(rmdir(fullName.c_str())== -1) //remove if empty directory
                    handlermdir(fullName);
                d = readdir(dp);
            }
            closedir(dp);
        }
    }
}

int main(int argc, char** argv)
{
    //Check if DUMPSTER exists and getinfo
    struct stat dumpsterInfo;
    char* dumpsterPath;
    dumpsterPath = getenv("DUMPSTER");
    if(dumpsterPath == NULL)
        usage();
    int q = stat(dumpsterPath, &dumpsterInfo);
    if(q == -1)
    {
        cerr << "rm: cannot stat DUMPSTER directory, exiting." << endl;
        perror("stat");
        exit( EXIT_FAILURE );
    }

    if(argc == 1)
    {
        cerr << "rm: missing operand" << endl;
        cerr << "Try 'rm -h' for more information." << endl;
        exit( EXIT_FAILURE );
    }

    //setup args to effect below flow
    setupArgs(argc, argv);

    //Loop through files and perform ncesssary actions
    struct stat fileInfo;
    int i = 0;
    int ret = 0;

    while(args.numInputFiles > i)
    {
        ret = stat(args.inputFiles[i], &fileInfo);
        if(ret == -1)
            cerr << "rm: cannot remove '" << args.inputFiles[i] << "' no such file or directory\n";
        else
        {
            string fullName = args.inputFiles[i];
            string newName = getNewName(args.inputFiles[i], dumpsterPath);

            //if permanently deleting
            if(args.force == 1)
            {
                if(args.recursive != 1)
                {
                    unLinkFile(fullName);
                }
                else //recursively deleting
                {
                    recursiveClear(fullName);
                }
            }
            else if(dumpsterInfo.st_dev == fileInfo.st_dev) //if on same drive just rename the file
            {
                renameDumpster(fullName, newName, &fileInfo);
            }
            else //moving to dumpster on different drive
            {
                if(args.recursive != 1)
                {
                    cutFile(fullName, newName, &fileInfo);
                }
                else
                {
                    cutRecursive(fullName, newName, &fileInfo);
                }
            }
        }
        i++;
    }
    return 0;
}

