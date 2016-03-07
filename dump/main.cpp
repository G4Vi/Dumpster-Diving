#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

using namespace std;

void usage()
{
    cout << "Usage: dump [OPTION]" << endl;
    cout << "Empties DUMPSTER directory" << endl;
        cout << "DUMPSTER environmental variable must be set!" << endl << endl;
    cout << "  -h        display this help and exit" << endl << endl;

    cout << "Program by Gavin Hayes <gahayes@wpi.edu> for CS4513 C16" << endl;
    exit( EXIT_FAILURE );
}

void setupArgs(int argc, char** argv)
{
    int opt = 0;
    while((opt = getopt(argc, argv, "h")) != -1)
    {
        switch(opt){
        case 'h':
            usage();
            break;

        default:
            //won't go here
            break;
        }
    }
}

int unLinkFile(string fileName)
{
    int i = unlink(fileName.c_str());
    return i;
}

void handlermdir(string fullName)
{
    if((errno != ENOTEMPTY) && (errno != ENOENT))
    {
        cerr << "dv: cannot remove '" << fullName << "'\n";
        perror("rmdir");
    }
}

void recursiveClear(string fullName, const string dumpster)
{
    //same as rm, but will not remove an empty dumpster directory
    DIR *dp;
    struct dirent *d;

    int c = unLinkFile(fullName);
    if(c == -1)
    {

        //is a directory
        dp = opendir(fullName.c_str());
        if (dp == NULL) {
            cerr << "dump: cannot remove '" << fullName << "' no such file or directory\n";
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

                    recursiveClear(toClear, dumpster);
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

int main(int argc, char** argv)
{
    //Check if DUMPSTER exists and getinfo
    char* dumpsterPath;
    dumpsterPath = getenv("DUMPSTER");
    if(dumpsterPath == NULL)
        usage();

    //setup args to effect below flow
    setupArgs(argc, argv);

    string fullName = dumpsterPath;
    string dumpster = dumpsterPath;
    recursiveClear(fullName, dumpster);

    return 0;
}

