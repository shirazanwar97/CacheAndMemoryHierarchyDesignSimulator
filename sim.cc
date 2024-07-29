#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "sim.h"
#include "cache.cpp"
#include <bits/stdc++.h>
#include <iostream>
#include <iomanip>
#include <math.h>
#include <vector>

using namespace std;

/*  "argc" holds the number of command-line arguments.
    "argv[]" holds the arguments themselves.

    Example:
    ./sim 32 8192 4 262144 8 3 10 gcc_trace.txt
    argc = 9
    argv[0] = "./sim"
    argv[1] = "32"
    argv[2] = "8192"
    ... and so on
*/

unsigned long long readCounterL1 = 0, readMissCounterL1 = 0, writeCounterL1 = 0, writeMissCounterL1 = 0, writeBackCounterL1 = 0;
unsigned long long readCounterL2 = 0, readMissCounterL2 = 0, writeCounterL2 = 0, writeMissCounterL2 = 0, writeBackL2ToMem = 0;
unsigned long long memoryTrafficCounter = 0;
unsigned int setsL1 = 0, blockOffsetL1 = 0, indexBitL1 = 0, tagBitL1 = 0, indexL1 = 0, tagAddressL1 = 0, offsetL1 = 0;
unsigned int setsL2 = 0, blockOffsetL2 = 0, indexBitL2 = 0, tagBitL2 = 0, indexL2 = 0, tagAddressL2 = 0, offsetL2 = 0;

vector < vector < Cache > > L1Cache;
vector < vector < Cache > > L2Cache;

void initL1Cache(unsigned int assoc_1) {
  L1Cache = vector < vector < Cache > > (setsL1, vector < Cache > (assoc_1));
  for (int i = 0; i < setsL1; i++) // initializing valid bits to 0, dirty bits to 0 and counter bits
  {
    for (int j = 0; j < assoc_1; j++) {
      L1Cache[i][j].valid = 0;
      L1Cache[i][j].counter = j;
      L1Cache[i][j].dirty = 0;
      L1Cache[i][j].address = -1;
    }
  }
}

void initL2Cache(unsigned int assoc_2) {
  L2Cache = vector < vector < Cache > > (setsL2, vector < Cache > (assoc_2));
  for (int i = 0; i < setsL2; i++) // initializing valid bits to 0, dirty bits to 0 and counter bits
  {
    for (int j = 0; j < assoc_2; j++) {
      L2Cache[i][j].valid = 0;
      L2Cache[i][j].counter = j;
      L2Cache[i][j].dirty = 0;
      L2Cache[i][j].address = -1;
    }
  }
}

void L1CacheAddressCalculations(unsigned int size, unsigned int assoc, unsigned int block_size, unsigned add) {
  setsL1 = size / (assoc * block_size);
  blockOffsetL1 = log2(block_size);
  indexBitL1 = log2(setsL1);
  tagBitL1 = 32 - blockOffsetL1 - indexBitL1;
  offsetL1 = (add >> 0) & ((1 << blockOffsetL1) - 1);
  indexL1 = (add >> blockOffsetL1) & ((1 << indexBitL1) - 1);
  tagAddressL1 = (add >> (indexBitL1 + blockOffsetL1)) & ((1 << tagBitL1) - 1); //tag in the address
}

void L2CacheAddressCalculations(unsigned int size_2, unsigned int assoc_2, unsigned int block_size, unsigned add) {
  setsL2 = size_2 / (assoc_2 * block_size);
  blockOffsetL2 = log2(block_size);
  indexBitL2 = log2(setsL2);
  tagBitL2 = 32 - blockOffsetL2 - indexBitL2;
  offsetL2 = (add >> 0) & ((1 << blockOffsetL2) - 1);
  indexL2 = (add >> blockOffsetL2) & ((1 << indexBitL2) - 1);
  tagAddressL2 = (add >> (indexBitL2 + blockOffsetL2)) & ((1 << tagBitL2) - 1);
}

