#include <iostream>
#include <fstream>
#include <unordered_map>
#include <filesystem>
#include <future>
#include <cstring>
#include "utimer.cpp"
#include "ThreadPool.hpp"
#include "utils.hpp"
#include "OccurrenceMap.hpp"
#include "CodeMap.hpp"
#include "MapQueue.hpp"

#define BUFSIZE 4096
#define THREAD_EOS '\0'

using namespace std;

ThreadPool *thread_pool = new ThreadPool();
atomic<int> toCompute=1; //used in the merging (Reduce) of the maps.

int merge_maps_work(mutex &m, MapQueue &maps){
    /*
	 * Work function for summing two OccurrenceMaps.
	 * Should probably be improved by making every thread
    *  sum more than two maps at a time.
	*/
	OccurrenceMap *submap1, *submap2;
	while(toCompute<1){
		submap1 = maps.front();
		maps.pop();
		submap2 = maps.front();
		maps.pop();
		for(auto i = submap1->beginIterator(); i != submap1->endIterator();i++){
			submap2->updateValue(submap1->getKey(i), submap1->getValue(i));
		}
		toCompute--;
		maps.push(submap2);
	}
	return 1;
}

void merge_maps(OccurrenceMap &finalMap, MapQueue& maps){
    /*
	 * Emitter for the reduce operation on the Occurrence Maps.
	 * We use an atomic variable (toCompute) because we need
     * to know the exact number of executions to be able to tell the threads
     * when they should stop popping maps from the queue.
	*/
	int nw = thread_pool->getWorkersNumber();
	int n=nw/2;
	mutex m;
	vector<future<int>> futures;

	while(n>1){
		toCompute+=n;
		n=n/2;
	}
	for(int k=0; k < (nw/2); k++){
		auto f = thread_pool->addJob([&m,&maps] () -> int {return merge_maps_work(m,maps);});
		futures.push_back(std::move(f));
	}
	for(auto &f : futures){
			auto v = f.get();
		}
	//compute reduction sequentially over remaining maps
	while(!maps.empty()){
		auto m = maps.front();
		for (auto i = m->beginIterator(); i != m->endIterator(); i++){
			finalMap.updateValue(m->getKey(i), m->getValue(i));
		}
		maps.pop();
	}
	return;
}

int occurrences_work(const string& infile_name, OccurrenceMap& local_map, int from, int to){
	/*
	 * Working function for counting the occurrences of characters in a file chunk.
	*/

	char c;
    int i= from;
	ifstream infile;

	infile.open(infile_name);
	infile.seekg(from,ios::beg);

	while(i<to && infile.get(c)){
		local_map.insert(c);
		i++;
    }
	infile.close();
	return 1;
	}

void find_occurrences(const string &infile_name,MapQueue &maps, OccurrenceMap &words){
    /*
	 * Emitter function for counting the occurrences of characters in the input file.
	 * It calculates the chunk size and distributes the task to the workers.
	*/

    auto size = std::filesystem::file_size(infile_name);
	int nw = thread_pool->getWorkersNumber();
	int delta = size/nw;
	vector<future<int>> int_futures;

	for	(int k=0; k<nw;k++){
		int from = k*delta;
		int to = (k == (nw-1) ? size : (k+1)*delta);
		OccurrenceMap *thread_map = new OccurrenceMap(); //every worker gets a local map to fill
		maps.push(thread_map);
		auto f = thread_pool->addJob([&,thread_map,from,to]()-> int {
			return occurrences_work(std::ref(infile_name),std::ref(*thread_map),std::move(from), std::move(to));
		});
		int_futures.push_back(std::move(f));
	}
	for (auto &f : int_futures){
		int val = f.get();       //We wait for the threads to finish and then we can reduce the maps.
	}
	merge_maps(words,maps);
	return;
}

void map_to_queue(OccurrenceMap &words, priority_queue<HufNode*,vector<HufNode*>, Compare> &pq){
    /*
	 * Creates nodes from the map elements (char-freq) and stores them in the priority queue.
	 * The queue holds items in increasing order of occurrence.
	*/
    for (auto i = words.beginIterator();
        i != words.endIterator(); i++){
        HufNode* node = new HufNode(i->first, i->second);
        pq.push(node);
    }
}

