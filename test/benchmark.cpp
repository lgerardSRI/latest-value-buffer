//
// Created by Leonard Gerard on 2/18/16.
//

#include <iostream>
#include <thread>

#include "lvb.hpp"


typedef std::chrono::high_resolution_clock::time_point time_point;
typedef std::chrono::nanoseconds nanoseconds;


void counting_writer(time_point start, nanoseconds period, Lvb<uint64_t, 4> &b,
                     bool &stop, uint64_t &steps, int &maxstale, uint64_t &pushed) {
  int stale;
  steps = maxstale = pushed = 0;
  while (!stop) {
    steps++;
    stale = b.put(steps);
    if (stale) {
      if (stale > maxstale) maxstale = stale;
    }
    else {
      pushed++;
    }
    start += period;
    std::this_thread::sleep_until(start);
  }
}


void counting_reader(time_point start, nanoseconds period, Lvb<uint64_t, 4> &b,
                     bool &stop, uint64_t &steps, int &maxstale, int &maxdiff, uint64_t &poped) {
  int stale;
  int diff;
  steps = maxdiff = maxstale = poped = 0;
  while(!stop) {
    steps++;
    diff = steps - b.get(stale);
    if (stale) {
      if (stale > maxstale) maxstale = stale;
    }
    else {
      poped++;
      if (diff > maxdiff) maxdiff = diff;
    }
    start += period;
    std::this_thread::sleep_until(start);
  }
}


int main(int argc, char * argv[]) {

  if (argc < 3) {
    std::cout << "Usage : "
              << argv[0] << "slack prod_period con_period"
              << std::endl;
    exit(-1);
  }
  size_t slack = atoi(argv[1]);
  std::chrono::nanoseconds prod_period(atoi(argv[2]));
  std::chrono::nanoseconds con_period(atoi(argv[2]));

  Lvb<uint64_t, 4> b;

  bool stop = false;
  uint64_t prod_steps, con_steps, prod_pushed, con_poped;
  int prod_maxstale, con_maxstale, con_maxdiff;

  auto start = std::chrono::high_resolution_clock::now() + nanoseconds(1000000);
  auto start2 = start + nanoseconds(10000);

  auto t1 = std::thread::thread([&](){counting_writer(
                                start, prod_period, b, stop,
                               prod_steps,
                      prod_maxstale, prod_pushed);});
  auto t2 = std::thread::thread([&](){counting_reader(
                                start2, con_period, b, stop,
                                con_steps,
                      con_maxstale, con_maxdiff, con_poped);});

  std::this_thread::sleep_for(std::chrono::seconds(4));
  stop = true;
  t1.join();
  t2.join();
  std::cout
    << " prod_steps " << prod_steps
    << " con_steps " << con_steps
    << " prod_pushed " << prod_pushed
    << " con_poped " << con_poped
    << std::endl
    << " prod_maxstale " << prod_maxstale
    << " con_maxstale " << con_maxstale
    << " con_maxdiff " << con_maxdiff
    << std::endl;
}
