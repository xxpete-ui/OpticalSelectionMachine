// =============================================================================
//  CircularQueue —— 有界阻塞队列（学习用）
//
//  当前这个文件是 **v0 反面教材**：能跑、逻辑基本对，但有两个致命问题：
//      1) 用 sleep/yield 轮询代替条件变量  → CPU 空转，延迟由轮询粒度决定
//      2) 等待循环里不看 shutdown 标志      → 关闭时线程永远醒不过来
//
//  你的任务（阶段 S1）：
//      把它重写成 mutex + condition_variable 版本，让 src/main.cpp 里 5 个测试
//      从「部分失败」变成「全部通过」，并且 T1/T2 的耗时明显下降。
//
//  验收标准见 ../学习计划.md 的 S1。
// -----------------------------------------------------------------------------
//  换实现时不必保留 v0 的任何一行，但 **public 接口不要改**，
//  否则 main.cpp 的测试代码要跟着改，就没法对比改造前后了。
// =============================================================================

#ifndef CIRCULARQUEUE_H
#define CIRCULARQUEUE_H

#include <atomic>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

// -----------------------------------------------------------------------------
// 统计信息：测试程序靠它判断「丢了多少 / 阻塞了多少次」
// 实现新版本时必须维护这几个计数，否则 T3/T4 无法判定
// -----------------------------------------------------------------------------
struct QueueStats
{
    std::atomic<std::size_t> pushed{ 0 };   // 成功入队条数
    std::atomic<std::size_t> popped{ 0 };   // 成功出队条数
    std::atomic<std::size_t> dropped{ 0 };  // 覆盖模式下令老数据被挤掉的条数
    std::atomic<std::size_t> blocked{ 0 };  // 因队列满而等待过的次数

    void reset()
    {
        pushed = 0; popped = 0; dropped = 0; blocked = 0;
    }
};

template <typename T>
class CircularQueue
{
public:
    /// @param capacity  队列容量上限（<=0 时按 1 处理）
    /// @param overwrite true  = 队列满时挤掉最老的一条（实时优先，会丢数据）
    ///                  false = 队列满时阻塞生产者（不丢数据，会反压上游）
    explicit CircularQueue(std::size_t capacity = 100, bool overwrite = true)
        : m_capacity(capacity > 0 ? capacity : 1)
        , m_overwrite(overwrite)
    {
        // v0 用一个环形缓冲区来存，避免 std::queue 反复分配
        m_buf.resize(m_capacity);
    }

    CircularQueue(const CircularQueue&)            = delete;
    CircularQueue& operator=(const CircularQueue&) = delete;
    CircularQueue(CircularQueue&&)                 = delete;
    CircularQueue& operator=(CircularQueue&&)      = delete;

    // -------------------------------------------------------------------------
    //  入队
    //  返回 true  = 数据已放进队列
    //  返回 false = 队列已 shutdown，数据没进去
    // -------------------------------------------------------------------------
    bool push(const T& v)
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        while (m_count >= m_capacity)
        {
            if (m_shutdown.load(std::memory_order_acquire))
                return false;

            if (m_overwrite)
            {
                // 覆盖模式：挤掉最老的
                m_head = (m_head + 1) % m_capacity;
                --m_count;
                m_stats.dropped.fetch_add(1, std::memory_order_relaxed);
                break;
            }

            // 阻塞模式：v0 —— 睡 1ms 再来看一眼
            // 问题：这是轮询，不是等待。睡眠期间即使消费者取走了数据，
            //      生产者也要等满 1ms 才知道；而且 shutdown() 也唤不醒它。
            m_stats.blocked.fetch_add(1, std::memory_order_relaxed);
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            lock.lock();
        }

        if (m_shutdown.load(std::memory_order_acquire))
            return false;

        const std::size_t tail = (m_head + m_count) % m_capacity;
        m_buf[tail]            = v;
        ++m_count;
        m_stats.pushed.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    bool push(T&& v)
    {
        // v0 偷懒：直接复用 const& 版本。移动语义没生效，会多一次拷贝。
        // 重写时请真正实现移动版（提示：cv::Mat 这类大对象差别很大）
        return push(static_cast<const T&>(v));
    }

    // -------------------------------------------------------------------------
    //  出队（非阻塞）—— 队列空时立即返回 nullopt
    // -------------------------------------------------------------------------
    std::optional<T> pop()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return pop_locked();
    }

    // -------------------------------------------------------------------------
    //  出队（无限等待）—— 队列一直空就一直等，直到 shutdown
    //  v0 的问题：只等 2ms 一轮，不支持 timeout，也不响应 shutdown
    // -------------------------------------------------------------------------
    std::optional<T> pop_wait()
    {
        for (;;)
        {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!m_count)
                {
                    // 这里如果返回 nullopt，上层无法区分「暂时没数据」和「已关闭」
                    // 为了让 T1 能跑通，v0 先硬等到有数据
                }
                else
                {
                    return pop_locked();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    /// 带超时的出队，超时返回 nullopt
    template <typename Rep, typename Period>
    std::optional<T> pop_wait_for(const std::chrono::duration<Rep, Period>& timeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        for (;;)
        {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_count)
                    return pop_locked();
            }
            if (std::chrono::steady_clock::now() >= deadline)
                return std::nullopt;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // -------------------------------------------------------------------------
    //  关闭：唤醒所有正在等待的线程，让它们返回 nullopt 后自行退出
    //  v0 只置了标志，没人被唤醒 —— 所以 T5 会挂住
    // -------------------------------------------------------------------------
    void shutdown()
    {
        m_shutdown.store(true, std::memory_order_release);
    }

    bool is_shutdown() const
    {
        return m_shutdown.load(std::memory_order_acquire);
    }

    // -------------------------------------------------------------------------
    //  状态查询
    // -------------------------------------------------------------------------
    std::size_t size() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_count;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_count == 0;
    }

    std::size_t capacity() const
    {
        return m_capacity;
    }

    QueueStats& stats()
    {
        return m_stats;
    }

private:
    /// 调用前必须已持有 m_mutex
    std::optional<T> pop_locked()
    {
        if (m_count == 0)
            return std::nullopt;

        T item = std::move(m_buf[m_head]);
        m_head = (m_head + 1) % m_capacity;
        --m_count;
        m_stats.popped.fetch_add(1, std::memory_order_relaxed);
        return item;
    }

private:
    mutable std::mutex m_mutex;
    std::vector<T>     m_buf;
    std::size_t        m_head     = 0;   // 队头下标
    std::size_t        m_count    = 0;   // 当前元素个数
    std::size_t        m_capacity = 100;
    bool               m_overwrite = true;   // true=覆盖, false=阻塞
    std::atomic<bool>  m_shutdown{ false };
    QueueStats         m_stats;
};

#endif // CIRCULARQUEUE_H