void generate_codeMap(CodeMap &char_code_map, HufNode* node, string code){
    /*
 	* Generates a map which contains pairs of characters and their relative huffman code
	*/
     if(node){
         if(node->character != SPECIAL_CHAR){
            char_code_map.appendValue(node->character,code);
        }
        generate_codeMap(char_code_map,node->left,code + "0");
        generate_codeMap(char_code_map,node->right,code + "1");
    }
}

HufNode* generateTree(priority_queue<HufNode*,vector<HufNode*>, Compare> &pq){
    /*
 	* generates the Huffman tree. 'SPECIAL_CHAR' as data indicates a non-leaf node.
	*/
    while (pq.size() != 1) {
        HufNode* left = pq.top();
        pq.pop();
        HufNode* right = pq.top();
        pq.pop();
        HufNode* node = new HufNode(SPECIAL_CHAR,
                                  left->frequency + right->frequency);
        node->left = left;
        node->right = right;
        pq.push(node);
    }

    return pq.top();
}

int bytes_to_bits(queue<string> &bytesQueue, mutex &m, condition_variable &cv, const string &outfile_name){
	/*
 	* Last pipeline stage for the creation of the output file.
 	* We use a 64 bit variable initialized to 0 as buffer and perform
    * a bitwise shift whenever we need to write a '1'.
	* If we need a '0' we just skip to the next bit (we don't need to shift).
	*/

	uint64_t bits = 0;
	ofstream outfile;
	outfile.open(outfile_name, ios::binary);
	string byte_str;
	int i=0;
	{
		unique_lock<mutex> lock(m);
		cv.wait(lock, [&] {return !bytesQueue.empty();});
		byte_str = bytesQueue.front();
		bytesQueue.pop();
	}
	auto iter = byte_str.begin();
	while(true){ //will be interrupted by EOS
		while (i < 64 && iter != byte_str.end()){
			if( *iter == '1' ) bits |= 1 << (63-i); //only bit-shift for the 1s, 0s are ignored

			else if (*iter == THREAD_EOS) { // we flush the last buffer to file even if incomplete
				outfile.write(reinterpret_cast<const char *>(&bits), sizeof(bits));
				outfile.close();
				return 1;
			}
		i++;
		iter++;
		}
		if (iter == byte_str.end()){ // if we processed the whole string, we pop the next one from the queue
			{
				unique_lock<mutex> lock(m);
				cv.wait(lock, [&] {return !bytesQueue.empty();});
				byte_str = bytesQueue.front();
				bytesQueue.pop();
				iter = byte_str.begin();
			}
		}
		if (i == 64){ // if the buffer is full, we write to file and clear the buffer
		outfile.write(reinterpret_cast<const char *>(&bits), sizeof(bits));
		bits=0;
		i=0;
		}
	}
	outfile.close();
	return -1; //never reached
}

int buffers_to_codes(queue<vector<char>> &in_bufferQueue, mutex &m, condition_variable &cv, CodeMap& codeMap, const string &outfile_name){

    /*
 	* Second pipeline stage for the creation of the output file.
 	* We take characters from the buffers generated by the previous stage, translate them to their correspondent
 	* huffman code, and store them in the queue for the next stage.
	*/

	char c;
	queue<string> out_bufferQueue;
	string out_buffer;
	vector<char> in_buffer;
	int i=0;
    future<int> fut;

	//to be sent to next pipeline stage
	mutex mutex_next;
	condition_variable cv_next;

	{
		unique_lock<mutex> lock(m);
		cv.wait(lock, [&] {return !in_bufferQueue.empty();});
		in_buffer = in_bufferQueue.front();
		in_bufferQueue.pop();
	}

	auto iter = in_buffer.begin();
	fut = thread_pool->addJob( [&out_bufferQueue,&mutex_next,&cv_next,&outfile_name] () -> int {return bytes_to_bits(std::ref(out_bufferQueue), mutex_next, cv_next, std::ref(outfile_name));});
	while (true){
		while (iter != in_buffer.end() && out_buffer.size() < BUFSIZE){ //out_buffer is not guaranteed to be exactly of BUFSIZE, but close
			c = *iter;
			if(c==THREAD_EOS){
				out_buffer.append(1,THREAD_EOS); //if we get EOS, we propagate and wait for next stage to finish
				{
					lock_guard<mutex> lock(mutex_next);
					out_bufferQueue.push(out_buffer);
				}
				cv_next.notify_one();
				auto v=fut.get();
				return 1;			//exit to main thread
			}
		out_buffer.append(codeMap.getCode(c));
		iter++;
		}

		if(iter == in_buffer.end()){
			{
				unique_lock<mutex> lock(m);
				cv.wait(lock, [&] {return !in_bufferQueue.empty();});
				in_buffer = in_bufferQueue.front();
				in_bufferQueue.pop();
				iter = in_buffer.begin();
			}
		}
		if(out_buffer.size() >= BUFSIZE ){
			{
				lock_guard<mutex> lock(mutex_next);
				out_bufferQueue.push(out_buffer);
			}
			cv_next.notify_one();
			out_buffer.clear();
			//i=0;
		}
	}
	return -1; //never reached because of while(true)
}