int L2_cache_read(unsigned int size_2, unsigned int assoc_2, unsigned int block_size, unsigned add) {
  L2CacheAddressCalculations(size_2, assoc_2, block_size, add);
  readCounterL2++;
  unsigned int x = 0, y = 0;
  int flag = 0;

  for (unsigned int i = 0; i < assoc_2; i++) {
    if ((L2Cache[indexL2][i].valid == 1) && (L2Cache[indexL2][i].tag == tagAddressL2)) //HIT case
    {

      y = L2Cache[indexL2][i].counter;
      L2Cache[indexL2][i].counter = 0;
      L2Cache[indexL2][i].address = add;
      x = i;
      flag = 1;
      //return 1;
      break;
    }
  }

  if (flag == 0) //Miss case
  {
    readMissCounterL2++;
    memoryTrafficCounter++; //fetch from memory

    for (unsigned int i = 0; i < assoc_2; i++) {
      if ((L2Cache[indexL2][i].counter == (assoc_2 - 1)) && (L2Cache[indexL2][i].valid == 0 || (L2Cache[indexL2][i].valid == 1 && L2Cache[indexL2][i].dirty == 0))) { //case for dirty = 0
        L2Cache[indexL2][i].tag = tagAddressL2;
        y = L2Cache[indexL2][i].counter;
        L2Cache[indexL2][i].counter = 0;
        L2Cache[indexL2][i].valid = 1;
        L2Cache[indexL2][i].address = add;
        x = i;
        break;
      } else if ((L2Cache[indexL2][i].counter == (assoc_2 - 1)) && (L2Cache[indexL2][i].dirty == 1)) { //case for dirty = 1
        writeBackL2ToMem++;
        memoryTrafficCounter++; //fetched from memory
        L2Cache[indexL2][i].tag = tagAddressL2;
        y = L2Cache[indexL2][i].counter;
        L2Cache[indexL2][i].counter = 0;
        L2Cache[indexL2][i].valid = 1;
        L2Cache[indexL2][i].dirty = 0;
        L2Cache[indexL2][i].address = add;
        x = i;
        break;
      }
    }
  }

  if (assoc_2 > 1) {
    for (unsigned int i = 0; i < assoc_2; i++) {
      if (i != x && L2Cache[indexL2][i].counter < y) {
        L2Cache[indexL2][i].counter++;
      }
    }
  }
  return flag;
}

int L2_cache_write(unsigned int size_2, unsigned int assoc_2, unsigned int block_size, unsigned add) { //writeback()
  writeCounterL2++;
  L2CacheAddressCalculations(size_2, assoc_2, block_size, add);
  unsigned int x = 0, y = 0;
  int flag = 0;

  for (unsigned int i = 0; i < assoc_2; i++) {
    if ((L2Cache[indexL2][i].valid == 1) && (L2Cache[indexL2][i].tag == tagAddressL2)) //HIT case
    {
      y = L2Cache[indexL2][i].counter;
      L2Cache[indexL2][i].counter = 0;
      L2Cache[indexL2][i].dirty = 1;
      L2Cache[indexL2][i].address = add;
      x = i;
      flag = 1;
      //return 1;
      break;
    }
  }

  if (flag == 0) //miss case
  {
    writeMissCounterL2++;
    memoryTrafficCounter++;
    for (unsigned int i = 0; i < assoc_2; i++) {
      if ((L2Cache[indexL2][i].counter == (assoc_2 - 1)) && (L2Cache[indexL2][i].valid == 0 || (L2Cache[indexL2][i].valid == 1 && L2Cache[indexL2][i].dirty == 0))) { //dirty = 0
        L2Cache[indexL2][i].tag = tagAddressL2;
        y = L2Cache[indexL2][i].counter;
        L2Cache[indexL2][i].counter = 0;
        L2Cache[indexL2][i].valid = 1;
        L2Cache[indexL2][i].dirty = 1;
        L2Cache[indexL2][i].address = add;
        x = i;
        break;
      } else if ((L2Cache[indexL2][i].counter == (assoc_2 - 1)) && (L2Cache[indexL2][i].dirty == 1)) { //dirty = 1
        writeBackL2ToMem++;
        memoryTrafficCounter++;
        L2Cache[indexL2][i].tag = tagAddressL2;
        y = L2Cache[indexL2][i].counter;
        L2Cache[indexL2][i].counter = 0;
        L2Cache[indexL2][i].valid = 1;
        L2Cache[indexL2][i].dirty = 1;
        L2Cache[indexL2][i].address = add;
        x = i;
        break;
      }
    }
  }

  if (assoc_2 > 1) {
    for (unsigned int i = 0; i < assoc_2; i++) {
      if (i != x && L2Cache[indexL2][i].counter < y) {
        L2Cache[indexL2][i].counter++;
      }
    }
  }
  return flag;
}

