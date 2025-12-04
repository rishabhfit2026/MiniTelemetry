#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "../src/core/thread_safe_queue.h"
#include "../src/core/telemetry_types.h"

TEST(ThreadSafeQueueTest, BasicPushPop) {
    ThreadSafeQueue<int> queue;
    
    queue.push(42);
    queue.push(100);
    
    int value;
    EXPECT_TRUE(queue.pop(value));
    EXPECT_EQ(42, value);
    
    EXPECT_TRUE(queue.pop(value));
    EXPECT_EQ(100, value);
}

TEST(ThreadSafeQueueTest, ProducerConsumer) {
    ThreadSafeQueue<SensorData> queue;
    const int num_items = 100;
    
    // Producer thread
    std::thread producer([&queue, num_items]() {
        for (int i = 0; i < num_items; ++i) {
            SensorData data;
            data.id = i;
            data.value = i * 1.5;
            data.timestamp = i;
            queue.push(data);
        }
    });
    
    // Consumer thread
    std::vector<SensorData> received;
    std::thread consumer([&queue, &received, num_items]() {
        for (int i = 0; i < num_items; ++i) {
            SensorData data;
            if (queue.pop(data)) {
                received.push_back(data);
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    EXPECT_EQ(num_items, received.size());
    
    // Verify first and last items
    EXPECT_EQ(0, received[0].id);
    EXPECT_EQ(99, received[99].id);
}

TEST(ThreadSafeQueueTest, StopSignal) {
    ThreadSafeQueue<int> queue;
    
    queue.push(1);
    queue.push(2);
    queue.stop();
    
    int value;
    EXPECT_TRUE(queue.pop(value));  // Should get 1
    EXPECT_TRUE(queue.pop(value));  // Should get 2
    EXPECT_FALSE(queue.pop(value)); // Should return false (stopped)
}