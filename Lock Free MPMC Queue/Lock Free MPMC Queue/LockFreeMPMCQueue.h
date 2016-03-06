#ifndef __LOCKFREEMPMCQUEUE_H__
#define __LOCKFREEMPMCQUEUE_H__

#include <atomic>

template <typename T, typename index_t = size_t> class LockFreeMPMCQueue
{
    public:
	explicit LockFreeMPMCQueue(size_t size)
	    : m_data(new T[size]), m_size(size), m_head_1(0), m_head_2(0), m_tail_1(0), m_tail_2(0)
	{
	}

	virtual ~LockFreeMPMCQueue() { delete[] m_data; }

	bool try_enqueue(const T& value)
	{
		index_t tail = m_tail_1.load(std::memory_order_relaxed);
		const index_t head = m_head_2.load(std::memory_order_relaxed);
		const index_t count = tail - head;

		if (count == m_size)
		{
			return false;
		}

		if (std::atomic_compare_exchange_weak_explicit(&m_tail_1, &tail, (tail + 1), std::memory_order_relaxed,
							       std::memory_order_relaxed) == false)
		{
			return false;
		}

		m_data[tail % m_size] = value;

		while (m_tail_2.load(std::memory_order_relaxed) != tail)
		{
			std::this_thread::yield();
		}
		m_tail_2.store(tail + 1, std::memory_order_relaxed);

		return true;
	}

	bool try_dequeue(T& out)
	{
		index_t head = m_head_1.load(std::memory_order_relaxed);
		const index_t tail = m_tail_2.load(std::memory_order_relaxed);

		if (head == tail)
		{
			return false;
		}

		if (std::atomic_compare_exchange_weak_explicit(&m_head_1, &head, (head + 1), std::memory_order_relaxed,
							       std::memory_order_relaxed) == false)
		{
			return false;
		}

		out = m_data[head % m_size];

		while (m_head_2.load(std::memory_order_relaxed) != head)
		{
			std::this_thread::yield();
		}
		m_head_2.store(head + 1, std::memory_order_relaxed);

		return true;
	}

	size_t capacity() const { return m_size; }

    private:
	T* m_data;
	size_t m_size;
	std::atomic<index_t> m_head_1;
	std::atomic<index_t> m_head_2;
	std::atomic<index_t> m_tail_1;
	std::atomic<index_t> m_tail_2;
};

#endif