int readRequestToL1(unsigned int size_1, unsigned int assoc_1, unsigned int size_2, unsigned int assoc_2, unsigned int block_size, unsigned add) {
  L1CacheAddressCalculations(size_1, assoc_1, block_size, add);
  readCounterL1++;
  unsigned int x = 0, y = 0; //used for LRU
  int flag = 0; //used for HIT MISS identification

  for (unsigned int i = 0; i < assoc_1; i++) {
    if ((L1Cache[indexL1][i].valid == 1) && (L1Cache[indexL1][i].tag == tagAddressL1)) //HIT case
    {
      y = L1Cache[indexL1][i].counter;
      L1Cache[indexL1][i].counter = 0;
      L1Cache[indexL1][i].address = add;
      x = i;
      flag = 1; //flag to imply a HIT
      //return 1;
      break;
    }
  }

  if (flag == 0) //MISS case
  {
    readMissCounterL1++;

    for (unsigned int i = 0; i < assoc_1; i++) {
      if ((L1Cache[indexL1][i].counter == (assoc_1 - 1)) && (L1Cache[indexL1][i].valid == 0 || (L1Cache[indexL1][i].valid == 1 && L1Cache[indexL1][i].dirty == 0))) { //case for dirty = 0
        if (size_2 > 0)
          L2_cache_read(size_2, assoc_2, block_size, add);
        else
          memoryTrafficCounter++; //fetched data from memory
        L1Cache[indexL1][i].tag = tagAddressL1;
        y = L1Cache[indexL1][i].counter;
        L1Cache[indexL1][i].counter = 0;
        L1Cache[indexL1][i].valid = 1;
        L1Cache[indexL1][i].address = add;
        x = i;
        break;
      } else if ((L1Cache[indexL1][i].counter == (assoc_1 - 1)) && (L1Cache[indexL1][i].dirty == 1) && (L1Cache[indexL1][i].valid == 1)) { //case for dirty = 1
        writeBackCounterL1++; //writeback
        if (size_2 > 0) {
          L2_cache_write(size_2, assoc_2, block_size, L1Cache[indexL1][i].address);
          L2_cache_read(size_2, assoc_2, block_size, add);
        } else {
          memoryTrafficCounter++; //writeback
          memoryTrafficCounter++; //fetched data from Memory
        }
        L1Cache[indexL1][i].tag = tagAddressL1;
        y = L1Cache[indexL1][i].counter;
        L1Cache[indexL1][i].counter = 0;
        L1Cache[indexL1][i].valid = 1;
        L1Cache[indexL1][i].address = add;
        L1Cache[indexL1][i].dirty = 0;
        x = i;
        break;
      }
    }
  }

  if (assoc_1 > 1) {
    for (unsigned int i = 0; i < assoc_1; i++) {
      if (i != x && L1Cache[indexL1][i].counter < y) {
        L1Cache[indexL1][i].counter++;
      }
    }
  }

  return flag;
}

int writeRequestToL2(unsigned int size_1, unsigned int assoc_1, unsigned int size_2, unsigned int assoc_2, unsigned int block_size, unsigned add) {
  L1CacheAddressCalculations(size_1, assoc_1, block_size, add);
  writeCounterL1++;
  unsigned int x = 0, y = 0;
  int flag = 0;

  for (unsigned int i = 0; i < assoc_1; i++) {
    if ((L1Cache[indexL1][i].valid == 1) && (L1Cache[indexL1][i].tag == tagAddressL1)) {
      y = L1Cache[indexL1][i].counter;
      L1Cache[indexL1][i].counter = 0;
      L1Cache[indexL1][i].dirty = 1;
      L1Cache[indexL1][i].address = add;
      x = i;
      flag = 1;
      //return 1;
      break;
    }
  }

  if (flag == 0) //miss case
  {
    writeMissCounterL1++;

    for (unsigned int i = 0; i < assoc_1; i++) {
      if ((L1Cache[indexL1][i].counter == (assoc_1 - 1)) && (L1Cache[indexL1][i].valid == 0 || (L1Cache[indexL1][i].valid == 1 && L1Cache[indexL1][i].dirty == 0))) { //dirty=0
        if (size_2 > 0)
          L2_cache_read(size_2, assoc_2, block_size, add);
        else
          memoryTrafficCounter++; //indicating writing to memory
        L1Cache[indexL1][i].tag = tagAddressL1;
        y = L1Cache[indexL1][i].counter;
        L1Cache[indexL1][i].counter = 0;
        L1Cache[indexL1][i].valid = 1;
        L1Cache[indexL1][i].dirty = 1;
        L1Cache[indexL1][i].address = add;
        x = i;
        break;
      } else if ((L1Cache[indexL1][i].counter == (assoc_1 - 1)) && (L1Cache[indexL1][i].dirty == 1)) { //dirty=1
        writeBackCounterL1++;
        if (size_2 > 0) {
          L2_cache_write(size_2, assoc_2, block_size, L1Cache[indexL1][i].address);
          L2_cache_read(size_2, assoc_2, block_size, add);
        } else {
          memoryTrafficCounter++; //writeback
          memoryTrafficCounter++; //fetch data from Memory
        }
        L1Cache[indexL1][i].tag = tagAddressL1;
        y = L1Cache[indexL1][i].counter;
        L1Cache[indexL1][i].counter = 0;
        L1Cache[indexL1][i].valid = 1;
        L1Cache[indexL1][i].dirty = 1;
        L1Cache[indexL1][i].address = add;
        x = i;
        break;
      }

    }

  }

  if (assoc_1 > 1) {
    for (unsigned int i = 0; i < assoc_1; i++) {
      if (i != x && L1Cache[indexL1][i].counter < y) {
        L1Cache[indexL1][i].counter++;
      }
    }
  }

  return flag;
}

