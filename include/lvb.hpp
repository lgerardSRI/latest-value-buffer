//
// Created by Leonard Gerard on 12/29/15.
//


#pragma once

#include <atomic>

#define INCR_MOD(X,M) (X+1>=M)?0:X+1


#ifndef L1CACHE_LINE_SIZE
#error "Please provide a L1CACHE_LINE_SIZE definition, usually the job of cmake."
#endif

/**
 * Rule of thumb to compute the slack needed to ensure that the consummer
 * does get the latest value.v
 * Keeps a reserve of 20percent of ratio unreliability and add 2 more for cache
 */
constexpr size_t slack_of_prod_ratio(size_t ratio) {
    return (ratio * 1.2) + 2;
}

template<typename T, size_t slack = 2>
class Lvb {
public:
    static constexpr size_t size = slack + 2;
private:
    struct alignas(L1CACHE_LINE_SIZE) Padded_T {
        T value;
    };
    Padded_T data[size];

    struct alignas(L1CACHE_LINE_SIZE) Writer {
        Writer(int r_pos, int w_pos)
                : next(w_pos+1), reader(r_pos), stale_cnt(0) {}
        int next;
        int reader;
        int stale_cnt;
    } w;

    struct alignas(L1CACHE_LINE_SIZE) Reader {
        // The reader staleness is -1 indicating that it hasn't been initialized
        Reader (int r_pos, int w_pos)
                : next(r_pos+1), writer(w_pos),
                  stale_cnt(0), initialized(false) {}
        int next;
        int writer;
        int stale_cnt;
        bool initialized;
    } r;

    //  The definition follows an align one so we are aligned here.
    //  Putting them at the end of the class (so they are initialized at the end
    //ensures that however is the lvb constructed (in shared threads, etc),
    //the class members are in sync with the positions.
    std::atomic_int writing_pos;
    std::atomic_int reading_pos; //TODO maybe seperate them? we are forcing cache comm here

public:

    Lvb(): w(0, 1), r(0, 1),
           reading(&data[0].value), writing(&data[1].value),
           reading_pos(0), writing_pos(1) {}

    Lvb(const Lvb&) = delete;
    Lvb& operator=(const Lvb&) = delete;

    T * reading;
    T * writing;


    /**
     * writer_advance is called by the writer to indicate that the
     * writing position has been written (is ready to be read).
     * If the writer isn't stale (return value of 0) then the writing
     * position is actually advanced and the reader can access the previous
     * one.
     * If the writer is stale then the writing position isn't changed and new
     * writing will overwrite the previous write. Therfore the previous value
     * won't be seen by the reader.
     *
     * If the writer is stale, it indicates that the buffer is full. It can
     * be caused by a buffer too small or by a reader too slow.
     *
     * The staleness value count the number of times in a row it has been stale.
     * When knowing maximum latencies, the staleness value can be used to
     * determine a timeout of the reader.
     *
     */
    int writer_advance() {
        if (w.next == w.reader) { // We catched up with the position we know of the reader
            w.reader = reading_pos.load(std::memory_order_acquire);
            if (w.next == w.reader) { // The reader apparently did not move, we stall
                w.stale_cnt++;
                return w.stale_cnt;
            }
        }
        // We have space to advance, commit the previous writing first
        writing_pos.store(w.next, std::memory_order_release);
        writing = & data[w.next].value;
        w.next = INCR_MOD(w.next, size);
        w.stale_cnt = 0;
        return w.stale_cnt;
    }

    /**
     * reader_advance is called by the reader to try to move the
     * reading position to the latest available position.
     *
     * If the reader isn't stale (return value of 0) then the reading
     * position is actually advanced (reading points to a fresh value (value
     * writen after the previous one)).
     *
     * If the reader is stale then the reading position isn't changed
     * (keeping the previous value).
     *
     * If the reader is stale, it indicates that the buffer is empty. It
     * usually means that the writer is too slow compared to the reader.
     *
     * The staleness value count the number of times in a row it has been stale.
     * When knowing maximum latencies, the staleness value can be used to
     * determine a timeout of the writer.
     *
     */
    int reader_advance() {
        if (r.next == r.writer) { // We catched up with the position we know of the writer
            r.writer = writing_pos.load(std::memory_order_acquire);
            if (r.next == r.writer) { // The writer apparently did not move, we stall
                if (r.initialized) {
                    r.stale_cnt++;
                } else {
                    r.stale_cnt--;
                }
                return r.stale_cnt;
            }
        }
        // We have space to advance, release the previous reading first
        reading_pos.store(r.next, std::memory_order_release);
        reading = & data[r.next].value;
        r.next = INCR_MOD(r.next, size);
        r.stale_cnt = 0;
        r.initialized = true;
        return r.stale_cnt;
    }


  /****
   * Helper functions for when movable
   */

  /**
   * requires copy movable
   */
  int put(const T&& x) {
      *writing = std::move(x);
      return writer_advance();
  }

  /**
   * requires move constructor
   * Note that if stale, the return value is "empty" (has been moved already).
   */
  T pop(int& staleness) {
      staleness = reader_advance();
      return std::move(*reading);
  }

    /****
     * Higher level easier to use when copying is possible and cheap
     */
    int put(const T& x) {
        *writing = x;
        return writer_advance();
    }
    T get() {
        reader_advance();
        return *reading;
    }
    T get(int& staleness) {
        staleness = reader_advance();
        return *reading;
    }

};
