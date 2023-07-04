#ifndef CODEMAP
#define CODEMAP

#include <unordered_map>
#include <iostream>
#include <mutex>
#include <condition_variable>

using codeMapIterator = typename std::unordered_map<char,string>::iterator;

class CodeMap{

private: 

	std::mutex m;
	std::condition_variable cv;
	std::unordered_map<char,string> wrapped_map; 

public:

	CodeMap() {}

	~CodeMap() {}

	void appendValue(char data, string code){
		
		{
		std::unique_lock<std::mutex> lock(m);
		this->wrapped_map[data].append(code);
		}

	cv.notify_one();
	}
	
	codeMapIterator beginIterator(){
		return wrapped_map.begin();
	}

	codeMapIterator endIterator(){
		return wrapped_map.end();
	}
	
	std::string getCode(char c){
		return wrapped_map[c];
	}
	
	char getKey(codeMapIterator i){
		return i->first;
	}
	
	std::string getValue(codeMapIterator i){
		return i->second;
	}
};

#endif
