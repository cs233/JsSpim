#include "worker.h"
#include <chrono>
#include <mutex>
#include <optional>
#include <thread>
#include <condition_variable>
#include <utility>

#include "CPU/scanner.h"
#include "CPU/spim-utils.h"
#include "CPU/spim.h"

std::map<unsigned int, MIPSImage> ctxs;
std::timed_mutex simulator_mtx; // Mutex for locking the simulator. Will be jointly used by main UI, message handler, and simulator thread
bool simulator_ready = false;
static std::thread simulator_thread;

//  1 - Finished
//  2 - Not running
//  3 - Incremented by at least a step since last check
// -1 - Breakpoint encountered
//  0 - Status reset
enum SimulatorStatusCode {
    BREAKPOINT_ENCOUNTERED = -1,
    NO_CHANGE = 0,
    FINISHED_RUNNING = 1,
    SIMULATOR_WAITING = 2,
    STEPPED_CYCLE = 3
};

static SimulatorStatusCode status = SimulatorStatusCode::NO_CHANGE;

static bool finished = false;
static std::optional<unsigned long> steps_left = 0;
static unsigned long cycle_delay_usec = 0; // no need to lock since there is 1 writer
static std::mutex settings_mtx; // Mutex for external settings that can be changed during runtime
static std::condition_variable steps_left_cv;
static unsigned long cycles_elapsed = 0;

int simulate();

void start_simulator(unsigned int max_contexts, std::set<unsigned int> active_ctxs) {
    reset(max_contexts, active_ctxs);

    // TODO: Add wait for incoming messages 
    // Have two distinct threads: one for message processing, one for the simulator
    // Reason why? to prevent overworking the simulator thread and to allow for messages
    // to be delivered quicker.
    //
    // Actually, what if we can use the proxy queue? Idk how useful that would be
}

void shutdown() {
    {
        std::lock_guard<std::mutex> lock(settings_mtx);
        finished = true;
        steps_left_cv.notify_all();
    }
    
    if (simulator_thread.joinable()) {
        simulator_thread.join();
    }
}

void reset(unsigned int max_contexts, std::set<unsigned int> &active_ctxs) {
    shutdown();

    // Since we join if a thread already exists, we dont need to lock. YIPPEE!
    /* std::lock(simulator_mtx, settings_mtx); */
    /* std::lock_guard<std::mutex> lk1(simulator_mtx, std::adopt_lock); */
    /* std::lock_guard<std::mutex> lk2(settings_mtx, std::adopt_lock); */
    
    ctxs.clear();

    for (unsigned int i : active_ctxs) {
        if (i >= max_contexts) {
            continue;
        }
        MIPSImage new_image(i);
        initialize_world(new_image, DEFAULT_EXCEPTION_HANDLER, false);
        initialize_run_stack(new_image, 0, nullptr);
        char file_name[64];
        sprintf(file_name, "./input_%d.s", i);
        if (read_assembly_file(new_image, file_name)) { // check if the file exists
            new_image.reg_image().PC = starting_address(new_image);
            ctxs.emplace(i, std::move(new_image));
        }
        yylex_destroy();
    }
    finished = false;
    steps_left = 0;

    if (cycles_elapsed) {
        fprintf(stderr, "The last program ran for %lu cycles!\n", cycles_elapsed);
        fflush(stderr);
    }
    cycles_elapsed = 0;
    simulator_ready = true;

    simulator_thread = std::thread(simulate);
}

void step_simulation(unsigned additional_steps = 1) {
    std::lock_guard<std::mutex> lock(settings_mtx);
    steps_left = steps_left.value_or(0) + additional_steps;
    steps_left_cv.notify_all();
}

// Play or continue simulation
void play_simulation() {
    std::lock_guard<std::mutex> lock(settings_mtx);
    steps_left.reset();
    steps_left_cv.notify_all();
} 

void pause_simulation() {
    std::lock_guard<std::mutex> lock(settings_mtx);
    steps_left = 0;
    steps_left_cv.notify_all();
}

