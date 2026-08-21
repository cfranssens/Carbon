#include <chrono>
#include <cstdint>
#include <iostream>
#include <linux/perf_event.h>
#include <locale>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>

template<typename T> class ThousandsSeparator : public std::numpunct<T> {
public:
    ThousandsSeparator(T Separator) : m_Separator(Separator) {}

protected:
    T do_thousands_sep() const  {
        return m_Separator;
    }

private:
    T m_Separator;
};

class BlockProfiler {
    int fd_cycles = -1;
    int fd_instr  = -1;
    int fd_misses = -1;

    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::high_resolution_clock::time_point end_time;

    int open_event(uint32_t type, uint64_t config) {
        struct perf_event_attr pe;
        std::memset(&pe, 0, sizeof(pe));
        pe.size = sizeof(pe);
        pe.type = type;
        pe.config = config;
        
        pe.disabled = 1;      
        pe.exclude_kernel = 1;
        pe.exclude_hv = 1;    
        pe.inherit = 1;       // Inherit across std::thread worker spawns
        pe.inherit_stat = 1;  // Sum metrics on thread exit

        return syscall(__NR_perf_event_open, &pe, 0, -1, -1, PERF_FLAG_FD_CLOEXEC);
    }

public:
    BlockProfiler() {
        fd_cycles = open_event(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
        fd_instr  = open_event(PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
        fd_misses = open_event(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES);
    }

    ~BlockProfiler() {
        if (fd_cycles != -1) close(fd_cycles);
        if (fd_instr  != -1) close(fd_instr);
        if (fd_misses != -1) close(fd_misses);
    }

    void start() {
        if (fd_cycles != -1) {
            ioctl(fd_cycles, PERF_EVENT_IOC_RESET, 0);
            ioctl(fd_instr,  PERF_EVENT_IOC_RESET, 0);
            ioctl(fd_misses, PERF_EVENT_IOC_RESET, 0);

            ioctl(fd_cycles, PERF_EVENT_IOC_ENABLE, 0);
            ioctl(fd_instr,  PERF_EVENT_IOC_ENABLE, 0);
            ioctl(fd_misses, PERF_EVENT_IOC_ENABLE, 0);
        }
        // Capture wall clock start
        start_time = std::chrono::high_resolution_clock::now();
    }

    void stop() {
        // Capture wall clock stop first to avoid ioctl delay in time measurement
        end_time = std::chrono::high_resolution_clock::now();

        if (fd_cycles != -1) {
            ioctl(fd_cycles, PERF_EVENT_IOC_DISABLE, 0);
            ioctl(fd_instr,  PERF_EVENT_IOC_DISABLE, 0);
            ioctl(fd_misses, PERF_EVENT_IOC_DISABLE, 0);
        }
    }

    void print_stats() {
        long long cycles = 0, instr = 0, misses = 0;
        
        if (fd_cycles != -1) read(fd_cycles, &cycles, sizeof(long long));
        if (fd_instr  != -1) read(fd_instr,  &instr,  sizeof(long long));
        if (fd_misses != -1) read(fd_misses, &misses, sizeof(long long));

        auto elapsed_ms = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(end_time - start_time).count();

        setlocale(LC_ALL, "");
        printf("Wall-clock Time:          %'8f ms\n", elapsed_ms);
        printf("Aggregated Instructions:  %'8u\n", instr);
        printf("Aggregated Cycles:        %'8u\n", cycles);
        printf("Aggregated Cache misses:  %'4u\n", misses);
        
        if (cycles > 0) {
            std::cout << "IPC (Instructions/Cycle): " << (double)instr / cycles << "\n";
        }
        if (elapsed_ms > 0) {
            std::cout << "Throughput:               " << (instr / (elapsed_ms / 1000.0)) / 1e6 << " M-inst/sec\n";
        }
        std::cout << "=========================================\n\n";
    }
};
