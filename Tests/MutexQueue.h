#ifndef __MUTEXQUEUE_H__
#define __MUTEXQUEUE_H__

#include <cstdint>
#include <mutex>

template <typename T, size_t cache_line_size = 64> class MutexQueue
{
    public:
	explicit MutexQueue( size_t capacity ) 
		: m_items( static_cast<Item*>( _aligned_malloc( sizeof( Item ) * capacity, cache_line_size ) ) )
		, m_capacity( capacity )
		, m_head( 0 )
		, m_tail( 0 ) 
	{}

	virtual ~MutexQueue() { _aligned_free( m_items ); }

	bool try_enqueue( const T& value )
	{
		m_mutex.lock();

		const uint64_t count = m_tail - m_head;

		if( count == m_capacity )
		{
			m_mutex.unlock();
			return false;
		}

		m_items[m_tail % m_capacity].value = value;
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

		out = m_items[m_head % m_capacity].value;
		++m_head;

		m_mutex.unlock();
		return true;
	}

	size_t capacity() const { return m_capacity; }

    private:
	struct alignas(cache_line_size)Item
	{
		std::atomic<uint64_t> version;
		T value;
	};

	struct alignas(cache_line_size)AlignedAtomicU64 : public std::atomic<uint64_t>
	{
		using std::atomic<uint64_t>::atomic;
	};

	Item* m_items;
	size_t m_capacity;
	std::mutex m_mutex;

	AlignedAtomicU64 m_head;
	AlignedAtomicU64 m_tail;
};

#endif