// Called by main thread
//
// Return codes:
// 0 - Added breakpoint successfully
// 1 - Failed to add breakpoint to context ctx
// 2 - ctx does not exist
int delete_breakpoint(int ctx, mem_addr addr) {
    std::lock_guard<std::timed_mutex> lock(simulator_mtx);
    if (auto search = ctxs.find(ctx); search != ctxs.end()) {
        return !delete_breakpoint(search->second, addr);
    }
    return 2;
}

// Called by main thread
//
// Return codes:
// 0 - Added breakpoint successfully
// 1 - Failed to add breakpoint to context ctx
// 2 - ctx does not exist
bool add_breakpoint(int ctx, mem_addr addr) {
    std::lock_guard<std::timed_mutex> lock(simulator_mtx);
    if (auto search = ctxs.find(ctx); search != ctxs.end()) {
        return !add_breakpoint(search->second, addr);
    }
    return 2;
}

// Called by main thread
void set_speed(unsigned long delay_usec) {
    cycle_delay_usec = delay_usec;
}

int get_simulator_status() {
    int status_to_return = static_cast<int>(status);
    status = SimulatorStatusCode::NO_CHANGE;
    return status_to_return;
}

// Simulator thread
// Returns status code
int simulate() {
    std::unique_lock<std::mutex> ul(settings_mtx);
    bool cont_bkpt = false;
    while (true) {
        unsigned long delay_usec = cycle_delay_usec;
        bool continue_after_delay = false;
        while (!finished && (steps_left.value_or(1) == 0 || (delay_usec && !continue_after_delay))) { // check if it should step again (if not set, continue)
            if (steps_left.value_or(1) == 0) {
                status = SimulatorStatusCode::SIMULATOR_WAITING;
                steps_left_cv.wait(ul);
            } else {
                steps_left_cv.wait_for(ul, std::chrono::microseconds(delay_usec));
            }
            continue_after_delay = true;
        }
        if (finished) {
            break;
        }

        if (status != SimulatorStatusCode::STEPPED_CYCLE) {
            status = SimulatorStatusCode::NO_CHANGE;
        }

        if (steps_left) {
            steps_left.value()--;
        }

        ul.unlock();

/*         if (cycle_delay_usec) { */
/*             std::this_thread::sleep_for(std::chrono::microseconds(cycle_delay_usec)); */
/*         } */

        // Run 1 step in simulator
        cycle_result_t result;
        {
            std::lock_guard<std::timed_mutex> lock(simulator_mtx);

            // call step function
            // should return back status code
            // possible outcomes
            // 
            // breakpoint occurred at what ctx
            // some ctx finished
            
            result = run_spim_cycle_multi_ctx(ctxs, cont_bkpt);
            cycles_elapsed++;
        }

        ul.lock();
        status = SimulatorStatusCode::STEPPED_CYCLE;
        cont_bkpt = false;

        if (result.finished_ctxs.size()) {
            for (auto &ctx_num : result.finished_ctxs) {
                error(ctxs.at(ctx_num), "Execution finished\n");
            }
            finished = true;
            status = SimulatorStatusCode::FINISHED_RUNNING;
            break;
        } else if (result.bp_encountered_ctxs.size()) {
            cont_bkpt = true;
            steps_left = 0;
            status = SimulatorStatusCode::BREAKPOINT_ENCOUNTERED;
            for (auto &[ctx_num, bkpt_addr] : result.bp_encountered_ctxs) {
                error(ctxs.at(ctx_num), "Breakpoint encountered at 0x%08x\n", bkpt_addr);
            }
        }
    }

    // Flush all buffers
    for (auto &[ctx, img] : ctxs) {
        std::ostream os_out(img.get_std_out_buf());
        std::ostream os_err(img.get_std_err_buf());

        os_out.flush();
        os_err.flush();
    }
    fprintf(stderr, "Cycles elpased: %lu\n", cycles_elapsed);
    fflush(stderr);
    // TODO: send status code here aand also return it
    return 0;
}
