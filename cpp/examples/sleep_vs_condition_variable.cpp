/**
 * sleep_for vs Condition Variables: When to Use Each
 *
 * This file demonstrates the fundamental difference between sleep_for and
 * condition variables, and provides guidance on when to use each approach.
 *
 * CORE PRINCIPLE:
 * - sleep_for: Use for TIME-BASED waiting
 * - Condition Variable: Use for EVENT-BASED waiting
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <fstream>

using namespace std;

// ==============================================================================
// Example 1: GOOD use of sleep_for - Rate Limiting
// ==============================================================================

void example_rate_limiting() {
    cout << "\n=== Example 1: Rate Limiting (GOOD use of sleep_for) ===\n";

    vector<string> api_requests = {"req1", "req2", "req3", "req4", "req5"};

    cout << "Sending API requests with rate limiting (max 2 req/sec)...\n";

    for (const auto& request : api_requests) {
        auto start = chrono::steady_clock::now();

        // Simulate sending request
        cout << "  Sending: " << request << endl;

        // GOOD: sleep_for for rate limiting
        this_thread::sleep_for(chrono::milliseconds(500));  // Max 2 requests per second

        auto elapsed = chrono::duration_cast<chrono::milliseconds>(
            chrono::steady_clock::now() - start
        ).count();
        cout << "    (elapsed: " << elapsed << "ms)" << endl;
    }

    cout << "\nWhy sleep_for is correct here:\n";
    cout << "  - We need a FIXED TIME DELAY between requests\n";
    cout << "  - No event to wait for - just enforcing time spacing\n";
    cout << "  - Condition variable would be unnecessarily complex\n";
}

// ==============================================================================
// Example 2: GOOD use of sleep_for - Retry Logic
// ==============================================================================

bool try_connect(int attempt) {
    cout << "  Attempt " << attempt << "... ";
    // Simulate connection (50% success rate)
    bool success = (rand() % 2) == 0;
    cout << (success ? "SUCCESS" : "FAILED") << endl;
    return success;
}

void example_retry_logic() {
    cout << "\n=== Example 2: Retry Logic (GOOD use of sleep_for) ===\n";

    bool connected = false;
    int attempt = 0;
    const int max_attempts = 5;

    while (!connected && attempt < max_attempts) {
        attempt++;
        connected = try_connect(attempt);

        if (!connected && attempt < max_attempts) {
            int wait_seconds = attempt;  // Exponential backoff: 1s, 2s, 3s, 4s
            cout << "  Waiting " << wait_seconds << " seconds before retry...\n";

            // GOOD: sleep_for for timed retry
            this_thread::sleep_for(chrono::seconds(wait_seconds));
        }
    }

    if (connected) {
        cout << "\nConnected successfully!\n";
    } else {
        cout << "\nFailed to connect after " << max_attempts << " attempts\n";
    }

    cout << "\nWhy sleep_for is correct here:\n";
    cout << "  - We need a FIXED TIME DELAY between retry attempts\n";
    cout << "  - This is exponential backoff - time-based strategy\n";
    cout << "  - No other thread will signal us when to retry\n";
}

// ==============================================================================
// Example 3: GOOD use of sleep_for - Fixed Frame Rate
// ==============================================================================

void example_fixed_framerate() {
    cout << "\n=== Example 3: Fixed Frame Rate (GOOD use of sleep_for) ===\n";

    const int target_fps = 60;
    const auto frame_duration = chrono::milliseconds(1000 / target_fps);  // ~16.67ms
    int frame_count = 0;
    const int total_frames = 10;

    cout << "Rendering at " << target_fps << " FPS...\n";

    while (frame_count < total_frames) {
        auto frame_start = chrono::steady_clock::now();

        // Simulate rendering work
        cout << "  Frame " << frame_count << " rendered\n";
        this_thread::sleep_for(chrono::milliseconds(5));  // Simulate work

        frame_count++;

        // Calculate how long to sleep to maintain framerate
        auto elapsed = chrono::steady_clock::now() - frame_start;
        auto sleep_time = frame_duration - elapsed;

        if (sleep_time > chrono::milliseconds(0)) {
            // GOOD: sleep_for to maintain fixed framerate
            this_thread::sleep_for(sleep_time);
        }
    }

    cout << "\nWhy sleep_for is correct here:\n";
    cout << "  - We need CONSISTENT TIMING (60 FPS)\n";
    cout << "  - This is time-based synchronization, not event-based\n";
    cout << "  - Condition variable doesn't fit this use case\n";
}

// ==============================================================================
// Example 4: GOOD use of sleep_for - Polling External Resources
// ==============================================================================

void example_polling_files() {
    cout << "\n=== Example 4: Polling Files (GOOD use of sleep_for) ===\n";

    const string filename = "/tmp/test_file_polling.txt";
    bool found = false;
    int checks = 0;

    cout << "Waiting for file to appear: " << filename << "\n";
    cout << "(Create the file in another terminal to see it detected)\n";

    // Simulate: create file after 2 seconds in background
    thread creator([&]() {
        this_thread::sleep_for(chrono::seconds(2));
        ofstream f(filename);
        f << "test content";
        f.close();
    });

    while (!found && checks < 10) {
        checks++;
        cout << "  Check " << checks << "... ";

        // Check if file exists
        ifstream f(filename);
        if (f.good()) {
            cout << "FOUND!\n";
            found = true;
        } else {
            cout << "not yet\n";
        }

        if (!found) {
            // GOOD: sleep_for for periodic polling
            this_thread::sleep_for(chrono::milliseconds(500));
        }
    }

    creator.join();
    remove(filename.c_str());  // Cleanup

    cout << "\nWhy sleep_for is correct here:\n";
    cout << "  - Polling EXTERNAL resource (filesystem)\n";
    cout << "  - No way for filesystem to 'notify' us\n";
    cout << "  - Fixed polling interval is the only option\n";
    cout << "  - (Note: inotify/filesystem watchers exist but are OS-specific)\n";
}

// ==============================================================================
// Example 5: BAD use of sleep_for - Waiting for Thread Events
// ==============================================================================

atomic<bool> data_ready_bad{false};

void example_bad_sleep_for_events() {
    cout << "\n=== Example 5: BAD - Using sleep_for for Events ===\n";

    thread producer([&]() {
        this_thread::sleep_for(chrono::milliseconds(250));
        cout << "  [Producer] Data ready at 250ms\n";
        data_ready_bad = true;
    });

    thread consumer([&]() {
        auto start = chrono::steady_clock::now();

        cout << "  [Consumer] Waiting for data...\n";

        // BAD: Using sleep_for to wait for event
        while (!data_ready_bad) {
            this_thread::sleep_for(chrono::milliseconds(100));
        }

        auto elapsed = chrono::duration_cast<chrono::milliseconds>(
            chrono::steady_clock::now() - start
        ).count();

        cout << "  [Consumer] Got data after " << elapsed << "ms\n";
    });

    producer.join();
    consumer.join();

    cout << "\nWhy this is BAD:\n";
    cout << "  - Producer signals at 250ms\n";
    cout << "  - Consumer checks at 0ms, 100ms, 200ms, 300ms\n";
    cout << "  - Data available at 250ms but not processed until 300ms\n";
    cout << "  - UNNECESSARY 50ms DELAY!\n";
    cout << "  - Should use condition variable instead\n";
}

// ==============================================================================
// Example 6: GOOD use of Condition Variable - Thread Events
// ==============================================================================

mutex cv_mutex;
condition_variable cv;
bool data_ready_good = false;

void example_good_condition_variable() {
    cout << "\n=== Example 6: GOOD - Using Condition Variable for Events ===\n";

    thread producer([&]() {
        this_thread::sleep_for(chrono::milliseconds(250));
        cout << "  [Producer] Data ready at 250ms\n";

        // Signal the event
        {
            lock_guard<mutex> lock(cv_mutex);
            data_ready_good = true;
        }
        cv.notify_one();  // Wake consumer immediately!
    });

    thread consumer([&]() {
        auto start = chrono::steady_clock::now();

        cout << "  [Consumer] Waiting for data...\n";

        // GOOD: Using condition variable for event
        {
            unique_lock<mutex> lock(cv_mutex);
            cv.wait(lock, []{ return data_ready_good; });
        }

        auto elapsed = chrono::duration_cast<chrono::milliseconds>(
            chrono::steady_clock::now() - start
        ).count();

        cout << "  [Consumer] Got data after " << elapsed << "ms\n";
    });

    producer.join();
    consumer.join();

    cout << "\nWhy this is GOOD:\n";
    cout << "  - Producer signals at 250ms\n";
    cout << "  - Consumer wakes IMMEDIATELY at 250ms\n";
    cout << "  - NO DELAY - processes data right away\n";
    cout << "  - This is the correct pattern for inter-thread events\n";
}

// ==============================================================================
// Example 7: Comparison - Event-Driven System
// ==============================================================================

// Version A: BAD - Using sleep_for (like example_integration.cpp original)
atomic<bool> new_data_sleep{false};

void example_event_system_sleep() {
    cout << "\n=== Example 7a: Event System with sleep_for (BAD) ===\n";

    thread data_source([&]() {
        for (int i = 1; i <= 3; i++) {
            this_thread::sleep_for(chrono::milliseconds(150));
            cout << "  [Source] Event " << i << " at " << (i * 150) << "ms\n";
            new_data_sleep = true;
        }
    });

    thread processor([&]() {
        auto start = chrono::steady_clock::now();

        for (int check = 0; check < 10; check++) {
            if (new_data_sleep.exchange(false)) {
                auto elapsed = chrono::duration_cast<chrono::milliseconds>(
                    chrono::steady_clock::now() - start
                ).count();
                cout << "    [Processor] Processed data at " << elapsed << "ms\n";
            }

            // BAD: Fixed sleep causes delays
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    });

    data_source.join();
    processor.join();

    cout << "\nProblem: Events at 150ms, 300ms, 450ms\n";
    cout << "         Checks at 0ms, 100ms, 200ms, 300ms, 400ms, 500ms...\n";
    cout << "         Event 1 (150ms) processed at 200ms - 50ms delay!\n";
    cout << "         Event 3 (450ms) processed at 500ms - 50ms delay!\n";
}

// Version B: GOOD - Using condition variable
mutex event_mutex;
condition_variable event_cv;
bool new_data_cv = false;

void example_event_system_cv() {
    cout << "\n=== Example 7b: Event System with Condition Variable (GOOD) ===\n";

    thread data_source([&]() {
        for (int i = 1; i <= 3; i++) {
            this_thread::sleep_for(chrono::milliseconds(150));
            cout << "  [Source] Event " << i << " at " << (i * 150) << "ms\n";

            {
                lock_guard<mutex> lock(event_mutex);
                new_data_cv = true;
            }
            event_cv.notify_one();
        }
    });

    thread processor([&]() {
        auto start = chrono::steady_clock::now();

        for (int i = 0; i < 3; i++) {
            {
                unique_lock<mutex> lock(event_mutex);
                event_cv.wait(lock, []{ return new_data_cv; });
                new_data_cv = false;
            }

            auto elapsed = chrono::duration_cast<chrono::milliseconds>(
                chrono::steady_clock::now() - start
            ).count();
            cout << "    [Processor] Processed data at " << elapsed << "ms\n";
        }
    });

    data_source.join();
    processor.join();

    cout << "\nSolution: Events at 150ms, 300ms, 450ms\n";
    cout << "          Processed at ~150ms, ~300ms, ~450ms\n";
    cout << "          IMMEDIATE processing - no delay!\n";
}

// ==============================================================================
// Decision Tree Summary
// ==============================================================================

void print_decision_tree() {
    cout << "\n\n";
    cout << "=========================================================================\n";
    cout << "                    DECISION TREE: sleep_for vs Condition Variable\n";
    cout << "=========================================================================\n";
    cout << "\n";
    cout << "Question: What am I waiting for?\n";
    cout << "\n";
    cout << "├─ TIME-BASED waiting (fixed delays, periodic tasks)\n";
    cout << "│  └─> USE sleep_for\n";
    cout << "│     Examples:\n";
    cout << "│     • Rate limiting (wait N ms between API calls)\n";
    cout << "│     • Retry logic with backoff (wait 1s, 2s, 4s...)\n";
    cout << "│     • Fixed frame rate (60 FPS = sleep 16.67ms)\n";
    cout << "│     • Polling external resources (check file every 100ms)\n";
    cout << "│     • Simulation delays (wait 5s between game ticks)\n";
    cout << "│\n";
    cout << "└─ EVENT-BASED waiting (waiting for something to happen)\n";
    cout << "   ├─ Event from ANOTHER THREAD\n";
    cout << "   │  └─> USE Condition Variable\n";
    cout << "   │     Examples:\n";
    cout << "   │     • WebSocket callback → main loop (your use case!)\n";
    cout << "   │     • Producer/consumer queues\n";
    cout << "   │     • Thread pool waiting for tasks\n";
    cout << "   │     • Graceful shutdown signals\n";
    cout << "   │\n";
    cout << "   └─ Event from EXTERNAL SOURCE (OS, filesystem, hardware)\n";
    cout << "      └─> USE sleep_for with polling\n";
    cout << "          (or OS-specific event mechanisms like epoll, inotify)\n";
    cout << "          Examples:\n";
    cout << "          • Waiting for file to appear\n";
    cout << "          • Waiting for network socket data (if not using select/poll)\n";
    cout << "          • Reading hardware sensor periodically\n";
    cout << "\n";
    cout << "=========================================================================\n";
    cout << "\n";
    cout << "REAL-WORLD EXAMPLES FROM YOUR CODEBASE:\n";
    cout << "\n";
    cout << "example_integration.cpp (sleep_for):\n";
    cout << "  while (running) {\n";
    cout << "      process_data();\n";
    cout << "      sleep_for(100ms);  // Periodic check - acceptable for some use cases\n";
    cout << "  }\n";
    cout << "  → Pros: Simple, easy to understand\n";
    cout << "  → Cons: Up to 100ms latency on new data\n";
    cout << "  → Best for: Applications where 100ms delay is acceptable\n";
    cout << "\n";
    cout << "example_integration_cond.cpp (condition variable):\n";
    cout << "  SIGNALER (WebSocket callback):\n";
    cout << "      {lock} data_available = true;\n";
    cout << "      cv.notify_one();\n";
    cout << "\n";
    cout << "  WAITER (Main loop):\n";
    cout << "      {lock} cv.wait(lock, predicate);\n";
    cout << "      process_data();  // Immediate!\n";
    cout << "  → Pros: Immediate response, no latency\n";
    cout << "  → Cons: Slightly more complex code\n";
    cout << "  → Best for: Low-latency systems (trading, real-time processing)\n";
    cout << "\n";
    cout << "=========================================================================\n";
}

// ==============================================================================
// Main
// ==============================================================================

int main() {
    cout << "=========================================================================\n";
    cout << "             sleep_for vs Condition Variables: Complete Guide\n";
    cout << "=========================================================================\n";

    // Good uses of sleep_for
    example_rate_limiting();
    example_retry_logic();
    example_fixed_framerate();
    example_polling_files();

    // Bad use of sleep_for (should use CV)
    data_ready_bad = false;
    example_bad_sleep_for_events();

    // Good use of condition variable
    data_ready_good = false;
    example_good_condition_variable();

    // Direct comparison
    new_data_sleep = false;
    example_event_system_sleep();

    new_data_cv = false;
    example_event_system_cv();

    // Summary
    print_decision_tree();

    cout << "\nAll examples completed!\n";
    cout << "\nKEY TAKEAWAY:\n";
    cout << "  sleep_for = TIME-based waiting\n";
    cout << "  Condition Variable = EVENT-based waiting between threads\n";
    cout << "\n";

    return 0;
}
