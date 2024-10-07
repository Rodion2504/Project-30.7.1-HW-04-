#include <vector>
#include <future>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <functional>

class PoolThread {
public:
    PoolThread() = default;
    void start();
    void stop();
    template<typename F, typename... Args>
    void push_task(F&& f, Args&&... args);
    void threadFunc();

private:
    std::vector<std::thread> m_threads;
    std::queue<std::function<void()>> m_task_queue;
    std::mutex m_locker;
    std::condition_variable m_event_holder;
    volatile bool m_work;
};

void PoolThread::start() {
    m_work = true;
    for (int i = 0; i < 4; i++) {
        m_threads.push_back(std::thread(&PoolThread::threadFunc, this));
    }
}

void PoolThread::stop() {
    m_work = false;
    m_event_holder.notify_all();
    for (auto& t : m_threads) t.join();
}

template<typename F, typename... Args>
void PoolThread::push_task(F&& f, Args&&... args) {
    {
        std::lock_guard<std::mutex> l(m_locker);
        m_task_queue.push(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    }
    m_event_holder.notify_one();
}

void PoolThread::threadFunc() {
    while (true) {
        std::function<void()> task_to_do;
        {
            std::unique_lock<std::mutex> l(m_locker);
            if (m_task_queue.empty() && !m_work)
                return;
            if (m_task_queue.empty()) {
                m_event_holder.wait(l, [&]() {return !m_task_queue.empty() || !m_work; });
            }
            if (!m_task_queue.empty()) {
                task_to_do = m_task_queue.front();
                m_task_queue.pop();
            }
        }
        if (task_to_do) task_to_do();
    }
}

class RequestHandler {
public:
    RequestHandler();
    ~RequestHandler();
    template<typename F, typename... Args>
    void push_task(F&& f, Args&&... args);

private:
    PoolThread m_pool;
};

RequestHandler::RequestHandler() {
    m_pool.start();
}

RequestHandler::~RequestHandler() {
    m_pool.stop();
}

template<typename F, typename... Args>
void RequestHandler::push_task(F&& f, Args&&... args) {
    m_pool.push_task(std::forward<F>(f), std::forward<Args>(args)...);
}

class QuickSort {
public:
    void swap(int& x, int& y);
    void quicksort_f(std::vector<int>& vec, int left, int right, std::shared_ptr<std::promise<void>> promise);
    void quicksort(std::vector<int>& vec, int left, int right);
    int partition(std::vector<int>& vec, int left, int right);
};

void QuickSort::swap(int& x, int& y) {
    int temp = x;
    x = y;
    y = temp;
}

void QuickSort::quicksort_f(std::vector<int>& vec, int left, int right, std::shared_ptr<std::promise<void>> promise) {
    std::future<void> f = std::async(std::launch::async, [&]() { quicksort(vec, left, right); });
    f.wait();
    promise->set_value();
}

void QuickSort::quicksort(std::vector<int>& vec, int left, int right) {
    if (left >= right) return;
    RequestHandler rh;
    auto promise = std::make_shared<std::promise<void>>();
    int pi = partition(vec, left, right);
    if (pi - left > 100000) {
        rh.push_task(&QuickSort::quicksort_f, this, std::ref(vec), left, pi - 1, promise);
        quicksort(vec, pi + 1, right);
    }
    else {
        quicksort(vec, left, pi - 1);
        quicksort(vec, pi + 1, right);
    }
}

int QuickSort::partition(std::vector<int>& vec, int left, int right) {
    int pivot = vec[right];
    int i = left - 1;

    for (int j = left; j < right; j++) {
        if (vec[j] < pivot) {
            i++;
            swap(vec[i], vec[j]);
        }
    }

    swap(vec[i + 1], vec[right]);
    return i + 1;
}

auto main() -> int {}