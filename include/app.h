#pragma once
#include "app_tasks.h"

class App{
public:
    bool start();

private:

    static void producer_trampoline(void *pv);
    // static void consumer_trampoline(void *pv);
    // static void logger_trampoline(void *pv);
    // static void health_trampoline(void *pv);

    void producer();
    // void consumer();
    // void logger();
    // void health();

    AppContext ctx_{};
};