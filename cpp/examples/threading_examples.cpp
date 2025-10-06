/**
 * Threading and Synchronization Examples
 *
 * This file demonstrates common C++ threading patterns and synchronization
 * mechanisms to handle events and coordination between threads.
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vector>

using namespace std;

// ==============================================================================
// Example 1: sleep_for vs sleep
// ==============================================================================

void example_sleep_comparison() {
    cout << "\n=== Example 1: sleep_for vs sleep ===\n";

    // C++ standard way (portable, type-safe)
    this_thread::sleep_for(chrono::milliseconds(100));
    this_thread::sleep_for(chrono::seconds(1));

    // POSIX way (platform-specific, less portable)
    // sleep(1);  // Requires <unistd.h>, seconds only, Unix/Linux only
    // Sleep(1000);  // Windows version, requires <windows.h>, milliseconds

    cout << "Prefer this_thread::sleep_for for:\n";
    cout << "  - Portability across platforms\n";
    cout << "  - Type safety with std::chrono\n";
    cout << "  - Integration with C++ threading model\n";
}

// ==============================================================================
// Example 2: Problem - Missing events during sleep
// ==============================================================================

atomic<bool> g_simple_flag{true};

void example_missing_events_problem() {
    cout << "\n=== Example 2: Missing Events Problem ===\n";

    thread worker([&]() {
        while (g_simple_flag) {
            cout << "Working...\n";
            this_thread::sleep_for(chrono::seconds(2));  // Long sleep!
        }
        cout << "Worker stopped\n";
    });

    this_thread::sleep_for(chrono::milliseconds(500));

    cout << "Setting flag to false...\n";
    g_simple_flag = false;  // Flag set during sleep

    cout << "Waiting for worker to notice... (could take up to 2 seconds!)\n";
    worker.join();

    cout << "\nProblem: Thread checks flag only after sleep completes.\n";
    cout << "Solution: Use condition variables or shorter sleep intervals.\n";
}

// ==============================================================================
// Example 3: Condition Variable - Proper Pattern
// ==============================================================================

mutex cv_mtx;
condition_variable cv;
bool event_occurred = false;

void example_condition_variable() {
    cout << "\n=== Example 3: Condition Variable (Correct Pattern) ===\n";

    // Thread 1: Waiter
    thread waiter([&]() {
        cout << "Waiter: Waiting for event...\n";

        unique_lock<mutex> lock(cv_mtx);
        cv.wait(lock, []{ return event_occurred; });
        // Alternative with timeout:
        // cv.wait_for(lock, chrono::seconds(5), []{ return event_occurred; });

        cout << "Waiter: Event received! Doing work...\n";
    });

    // Thread 2: Signaler
    thread signaler([&]() {
        this_thread::sleep_for(chrono::seconds(1));

        cout << "Signaler: Event happening now!\n";

        // BOTH steps required:
        // 1. Set the flag (under mutex protection)
        {
            lock_guard<mutex> lock(cv_mtx);
            event_occurred = true;
        }

        // 2. Notify waiting threads
        cv.notify_one();  // Or notify_all() for multiple waiters
    });

    waiter.join();
    signaler.join();

    cout << "\nKey points:\n";
    cout << "  1. Shared state (event_occurred) protected by mutex\n";
    cout << "  2. wait() automatically releases lock while sleeping\n";
    cout << "  3. Signaler must BOTH set flag AND call notify\n";
    cout << "  4. Thread wakes immediately when notified (no sleep delay!)\n";
}

// ==============================================================================
// Example 4: What happens if you forget to notify?
// ==============================================================================

mutex forgotten_mtx;
condition_variable forgotten_cv;
bool forgotten_event = false;

void example_forgotten_notify() {
    cout << "\n=== Example 4: Forgetting to Notify (Common Mistake) ===\n";

    thread waiter([&]() {
        unique_lock<mutex> lock(forgotten_mtx);

        // Use wait_for with timeout to avoid hanging forever
        bool woke_up = forgotten_cv.wait_for(
            lock,
            chrono::seconds(2),
            []{ return forgotten_event; }
        );

        if (woke_up) {
            cout << "Waiter: Woke up because event occurred\n";
        } else {
            cout << "Waiter: Timeout! Event flag=" << forgotten_event << "\n";
        }
    });

    thread bad_signaler([&]() {
        this_thread::sleep_for(chrono::milliseconds(500));

        // BAD: Set flag but forget to notify
        {
            lock_guard<mutex> lock(forgotten_mtx);
            forgotten_event = true;
        }
        // forgotten_cv.notify_one();  // FORGOT THIS!

        cout << "Bad Signaler: Set flag but forgot to notify!\n";
    });

    waiter.join();
    bad_signaler.join();

    cout << "\nLesson: Always call notify after changing condition!\n";
}

// ==============================================================================
// Example 5: Atomic vs Mutex for shared state
// ==============================================================================

// Pattern A: Mutex-protected bool (standard)
mutex mtx_a;
condition_variable cv_a;
bool flag_a = false;

// Pattern B: Atomic bool (alternative)
condition_variable cv_b;
mutex mtx_b;  // Still need mutex for cv
atomic<bool> flag_b{false};

void example_atomic_vs_mutex() {
    cout << "\n=== Example 5: Atomic vs Mutex with Condition Variables ===\n";

    cout << "\nPattern A (RECOMMENDED): Mutex-protected bool\n";
    thread waiter_a([&]() {
        unique_lock<mutex> lock(mtx_a);
        cv_a.wait(lock, []{ return flag_a; });
        cout << "  Waiter A: Event received\n";
    });

    thread signaler_a([&]() {
        this_thread::sleep_for(chrono::milliseconds(100));
        {
            lock_guard<mutex> lock(mtx_a);
            flag_a = true;  // Regular bool, protected by mutex
        }
        cv_a.notify_one();
        cout << "  Signaler A: Notified (mutex-protected bool)\n";
    });

    waiter_a.join();
    signaler_a.join();

    cout << "\nPattern B (WORKS BUT LESS COMMON): Atomic bool\n";
    thread waiter_b([&]() {
        unique_lock<mutex> lock(mtx_b);
        cv_b.wait(lock, []{ return flag_b.load(); });
        cout << "  Waiter B: Event received\n";
    });

    thread signaler_b([&]() {
        this_thread::sleep_for(chrono::milliseconds(100));
        flag_b.store(true);  // Atomic, no lock needed for flag itself
        cv_b.notify_one();   // But still need cv for waking
        cout << "  Signaler B: Notified (atomic bool)\n";
    });

    waiter_b.join();
    signaler_b.join();

    cout << "\nWhy prefer Pattern A?\n";
    cout << "  - Condition variables internally require mutex coordination\n";
    cout << "  - Mixing sync primitives (atomic + cv) is less clear\n";
    cout << "  - Standard pattern is more maintainable\n";
    cout << "\nWhen to use atomic alone?\n";
    cout << "  - Simple flag polling (like g_running in example_integration.cpp)\n";
    cout << "  - No need to wake threads immediately\n";
    cout << "  - Lock-free algorithms\n";
}

// ==============================================================================
// Example 6: Multiple waiting threads (notify_one vs notify_all)
// ==============================================================================

mutex multi_mtx;
condition_variable multi_cv;
bool multi_event = false;
int worker_count = 0;

void example_notify_multiple() {
    cout << "\n=== Example 6: notify_one vs notify_all ===\n";

    const int NUM_WORKERS = 3;
    vector<thread> workers;

    for (int i = 0; i < NUM_WORKERS; i++) {
        workers.emplace_back([i]() {
            unique_lock<mutex> lock(multi_mtx);
            cout << "  Worker " << i << ": Waiting...\n";

            multi_cv.wait(lock, []{ return multi_event; });

            cout << "  Worker " << i << ": Woke up!\n";
            worker_count++;
        });
    }

    this_thread::sleep_for(chrono::milliseconds(500));

    cout << "\nSignaler: Broadcasting event to ALL workers...\n";
    {
        lock_guard<mutex> lock(multi_mtx);
        multi_event = true;
    }
    multi_cv.notify_all();  // Wake ALL waiting threads

    for (auto& w : workers) {
        w.join();
    }

    cout << "\nAll " << worker_count << " workers completed.\n";
    cout << "\nUse notify_one() when: Only one thread should handle event\n";
    cout << "Use notify_all() when: All threads should wake and check condition\n";
}

// ==============================================================================
// Example 7: Graceful shutdown pattern (like example_integration.cpp)
// ==============================================================================

atomic<bool> g_running{true};
mutex shutdown_mtx;
condition_variable shutdown_cv;

void example_graceful_shutdown() {
    cout << "\n=== Example 7: Graceful Shutdown Pattern ===\n";

    thread worker([&]() {
        cout << "Worker: Starting...\n";

        while (g_running) {
            // Simulate work
            cout << "Worker: Processing...\n";

            // IMPROVED: Use cv with timeout instead of plain sleep
            unique_lock<mutex> lock(shutdown_mtx);
            shutdown_cv.wait_for(lock, chrono::seconds(1), []{ return !g_running; });

            // Wakes up either after 1 second OR immediately when g_running becomes false
        }

        cout << "Worker: Shutting down gracefully...\n";
    });

    this_thread::sleep_for(chrono::milliseconds(1500));

    cout << "\nMain: Requesting shutdown...\n";
    g_running = false;
    shutdown_cv.notify_all();  // Wake worker immediately

    worker.join();
    cout << "Main: Worker stopped cleanly\n";

    cout << "\nThis pattern combines:\n";
    cout << "  - Atomic flag for simple state check\n";
    cout << "  - Condition variable for immediate wakeup\n";
    cout << "  - Periodic timeout for safety/progress\n";
}

// ==============================================================================
// Main - Run all examples
// ==============================================================================

int main() {
    cout << "=========================================\n";
    cout << "C++ Threading and Synchronization Examples\n";
    cout << "=========================================\n";

    example_sleep_comparison();

    g_simple_flag = true;  // Reset
    example_missing_events_problem();

    event_occurred = false;  // Reset
    example_condition_variable();

    forgotten_event = false;  // Reset
    example_forgotten_notify();

    flag_a = false;  // Reset
    flag_b = false;
    example_atomic_vs_mutex();

    multi_event = false;  // Reset
    worker_count = 0;
    example_notify_multiple();

    g_running = true;  // Reset
    example_graceful_shutdown();

    cout << "\n=========================================\n";
    cout << "All examples completed!\n";
    cout << "=========================================\n";

    return 0;
}
