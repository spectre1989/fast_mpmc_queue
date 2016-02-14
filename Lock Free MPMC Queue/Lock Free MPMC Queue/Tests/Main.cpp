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

template <template<typename, typename> class Queue, typename Value, typename IndexType> std::chrono::microseconds::rep test(const size_t num_threads, char* memory, const size_t num_values, Queue<Value, IndexType>& queue)
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

	return std::chrono::duration_cast<std::chrono::milliseconds>(time_taken).count();
}

template <class Queue>
std::vector<double> test_batch(const size_t num_threads_max, const size_t num_values, const size_t queue_size, const size_t num_samples, char* memory)
{
    std::vector<double> results;
    Queue queue(queue_size);
    double inv_num_samples = 1.0 / double(num_samples);
    double avg_time_taken = 0.0;
    
    for (size_t num_threads = 1; num_threads <= num_threads_max; num_threads *= 2)
    {
        for (size_t i = 0; i < num_samples; ++i)
        {
            avg_time_taken += test(num_threads, memory, num_values, queue) * inv_num_samples;
        }
        
        results.push_back(avg_time_taken);
    }
    
    return results;
}

// TODO
// mutex queue which allows simultaneous read/write
// deal with index wrap around

int main(int argc, char* argv[])
{
	const size_t num_threads_max = 16;
    const size_t num_values = 1 << 9;//1 << 12;
	const size_t queue_size = 32;
    const size_t num_samples = 10;//128;

    std::vector<double> results[2];
    
	char* memory = new char[num_values];

    results[0] = test_batch<LockFreeMPMCQueue<size_t, std::uint32_t>>(num_threads_max, num_values, queue_size, num_samples, memory);
    results[1] = test_batch<MutexQueue<size_t, std::uint32_t>>(num_threads_max, num_values, queue_size, num_samples, memory);
    
    printf("<html>\n");
    printf("<head>\n");
    printf("<!--Load the AJAX API-->\n");
    printf("<script type=\"text/javascript\" src=\"https://www.gstatic.com/charts/loader.js\"></script>\n");
    printf("<script type=\"text/javascript\">\n\n");
    printf("google.charts.load('current', {'packages':['corechart', 'line']});\n\n");
    printf("google.charts.setOnLoadCallback(drawChart);\n\n");
    printf("function drawChart() {\n");
    printf("var data = new google.visualization.DataTable();\n");
    printf("data.addColumn('number', 'X');\n");
    printf("data.addColumn('number', 'LockFreeMPMCQueue');\n");
    printf("data.addColumn('number', 'MutexQueue');\n");
    printf("data.addRows([\n");
    
    for(size_t i = 0, num_threads = 1; i < results[0].size(); ++i, num_threads *= 2)
    {
        printf("[%zu,%f,%f]", num_threads, results[0][i], results[1][i]);
        if(i < (results[0].size() - 1))
        {
            printf(",");
        }
        printf("\n");
    }
    
    printf("]);\n");
    printf("var options = {\n");
    printf("hAxis: {title: 'Threads'},\n");
    printf("vAxis: {title: 'Avg Time Taken (milliseconds)'},\n");
    printf("series: {1: {curveType: 'function'}},\n");
    printf("height: 800\n");
    printf("};\n");
    printf("var chart = new google.visualization.LineChart(document.getElementById('chart_div'));\n");
    printf("chart.draw(data, options);\n");
    printf("}\n");
    printf("</script>\n");
    printf("</head>\n");
    printf("<body>\n");
    printf("<div id=\"chart_div\"></div>\n");
    printf("</body>\n");
    printf("</html>\n");

	delete[] memory;

	char c;
	scanf("%c", &c);

	return 0;
}
