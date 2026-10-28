// =============================================================================
//  环形队列验收测试
//
//  用法：
//      cmake -B build -A x64
//      cmake --build build --config Release
//      build\Release\thread_lab.exe
//
//  目标：5 个测试全 PASS，且 T1/T2 的 CPU 周期数尽量低。
//
//  当前 v0 实现的预期结果：
//      T1 PASS（慢）  T2 PASS（慢）  T3 PASS  T4 PASS（慢）  T5 FAIL（shutdown 唤不醒）
//
//  你重写 CircularQueue.h 后，T5 必须变 PASS，T1/T2/T4 的墙钟和 CPU 应大幅下降。
//
//  阅读顺序建议：
//      1. 先看 CircularQueue.h 的注释（知道要改什么）
//      2. 跑一遍，把下面这行读数记下来当基线
//      3. 再改实现，跑第二遍对比
// =============================================================================

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "CircularQueue.h"

// ---------------------------------------------------------------------------
//  计时
//
//  墙钟用 steady_clock（单调时钟，不受系统时间调整影响）。
//
//  CPU 时间不能用 clock() / GetProcessTimes —— 它们的精度是系统时钟 tick
//  (Windows 上 15.6ms)，测试只跑几十毫秒时读出来全是 0.0。
//  这里用 QueryProcessCycleTime，精度是 CPU 周期，能反映「忙轮询」的真实代价。
// ---------------------------------------------------------------------------
#ifdef _WIN32
#include <windows.h>
#include <realtimeapiset.h>
static std::uint64_t cpu_cycles()
{
    ULONG64 c = 0;
    QueryProcessCycleTime(GetCurrentProcess(), &c);
    return static_cast<std::uint64_t>(c);
}
#else
#include <ctime>
static std::uint64_t cpu_cycles()
{
    return static_cast<std::uint64_t>(std::clock()) * 1000ULL;
}
#endif

static int g_pass = 0;
static int g_fail = 0;

struct Timer
{
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    std::uint64_t c0 = cpu_cycles();

    double wall_ms() const
    {
        auto d = std::chrono::steady_clock::now() - t0;
        return std::chrono::duration<double, std::milli>(d).count();
    }
    /// 换算成「百万 CPU 周期」。16 核 3GHz 满载 1 秒 ≈ 48000
    double cpu_mcyc() const
    {
        return static_cast<double>(cpu_cycles() - c0) / 1e6;
    }
};

static void report(const char* name, bool ok, const Timer& t, const std::string& detail)
{
    std::printf("[%s] %-30s 墙钟 %7.1fms | CPU %8.1f Mcyc | %s\n",
                ok ? "PASS" : "FAIL", name, t.wall_ms(), t.cpu_mcyc(), detail.c_str());
    std::fflush(stdout);
    (ok ? g_pass : g_fail)++;
}

// ---------------------------------------------------------------------------
//  WorkerGroup：统一管理线程 + 超时保护
//
//  ⚠ 这里有个坑值得记住：
//  如果某个线程卡在 pop_wait() 里永远不返回，直接 join() 会让整个进程挂死。
//  所以超时后必须 detach。但 **detach 的线程仍然会继续访问它捕获的对象**，
//  如果那个对象是栈上的（测试函数返回就析构了），就会踩到 use-after-free ——
//  表现为后面几个测试的计数变成天文数字。
//
//  所以下面各个测试里的队列一律用 new 分配、故意不释放（进程退出时由 OS 回收）。
//  这不是好习惯，但对「可能有僵尸线程」的测试程序是必须的。
// ---------------------------------------------------------------------------
class WorkerGroup
{
public:
    void add(std::function<void()> fn)
    {
        m_alive.fetch_add(1);
        m_threads.emplace_back([this, fn = std::move(fn)] {
            fn();
            m_alive.fetch_sub(1);
        });
    }

