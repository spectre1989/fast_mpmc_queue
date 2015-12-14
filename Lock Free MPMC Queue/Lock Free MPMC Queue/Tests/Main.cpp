#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include "../LockFreeMPMCQueue.h"
#include "MutexQueue.h"

struct PaddedValue
{
	size_t value;
	char padding[64 - sizeof(size_t)];

	PaddedValue() : value(0) {}

	PaddedValue(size_t value) : value(value) {}
    
    operator size_t() const { return value; }
};

template <typename Value, typename Queue> std::chrono::microseconds::rep test(const size_t num_threads, char* memory, const size_t num_values, Queue& queue)
{
	memset(memory, 0, sizeof(char) * num_values);

	const size_t num_values_per_thread = num_values / num_threads;

	std::thread* reader_threads = new std::thread[num_threads];
	std::thread* writer_threads = new std::thread[num_threads];

	auto start = std::chrono::high_resolution_clock::now();

	for (size_t i = 0; i < num_threads; ++i)
	{
		reader_threads[i] = std::thread([i, &queue, memory, num_values_per_thread]()
						{
							for (size_t x = 0; x < num_values_per_thread; ++x)
							{
								Value value;
								while (!queue.try_dequeue(value))
								{
								}
								memory[value] = 1;
							}
						});
	}

	for (size_t i = 0; i < num_threads; ++i)
	{
		writer_threads[i] = std::thread([i, &queue, num_values_per_thread]()
						{
							const size_t offset = i * num_values_per_thread;
							for (size_t x = 0; x < num_values_per_thread; ++x)
							{
								const size_t value = offset + x;
								while (!queue.try_enqueue(value))
								{
								}
							}
						});
	}

	for (size_t i = 0; i < num_threads; ++i)
	{
		reader_threads[i].join();
		writer_threads[i].join();
	}

	auto time_taken = std::chrono::high_resolution_clock::now() - start;

	delete[] reader_threads;
	delete[] writer_threads;

	bool fail = false;
	for (size_t i = 0; i < num_values; ++i)
	{
		if (memory[i] == 0)
		{
			printf("%zu = 0\n", i);
			fail = true;
		}
	}

	if (fail)
	{
		printf("FAIL!\n");
	}

	return std::chrono::duration_cast<std::chrono::microseconds>(time_taken).count();
}

template <typename Queue> std::vector<std::pair<size_t, std::vector<std::chrono::microseconds::rep>>> test_batch(const size_t num_threads_max, const size_t num_values, const size_t queue_size, const size_t num_samples, char* memory)
{
	std::vector<std::pair<size_t, std::vector<std::chrono::microseconds::rep>>> results;

	Queue queue(queue_size);

	for(size_t num_threads = 1; num_threads <= num_threads_max; num_threads *= 2)
	{
		std::vector<std::chrono::microseconds::rep> samples(num_samples);

		for(size_t i = 0; i < num_samples; ++i)
		{
			samples.push_back(test(num_threads, memory, num_values, queue));
		}

		results.push_back(std::make_pair(num_threads, std::move(samples)));
	}

	return results;
}

// TODO
// timing data
// mutex queue which allows simultaneous read/write
// deal with index wrap around

int main(int argc, char* argv[])
{
	const size_t num_threads_max = 16;
	const size_t num_values = 1 << 12;
	const size_t queue_size = 32;
	const size_t num_samples = 128;

	char* memory = new char[num_values];

	std::vector<std::pair<size_t, std::vector<std::chrono::microseconds::rep>>> lock_free_queue_results		= test_batch<LockFreeMPMCQueue<PaddedValue, std::uint32_t>>(num_threads_max, num_values, queue_size, num_samples, memory);
	std::vector<std::pair<size_t, std::vector<std::chrono::microseconds::rep>>> mutex_queue_results			= test_batch<MutexQueue<PaddedValue, std::uint32_t>>(num_threads_max, num_values, queue_size, num_samples, memory);

	delete[] memory;

	printf("queue_size = %d, num_values = %d", queue_size, num_values);
	printf("num_threads;LockFreeMPMCQueue;MutexQueue\n");

	for(size_t i = 0; i < lock_free_queue_results.size(); ++i)
	{
		const std::pair<size_t, std::vector<std::chrono::microseconds::rep>>& lock_free_samples		= lock_free_queue_results[i];
		const std::pair<size_t, std::vector<std::chrono::microseconds::rep>>& mutex_queue_samples	= mutex_queue_results[i];

		if(lock_free_samples.first != mutex_queue_samples.first)
		{
			printf("num_threads mismatch!");
			char c;
			scanf("%c", &c);
		}

		const size_t num_threads = lock_free_samples.first;

		for(size_t j = 0; j < lock_free_samples.second.size(); ++j)
		{
			printf("%d;%f;%f\n", num_threads, lock_free_samples.second[j], mutex_queue_samples.second[j]);
		}
	}

	char c;
	scanf("%c", &c);

	return 0;
}
