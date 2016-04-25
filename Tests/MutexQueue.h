#ifndef __MUTEXQUEUE_H__
#define __MUTEXQUEUE_H__

#include <cstdint>
#include <mutex>

template <typename T, size_t cache_line_size = 64> class MutexQueue
{
    public:
	explicit MutexQueue( size_t size ) : m_data( new T[size] ), m_size( size ), m_head( 0 ), m_tail( 0 ) {}

	virtual ~MutexQueue() { delete[] m_data; }

	bool try_enqueue( const T& value )
	{
		m_mutex.lock();

		const size_t count = m_tail - m_head;

		if( count == m_size )
		{
			m_mutex.unlock();
			return false;
		}

		m_data[m_tail % m_size] = value;
		++m_tail;

		m_mutex.unlock();
		return true;
	}

	bool try_dequeue( T& out )
	{
		m_mutex.lock();

		if( m_head == m_tail )
		{
			m_mutex.unlock();
			return false;
		}

		out = m_data[m_head % m_size];
		++m_head;

		m_mutex.unlock();
		return true;
	}

	size_t capacity() const { return m_size; }

    private:
	T* m_data;
	size_t m_size;
	std::mutex m_mutex;

	char _pad1[cache_line_size - 8];
	std::uint64_t m_head;
	char _pad2[cache_line_size - 8];
	std::uint64_t m_tail;
};

#endif