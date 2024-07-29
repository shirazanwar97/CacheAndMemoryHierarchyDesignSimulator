#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <iostream>
#include <sstream>
#include <bitset>
#include <string>


class Cache {
public:
    int valid;      //valid bit
    unsigned int tag;       //tag bit
    int dirty;      // dirty bit
    int counter;        //LRU
    unsigned long int address;
    Cache(){
    }
};