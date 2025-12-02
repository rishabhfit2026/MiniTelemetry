#include <gtest/gtest.h>
#include <thread>
#include "../src/core/thread_safe_queue.h"

// Test 1: Does it push and pop a basic value?
TEST(QueueTest, BasicPushPop) {
    ThreadSafeQueue<int> q;
    q.push(42);
    
    int val = 0;
    bool success = q.pop(val);
    
    EXPECT_TRUE(success);
    EXPECT_EQ(val, 42);
}

// Test 2: Does it handle multiple threads correctly?
TEST(QueueTest, MultiThreadedPush) {
    ThreadSafeQueue<int> q;
    std::atomic<int> count{0};

    // Consumer thread
    std::thread consumer([&]() {
        int val;
        while(q.pop(val)) {
            count++;
            if(count == 100) break;
        }
    });

    // Producer threads
    std::vector<std::thread> producers;
    for(int i = 0; i < 10; ++i) {
        producers.emplace_back([&]() {
            for(int j=0; j<10; ++j) q.push(1);
        });
    }

    // Wait for producers
    for(auto& t : producers) t.join();
    
    // Stop queue
    q.stop();
    consumer.join();

    EXPECT_EQ(count, 100);
}