void printL1CacheData(unsigned long int assoc_1) {
  Cache temp;
  for (int i = 0; i < setsL1; i++) {
    for (unsigned long int j = 0; j < assoc_1; j++) {
      for (unsigned long int k = j + 1; k < assoc_1; k++) {
        if (L1Cache[i][k].counter < L1Cache[i][j].counter) {
          temp = L1Cache[i][j];
          L1Cache[i][j] = L1Cache[i][k];
          L1Cache[i][k] = temp;
        }
      }
    }
  }

  cout << "===== L1 contents =====" << endl;

  for (int i = 0; i < setsL1; i++) {
    cout << "set" << "  " << dec << i << ":" << "  ";
    for (unsigned long int j = 0; j < assoc_1; j++) {
      cout << hex << L1Cache[i][j].tag << " ";
      if (L1Cache[i][j].dirty == 1)
        cout << "D" << " ";
    }
    cout << endl;
  }
}

void printL2CacheData(unsigned long int assoc_2) {
  cout << endl;
  Cache temp;
  for (int i = 0; i < setsL2; i++) {
    for (unsigned long int j = 0; j < assoc_2; j++) {
      for (unsigned long int k = j + 1; k < assoc_2; k++) {
        if (L2Cache[i][k].counter < L2Cache[i][j].counter) {
          temp = L2Cache[i][j];
          L2Cache[i][j] = L2Cache[i][k];
          L2Cache[i][k] = temp;
        }
      }
    }
  }

  cout << "===== L2 contents =====" << endl;

  for (int i = 0; i < setsL2; i++) {
    cout << "set" << "  " << dec << i << ":" << "  ";
    for (unsigned long int j = 0; j < assoc_2; j++) {
      cout << hex << L2Cache[i][j].tag << " ";
      if (L2Cache[i][j].dirty == 1)
        cout << "D" << " ";
    }
    cout << endl;
  }
}

void printAllCounterData(unsigned long int size_2) {
  cout << endl;
  cout << "===== Measurements =====" << endl;
  cout << dec;
  cout << " a." << " " << "L1 reads:" << "                   " << readCounterL1 << endl;
  cout << " b." << " " << "L1 read misses:" << "             " << readMissCounterL1 << endl;
  cout << " c." << " " << "L1 writes:" << "                  " << writeCounterL1 << endl;
  cout << " d." << " " << "L1 write misses:" << "            " << writeMissCounterL1 << endl;
  cout << " e." << " " << "L1 miss rate:" << "               " << fixed << setprecision(4) << (float)(readMissCounterL1 + writeMissCounterL1) / (readCounterL1 + writeCounterL1) << endl;
  cout << "f." << " " << "L1 writebacks:" << "               " << writeBackCounterL1 << endl;
  cout << " g." << " " << "L1 prefetches:" << "              " << 0 << endl;

  cout << " h." << " " << "L2 reads (demand):" << "          " << readCounterL2 << endl;
  cout << " i." << " " << "L2 read misses (demand):" << "    " << readMissCounterL2 << endl;
  cout << " j." << " " << "L2 reads (prefetch):" << "        " << 0 << endl;
  cout << " k." << " " << "L2 read misses (prefetch):" << "  " << 0 << endl;
  cout << " l." << " " << "L2 writes:" << "                  " << writeCounterL2 << endl;
  cout << " m." << " " << "L2 write misses:" << "            " << writeMissCounterL2 << endl;
  if (size_2 > 0) {
    cout << " n." << " " << "L2 miss rate:" << "              " << fixed << setprecision(4) << (float)(readMissCounterL2) / (readCounterL2) << endl;
  } else {
    cout << " n." << " " << "L2 miss rate:" << "              " << fixed << setprecision(4) << (float) 0 << endl;
  }
  cout << " o." << " " << "L2 writebacks:" << "               " << writeBackL2ToMem << endl;
  cout << " p." << " " << "L2 prefetches:" << "              " << 0 << endl;
  cout << " q." << " " << "memory traffic:" << "             " << memoryTrafficCounter << endl;
}

