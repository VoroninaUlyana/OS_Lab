#include <windows.h>
#include <iostream>
#include <string>
using namespace std;

void parent_mode(const char* exePath);
void child_mode();

int main(int argc, char* argv[]) 
{
    if (argc > 1 && string(argv[1]) == "child") 
    {
        cout << "[Child] Started in child mode.\n";
        child_mode();
    }
    else 
    {
        cout << "[Parent] Started in parent mode.\n";
        parent_mode(argv[0]);
    }
    return 0;
}

void parent_mode(const char* exePath) 
{
    cout << "[Parent] Parent mode placeholder.\n";
}

void child_mode() 
{
    cout << "[Child] Child mode placeholder.\n";
}
