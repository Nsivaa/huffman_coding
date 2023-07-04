#ifndef OCCURRENCEMAP
#define OCCURRENCEMAP

#include <unordered_map>
#include <iostream>
#include <mutex>
#include <condition_variable>

using occurrenceMapIterator = typename std::unordered_map<char,int>::iterator;

class OccurrenceMap{

private: 

	std::mutex m;
	std::condition_variable cv;
	std::unordered_map<char,int> wrapped_map; 

public:

void updateValue(char k, int v){
	wrapped_map[k] += v;
	return;
}

void toFile (std::ostream& os){
		for (auto i = beginIterator(); i!= endIterator();i++){
			os << getKey(i);
			os << getValue(i);
			os << std::endl;
		}
	//return os;
	}

	OccurrenceMap() {}

	~OccurrenceMap() {}

	void insert(char const& data){
		
		{
		std::unique_lock<std::mutex> lock(m);
		wrapped_map[data]++;
		}

	cv.notify_one();
	}
	
	occurrenceMapIterator beginIterator(){
		return wrapped_map.begin();
	}

	occurrenceMapIterator endIterator(){
		return wrapped_map.end();
	}

	char getKey(occurrenceMapIterator i){
		return i->first;
	}
	
	int getValue(occurrenceMapIterator i){
		return i->second;
	}
};

#endif
