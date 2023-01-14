#ifndef WORKER_H
#define WORKER_H

#include <mutex>
#include <map>
/* #include <memory> */
#include <set>
#include "CPU/spim.h"
#include "CPU/image.h"

extern std::map<unsigned int, MIPSImage> ctxs;
extern std::timed_mutex simulator_mtx;
extern bool simulator_ready;

// Starts the SPIM simulator with max_contexts
//
// The program may not use certain contexts as in certain
// cases we want only want one instance running in a particular
// context (e.g. testing a singular bot for Spimbot as bot 1)
void start_simulator(unsigned int max_contexts, std::set<unsigned int> active_ctxs);

void reset(unsigned int max_contexts, std::set<unsigned int> &active_ctxs);
void step(unsigned additional_steps);
void play_simulation();
void pause_simulation();
bool add_breakpoint(int ctx, mem_addr addr);
int delete_breakpoint(int ctx, mem_addr addr);  
void set_speed(unsigned long delay_usec);
int get_simulator_status();

#endif
