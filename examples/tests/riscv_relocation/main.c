// riscv_relocation: checks that the addresses stored in variables are correct
// wherever the app runs. Prints where main() and a variable ended up, and PASS
// or FAIL.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libtock-sync/services/alarm.h>
#include <libtock/tock.h>

static volatile int counter = 5;                              // a variable
volatile int* counter_ptr   = &counter;                       // holds a variable's address
const char* msg = "hello";                                    // holds a string's address
static int big_buf[600];                                      // a zeroed variable
static void cb(void) { counter++; }
static void cb2(void) { counter += 2; }
void (*table[2])(void) = { cb, cb2 };                         // holds code addresses
volatile int* const ro_ptrs[2] = { &counter, &big_buf[0] };   // constant, holds variable addresses
void(*const ro_fns[2])(void) = { cb, cb2 };                   // constant, holds code addresses
const char* const ro_strs[3] = { "alpha", "beta", "gamma" };  // constant, holds string addresses
static volatile int zero = 0, one = 1, two = 2;               // stop the compiler pre-computing
extern void not_linked(void) __attribute__((weak));           // a missing weak function: NULL

// A static local pointer is a variable too, with a starting value.
__attribute__((noinline)) static int* next_from_pool(void) {
  static int pool[4];
  static int* sp = pool;
  return sp++;
}

// A switch, which the compiler turns into a table of code addresses (or of
// distances, with -fPIC). Reading a volatile in each case stops the compiler
// from replacing the switch with arithmetic.
__attribute__((noinline)) static int sw(int x) {
  switch (x) {
    case 0: return one + 10;
    case 1: return two * 11;
    case 2: return zero + 33;
    case 3: return one * 44;
    case 4: return two + 53;
    case 5: return one + 65;
    case 6: return two + 75;
    default: return zero;
  }
}

int main(void) {
  int ok = 1;
  table[zero]();
  table[one]();
  ro_fns[zero]();
  ro_fns[one]();                                                // 5 + 1 + 2 + 1 + 2 = 11
  if (counter != 11 || *counter_ptr != 11 || ro_ptrs[zero] != &counter) ok = 0;
  if (strcmp(msg, "hello") != 0 || strcmp(ro_strs[two], "gamma") != 0) ok = 0;
  int* a = next_from_pool();
  int* b = next_from_pool();
  if (b != a + 1) ok = 0;
  *b = 7;
  int s = 0;
  for (int i = 0; i < 8; i++) s += sw(i);                        // 308
  if (s != 308) ok = 0;
  if (not_linked != NULL) ok = 0;                                // must stay NULL after the move
  big_buf[599] = 42;
  if (big_buf[599 - zero] != 42) ok = 0;
  libtocksync_alarm_delay_ms(5);                                 // the kernel calls back into the app
  char* p = malloc(32);
  snprintf(p, 32, "%s-%d", ro_strs[one], counter);
  if (p == NULL || strcmp(p, "beta-11") != 0) ok = 0;
  printf("riscv_relocation main=%p data=%p %s\n", (void*)main, (void*)&counter, ok ? "PASS" : "FAIL");
  // Returning calls exit(), which checks for a missing weak function.
  return 0;
}
