#pragma once

#include <atomic>

template <typename T, typename index_t = size_t> class LockFreeMPMCQueue
{
    public:
	explicit LockFreeMPMCQueue( size_t size )
	    : m_data( new T[size] ), m_size( size ), m_head_1( 0 ), m_head_2( 0 ), m_tail_1( 0 ), m_tail_2( 0 )
	{
	}

	virtual ~LockFreeMPMCQueue() { delete[] m_data; }

	bool try_enqueue( const T& value )
	{
		index_t tail = m_tail_1.load( std::memory_order_relaxed );
		const index_t head = m_head_2.load( std::memory_order_relaxed );

		const index_t count =
		    tail > head ? tail - head : ( std::numeric_limits<index_t>::max() - head ) + tail + 1;

		// count could be greater than size if between the reading of head, and the reading of tail, both head
		// and tail have been advanced
		if( count >= m_size )
		{
			return false;
		}

		const index_t next_tail = tail < std::numeric_limits<index_t>::max() ? tail + 1 : 0;

		if( !std::atomic_compare_exchange_strong_explicit(
			&m_tail_1, &tail, next_tail, std::memory_order_relaxed, std::memory_order_relaxed ) )
		{
			return false;
		}

		m_data[tail % m_size] = value;

		while( m_tail_2.load( std::memory_order_relaxed ) != tail )
		{
			std::this_thread::yield();
		}

		// Release - read/write before can't be reordered with writes after
		// Make sure the write of the value to m_data is
		// not reordered past the write to m_tail_2
		std::atomic_thread_fence( std::memory_order_release );
		m_tail_2.store( tail + 1, std::memory_order_relaxed );

		return true;
	}

	bool try_dequeue( T& out )
	{
		// Order matters here, because we're testing for emptiness with head==tail
		// Could test with head >= tail, but if index_t is < 64bits then this will have
		// issues when tail has wrapped around to 0
		index_t head = m_head_1.load( std::memory_order_relaxed );
		// Acquire - read before can't be reordered with read/write after
		std::atomic_thread_fence( std::memory_order_acquire );
		const index_t tail = m_tail_2.load( std::memory_order_relaxed );

		if( head == tail )
		{
			return false;
		}

		const index_t next_head = head < std::numeric_limits<index_t>::max() ? head + 1 : 0;

		if( !std::atomic_compare_exchange_strong_explicit(
			&m_head_1, &head, next_head, std::memory_order_relaxed, std::memory_order_relaxed ) )
		{
			return false;
		}

		out = m_data[head % m_size];

		while( m_head_2.load( std::memory_order_relaxed ) != head )
		{
			std::this_thread::yield();
		}

		// Release - read/write before can't be reordered with writes after
		// Make sure the read of value from m_data is
		// not reordered past the write to m_head_2
		std::atomic_thread_fence( std::memory_order_release );
		m_head_2.store( head + 1, std::memory_order_relaxed );

		return true;
	}

	size_t capacity() const { return m_size; }

    private:
	T* m_data;
	size_t m_size;
	char pad1[64];
	std::atomic<index_t> m_head_1;
	char pad2[64];
	std::atomic<index_t> m_head_2;
	char pad3[64];
	std::atomic<index_t> m_tail_1;
	char pad4[64];
	std::atomic<index_t> m_tail_2;
};
