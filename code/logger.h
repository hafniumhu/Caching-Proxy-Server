#ifndef __LOGGER_H__
#define __LOGGER__

#include <fstream>
#include <iostream>
#include <string>

using namespace std;

class Logger {
private:
    string file = "/var/log/erss/proxy.log";

public:
    void addMessage(int id, string message) {
        ofstream writer;
        writer.open(file, ofstream::out | ofstream::app);
        writer << id << ": "<< message << endl;
        writer.close();
    }
};

#endif