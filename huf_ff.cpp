#include <iostream>
#include <fstream>
#include <filesystem>
#include "utils.hpp"
#include "OccurrenceMap.hpp"
#include "CodeMap.hpp"
#include "MapQueue.hpp"
#include <future>
#include <cstring>
#include <ff/ff.hpp>
#include <ff/parallel_for.hpp>
#include "ThreadPool.hpp"
#include "utimer.cpp"
#include <mutex>

#define BUFSIZE 4096
#define EOS '\0'

using namespace std;
using namespace ff;

ThreadPool *thread_pool = new ThreadPool();
atomic<int> toCompute=1;

int merge_maps_work(mutex &m, MapQueue &maps){
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

void merge_maps(OccurrenceMap &final, MapQueue& maps){
	int nw = thread_pool->getWorkersNumber();
	int n=nw/2;
	mutex m;
	while(n>1){
		toCompute+=n;
		n=n/2;
	}
	vector<future<int>> futures;
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
			final.updateValue(m->getKey(i), m->getValue(i));
		}
		maps.pop();
	}
	return;
}

int occurrences_work(const string& infile_name, OccurrenceMap& local_map, int from, int to){
	char c;
	ifstream infile;
	infile.open(infile_name);
	infile.close();
	return 1;
	}

void find_occurrences(const string &infile_name,MapQueue &maps, OccurrenceMap &words){
/*master worker computes file chunks according to size and assigns work to workers
	{
	utimer t1("conta occorrenze");
    auto size = std::filesystem::file_size(infile_name);
	int nw = thread_pool->getWorkersNumber();
	int delta = size/nw;
	vector<future<int>> int_futures;
	for (int k=0; k<nw;k++){
		int from = k*delta;
		int to = (k == (nw-1) ? size : (k+1)*delta);
		OccurrenceMap *thread_map = new OccurrenceMap();
		maps.push(thread_map);
		auto f = thread_pool->addJob([&,thread_map,from,to]()-> int {
			 return occurrences_work(std::ref(infile_name),std::ref(*thread_map),std::move(from), std::move(to));
			});
			int_futures.push_back(std::move(f));
	}
	for (auto &f : int_futures){
		int val = f.get();
	}
	}
	{
	utimer t3("Maps merge");
	merge_maps(words,maps);
	}
	return;*/
	auto size = std::filesystem::file_size(infile_name);
	int nw= thread_pool->getWorkersNumber();
	ParallelFor pf(nw);
	ifstream infile;
	infile.open(infile_name);
	int chunksize = size/nw;
	pf.parallel_for(0,size,1,chunksize, [&] (int from) {
		char c;
		infile.seekg(from,ios::beg);
		infile.get(c);
		words.insert(c);
	},nw);
	infile.close();
	print_map(words);
	return;
}

void map_to_queue(OccurrenceMap &words, priority_queue<HufNode*,vector<HufNode*>, Compare> &pq){
    for (auto i = words.beginIterator();
        i != words.endIterator(); i++){
        HufNode* node = new HufNode(i->first, i->second);
        pq.push(node);
    }
}

void generate_char_to_code_map(CodeMap &char_code_map, HufNode* node, string code){
     if(node){
         if(node->character != SPECIAL_CHAR){
            char_code_map.appendValue(node->character,code);
        }
        generate_char_to_code_map(char_code_map,node->left,code + "0");
        generate_char_to_code_map(char_code_map,node->right,code + "1");
    }
}

HufNode* generateTree(priority_queue<HufNode*,vector<HufNode*>, Compare> &pq){

    while (pq.size() != 1) { //not able to vectorize
        HufNode* left = pq.top();
        pq.pop();
        HufNode* right = pq.top();
        pq.pop();
        //'$' as data indicates a non-leaf node
        HufNode* node = new HufNode(SPECIAL_CHAR,
                                  left->frequency + right->frequency);
        node->left = left;
        node->right = right;
        pq.push(node);
    }

    return pq.top();
}

int bytes_to_bits(queue<string> &bytesQueue, mutex &m, condition_variable &cv, const string &outfile_name){
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
	while(true){
		while (i < 64 && iter != byte_str.end()){
			if( *iter == '1' ) bits |= 1 << (63-i);

			else if (*iter == EOS) {
				outfile.write(reinterpret_cast<const char *>(&bits), sizeof(bits));
				outfile.close();
				return 1;
			}
		i++;
		iter++;
		}
		if (iter == byte_str.end()){
			{
				unique_lock<mutex> lock(m);
				cv.wait(lock, [&] {return !bytesQueue.empty();});
				byte_str = bytesQueue.front();
				bytesQueue.pop();
				iter = byte_str.begin();
			}
		}
		if (i == 64){
		outfile.write(reinterpret_cast<const char *>(&bits), sizeof(bits));
		bits=0;
		i=0;
		}
	}
	outfile.close();
	return -1; //never reached
}

int buffers_to_codes(queue<vector<char>> &in_bufferQueue, mutex &m, condition_variable &cv, CodeMap& codeMap, const string &outfile_name){
	//PREALLOCATE VECTORS
	char c;
	queue<string> out_bufferQueue;
	string out_buffer;
	vector<char> in_buffer;
	int i=0;

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

	future<int> fut;
	fut = thread_pool->addJob( [&out_bufferQueue,&mutex_next,&cv_next,&outfile_name] () -> int {return bytes_to_bits(std::ref(out_bufferQueue), mutex_next, cv_next, std::ref(outfile_name));});
	while (true){
		while (iter != in_buffer.end() && out_buffer.size() < BUFSIZE){
			c = *iter;
			if(c==EOS){
				out_buffer.append(1,EOS);
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
	//REPLACE VECCTOR PUSH Bck with pre-allocated
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
				buffer.push_back(EOS);//send EOS
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
    priority_queue<HufNode*,vector<HufNode*>,Compare> pQueue;
    CodeMap *char_to_code_map = new CodeMap();
	MapQueue maps;
	OccurrenceMap *words = new OccurrenceMap();
	{
	utimer t4("count occurrences");
	find_occurrences(infile_name,maps,*words);
	}
	HufNode* huffmanTree;
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
    generate_char_to_code_map(*char_to_code_map, huffmanTree, "");
    }

	{
	utimer t8("print to file");
	encode_to_file(infile_name,outfile_name,*char_to_code_map);
	}
}

int main(int argc, char* argv[])
/*
aggiungere controllo carattere speciale
*/
{
	cout << argc << endl;
    if (argc < 3 || strcmp(argv[1],"-h")==0 || strcmp(argv[1], "--help")==0 ){
        cout << "Usage is: [file to compress] [compressed file name] [nw](optional)" << endl;
		cout << "If [nw] is omitted, the maximum nw allowed by the system will be used" << endl;
        return EXIT_FAILURE;
    }
    int nw = (argc > 3 ? atoi(argv[3]) : 0);   // par degree
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
