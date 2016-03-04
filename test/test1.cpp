//
// Created by Leonard Gerard on 12/29/15.
//


#include <gtest/gtest.h>

#include "lvb.hpp"

#include <thread>


template<typename T, size_t size>
bool spin_until_read(Lvb<T,size>& b, unsigned int max_spin) {
    unsigned int n = 0;
    while(b.reader_advance() && n < max_spin) {
        n++;
        std::this_thread::yield();
    }
    return (n < max_spin); // the reader advanced
}

template<typename T, size_t size>
bool spin_until_read_one(Lvb<T,size>& b, unsigned int max_spin) {
    unsigned int n = 0;
    while(b.reader_advance_one() && n < max_spin) {
        n++;
        std::this_thread::yield();
    }
    return (n < max_spin); // the reader advanced
}


/* Basic tests are single threaded.
 * They allow to assess the queue behavior. A read should be stale iff there
 * hasn't been a new read since last read.
 */


TEST(Basic, PutNGetN) {
    Lvb<int> b;
    for(int i=0; i<100; i++){
        b.put(42+i);
        ASSERT_EQ(42+i, b.get());
    }
}

TEST(Basic, ReaderStaleCntInit) {
    Lvb<int> b;
    int stale_cnt = -42;
    for (int i=0; i<100; i++) {
        b.get(stale_cnt);
        ASSERT_EQ(-i-1, stale_cnt);
    }
}

TEST(Basic, ReaderStaleCntIncrease) {
    Lvb<int> b;
    int stale_cnt = -42;
    b.put(33);
    for (int i=0; i<100; i++){
        b.get(stale_cnt);
        ASSERT_EQ(i, stale_cnt);
    }
}

TEST(Basic, ReaderStaleCntReset) {
    Lvb<int> b;
    int stale_cnt = -42;
    b.get(stale_cnt);
    ASSERT_EQ(-1, stale_cnt);
    b.put(33);
    b.get(stale_cnt);
    ASSERT_EQ(0, stale_cnt);
    b.get(stale_cnt);
    ASSERT_EQ(1, stale_cnt);
    b.put(33);
    b.get(stale_cnt);
    ASSERT_EQ(0, stale_cnt);
    b.get(stale_cnt);
    ASSERT_EQ(1, stale_cnt);
}


TEST(Types, pint) {
    Lvb<int*> b;
    int a[10];
    for(int i=0; i<10; i++){
        b.put(&a[i]);
        ASSERT_EQ(&a[i], b.get());
    }
}

TEST(Types, nonCopyable) {
    class NC {
    public:
        NC(const NC&) = delete;
        NC() { s = std::rand(); };
        NC& operator=(const NC&) = delete;
        int val() { return s; }
    private:
        int s;
    };
    Lvb<NC> b;
    new (b.writing) NC();
    NC & x = *b.writing;
    b.writer_advance();
    b.reader_advance();

    ASSERT_EQ(x.val(), b.reading->val());
}

TEST(Types, nonCopyableButMovable) {
    class NCBM {
    public:
      NCBM(const NCBM&) = delete;
      NCBM& operator=(const NCBM&) = delete;

      NCBM() { s = std::rand(); };
      NCBM& operator=(const NCBM&& x) { s = x.s; return *this; };

      int val() { return s; }
    private:
      int s;
    };
    Lvb<NCBM> b;
    NCBM x;
    b.put(std::move(x));
    b.reader_advance();
    ASSERT_EQ(x.val(), b.reading->val());
}

TEST(Types, Movable) {
    class M {
    public:
      M(const M&) = delete;
      M& operator=(const M&) = delete;

      M() { s = std::rand(); };
      M(const M&& x) { s = x.s; };
      M& operator=(const M&& x) { s = x.s; return *this; };

      int val() { return s; }
    private:
      int s;
    };
    Lvb<M> b;
    M x;
    int stale;

    b.put(std::move(x));
    ASSERT_EQ(x.val(), b.pop(stale).val());
    b.pop(stale);
    ASSERT_TRUE(stale);
}



/*
 * Two threads... behavior of the tests are much harder to test.
 */


constexpr unsigned int maxspin = 100000;
constexpr std::chrono::nanoseconds synclatency(8000);

TEST(TwoThreads, Put1Get1) {
    Lvb<int> b;
    //producer
    std::function<void()> p = [&b](){
        b.put(42);
        std::this_thread::sleep_for(synclatency);
    };
    //consummer
    std::function<void()> c = [&b]() {
        ASSERT_TRUE(spin_until_read(b, maxspin));
        ASSERT_EQ(42, *b.reading);
    };
    auto tp = std::thread::thread(p);
    auto tc = std::thread::thread(c);
    tp.join();
    tc.join();
}

TEST(TwoThreads, SlackNPutNGetN) {
    Lvb<int, 100> b; // We use a queue of slack 100
    //producer
    std::function<void()> p = [&b](){
        for(int i=0; i<100; i++) {
            ASSERT_FALSE(b.put(i)); // Shouldn't be stale (slack bigger)
        }
    };
    //consummer
    std::function<void()> c = [&b]() {
        int x;
        // Give a headsup to prod to increase failure chances
        std::this_thread::sleep_for(synclatency);
        do {
            spin_until_read(b, maxspin);
            x = *b.reading;
            ASSERT_LE(0, x);
            ASSERT_LE(x, 99);
        } while (x < 99);

    };
    auto tp = std::thread::thread(p);
    auto tc = std::thread::thread(c);
    tp.join();
    tc.join();
}

TEST(TwoThreads, Slack1PutNGetN) {
    Lvb<int> b;
    //producer
    std::function<void()> p = [&b](){
        int i = 0;
        while (i < 100000) {
            if (!b.put(i)) {
                i++;
            }
        }
    };
    //consummer
    std::function<void()> c = [&b]() {
        int x = 0;
        int stale = 0;
        // Give a headsup to prod to increase failure chances
        std::this_thread::sleep_for(synclatency);
        do {
            b.get(stale);
            if (!stale) {
                ASSERT_EQ(x, *b.reading);
                x++;
            }
        } while (x < 99999);
    };
    auto tp = std::thread::thread(p);
    auto tc = std::thread::thread(c);
    tp.join();
    tc.join();
}


TEST(TwoThreads, SlackNPutMGetNp) {
    Lvb<int> b;
    int last_commited = -1;
    int last_read = -2;

    //producer
    std::function<void()> p = [&](){
        for(int i=0; i<100000; i++) {
            if (!b.put(i)) {
                //we commited i
                last_commited = i;
            };
        }
    };
    //consummer
    std::function<void()> c = [&]() {
        // Give a headsup to prod to increase failure chances
        std::this_thread::sleep_for(synclatency);
        while(spin_until_read(b, maxspin)) {
            last_read = *b.reading;
            ASSERT_LE(0, last_read);
            ASSERT_LE(last_read, 99999);
        }
    };
    auto tp = std::thread::thread(p);
    auto tc = std::thread::thread(c);
    tp.join();
    tc.join();
    ASSERT_EQ(last_read, last_commited);
}

