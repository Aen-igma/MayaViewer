#pragma once
#include<iostream>


template<class T>
void PrintPackAt(uint32_t n, T t) {
    if(!n) std::cout << t;
}

template<class T, class... Targs>
void PrintPackAt(uint32_t n, T t, Targs... targs) {
    if(!n) std::cout << t;
    else PrintPackAt(n - 1u, targs...);
}

template<class T>
void Print(T t) {
    std::cout << t;
}

template<class T, class... Targs>
void Print(T t, Targs... targs) {
    std::cout << t;
    Print(targs...);
}

template<class T, class... Targs>
void Print(const char* msg, T t, Targs... targs) {
    while(*msg != '\0') {
        const int num = atoi(msg + 1);
        if(*msg == '{' && num < 10 && *(msg + 2) == '}') {
            PrintPackAt(num, t, targs...);
            msg += 2;
        } else
            std::cout << *msg;
        msg++;
    }
}