int main(int argc, char * argv[]) {
  FILE * fp; // File pointer.
  char * trace_file; // This variable holds the trace file name.
  cache_params_t params; // Look at the sim.h header file for the definition of struct cache_params_t.
  char rw; // This variable holds the request's type (read or write) obtained from the trace.
  uint32_t addr; // This variable holds the request's address obtained from the trace.
  // The header file <inttypes.h> above defines signed and unsigned integers of various sizes in a machine-agnostic way.  "uint32_t" is an unsigned integer of 32 bits.

  // Exit with an error if the number of command-line arguments is incorrect.
  if (argc != 9) {
    printf("Error: Expected 8 command-line arguments but was provided %d.\n", (argc - 1));
    exit(EXIT_FAILURE);
  }

  // "atoi()" (included by <stdlib.h>) converts a string (char *) to an integer (int).
  params.BLOCKSIZE = (uint32_t) atoi(argv[1]);
  params.L1_SIZE = (uint32_t) atoi(argv[2]);
  params.L1_ASSOC = (uint32_t) atoi(argv[3]);
  params.L2_SIZE = (uint32_t) atoi(argv[4]);
  params.L2_ASSOC = (uint32_t) atoi(argv[5]);
  params.PREF_N = (uint32_t) atoi(argv[6]);
  params.PREF_M = (uint32_t) atoi(argv[7]);
  trace_file = argv[8];

  // Open the trace file for reading.
  fp = fopen(trace_file, "r");
  if (fp == (FILE * ) NULL) {
    // Exit with an error if file open failed.
    printf("Error: Unable to open file %s\n", trace_file);
    exit(EXIT_FAILURE);
  }

  // Print simulator configuration.
  printf("===== Simulator configuration =====\n");
  printf("BLOCKSIZE:  %u\n", params.BLOCKSIZE);
  printf("L1_SIZE:    %u\n", params.L1_SIZE);
  printf("L1_ASSOC:   %u\n", params.L1_ASSOC);
  printf("L2_SIZE:    %u\n", params.L2_SIZE);
  printf("L2_ASSOC:   %u\n", params.L2_ASSOC);
  printf("PREF_N:     %u\n", params.PREF_N);
  printf("PREF_M:     %u\n", params.PREF_M);
  printf("trace_file: %s\n", trace_file);

  L1CacheAddressCalculations(params.L1_SIZE, params.L1_ASSOC, params.BLOCKSIZE, addr);
  initL1Cache(params.L1_ASSOC);

  if (params.L2_SIZE > 0) {
    L2CacheAddressCalculations(params.L2_SIZE, params.L2_ASSOC, params.BLOCKSIZE, addr);
    initL2Cache(params.L2_ASSOC);
  }

  // Read requests from the trace file and echo them back.
  while (fscanf(fp, "%c %x\n", & rw, & addr) == 2) { // Stay in the loop if fscanf() successfully parsed two tokens as specified.
    if (rw == 'r')
      readRequestToL1(params.L1_SIZE, params.L1_ASSOC, params.L2_SIZE, params.L2_ASSOC, params.BLOCKSIZE, addr);
    else if (rw == 'w')
      writeRequestToL2(params.L1_SIZE, params.L1_ASSOC, params.L2_SIZE, params.L2_ASSOC, params.BLOCKSIZE, addr);
    else {
      printf("Error: Unknown request type %c.\n", rw);
      exit(EXIT_FAILURE);
    }
  }

  cout << endl;
  printL1CacheData(params.L1_ASSOC);
  if (params.L2_ASSOC > 0)
    printL2CacheData(params.L2_ASSOC);
  printAllCounterData(params.L2_SIZE);

  return (0);
}