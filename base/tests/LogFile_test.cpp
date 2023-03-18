#include "base/LogFile.h"
#include "base/Logger.h"
#include <unistd.h>

std::unique_ptr<LogFlie> g_logFile;

void myOutputFunc(const char *msg,int n){
    g_logFile->append(msg,n);
}

void myFlushFunc(){
    g_logFile->flush();
}

int main( int argc,char *argv[] ){
    
    g_logFile.reset(new LogFlie(basename(__FILE__),200));
    Logger::setOutput(myOutputFunc);
    Logger::setFlush(myFlushFunc);

    string line = "1234567890 abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
    for(int i = 0;i<1000; ++i){
        LOG_TRACE << line <<i;
        usleep(1000); //usleep(1000);睡眠1000微秒
    }
}