    /// 等待全部线程结束；超时则 detach（保留僵尸线程，见上面的说明）并返回 false
    bool wait_all(std::chrono::milliseconds timeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (m_alive.load() > 0 && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        const bool finished = (m_alive.load() == 0);
        for (auto& th : m_threads)
        {
            if (!th.joinable()) continue;
            if (finished) th.join();
            else          th.detach();
        }
        m_threads.clear();
        return finished;
    }

private:
    std::vector<std::thread> m_threads;
    std::atomic<int>         m_alive{ 0 };
};

// ===========================================================================
//  T1  单生产者 - 单消费者：100,000 条，要求不丢、不乱序
// ===========================================================================
static void test_single_producer_consumer()
{
    const int N = 100'000;
    Timer t;

    // 队列容量给大一点，减少 v0 的空等次数
    auto* q = new CircularQueue<int>(8192, /*overwrite=*/false);

    std::atomic<int>  received{ 0 };
    std::atomic<bool> order_ok{ true };

    WorkerGroup wg;
    wg.add([&, q] {                                  // 生产者
        for (int i = 0; i < N; ++i)
            q->push(i);
    });
    wg.add([&, q] {                                  // 消费者
        int expect = 0;
        while (expect < N)
        {
            auto v = q->pop_wait();
            if (!v)
            {
                if (q->is_shutdown()) break;
                continue;
            }
            if (*v != expect) order_ok.store(false);
            ++expect;
            received.store(expect);
        }
    });

    const bool finished = wg.wait_all(std::chrono::seconds(20));
    const bool ok = finished && order_ok.load() && received.load() == N;

    char buf[160];
    std::snprintf(buf, sizeof buf, "收到 %d/%d，顺序%s%s",
                  received.load(), N,
                  order_ok.load() ? "正确" : "错乱",
                  finished ? "" : " (超时)");
    report("T1 单生产者-单消费者", ok, t, buf);
}

// ===========================================================================
//  T2  4 生产者 - 4 消费者：共 100,000 条，要求总数准确
// ===========================================================================
static void test_multi_producer_consumer()
{
    const int kProducers   = 4;
    const int kConsumers   = 4;
    const int kPerProducer = 25'000;
    const int kTotal       = kProducers * kPerProducer;

    Timer t;
    auto* q = new CircularQueue<int>(8192, /*overwrite=*/false);

    std::atomic<int> consumed{ 0 };

    WorkerGroup wg;
    for (int p = 0; p < kProducers; ++p)
        wg.add([p, q] {
            for (int i = 0; i < kPerProducer; ++i)
                q->push(p * kPerProducer + i);
        });

    for (int c = 0; c < kConsumers; ++c)
        wg.add([q, &consumed, kTotal] {
            while (consumed.load() < kTotal)
            {
                auto v = q->pop_wait();
                if (!v)
                {
                    if (q->is_shutdown()) break;
                    continue;
                }
                consumed.fetch_add(1);
            }
        });

    // 主线程监视：数据收齐后立刻 shutdown，正在等待的消费者应该被唤醒来退出。
    // 这一步专门考 shutdown 能力 —— v0 唤不醒，所以会在这里判 FAIL。
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (consumed.load() < kTotal && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    const bool all_consumed = (consumed.load() == kTotal);
    q->shutdown();
    const bool finished = wg.wait_all(std::chrono::seconds(2));
    const bool ok = all_consumed && finished;

    char buf[160];
    std::snprintf(buf, sizeof buf, "消费 %d/%d%s%s",
                  consumed.load(), kTotal,
                  all_consumed ? "" : " (数据没消费完)",
                  finished ? "" : " (线程未被唤醒)");
    report("T2 4生产者-4消费者", ok, t, buf);
}

// ===========================================================================
//  T3  覆盖模式：队列满时挤掉最老的，要求 dropped > 0 且不卡死
// ===========================================================================
static void test_overwrite_mode()
{
    const int N = 50'000;
    Timer t;
    auto* q = new CircularQueue<int>(8, /*overwrite=*/true);

    std::atomic<int>  consumed{ 0 };
    std::atomic<bool> producer_done{ false };

    WorkerGroup wg;
    wg.add([&, q] {
        for (int i = 0; i < N; ++i)
        {
            q->push(i);
            // 生产者稍微放慢，模拟「相机按固定帧率出图」，
            // 否则 5 万条瞬间推完，消费者只来得及看到最后十几条
            if ((i & 0x3F) == 0)
                std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
        producer_done.store(true);
    });
    wg.add([&, q] {
        while (!producer_done.load() || !q->empty())
        {
            if (q->pop()) consumed.fetch_add(1);
            else          std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    const bool finished = wg.wait_all(std::chrono::seconds(20));
    const std::size_t dropped = q->stats().dropped.load();
    const bool ok = finished && dropped > 0 && consumed.load() > 0;

    char buf[160];
    std::snprintf(buf, sizeof buf, "丢弃 %zu 条 / 消费 %d 条%s",
                  dropped, consumed.load(), finished ? "" : " (超时)");
    report("T3 覆盖模式(实时优先)", ok, t, buf);
}

// ===========================================================================
//  T4  阻塞模式：一条不丢，生产者被反压（最有说服力的性能对比）
// ===========================================================================
static void test_blocking_mode()
{
    // 这里故意只用 2 万条：v0 的轮询实现吞吐只有 3000~4000 条/秒，
    // 10 万条要跑近半分钟，等你换成条件变量版再改回 10 万看差距
    const int N = 20'000;
    Timer t;
    auto* q = new CircularQueue<int>(8, /*overwrite=*/false);   // 故意小，逼出阻塞

    std::atomic<int>  consumed{ 0 };
    std::atomic<bool> order_ok{ true };

    WorkerGroup wg;
    wg.add([&, q] {
        for (int i = 0; i < N; ++i)
            q->push(i);
    });
    wg.add([&, q] {
        int expect = 0;
        while (expect < N)
        {
            auto v = q->pop_wait();
            if (!v)
            {
                if (q->is_shutdown()) break;
                continue;
            }
            if (*v != expect) order_ok.store(false);
            ++expect;
            consumed.store(expect);
        }
    });

    const bool finished = wg.wait_all(std::chrono::seconds(20));
    const std::size_t blocked = q->stats().blocked.load();
    const bool ok = finished && consumed.load() == N && order_ok.load();

    char buf[192];
    std::snprintf(buf, sizeof buf, "消费 %d/%d，生产者阻塞 %zu 次%s",
                  consumed.load(), N, blocked, finished ? "" : " (超时)");
    report("T4 阻塞模式(不丢数据)", ok, t, buf);
}

// ===========================================================================
//  T5  优雅关闭：shutdown() 必须唤醒正在等待的线程
//      v0 一定失败 —— 这是你要修的核心缺陷
// ===========================================================================
static void test_shutdown_wakes_waiter()
{
    Timer t;
    auto* q = new CircularQueue<int>(8, /*overwrite=*/true);

    std::atomic<bool> exited{ false };

    WorkerGroup wg;
    wg.add([&, q] {
        q->pop_wait();                 // 队列空，这里应该阻塞住
        exited.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    q->shutdown();                     // 期望：立刻唤醒上面的 pop_wait

    const bool finished = wg.wait_all(std::chrono::milliseconds(200));
    const bool ok = finished && exited.load();

    report("T5 shutdown唤醒等待线程", ok, t,
           ok ? "线程已退出" : "200ms内没醒 -> shutdown未唤醒等待者");
}

// ===========================================================================
//  main
// ===========================================================================
int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::printf("===============================================================\n");
    std::printf("  环形队列验收测试        硬件并发: %u 线程\n",
                std::thread::hardware_concurrency());
    std::printf("===============================================================\n");

    test_single_producer_consumer();
    test_multi_producer_consumer();
    test_overwrite_mode();
    test_blocking_mode();
    test_shutdown_wakes_waiter();

    std::printf("---------------------------------------------------------------\n");
    std::printf("  结果: %d PASS / %d FAIL\n", g_pass, g_fail);
    std::printf("---------------------------------------------------------------\n");
    std::fflush(stdout);

    // 有失败时可能存在被 detach 的僵尸线程，用 _Exit 跳过析构
    if (g_fail) std::_Exit(g_fail);
    return 0;
}
