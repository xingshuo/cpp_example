#include <iostream>
#include <stdio.h>
#include <csignal>
#include <unistd.h>
#include <setjmp.h>
#include <thread>
#include <atomic>
#include <assert.h>


using std::cout;
using std::atomic;
using std::thread;

pthread_t g_main_tid;
jmp_buf* g_jmpbuf_ptr;
atomic<int> g_tick_no(0);
int g_break_sig = SIGURG;
int g_monitor_check_interval = 5; // seconds


void app_update(int tick)
{
    cout << "------------------- tick: " << tick << " ts:" << std::time(nullptr) <<" ----------------\n";
    if (tick%2 == 0) {
        cout << "before exec endless loop\n";
        while (1) {}
        // never run here!!
        cout << "after exec endless loop\n";
    } else {
        cout << "exec normal\n";
    }
}

void thread_monitor() {
    pthread_t curr_tid = pthread_self();
    cout << "start run monitor in thread: " << curr_tid << "\n";
    int v1;
    int v2 = g_tick_no.load();
    cout << "monitor check, tick: " << v2 << " ts:" << std::time(nullptr) << "\n";
    while (true)
    {
        v1 = v2;
        sleep(g_monitor_check_interval);
        v2 = g_tick_no.load();
        cout << "monitor check, tick: " << v2 << " ts:" << std::time(nullptr) << "\n";
        if (v1 == v2) {
            cout << "touch wake up endless\n";
            pthread_kill(g_main_tid, g_break_sig);
        }
    }
}

void signal_handler(int signum) {
    pthread_t curr_tid = pthread_self();
    cout << "recv break sig:" << signum << " on thread:" << curr_tid << "\n";
    assert(curr_tid == g_main_tid);
    longjmp(*g_jmpbuf_ptr, 1);
    cout << "never run here!\n";
}


/*
Build:
    g++ -g3 -O0 break_endless_loop.cpp -std=c++11 -lpthread -o break_endless_loop
Run:
    ./break_endless_loop
*/
int main()
{
    g_main_tid = pthread_self();
    cout << "start run app in main thread: " << g_main_tid << "\n";
    signal(g_break_sig, signal_handler);
    thread t(thread_monitor);
    while (true)
    {
        g_tick_no++;
        jmp_buf jb;
        g_jmpbuf_ptr = &jb;
        if (setjmp(jb) == 0) {
            app_update(g_tick_no.load());
        } else {
            // 解除主线程信号屏蔽
            sigset_t mask;
            sigemptyset(&mask);
            sigaddset(&mask, g_break_sig);
            pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
            cout << "jump break endless loop, tick "<< g_tick_no.load() <<"\n";
        }
    }
    return 0;
}