#include <thread>
#include <stdio.h>
#include <string.h>
#include <iostream>
using namespace std;

thread_local char threadIdBuffer[32] = {'\0'};
int cnt = 0;
char* threadIdToString(std::thread::id id){
    if(strlen(threadIdBuffer) == 0){
        snprintf(threadIdBuffer,32,"%lld",id);
    }
    cout<<strlen(threadIdBuffer)<<" cnt ="<<cnt++<<endl;
    return threadIdBuffer;
}

int main(){
    cout<< threadIdToString(this_thread::get_id())<<endl;
    thread t1([](){
            cout<< threadIdToString(this_thread::get_id())<<endl;
    });
    t1.join();

}