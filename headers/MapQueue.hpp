#ifndef MAPQUEUE
#define MAPQUEUE

#include "OccurrenceMap.hpp"
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <queue>



class MapQueue{

private: 

	std::mutex m;
	//std::condition_variable cv;
	std::queue<OccurrenceMap*> wrapped_queue; 

public:
	auto size(){
		return wrapped_queue.size();
}

	bool empty(){
		return wrapped_queue.empty();
	}

	void push(OccurrenceMap* data){
		{
			std::unique_lock<std::mutex> lock(m);
			wrapped_queue.push(data);
		}
	//cv.notify_one();
	}

	MapQueue() {}

	~MapQueue() {}

	OccurrenceMap* front(){
		{
		std::unique_lock<std::mutex> lock(m);
		return wrapped_queue.front();
		}
	}
	
	void pop(){
		{
			std::unique_lock<std::mutex> lock(m);
			wrapped_queue.pop();
		}
		return;
	}
};

#endif