void encode_to_file(const string& infile_name, const string &outfile_name, CodeMap& codeMap){
	/*
 	* Pipeline source for the creation of the output file.
 	* Reads characters from the file, puts them into buffers and calls the next pipeline stage.
    * The buffers are sent via a shared synchronized queue
	*/
	queue<vector<char>> bufferQueue;
	vector<char> buffer;
	char c;
	int i=0;
	mutex m;
	condition_variable cv;

	ifstream infile;
	infile.open(infile_name);
	future<int> fut = thread_pool->addJob( [&bufferQueue,&m, &cv, &codeMap, &outfile_name] () -> int {return buffers_to_codes(std::ref(bufferQueue),m,cv,codeMap, std::ref(outfile_name));});
	while(true){
		i=0;
		while(i<BUFSIZE){
			if(infile.get(c)){
				buffer.push_back(c);
				i++;
			}
			else{
				buffer.push_back(THREAD_EOS);//send THREAD_EOS
				{
				lock_guard<mutex> lock(m);
				bufferQueue.push(buffer);
				}
				cv.notify_one();
				auto v = fut.get();
				return;
			}
		}
	{
		lock_guard<mutex> lock(m);
		bufferQueue.push(buffer);
	}
	cv.notify_one();
	buffer.clear();
	}
}

void compress(const string &infile_name, const string &outfile_name){
    /*
 	* Main procedure of the program. Creates the data structures and calls all the functions of the application, timing their execution.
	*/

    priority_queue<HufNode*,vector<HufNode*>,Compare> pQueue;
    CodeMap *char_to_code_map = new CodeMap();
	MapQueue maps;
	OccurrenceMap *words = new OccurrenceMap();
	HufNode* huffmanTree;

	{
	utimer t1("count occurrences");
	find_occurrences(infile_name,maps,*words);
	}

	{
	utimer t5("map to queue ");
	map_to_queue(*words,pQueue);
	}

	{
	utimer t6("generate tree");
    huffmanTree = generateTree(pQueue);
    }

	{
	utimer t7("generate char to code");
    generate_codeMap(*char_to_code_map, huffmanTree, "");
    }

	{
	utimer t8("print to file");
	encode_to_file(infile_name,outfile_name,*char_to_code_map);
	}
}

int main(int argc, char* argv[])
{
    if (argc < 3 || atoi(argv[3]) == 1 || strcmp(argv[1],"-h")==0 || strcmp(argv[1], "--help")==0 ){
        cout << "Usage is: [file to compress] [compressed file name] [nw>1]" << endl;
        cout << "Nw (if specified) has to be at least 2 in order for the pipeline to work" << endl;
        return EXIT_FAILURE;
    }
    int nw=atoi(argv[3]);   // par degree
	thread_pool->start(nw);
    string infile_name = (argv[1]);
    string outfile_name = (argv[2]);
    {
        utimer t0("whole program");
        compress(infile_name, outfile_name);
    }
	thread_pool->stop();
    return 0;
}

